#include "system/includes.h"
#include "media/includes.h"
#include "tone_player.h"
#include "audio_config.h"
#include "app_main.h"
#include "file_operate/file_operate.h"
#include "fm_emitter/fm_emitter_manage.h"
#include "clock_cfg.h"
#include "audio_dec.h"
#include "app_action.h"

#define END_NORMAL    0
#define END_ABNORMAL  1

#define TONE_SRC_OUT_BUF_LEN			(512+4*2)
#define TONE_SRC_IN_BUF_LEN(isr,osr)	((isr) * ((TONE_SRC_OUT_BUF_LEN-4*2)/2) / (osr))

#define LOG_TAG_CONST   APP_TONE
#define LOG_TAG         "[APP-TONE]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"

#define TONE_LIST_MAX_NUM 4


#define TONE_FILE_NOR  0
#define TONE_FILE_DEV  1

#define INPUT_FILE_TYPE  TONE_FILE_DEV
#define FILE_DEC_FASTEST_TYPE_MASK	(AUDIO_CODING_FLAC | AUDIO_CODING_APE | AUDIO_CODING_DTS)
#define FILE_DEC_FAST_TYPE_MASK		(AUDIO_CODING_M4A | AUDIO_CODING_AMR)

struct tone_file_handle {
    u8 start : 1;
    u8 dec_mix : 1;
    u8 end_flag : 1;
    u8 idx;
    u8 repeat_begin;
    u16 loop;
    u32 magic;
    void *file;
    const char **list;
    enum audio_channel channel;
    struct audio_decoder decoder;

    u8 ch_num;
    u16 src_out_sr;
    struct audio_src_handle *hw_src;
    struct audio_mixer_ch mix_ch;

    u8 read_err;
    u8 dev_file_en;
    u8 remain;
};

struct tone_sine_handle {
    u8 start;
    u8 repeat;
    u32 sine_id;
    u32 sine_offset;
    void *sin_maker;
    struct audio_decoder decoder;
    struct audio_mixer_ch mix_ch;
    struct sin_param sin_dynamic_params[8];
    u8 dec_mix;
};

struct tone_dec_handle {
    u8 r_index;
    u8 w_index;
    u8 list_cnt;
    u8 preemption;
    volatile u8 is_mode;
    const char **list[4];
    struct audio_res_wait wait;
    const char *user_evt_owner;
    void (*user_evt_handler)(void *priv);
    void *priv;
};


extern struct audio_mixer mixer;
extern struct audio_decoder_task decode_task;

static struct tone_file_handle *file_dec;
static struct tone_sine_handle *sine_dec;
static char *single_file[2] = {NULL};
struct tone_dec_handle *tone_dec;

int sine_dec_close(u8 end_flag);
int tone_dec_stop(u8 end_flag);
int tone_dec_start();

u32 audio_output_rate(int input_rate);
u32 audio_output_channel_num(void);
int audio_output_set_start_volume(u8 state);
void tone_set_user_event_handler(struct tone_dec_handle *dec, void (*user_evt_handler)(void *priv), void *priv);
static int tone_event_handler(struct tone_dec_handle *dec, u8 end_flag);

static u8 tone_dec_idle_query()
{
    if (file_dec || sine_dec) {
        return 0;
    }
    return 1;
}
REGISTER_LP_TARGET(tone_dec_lp_target) = {
    .name = "tone_dec",
    .is_idle = tone_dec_idle_query,
};


void tone_event_to_user(u8 event)
{
    struct sys_event e;
    e.type = SYS_DEVICE_EVENT;
    e.arg  = (void *)DEVICE_EVENT_FROM_TONE;
    e.u.dev.event = event;
    e.u.dev.value = 0;
    sys_event_notify(&e);
}
void tone_event_clear()
{
    struct sys_event e = {0};
    e.type = SYS_DEVICE_EVENT;
    e.arg  = (void *)DEVICE_EVENT_FROM_TONE;
    sys_event_clear(&e);
}
static char *get_file_ext_name(char *name)
{
    int len = strlen(name);
    char *ext = (char *)name;

    while (len--) {
        if (*ext++ == '.') {
            break;
        }
    }
	if (len <= 0){
		ext = name + (strlen(name)-3);
	}

    return ext;
}

static void tone_file_dec_release()
{
    if (file_dec) {
        if (file_dec->file) {
            fclose(file_dec->file);
        }

        local_irq_disable();
        free(file_dec);
        file_dec = NULL;
        local_irq_enable();
    }
}
static OS_MUTEX mutex;
static u8 mflag = 0;
int tone_dec_open(const char **list, u8 preemption);

void tone_dec_release(u8 is_mode_initiative, u8 end_flag)
{
    if (!tone_dec) {
        return;
    }
    log_info("is_mode_initiative = %d, tone_dec->is_mode = %d\n", is_mode_initiative, tone_dec->is_mode);
    if (is_mode_initiative && tone_dec->is_mode) {
        log_info("is_mode_initiative , no need callback\n");
    } else {
        tone_event_handler(tone_dec, end_flag);
    }

    if (tone_dec->wait.handler) {
        audio_decoder_task_del_wait(&decode_task, &tone_dec->wait);
    }

    for (int i = 0; i < TONE_LIST_MAX_NUM; i++) {
        if (tone_dec->list[i]) {
            free(tone_dec->list[i]);
        }
    }

    clock_remove(DEC_TONE_CLK);

    local_irq_disable();
    free(tone_dec);
    tone_dec = NULL;
    local_irq_enable();
}

void tone_dec_end_handler(int event, u8 end_flag)
{

    if (++tone_dec->r_index >= TONE_LIST_MAX_NUM) {
        tone_dec->r_index = 0;
    }
    if (--tone_dec->list_cnt > 0) {
        tone_dec_open(tone_dec->list[tone_dec->r_index], tone_dec->preemption);
    } else {
        tone_dec_release(0, end_flag);
    }

    if (!end_flag) {
        tone_event_to_user(event);
    }
}


static int tone_file_list_repeat(struct audio_decoder *decoder)
{
    int err = 0;

    if (file_dec->dev_file_en == INPUT_FILE_TYPE) {
        log_info("dev_file_en INPUT_FILE_TYPE\n");
        return 0;
    }
    file_dec->idx++;
    if (!file_dec->list[file_dec->idx]) {
        log_info("repeat end 1:idx end");
        return 0;
    }

    if (IS_REPEAT_END(file_dec->list[file_dec->idx])) {
        //log_info("repeat_loop:%d",file_dec->loop);
        if (file_dec->loop) {
            file_dec->loop--;
            file_dec->idx = file_dec->repeat_begin;
        } else {
            file_dec->idx++;
            if (!file_dec->list[file_dec->idx]) {
                log_info("repeat end 2:idx end");
                return 0;
            }
        }
    }

    if (IS_REPEAT_BEGIN(file_dec->list[file_dec->idx])) {
        if (!file_dec->loop) {
            file_dec->loop = TONE_REPEAT_COUNT(file_dec->list[file_dec->idx]);
            log_info("repeat begin:%d", file_dec->loop);
        }
        file_dec->idx++;
        file_dec->repeat_begin = file_dec->idx;
    }

    log_info("repeat idx:%d,%s", file_dec->idx, file_dec->list[file_dec->idx]);
    file_dec->file = fopen(file_dec->list[file_dec->idx], "r");
    if (!file_dec->file) {
        log_error("repeat end:fopen repeat file faild");
        return 0;
    }

    return 1;
}

static int tone_dec_close(u8 rpt)
{
    if (!file_dec) {
        return 0;
    }
    if (file_dec->start) {
        audio_decoder_close(&file_dec->decoder);
    }

    if (file_dec->hw_src) {
        audio_hw_resample_close(file_dec->hw_src);
        file_dec->hw_src = NULL;
    }
    if (file_dec->start) {
        audio_mixer_ch_close(&file_dec->mix_ch);
        file_dec->start = 0;
    }

    if (!rpt) {
        if (app_audio_get_state() == APP_AUDIO_STATE_WTONE) {
            app_audio_state_exit(APP_AUDIO_STATE_WTONE);
        }
    }

    return 0;
}

static void tone_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    int repeat = 0;
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
    case AUDIO_DEC_EVENT_ERR:

        os_mutex_pend(&mutex, 0);
        if (file_dec) {
            if (argv[1] != file_dec->magic) {
                log_error("file_dec magic no match:%d-%d", argv[1], file_dec->magic);
                os_mutex_post(&mutex);
                break;
            }
            repeat = tone_file_list_repeat(decoder);
            log_info("AUDIO_DEC_EVENT_END,err=%x,repeat=%d\n", argv[0], repeat);

            if (repeat) {
                tone_dec_close(repeat);
                tone_dec_start();
            } else {
                tone_dec_stop(file_dec->end_flag);
                audio_decoder_resume_all(&decode_task);
            }
        }
        os_mutex_post(&mutex);
        break;
    default:
        return;
    }
}

int tone_get_status()
{
    return tone_dec ? TONE_START : TONE_STOP;
}

int tone_get_dec_status()
{
    if (tone_dec && file_dec && (file_dec->decoder.state != DEC_STA_WAIT_STOP)) {
        return TONE_START;
    }
    if (tone_dec && sine_dec && (sine_dec->decoder.state != DEC_STA_WAIT_STOP)) {
        return TONE_START;
    }
    return 	TONE_STOP;
}

int tone_dec_wait_stop(u32 timeout_ms)
{
    u32 to_cnt = 0;
    while (tone_get_dec_status()) {
        /* putchar('t'); */
        os_time_dly(1);
        if (timeout_ms) {
            to_cnt += 10;
            if (to_cnt >= timeout_ms) {
                break;
            }
        }
    }
    return tone_get_dec_status();
}

static int tone_fread(struct audio_decoder *decoder, void *buf, u32 len)
{
    int rlen = 0;

    if (!file_dec->file) {
        return 0;
    }

    rlen = fread(file_dec->file, buf, len);

    if (file_dec->dev_file_en == INPUT_FILE_TYPE) {
        if (rlen > len) {
            rlen = 0;
            file_dec->read_err = 1;
        } else {
            file_dec->read_err = 0;

        }
    } else {
        if (rlen < len) {
            if (file_dec->file) {
                fclose(file_dec->file);
                file_dec->file = NULL;
            }
        }
    }
    return rlen;

}

static int tone_fseek(struct audio_decoder *decoder, u32 offset, int seek_mode)
{
	if (!file_dec->file){
		return -1;
	}

    return fseek(file_dec->file, offset, seek_mode);
}

static int tone_flen(struct audio_decoder *decoder)
{
    void *tone_file = NULL;
    int len = 0;

    if (file_dec->file) {
        len = flen(file_dec->file);
        return len;
    }

    tone_file = fopen(file_dec->list[file_dec->idx], "r");
    if (tone_file) {
        len = flen(tone_file);
        fclose(tone_file);
        tone_file = NULL;
    }
    return len;
}

static int tone_fclose(void *file)
{
    if (file_dec->file) {
        fclose(file_dec->file);
        file_dec->file = NULL;
    }

    file_dec->idx = 0;
    return 0;
}
struct tone_format {
    const char *fmt;
    u32 coding_type;
};

const struct tone_format tone_fmt_support_list[] = {
    {"wtg", AUDIO_CODING_G729},
    {"msbc", AUDIO_CODING_MSBC},
    {"sbc", AUDIO_CODING_SBC},
    {"mty", AUDIO_CODING_MTY},
    {"aac", AUDIO_CODING_AAC},
    {"mp3", AUDIO_CODING_MP3},
};

static u32 tone_file_format_match(char *fmt)
{
    int list_num = ARRAY_SIZE(tone_fmt_support_list);
    int i = 0;

    if (fmt == NULL) {
        return AUDIO_CODING_UNKNOW;
    }

    for (i = 0; i < list_num; i++) {
        if (ASCII_StrCmpNoCase(fmt, tone_fmt_support_list[i].fmt, 4) == 0) {
            return tone_fmt_support_list[i].coding_type;
        }
    }

    return AUDIO_CODING_UNKNOW;
}




#if 0
static u8  msbc_frame[58] __attribute__((aligned(4)));;
static int sbc_dec_get_frame(struct audio_decoder *decoder, u8 **frame)
{
    int len = 58;

    if (!file_dec->file) {
        return 0;
    }

    int rlen = fread(file_dec->file, msbc_frame, len);
    *frame = msbc_frame;
    if (file_dec->dev_file_en == INPUT_FILE_TYPE) {
        if (rlen > len) {
            rlen = 0;
            file_dec->read_err = 1;
        } else {
            file_dec->read_err = 0;

        }

    } else {
        if (rlen < len) {
            if (file_dec->file) {
                fclose(file_dec->file);
                file_dec->file = NULL;
            }
        }
    }
    return rlen;

}

/*
 *free使用完的数据帧占用的内存
 */

static void sbc_dec_put_frame(struct audio_decoder *decoder, u8 *frame)
{


}

/*
 *这里是获取一帧数据用来解析sbc格式参数：声道，采样率...
 *数据以0x9c开头
 */
static int sbc_dec_fetch_frame(struct audio_decoder *decoder, u8 **frame)
{
    return 0;
}


static const struct audio_dec_input tone_msbc_input = {
    .coding_type = AUDIO_CODING_MSBC,
    .data_type   = AUDIO_INPUT_FRAME,
    .ops = {
        .frame = {
            .fget = sbc_dec_get_frame,
            .fput = sbc_dec_put_frame,
            .ffetch = NULL,
        }
    }
};

#endif



static struct audio_dec_input tone_input = {
#ifdef CONFIG_CPU_BR22
    .coding_type = AUDIO_CODING_G729,
#elif CONFIG_CPU_BR26
    .coding_type = AUDIO_CODING_G729,
#elif CONFIG_CPU_BR23
    .coding_type = AUDIO_CODING_G729,
#elif CONFIG_CPU_BR25
    .coding_type = AUDIO_CODING_G729,
#elif CONFIG_CPU_BR18
    .coding_type = AUDIO_CODING_MP3,
#endif
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
            .fread = tone_fread,
            .fseek = tone_fseek,
            .flen  = tone_flen,
        }
    }
};

static const u32 file_input_coding_more[] = {
#if (defined(TCFG_DEC_MP3_ENABLE) && (TCFG_DEC_MP3_ENABLE))
    AUDIO_CODING_MP3,
#endif
    0,
};

static struct audio_dec_input file_input = {
    .coding_type = 0
#if (defined(TCFG_DEC_WMA_ENABLE) && (TCFG_DEC_WMA_ENABLE))
    | AUDIO_CODING_WMA
#endif
#if (defined(TCFG_DEC_WAV_ENABLE) && (TCFG_DEC_WAV_ENABLE))
    | AUDIO_CODING_WAV
#endif
#if (defined(TCFG_DEC_FLAC_ENABLE) && (TCFG_DEC_FLAC_ENABLE))
    | AUDIO_CODING_FLAC
#endif
#if (defined(TCFG_DEC_APE_ENABLE) && (TCFG_DEC_APE_ENABLE))
    | AUDIO_CODING_APE
#endif
#if (defined(TCFG_DEC_M4A_ENABLE) && (TCFG_DEC_M4A_ENABLE))
    | AUDIO_CODING_M4A
#endif
#if (defined(TCFG_DEC_AMR_ENABLE) && (TCFG_DEC_AMR_ENABLE))
    | AUDIO_CODING_AMR
#endif
#if (defined(TCFG_DEC_DTS_ENABLE) && (TCFG_DEC_DTS_ENABLE))
    | AUDIO_CODING_DTS
#endif
#if (defined(TCFG_DEC_G726_ENABLE) && (TCFG_DEC_G726_ENABLE))
    | AUDIO_CODING_G726
#endif
    ,
    .p_more_coding_type = (u32 *)file_input_coding_more,
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
            .fread = tone_fread,
            .fseek = tone_fseek,
            .flen  = tone_flen,
        }
    }
};


static int tone_dec_probe_handler(struct audio_decoder *decoder)
{
    return 0;
}

#define MONO_TO_DUAL_POINTS 30
/*extern OS_SEM dac_sem;*/
static int tone_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    int wlen = 0, tmp_len = 0;
    int total_len = len;
    struct tone_file_handle *dec = container_of(decoder, struct tone_file_handle, decoder);
    s16 xch_data[MONO_TO_DUAL_POINTS * 2];
    s16 point_num = 0;
    u8 mono_to_xchannel = 0;//单声道转换成x声道
    s16 *mono_data = (s16 *)data;
    u16 remain_points = (len >> 1);
    u16 convert_len = 0;


    if (!dec->remain) {
#if ((defined(DEC_OUT_OTHER_DATA) && (DEC_OUT_OTHER_DATA)))
        /* printf("dec->ch_num %d\n", dec->ch_num, dec->decoder.fmt.sample_rate); */
        other_audio_dec_output(decoder, data, len, dec->ch_num, dec->decoder.fmt.sample_rate);
#endif
    }


    if (dec->decoder.fmt.coding_type == AUDIO_CODING_MSBC && dec->ch_num == 2) {
        mono_to_xchannel = 2;
        do {
            point_num = MONO_TO_DUAL_POINTS;
            if (point_num >= remain_points) {
                point_num = remain_points;
            }
            convert_len = point_num * mono_to_xchannel * 2;
            audio_pcm_mono_to_dual(xch_data, mono_data, point_num);

            if (dec->hw_src) {
                /* putchar('s'); */
                tmp_len = audio_src_resample_write(dec->hw_src, xch_data, convert_len);
            } else {

                tmp_len = audio_mixer_ch_write(&dec->mix_ch, xch_data, convert_len);
            }
            wlen += tmp_len;
            remain_points -= (tmp_len / 2 / mono_to_xchannel);
            if (tmp_len < convert_len) {
                break;
            }
            mono_data += point_num;
        } while (remain_points);

        if (remain_points == 0) {
            dec->remain = 0;

        } else {
            dec->remain = 1;
        }
    } else {
        do {
            if (dec->hw_src) {
                /* putchar('s'); */
                wlen = audio_src_resample_write(dec->hw_src, data, total_len);
            } else {
                wlen = audio_mixer_ch_write(&dec->mix_ch, data, total_len);
            }
            if (!wlen) {
                /* putchar('z'); */
                /* printf("tone wlen 0 break\n"); */
                break;
            }
            /* if (!file_dec->hw_src){ */
            /* printf("dec->hw_src %x, %x\n", file_dec->hw_src, dec->hw_src);	 */
            /* } */
            total_len -= wlen;
            data += wlen / 2;
        } while (total_len);

        if (total_len == 0) {
            dec->remain = 0;
            /* putchar('A'); */

        } else {
            /* putchar('B'); */
            dec->remain = 1;
        }
    }

	return mono_to_xchannel ? (wlen >> 1) : (len - total_len);
}

#ifdef CONFIG_TONE_LOCK_BY_BT_TIME
// 使用蓝牙时间做精准对齐

#define     APP_IO_DEBUG_0(i,x)       // {JL_PORT##i->DIR &= ~BIT(x), JL_PORT##i->OUT &= ~BIT(x);}
#define     APP_IO_DEBUG_1(i,x)       // {JL_PORT##i->DIR &= ~BIT(x), JL_PORT##i->OUT |= BIT(x);}

#define TONE_OUT_DELAY_BT_TIME			(6)
#define TONE_OUT_DELAY_TIME_US			(TONE_OUT_DELAY_BT_TIME*625)
#define TONE_OUT_DELAY_USE_EMPTY_BUF	(0)	// 1-填空buf，0-死等

struct tone_lock_bt_time {
    u16 tmr;
    u16 empty_len;
    u32 bt_time;
};

static struct tone_lock_bt_time tone_lock_bt_time_hdl = {0};
static struct tone_lock_bt_time *p_tone_lock_hdl = (&tone_lock_bt_time_hdl);

extern u32 audio_bt_time_read(u16 *bt_phase, s32 *bt_bitoff);

static void tone_out_lock_timer(void *priv)
{
    u32 cur_tmr = audio_bt_time_read(NULL, NULL);
    if (cur_tmr >= p_tone_lock_hdl->bt_time) {
        /* putchar('&'); */
        sys_hi_timer_del(p_tone_lock_hdl->tmr);
        p_tone_lock_hdl->tmr = 0;
        audio_decoder_resume_all(&decode_task);
    }
}
void tone_out_start_by_bt_time(u32 bt_time)
{
    local_irq_disable();
    if (p_tone_lock_hdl->tmr) {
        sys_hi_timer_del(p_tone_lock_hdl->tmr);
        p_tone_lock_hdl->tmr = 0;
    }
    p_tone_lock_hdl->empty_len = 0;
    p_tone_lock_hdl->bt_time = bt_time;
    p_tone_lock_hdl->tmr = sys_hi_timer_add(NULL, tone_out_lock_timer, 2);
    local_irq_enable();
}
static int tone_dec_output_by_bt_time_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    if (p_tone_lock_hdl->tmr) {
        return 0;
    } else if (p_tone_lock_hdl->bt_time) {
        u16 phase = 0;
        s32 bitoff = 0;
        u32 cur_tmr = audio_bt_time_read(&phase, &bitoff);
        int offset_us = (cur_tmr - p_tone_lock_hdl->bt_time) * 625 + phase - bitoff;
        printf("cur:%d, bt:%d, ph:%d, bo:%d, offset_us:%d \n", cur_tmr, p_tone_lock_hdl->bt_time, phase, bitoff, offset_us);
        if (offset_us < ((int)TONE_OUT_DELAY_TIME_US * (-1))) {
        } else if (offset_us > TONE_OUT_DELAY_TIME_US) {
        } else {
#if TONE_OUT_DELAY_USE_EMPTY_BUF
            p_tone_lock_hdl->empty_len = (TONE_OUT_DELAY_TIME_US - offset_us) * decoder->fmt.sample_rate / 1000000 * 2;
            if (decoder->fmt.channel == 2) {
                p_tone_lock_hdl->empty_len *= 2;
            }
            printf("emp:%d \n", p_tone_lock_hdl->empty_len);
#else
            u32 cnt = 2000 * TONE_OUT_DELAY_BT_TIME;
            /* APP_IO_DEBUG_1(A, 2); */
            while (cnt--) {
                cur_tmr = audio_bt_time_read(&phase, &bitoff);
                offset_us = (cur_tmr - p_tone_lock_hdl->bt_time) * 625 + phase - bitoff;
                if (offset_us > TONE_OUT_DELAY_TIME_US) {
                    /* APP_IO_DEBUG_0(A, 2); */
                    printf("bt time ok, cnt:%d \n", cnt);
                    break;
                } else if (offset_us < ((int)TONE_OUT_DELAY_TIME_US * (-1))) {
                    break;
                }
            }
            /* APP_IO_DEBUG_0(A, 2); */
#endif
        }
        p_tone_lock_hdl->bt_time = 0;
    }
#if TONE_OUT_DELAY_USE_EMPTY_BUF
    if (p_tone_lock_hdl->empty_len) {
        u16 wlen;
        s16 tmp_buf[16];
        memset(tmp_buf, 0, sizeof(tmp_buf));
        do {
            wlen = p_tone_lock_hdl->empty_len;
            if (wlen > sizeof(tmp_buf)) {
                wlen = sizeof(tmp_buf);
            }
            wlen = tone_dec_output_handler(decoder, tmp_buf, wlen, priv);
            if (!wlen) {
                return 0;
            }
            p_tone_lock_hdl->empty_len -= wlen;
        } while (p_tone_lock_hdl->empty_len);
    }
#endif
    return tone_dec_output_handler(decoder, data, len, priv);
}
#endif /* CONFIG_TONE_LOCK_BY_BT_TIME */

static int tone_dec_post_handler(struct audio_decoder *decoder)
{
    return 0;
}

const struct audio_dec_handler tone_dec_handler = {
    .dec_probe  = tone_dec_probe_handler,
#ifdef CONFIG_TONE_LOCK_BY_BT_TIME
    .dec_output = tone_dec_output_by_bt_time_handler,
#else
    .dec_output = tone_dec_output_handler,
#endif
    .dec_post   = tone_dec_post_handler,
};


static void tone_dec_set_output_channel(struct tone_file_handle *dec, u8 file_channel)
{
    int state;
    enum audio_channel channel;

    u8 ch_num = audio_output_channel_num();
    dec->ch_num = ch_num;

    if (ch_num == 1) {
        if (file_channel == 2) {
            channel = AUDIO_CH_DIFF;
        } else {
            channel = AUDIO_CH_DIFF;
        }
    } else if (ch_num == 2) {
        channel = AUDIO_CH_LR;
    } else {
        channel = AUDIO_CH_DIFF;
    }
    printf("set_channel: %d\n", channel);
    audio_decoder_set_output_channel(&dec->decoder, channel);
    dec->channel = channel;
}

static int tone_dec_src_output_handler(struct audio_decoder *decoder, s16 *data, int len)
{
    int wlen = 0;
    int rlen = len;

    if (!tone_dec || !file_dec || !file_dec->start) {
        /* putchar('O'); */
        return 0;
    }

    do {
        wlen = audio_mixer_ch_write(&file_dec->mix_ch, data, rlen);
        if (!wlen) {
            break;
        }
        data += wlen / 2;
        rlen -= wlen;
    } while (rlen);

    return len - rlen;
}



int tone_dec_start()
{
    int err;
    struct audio_fmt *fmt;
    u8 file_name[16];

    if (!file_dec || !file_dec->file) {
        return -EINVAL;
    }

    

    if (file_dec->dev_file_en == INPUT_FILE_TYPE) {
        err = audio_decoder_open(&file_dec->decoder, &file_input, &decode_task);
	} else {

		fget_name(file_dec->file, file_name, 16);
		tone_input.coding_type = tone_file_format_match(get_file_ext_name((char *)file_name));
		if (tone_input.coding_type == AUDIO_CODING_UNKNOW) {
			log_e("unknow tone file format\n");
			return -EINVAL;
		}
/*        
		if ((tone_input.coding_type == AUDIO_CODING_MSBC) || (tone_input.coding_type == AUDIO_CODING_SBC)) {
			err = audio_decoder_open(&file_dec->decoder, &tone_msbc_input, &decode_task);
		} else {
			err = audio_decoder_open(&file_dec->decoder, &tone_input, &decode_task);
		}
*/        
        err = audio_decoder_open(&file_dec->decoder, &tone_input, &decode_task);
	}

    if (err) {
        log_e("tone file dec decoder open err \n\n\n ");
        return err;
    }

    audio_decoder_set_handler(&file_dec->decoder, &tone_dec_handler);
    /*用于处理DEC_EVENT与当前解码的匹配*/
    file_dec->magic = rand32();
    audio_decoder_set_event_handler(&file_dec->decoder, tone_dec_event_handler, file_dec->magic);

    err = audio_decoder_get_fmt(&file_dec->decoder, &fmt);
    if (err) {
        goto __err1;
    }
/*
    if (fmt->sample_rate == 0 && fmt->channel == 0) {
        fmt->sample_rate = 8 * 1000;
        fmt->channel = 1;
    }
*/
    tone_dec_set_output_channel(file_dec, fmt->channel);

    audio_mixer_ch_open(&file_dec->mix_ch, &mixer);
    file_dec->src_out_sr = 0;
    if (file_dec->dec_mix) {
        file_dec->src_out_sr = audio_mixer_get_sample_rate(&mixer);
    }
    if (!file_dec->src_out_sr) {
        file_dec->src_out_sr = audio_output_rate(fmt->sample_rate);
    }
    audio_mixer_ch_set_sample_rate(&file_dec->mix_ch, file_dec->src_out_sr);
    printf("fmt->sample_rate %d\n", fmt->sample_rate);
    /* printf("mixer sr:[%d]\n\n",audio_mixer_get_sample_rate(&mixer)); */
    /* printf("\n sr:[%d];src sr:[%d] \n\n",fmt->sample_rate,file_dec->src_out_sr); */
    if (fmt->sample_rate != file_dec->src_out_sr) {
        printf("src->sr:%d, or:%d ", fmt->sample_rate, file_dec->src_out_sr);
        file_dec->hw_src = audio_hw_resample_open(&file_dec->decoder, tone_dec_src_output_handler,
                           file_dec->ch_num, fmt->sample_rate, file_dec->src_out_sr);
    }

    if (file_dec->dec_mix && (audio_mixer_get_ch_num(&mixer) > 1)) {
        goto __dec_start;
    }

    audio_output_set_start_volume(APP_AUDIO_STATE_WTONE);




__dec_start:
    clock_add(DEC_TONE_CLK);
    /* clk_set("sys", 240*1000000L); */
    audio_decoder_set_run_max(&file_dec->decoder, 100);
    err = audio_decoder_start(&file_dec->decoder);
    if (err) {
        goto __err1;
    }
    clock_set_cur();
    file_dec->start = 1;

    return 0;

__err1:
    audio_decoder_close(&file_dec->decoder);
    if (file_dec->hw_src) {
        audio_hw_resample_close(file_dec->hw_src);
        file_dec->hw_src = NULL;
    }


    tone_file_dec_release();
    return err;
}

static int tone_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;

    log_info(" tone_wait_res_handler %d\n", event);
    if (event == AUDIO_RES_GET) {
        err = tone_dec_start();
    } else if (event == AUDIO_RES_PUT) {
        tone_dec_stop(END_ABNORMAL);
    }
    return err;
}

/*
 *停止tone_dec
 * */
int tone_dec_stop(u8 end_flag)
{
    log_info("%s end_flag %d\n", __FUNCTION__, end_flag);

    if (!file_dec) {
        log_info("%s out 0\n", __FUNCTION__);
        return 0;
    }
    tone_dec_close(0);
    tone_file_dec_release();
    tone_dec_end_handler(AUDIO_DEC_EVENT_END, end_flag);
    clock_set_cur();
    log_info("%s out\n", __FUNCTION__);

#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))
#if TCFG_APP_RECORD_EN
	//for test cycle play
	extern u8 get_record_replay_mode(void);
	extern void set_record_replay_mode(u8 mark);
	printf("\n-------------------end1-----\n");
	if (true == app_cur_task_check(APP_NAME_RECORD)) {
		if(get_record_replay_mode()){
			printf("\n-------------------end2-----\n");
			struct sys_event e;
			e.type = SYS_KEY_EVENT;
			e.u.key.init = 1;
			e.u.key.type = KEY_DRIVER_TYPE_AD;//区分按键类型
			e.u.key.event = KEY_EVENT_CLICK;
			e.u.key.value = 0;
			e.arg  = (void *)DEVICE_EVENT_FROM_KEY;
			set_record_replay_mode(0);
			sys_event_notify(&e);
		}
	}
#endif
#endif
    return 0;
}












void *sin_tone_open(const struct sin_param *param, int num, u8 channel, u8 repeat);
int sin_tone_make(void *_maker, void *data, int len);
int sin_tone_points(void *_maker);
void sin_tone_close(void *_maker);

/*static const u8 pcm_wav_header[] = {
    'R', 'I', 'F', 'F',         //rid
    0xff, 0xff, 0xff, 0xff,     //file length
    'W', 'A', 'V', 'E',         //wid
    'f', 'm', 't', ' ',         //fid
    0x14, 0x00, 0x00, 0x00,     //format size
    0x01, 0x00,                 //format tag
    0x01, 0x00,                 //channel num
    0x80, 0x3e, 0x00, 0x00,     //sr 16K
    0x00, 0x7d, 0x00, 0x00,     //avgbyte
    0x02, 0x00,                 //blockalign
    0x10, 0x00,                 //persample
    0x02, 0x00,
    0x00, 0x00,
    'f', 'a', 'c', 't',         //f2id
    0x40, 0x00, 0x00, 0x00,     //flen
    0xff, 0xff, 0xff, 0xff,     //datalen
    'd', 'a', 't', 'a',         //"data"
    0xff, 0xff, 0xff, 0xff,     //sameple  size
};*/

static int sine_fread(struct audio_decoder *decoder, void *buf, u32 len)
{
    int offset;
    u8 *data = (u8 *)buf;

    offset = sin_tone_make(sine_dec->sin_maker, data, len);
    sine_dec->sine_offset += offset;

    return offset;
}

static int sine_fseek(struct audio_decoder *decoder, u32 offset, int seek_mode)
{
    sine_dec->sine_offset = 0;
    return 0;
}

static int sine_flen(struct audio_decoder *decoder)
{
    return sin_tone_points(sine_dec->sin_maker) * 2;
}



static const struct audio_dec_input sine_input = {
    .coding_type = AUDIO_CODING_PCM,
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
            .fread = sine_fread,
            .fseek = sine_fseek,
            .flen  = sine_flen,
        }
    }
};


static void sine_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    log_info("sin_dec_event:%x", argv[0]);
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
    case AUDIO_DEC_EVENT_ERR:
        log_info("sine player end\n");
        sine_dec_close(0);
        audio_decoder_resume_all(&decode_task);
        break;
    default:
        return;
    }

}


/*
 * 参数配置:
 * freq : 实际频率 * 512
 * points : 正弦波点数
 * win : 正弦窗
 * decay : 衰减系数(百分比), 正弦窗模式下为频率设置：频率*512
 *
 */
static const struct sin_param sine_16k_normal[] = {
    /*{0, 1000, 0, 100},*/
    {200 << 9, 4000, 0, 100},
};

#if CONFIG_USE_DEFAULT_SINE
static const struct sin_param sine_tws_disconnect_16k[] = {
    /*
    {390 << 9, 4026, SINE_TOTAL_VOLUME / 4026},
    {262 << 9, 8000, SINE_TOTAL_VOLUME / 8000},
    */
    {262 << 9, 4026, 0, 100},
    {390 << 9, 8000, 0, 100},
};

static const struct sin_param sine_tws_connect_16k[] = {
    /*
    {262 << 9, 4026, SINE_TOTAL_VOLUME / 4026},
    {390 << 9, 8000, SINE_TOTAL_VOLUME / 8000},
    */
    {262.298 * 512, 8358, 0, 100},
};

static const struct sin_param sine_low_power[] = {
    {848 << 9, 3613, 0, 50},
    {639 << 9, 3623, 0, 50},
};

static const struct sin_param sine_ring[] = {
    {450 << 9, 24960, 1, 16.667 * 512},
    {0, 16000, 0, 100},
};

static const struct sin_param sine_tws_max_volume[] = {
    {210 << 9, 2539, 0, 100},
    {260 << 9, 7619, 0, 100},
    {400 << 9, 2539, 0, 100},
};
#endif



static void sine_param_resample(struct sin_param *dst, const struct sin_param *src, u32 sample_rate)
{
    u32 coef = (sample_rate << 10) / DEFAULT_SINE_SAMPLE_RATE;

    dst->freq = (src->freq << 10) / coef;
    dst->points = ((u64)src->points * coef) >> 10;
    dst->win = src->win;
    if (src->win) {
        dst->decay = ((u64)src->decay << 10) / coef;
    } else {
        dst->decay = ((u64)SINE_TOTAL_VOLUME * src->decay / 100) / dst->points;
    }
}

static struct sin_param *get_default_sine_param(const struct sin_param *data, u32 sample_rate, u8 data_num)
{
    int i = 0;
    for (i = 0; i < data_num; i++) {
        /*sin_dynamic_params[i].idx_increment = ((u64)data[i].idx_increment << 8) / coef;*/
        sine_param_resample(&sine_dec->sin_dynamic_params[i], data + i, sample_rate);
    }
    return sine_dec->sin_dynamic_params;
}

struct sine_param_head {
    u16 repeat_time;
    u8  set_cnt;
    u8  cur_cnt;
};

static struct sin_param *get_sine_file_param(const char *name, u32 sample_rate, u8 *data_num)
{
    FILE *file;
    struct sine_param_head head;
    struct sin_param param;
    int r_len = 0;
    int i = 0;

    file = fopen(name, "r");
    if (!file) {
        return NULL;
    }

    r_len = fread(file, (void *)&head, sizeof(head));
    if (r_len != sizeof(head)) {
        fclose(file);
        return NULL;
    }

    do {
        r_len = fread(file, (void *)&param, sizeof(param));
        if (r_len != sizeof(param)) {
            break;
        }
        /*
        printf("sine param : \nfreq : %d\npoints : %d\nwin : %d\ndecay : %d\n",
               param.freq, param.points, param.win, param.decay);
        */
        if (!param.points) {
            break;
        }

        if (!param.win) {
            param.decay = param.decay * 100 / 32767;
        }
        sine_param_resample(&sine_dec->sin_dynamic_params[i], (const struct sin_param *)&param, sample_rate);
        i++;
    } while (1);

    *data_num = i;
    fclose(file);

    return sine_dec->sin_dynamic_params;
}

static const struct sin_param *get_sine_param_data(u8 id, u8 *num)
{
    const struct sin_param *param_data;

    switch (id) {
    case SINE_WTONE_NORAML:
        param_data = sine_16k_normal;
        *num = ARRAY_SIZE(sine_16k_normal);
        break;
#if CONFIG_USE_DEFAULT_SINE
    case SINE_WTONE_TWS_CONNECT:
        param_data = sine_tws_connect_16k;
        *num = ARRAY_SIZE(sine_tws_connect_16k);
        break;
    case SINE_WTONE_TWS_DISCONNECT:
        param_data = sine_tws_disconnect_16k;
        *num = ARRAY_SIZE(sine_tws_disconnect_16k);
        break;
    case SINE_WTONE_LOW_POWER:
        param_data = sine_low_power;
        *num = ARRAY_SIZE(sine_low_power);
        break;
    case SINE_WTONE_RING:
        param_data = sine_ring;
        *num = ARRAY_SIZE(sine_ring);
        break;
    case SINE_WTONE_MAX_VOLUME:
        param_data = sine_tws_max_volume;
        *num = ARRAY_SIZE(sine_tws_max_volume);
        break;
#endif
    default:
        return NULL;
    }

    return param_data;
}

static struct sin_param *get_sine_param(u32 sine_id, u32 sample_rate, u8 *data_num)
{
    const struct sin_param *sin_data_param;
    u8 num = 0;

    if (IS_DEFAULT_SINE(sine_id)) {
        sin_data_param = get_sine_param_data(DEFAULT_SINE_ID(sine_id), &num);
        if (!sin_data_param) {
            return NULL;
        }
        *data_num = num;
        return get_default_sine_param(sin_data_param, sample_rate, num);
    } else {
        return get_sine_file_param((const char *)sine_id, sample_rate, data_num);
    }

}


static int sine_dec_probe_handler(struct audio_decoder *decoder)
{
    u8 num = 0;
    const struct sin_param *param;

    if (!sine_dec->sin_maker) {
#if TCFG_APP_FM_EMITTER_EN
        int channel     = 2;
#else
        int channel     = app_audio_output_mode_get() == DAC_OUTPUT_LR ? 2 : 1;
#endif


        int sample_rate = 0;
        if (sine_dec->dec_mix) {
            sample_rate = audio_mixer_get_sample_rate(&mixer);
        }
        if (!sample_rate) {
            sample_rate = app_audio_output_samplerate_get();
        }
        if (get_source_sample_rate()) {
            sample_rate = get_source_sample_rate();
        }
        printf("sine: %d, %d\n", sample_rate, channel);

        param = get_sine_param(sine_dec->sine_id, sample_rate, &num);
        if (!param) {
            return -ENOENT;
        }
        sine_dec->sin_maker = sin_tone_open(param, num, channel, sine_dec->repeat);
        if (!sine_dec->sin_maker) {
            return -ENOENT;
        }
        audio_mixer_ch_set_sample_rate(&sine_dec->mix_ch, audio_output_rate(sample_rate));
    }

    return 0;
}

static int sine_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{

    int wlen = 0;
    int rlen = len;
    /* #if TCFG_APP_FM_EMITTER_EN */
    /* fm_emitter_cbuf_write((u8 *)data, len); */
    /* return len; */
    /* #if 0 */
    /* do { */
    /* wlen = audio_mixer_ch_write(&sine_dec->mix_ch, data, rlen); */
    /* if (!wlen) { */
    /* break; */
    /* } */
    /* data += wlen / 2; */
    /* rlen -= wlen; */
    /* } while (rlen); */
    /* return len; */
    /* #endif */
    /* #else */
    return audio_mixer_ch_write(&sine_dec->mix_ch, data, len);
    /* #endif */
}

static int sine_dec_post_handler(struct audio_decoder *decoder)
{
    return 0;
}

const struct audio_dec_handler sine_dec_handler = {
    .dec_probe  = sine_dec_probe_handler,
    .dec_output = sine_dec_output_handler,
    .dec_post   = sine_dec_post_handler,
};

static void sine_dec_release()
{
    local_irq_disable();
    free(sine_dec);
    sine_dec = NULL;
    local_irq_enable();
}

int sine_dec_start()
{
    int err;
    int decode_task_state;
    struct audio_fmt *fmt;

    if (!sine_dec) {
        return -EINVAL;
    }
    if (sine_dec->start) {
        return 0;
    }

    printf("sine_dec_start: id = %x, repeat = %d\n", sine_dec->sine_id, sine_dec->repeat);

    err = audio_decoder_open(&sine_dec->decoder, &sine_input, &decode_task);
    if (err) {
        return err;
    }
    err = audio_decoder_get_fmt(&sine_dec->decoder, &fmt);

    decode_task_state = audio_decoder_task_wait_state(&decode_task);
    /*
     *以下情况需要独立设置提示音音量
     *(1)抢断播放
     *(2)当前只有提示音一个解码任务
     */
    if (tone_dec->wait.handler && ((tone_dec->wait.preemption == 1) || (decode_task_state == 1))) {
        audio_output_set_start_volume(APP_AUDIO_STATE_WTONE);
    }

    audio_decoder_set_handler(&sine_dec->decoder, &sine_dec_handler);
    audio_decoder_set_event_handler(&sine_dec->decoder, sine_dec_event_handler, 0);

    audio_mixer_ch_open(&sine_dec->mix_ch, &mixer);

    audio_decoder_set_run_max(&sine_dec->decoder, 100);

    clock_add(DEC_TONE_CLK);
    err = audio_decoder_start(&sine_dec->decoder);
    if (err) {
        goto __err2;
    }
    clock_set_cur();
    sine_dec->start = 1;

    return 0;

__err2:
    audio_mixer_ch_close(&sine_dec->mix_ch);
__err1:
    audio_decoder_close(&sine_dec->decoder);
    sine_dec_release();
    return err;

}

static int sine_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;
    log_info("sine_wait_res_handler %d\n", event);
    if (event == AUDIO_RES_GET) {
        err = sine_dec_start();
    } else if (event == AUDIO_RES_PUT) {
        sine_dec_close(1);
    }

    return err;
}

int sine_dec_open(u32 sine_id, u8 repeat, u8 preemption)
{
    int err = 0;

    sine_dec = zalloc(sizeof(*sine_dec));
    if (!sine_dec) {
        log_error("sine_dec zalloc failed");
        return -ENOMEM;
    }

    sine_dec->repeat      = repeat;
    sine_dec->sine_id     = sine_id;

    if (!preemption) {
        tone_dec->wait.protect = 1;
        sine_dec->dec_mix      = 1;
    }
    tone_dec->wait.priority = 3;
    tone_dec->wait.preemption = preemption;
    tone_dec->wait.handler = sine_wait_res_handler;
    printf("sine_dec_open,preemption = %d", preemption);
    audio_decoder_task_add_wait(&decode_task, &tone_dec->wait);


    return err;
}

int sine_dec_close(u8 end_flag)
{
    if (!sine_dec) {
        return 0;
    }

    puts("sine_dec_close\n");

    audio_decoder_close(&sine_dec->decoder);
    audio_mixer_ch_close(&sine_dec->mix_ch);
    if (sine_dec->sin_maker) {
        sin_tone_close(sine_dec->sin_maker);
    }

    if (app_audio_get_state() == APP_AUDIO_STATE_WTONE) {
        app_audio_state_exit(APP_AUDIO_STATE_WTONE);
    }

    sine_dec_release();
    tone_dec_end_handler(AUDIO_DEC_EVENT_END, end_flag);
    clock_set_cur();
    return 0;
}

/*
 *提示音播放，包括正弦波与flash中提示音
 * */
int tone_dec_open(const char **list, u8 preemption)
{
    int err;
    u8 file_name[16];
    char *format = NULL;
    FILE *file = NULL;
    int index = 0;

    if (IS_REPEAT_BEGIN(list[0])) {
        index = 1;
    }

    if (IS_DEFAULT_SINE(list[index])) {
        format = "sin";
    } else {
        file = fopen(list[index], "r");
        if (!file) {
            return -EINVAL;
        }
        fget_name(file, file_name, 16);
        format = get_file_ext_name((char *)file_name);
    }

    if (ASCII_StrCmpNoCase(format, "sin", 3) == 0) {
        if (file) {
            fclose(file);
        }
        /*正弦波参数文件*/
        return sine_dec_open((u32)list[index], index == 1, preemption);
    } else {
        file_dec = zalloc(sizeof(*file_dec));

        file_dec->list = list;
        file_dec->idx  = index;
        file_dec->file = file;
        if (index == 1) {
            file_dec->loop = TONE_REPEAT_COUNT(list[0]);
        }

        if (!preemption) {
            file_dec->dec_mix = 1;
            tone_dec->wait.protect = 1;
        }

        tone_dec->wait.priority = 3;
        tone_dec->wait.preemption = preemption;
        tone_dec->wait.handler = tone_wait_res_handler;
        err = audio_decoder_task_add_wait(&decode_task, &tone_dec->wait);
        return err;
    }
}

static int tone_dec_open_with_callback(u8 is_mode, const char **list, u8 preemption, void (*user_evt_handler)(void *priv), void *priv)
{
    int i = 0;
    int err = 0;

    if (!mflag) {
        mflag = 1;
        os_mutex_create(&mutex);
    }

    os_mutex_pend(&mutex, 0);
    if (!list) {
        os_mutex_post(&mutex);
        return -EINVAL;
    }


    if (tone_dec == NULL) {
        tone_dec = zalloc(sizeof(*tone_dec));
        if (tone_dec == NULL) {
            log_error("tone dec zalloc failed");
            os_mutex_post(&mutex);
            return -ENOMEM;
        }
    }

    while (list[i] != NULL) {
        i++;
    }
    char **p = malloc(4 * (i + 1));
    memcpy(p, list, 4 * (i + 1));

    tone_dec->list[tone_dec->w_index++] = (const char **)p;
    if (tone_dec->w_index >= TONE_LIST_MAX_NUM) {
        tone_dec->w_index = 0;
    }


    if (user_evt_handler) {
        tone_set_user_event_handler(tone_dec, user_evt_handler, priv);
    }

    tone_dec->list_cnt++;
    tone_dec->preemption = preemption;
    tone_dec->is_mode = is_mode;
    if (tone_dec->list_cnt == 1) {
        err = tone_dec_open(tone_dec->list[tone_dec->r_index], tone_dec->preemption);

        if (err == -EINVAL) {
            if (tone_dec->wait.handler) {
                audio_decoder_task_del_wait(&decode_task, &tone_dec->wait);
            }
            local_irq_disable();
            free(p);
            free(tone_dec);
            tone_dec = NULL;
            local_irq_enable();
        }
        os_mutex_post(&mutex);
        return err;
    } else {
        puts("tone_file_add_tail\n");
    }

    os_mutex_post(&mutex);
    return 0;
}



int tone_file_list_play(const char **list, u8 preemption)
{
    return tone_dec_open_with_callback(0, list, preemption, NULL, NULL);
}
/*
 *停止sine_dec ,file_dec, tone_dec
 *
 * */
static void tone_stop(u8 end_flag)
{
    if (!mflag) {
        mflag = 1;
        os_mutex_create(&mutex);
    }
    os_mutex_pend(&mutex, 0);
    if (tone_dec == NULL) {
        os_mutex_post(&mutex);
        return;
    }
    tone_dec_stop(end_flag);
    sine_dec_close(end_flag);
    tone_dec_release(0, end_flag);
    os_mutex_post(&mutex);

}

static u8 audio_tone_idle_query()
{
    if (tone_dec) {
        return 0;
    }
    return 1;
}
REGISTER_LP_TARGET(audio_tone_lp_target) = {
    .name = "audio_tone",
    .is_idle = audio_tone_idle_query,
};

static const char *const tone_index[] = {
    TONE_NUM_0,
    TONE_NUM_1,
    TONE_NUM_2,
    TONE_NUM_3,
    TONE_NUM_4,
    TONE_NUM_5,
    TONE_NUM_6,
    TONE_NUM_7,
    TONE_NUM_8,
    TONE_NUM_9,
    TONE_BT_MODE,
    TONE_BT_CONN,
    TONE_BT_DISCONN,
    TONE_TWS_CONN,
    TONE_TWS_DISCONN,
    TONE_LOW_POWER,
    TONE_POWER_OFF,
    TONE_POWER_ON,
    TONE_RING,
    TONE_MAX_VOL,
    TONE_NORMAL,
#if (defined(TCFG_APP_MUSIC_EN) && (TCFG_APP_MUSIC_EN))
    TONE_MUSIC,
#endif
#if (defined(TCFG_APP_LINEIN_EN) && (TCFG_APP_LINEIN_EN))
    TONE_LINEIN,
#endif
#if (defined(TCFG_APP_FM_EN) && (TCFG_APP_FM_EN))
    TONE_FM,
#endif
#if (defined(TCFG_APP_PC_EN) && (TCFG_APP_PC_EN))
    TONE_PC,
#endif
#if (defined(TCFG_APP_RTC_EN) && (TCFG_APP_RTC_EN))
    TONE_RTC,
#endif
#if (defined(TCFG_APP_RECORD_EN) && (TCFG_APP_RECORD_EN))
    TONE_RECORD,
#endif
} ;
int tone_play_index_with_callback(u8 index, u8 preemption, void (*user_evt_handler)(void *priv), void *priv)
{
    log_info("%s:%d,preemption:%d", __FUNCTION__, index, preemption);
    if (index >= IDEX_TONE_NONE) {
        return 0;
    }

    if (tone_dec) {
        tone_event_clear();
        log_info("tone dec busy now,tone stop first");
        tone_stop(END_ABNORMAL);
    }

    single_file[0] = (char *)tone_index[index];
    single_file[1] = NULL;
    return tone_dec_open_with_callback(0, (const char **)single_file, preemption, user_evt_handler, priv);
}

/*
 *index:提示音索引
 *preemption:抢断标志
 */
int tone_play_index(u8 index, u8 preemption)
{
    return tone_play_index_with_callback(index, preemption, NULL, NULL);
}
int tone_play_stop(void)
{
    log_info("tone_play_stop");
    tone_stop(END_ABNORMAL);
    return 0;
}

/*
 *用于录制的声音回放
 * */
int record_file_dec_open(const char *path, u8 preemption, void (*user_evt_handler)(void *priv), void *priv)
{
    if (!mflag) {
        mflag = 1;
        os_mutex_create(&mutex);
    }

    if (tone_dec) {
        tone_event_clear();
        log_info("tone file dec busy now,tone stop first");
        tone_stop(END_ABNORMAL);
    }


    int err = 0;
    FILE *file = NULL;
    log_info("path %s\n", path);
    file = fopen(path, "r");
    if (file) {
        if (file_dec == NULL) {
            file_dec = zalloc(sizeof(*file_dec));
        }

        if (file_dec) {
            file_dec->file = file;
            file_dec->dev_file_en = INPUT_FILE_TYPE;
        } else {
            log_error("zalloc err file_dec\n");
            return err;
        }
        if (tone_dec == NULL) {
            tone_dec = zalloc(sizeof(*tone_dec));
            if (tone_dec == NULL) {
                log_error("tone dec zalloc failed");
                return -ENOMEM;
            }
        }
        if (user_evt_handler) {
            tone_set_user_event_handler(tone_dec, user_evt_handler, priv);
        } else {
            tone_dec->user_evt_owner   = NULL;
            tone_dec->user_evt_handler = NULL;
        }
        if (!preemption) {
            file_dec->dec_mix = 1;
            tone_dec->wait.protect = 1;
        }

        tone_dec->list_cnt = 1;
        tone_dec->wait.priority = 3;
        tone_dec->wait.preemption = preemption;/*需要解码的，默认抢断播放*/
        tone_dec->wait.handler = tone_wait_res_handler;
        err = audio_decoder_task_add_wait(&decode_task, &tone_dec->wait);
    } else {
        err = -1;
        log_info("open %s err\n", path);
    }
    return err;
}


void tone_set_user_event_handler(struct tone_dec_handle *dec, void (*user_evt_handler)(void *priv), void *priv)

{
    dec->user_evt_owner = os_current_task();
    dec->user_evt_handler = user_evt_handler;
    dec->priv = priv;
}


int tone_event_handler(struct tone_dec_handle *dec, u8 end_flag)
{
    int argv[4];
    if (!dec->user_evt_handler) {
        log_info("user_evt_handler null\n");
        return -1;
    }
    /* dec->user_evt_handler(dec->priv); */
    argv[0] = (int)dec->user_evt_handler;
    argv[1] = 1;
    argv[2] = (int)dec->priv;
    argv[3] = (int)end_flag;//是否是被打断 关闭，0正常关闭，1被打断关闭, 模式切换时，由file_dec->end_flag, 决定该值

    return os_taskq_post_type(dec->user_evt_owner, Q_CALLBACK, 4, argv);
    /* return 0; */
}



void record_file_play_evt_handler(void *priv)
{
    /* printf("fun = %s\n", __FUNCTION__); */
}

int record_file_play(void)
{
    extern int last_enc_file_path_get(char path[64]);
    char path[64] = {0};
    if (!last_enc_file_path_get(path)) {
        return record_file_dec_open(path, 1, record_file_play_evt_handler, NULL);
    } else {
        return -1;
    }
}
int record_file_play_prev(void)
{
#if TCFG_NOR_FS_ENABLE
	char path[64] = {0};
	int err;
	extern int nor_fs_index;
	extern int music_flash_file_set_index(u8 file_sel, u32 index);
	err = music_flash_file_set_index(FSEL_PREV_FILE,nor_fs_index);
	if(err){
		err = music_flash_file_set_index(FSEL_NEXT_FILE,nor_fs_index);
		if(err)
			return -1;
	}
	char index_str[5] = {0};
	index_str[0] = nor_fs_index / 1000 + '0';
	index_str[1] = nor_fs_index % 1000 / 100 + '0';
	index_str[2] = nor_fs_index % 100 / 10 + '0';
	index_str[3] = nor_fs_index % 10 + '0';

	struct storage_dev *last_dev = storage_dev_check("nor_fs");//storage_dev_last();
	sprintf(path, "%s%s%s%s", last_dev->root_path, "JL_REC/AC69", index_str, ".WAV");
	y_printf("rec file =%s\n", path);
	return record_file_dec_open(path, 1, record_file_play_evt_handler, NULL);
#else
	return -1;
#endif
}
int record_file_play_next(void)
{
#if TCFG_NOR_FS_ENABLE
	char path[64] = {0};
	int err;
	extern int nor_fs_index;
	extern int music_flash_file_set_index(u8 file_sel, u32 index);
	err = music_flash_file_set_index(FSEL_NEXT_FILE,nor_fs_index);
	if(err){
		err = music_flash_file_set_index(FSEL_PREV_FILE,nor_fs_index);
		if(err)
			return -1;
	}
	char index_str[5] = {0};
	index_str[0] = nor_fs_index / 1000 + '0';
	index_str[1] = nor_fs_index % 1000 / 100 + '0';
	index_str[2] = nor_fs_index % 100 / 10 + '0';
	index_str[3] = nor_fs_index % 10 + '0';

	struct storage_dev *last_dev = storage_dev_check("nor_fs");//storage_dev_last();
	sprintf(path, "%s%s%s%s", last_dev->root_path, "JL_REC/AC69", index_str, ".WAV");
	y_printf("rec file =%s\n", path);
	return record_file_dec_open(path, 1, record_file_play_evt_handler, NULL);
#else
	return -1;
#endif
}

int record_file_get_total_time(void)
{
    int ret;

    if (!file_dec) {
        return 0;
    }
    ret = audio_decoder_get_total_time(&file_dec->decoder);
    return ret;
}

int record_file_dec_get_cur_time(void)
{
    int ret;

    if (!file_dec) {
        return 0;
    }
    ret = audio_decoder_get_play_time(&file_dec->decoder);
    return ret;
}

int tone_play_by_path(const char *name, u8 preemption)
{
    if (!mflag) {
        mflag = 1;
        os_mutex_create(&mutex);
    }

    log_info("tone_play_by_path:%s", IS_DEFAULT_SINE(name) ? "sine" : name);

    if (tone_dec) {
        tone_event_clear();
        log_info("tone dec busy now,tone stop first");
        tone_stop(END_ABNORMAL);
    }
    single_file[0] = (char *)name;
    single_file[1] = NULL;
    return tone_dec_open_with_callback(0, (const char **)single_file, preemption, NULL, NULL);
}


#define NORFLASH_RES_ROOT_PATH "storage/nor_tone/C/"
#define TONE_NORFLASH_TEST0   NORFLASH_RES_ROOT_PATH"tone/power_on.*" //使用时根据外挂flash存储文件的实际路径填写
#define TONE_NORFLASH_TEST1   NORFLASH_RES_ROOT_PATH"tone/0.*"

static const char *const tone_index_ex[] = {
    TONE_NORFLASH_TEST0,
    TONE_NORFLASH_TEST1,
} ;

/*
 *dev_logo：设备logo，写"nor_tone"是指定外挂flash播放，NULL 默认用内置flash
 *index:tone_index_ex存储的，数组下标（提示音索引）
 *preemption:抢断标志
 */
int tone_play_dev_index(u8 *dev_logo, u8 index, u8 preemption)
{
    char *name = NULL;
    if (dev_logo && !strcmp("nor_tone", dev_logo)) {
        name = (char *)tone_index_ex[index];
    } else {
        return tone_play_index(index, preemption);
    }

    return tone_play_by_path(name, preemption);
}



int mode_tone_play(const char *name, void (*user_evt_handler)(void *priv), void *priv)
{

    if (!mflag) {
        mflag = 1;
        os_mutex_create(&mutex);
    }

    log_info("mode_tone_play:%s", IS_DEFAULT_SINE(name) ? "sine" : name);

    if (tone_dec) {
        tone_event_clear();
        log_info("tone dec busy now,tone stop first");
        tone_stop(END_ABNORMAL);
    }
    single_file[0] = (char *)name;
    single_file[1] = NULL;
    return tone_dec_open_with_callback(1, (const char **)single_file, 1, user_evt_handler, priv);
}

void mode_tone_stop(void)
{
    if (!mflag) {
        mflag = 1;
        os_mutex_create(&mutex);
    }
    os_mutex_pend(&mutex, 0);
    if (tone_dec == NULL) {
        os_mutex_post(&mutex);
        return;
    }

    tone_dec_close(0);

    tone_file_dec_release();
    tone_dec_release(1, END_ABNORMAL);

    sine_dec_close(0);
    os_mutex_post(&mutex);
}


void mode_tone_play_set_no_end(void)
{
    if (tone_dec && file_dec) {
        file_dec->end_flag = 1;
    }
}
void test_xxx(void *p)
{
    tone_play_index(10, 0);
}
