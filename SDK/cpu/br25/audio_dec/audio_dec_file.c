/*
 ****************************************************************
 *File : audio_file_dec.c
 *Note :
 *
 ****************************************************************
 */
//////////////////////////////////////////////////////////////////////////////
#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "effectrs_sync.h"
#include "audio_eq.h"
#include "audio_drc.h"
#include "app_config.h"
#include "audio_config.h"
#include "audio_dec.h"
#include "app_config.h"
#include "app_main.h"
#include "classic/tws_api.h"

#include "music/music_decrypt.h"
#include "music/music_id3.h"
#include "pitchshifter/pitchshifter_api.h"
#include "mono2stereo/reverb_mono2stero_api.h"
#include "audio_enc.h"
#include "clock_cfg.h"
#include "audio_pitch.h"
#include "MIDI_DEC_API.h"
#if (defined(TCFG_APP_MUSIC_EN) && (TCFG_APP_MUSIC_EN))

#define FILE_DEC_REPEAT_EN			0

#if TCFG_SPEED_PITCH_ENABLE
extern PITCH_SHIFT_PARM *get_effect_parm(void);
void update_audio_effect_parm(PITCH_SHIFT_PARM *parm);
#endif

#define FILE_DEC_FASTEST_TYPE_MASK	(AUDIO_CODING_FLAC | AUDIO_CODING_APE | AUDIO_CODING_DTS)
#define FILE_DEC_FAST_TYPE_MASK		(AUDIO_CODING_M4A | AUDIO_CODING_AMR)

#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
#define FILE_DEC_PCM_ENC_TYPE		AUDIO_CODING_SBC
#define FILE_DEC_PCM_ENC_CHANNEL	AUDIO_CH_LR
#else
#define FILE_DEC_PCM_ENC_TYPE		AUDIO_CODING_MP3
#define FILE_DEC_PCM_ENC_CHANNEL	AUDIO_CH_LR
#endif

#define FILE_DEC_TWS_USE_PCM		1	// 全部采用先解码再编码的方式

#if FILE_DEC_TWS_USE_PCM
#define FILE_DEC_PCM_DEC_TYPE_MASK	((u32)-1)//(AUDIO_CODING_WAV | AUDIO_CODING_MP3 | AUDIO_CODING_WMA)
#else
#define FILE_DEC_PCM_DEC_TYPE_MASK	(AUDIO_CODING_WAV)
#endif

#define MUSIC_EQ_SUPPORT_ASYNC		1

#ifndef CONFIG_EQ_SUPPORT_ASYNC
#undef MUSIC_EQ_SUPPORT_ASYNC
#define MUSIC_EQ_SUPPORT_ASYNC		0
#endif

#if MUSIC_EQ_SUPPORT_ASYNC && TCFG_MUSIC_MODE_EQ_ENABLE
#define MUSIC_EQ_SUPPORT_32BIT		1
#else
#define MUSIC_EQ_SUPPORT_32BIT		0
#endif


#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
#define FILE_DEC_ONCE_OUT_NUM		((512 * 4) * 2)	// 一次最多输出长度
#else
#define FILE_DEC_ONCE_OUT_NUM		(0)
#endif

enum {
    FILE_DEC_STATUS_STOP = 0,
    FILE_DEC_STATUS_PLAY,
    FILE_DEC_STATUS_PAUSE,
};
struct file_dec_hdl {
    struct audio_decoder decoder;
    struct audio_res_wait wait;
    struct audio_mixer_ch mix_ch;
    enum audio_channel ch_type;
    void *file;
    u32 status : 3;
    u32 pick_flag : 1;		// 挑出数据帧发送（如MP3等)
    u32 pcm_enc_flag : 1;	// pcm压缩成数据帧发送（如WAV等）
    u32 dec_no_out_sound : 1;	// 解码不直接输出声音（用于TWS转发）
    u32 read_err : 1;
    u32 tmp_pause : 1;
    u32 ch_num : 4;
    u32 dec_mix : 1;
    u32 remain : 1;
    u32 eq_remain : 1;
    u32 quad_pcm_point_offset;
    u32 prev_dual_pcm_len;

    u32 dec_total_time;
    u32 dec_cur_time;

#if FILE_DEC_ONCE_OUT_NUM
    u32 once_out_cnt;
#endif

#if TCFG_MUSIC_MODE_EQ_ENABLE
    struct audio_eq *p_eq;

#endif
#if TCFG_MUSIC_MODE_DRC_ENABLE
    struct audio_drc *p_drc;
#endif
#if TCFG_DEC_DECRYPT_ENABLE
    CIPHER mply_cipher;
#endif
#if TCFG_DEC_ID3_V1_ENABLE
    MP3_ID3_OBJ *p_mp3_id3_v1;
#endif
#if TCFG_DEC_ID3_V2_ENABLE
    MP3_ID3_OBJ *p_mp3_id3_v2;
#endif


#if FILE_DEC_REPEAT_EN
	u8 repeat_num;
	struct fixphase_repair_obj repair_buf;
#endif

    u16 src_out_sr;
    struct audio_src_handle *hw_src;
    struct audio_dec_breakpoint *bp;
    void (*evt_cb)(void *, int argc, int *argv);
    void *evt_priv;

#if TCFG_SPEED_PITCH_ENABLE
    s_pitch_hdl *pitch_hdl;
#endif

#if (defined(TCFG_DEC_MIDI_ENABLE) && (TCFG_DEC_MIDI_ENABLE))
    MIDI_INIT_STRUCT  midi_init_info_val;
#endif


    struct user_audio_parm *user_hdl;

#if MUSIC_EQ_SUPPORT_32BIT 
	s16 *eq_out_buf;
	int eq_out_buf_len;
	int eq_out_points;
	int eq_out_total;
#endif

};
struct file_dec_hdl *file_dec = NULL;
static u8 file_dec_maigc = 0;

extern void sys_auto_shut_down_disable(void);

extern void local_tws_sync_no_check_data_buf(u8 no_check);

extern int tws_api_get_tws_state();

void file_eq_drc_open(struct file_dec_hdl *dec, struct audio_fmt *fmt);
void file_eq_drc_close(struct file_dec_hdl *dec);
struct dec_type {
    u32 type;
    u32 clk;
};

const struct dec_type  dec_clk_tb[] = {
    {AUDIO_CODING_MP3,  DEC_MP3_CLK},
    {AUDIO_CODING_WAV,  DEC_WAV_CLK},
    {AUDIO_CODING_G729, DEC_G729_CLK},
    {AUDIO_CODING_G726, DEC_G726_CLK},
    {AUDIO_CODING_PCM,  DEC_PCM_CLK},
    {AUDIO_CODING_MTY,  DEC_MTY_CLK},
    {AUDIO_CODING_WMA,  DEC_WMA_CLK},

    {AUDIO_CODING_APE,  DEC_APE_CLK},
    {AUDIO_CODING_FLAC, DEC_FLAC_CLK},
    {AUDIO_CODING_DTS,  DEC_DTS_CLK},
    {AUDIO_CODING_M4A,  DEC_M4A_CLK},
    {AUDIO_CODING_MIDI, DEC_MIDI_CLK},

    {AUDIO_CODING_MP3 | AUDIO_CODING_STU_PICK,  DEC_MP3PICK_CLK},
    {AUDIO_CODING_WMA | AUDIO_CODING_STU_PICK,  DEC_WMAPICK_CLK},
};

static void dec_clock_add(u32 type)
{
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(dec_clk_tb); i++) {
        if (type == dec_clk_tb[i].type) {
            clock_add(dec_clk_tb[i].clk);
            return;
        }
    }
}

static void dec_clock_remove(u32 type)
{
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(dec_clk_tb); i++) {
        if (type == dec_clk_tb[i].type) {
            clock_remove(dec_clk_tb[i].clk);
            return;
        }
    }
}

static int file_fread(struct audio_decoder *decoder, void *buf, u32 len)
{
    int rlen;
#if TCFG_DEC_DECRYPT_ENABLE
    u32 addr;
    addr = fpos(file_dec->file);
    rlen = fread(file_dec->file, buf, len);
    if (rlen && (rlen <= len)) {
        cryptanalysis_buff(&file_dec->mply_cipher, buf, addr, rlen);
    }
#else
    rlen = fread(file_dec->file, buf, len);
#endif
    if (rlen > len) {
        // putchar('r');
        rlen = 0;
        file_dec->read_err = 1;
    } else {
        // putchar('R');
        file_dec->read_err = 0;
    }
    return rlen;
}
static int file_fseek(struct audio_decoder *decoder, u32 offset, int seek_mode)
{
    return fseek(file_dec->file, offset, seek_mode);
}
static int file_flen(struct audio_decoder *decoder)
{
    int len = 0;
    len = flen(file_dec->file);
    return len;
}

static const u32 file_input_coding_more[] = {
#if (defined(TCFG_DEC_MP3_ENABLE) && (TCFG_DEC_MP3_ENABLE))
    AUDIO_CODING_MP3,
#endif
    0,
};

static const struct audio_dec_input file_input = {
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
#if (defined(TCFG_DEC_MIDI_ENABLE) && (TCFG_DEC_MIDI_ENABLE))
    | AUDIO_CODING_MIDI
#endif
    ,
    .p_more_coding_type = (u32 *)file_input_coding_more,
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
            .fread = file_fread,
            .fseek = file_fseek,
            .flen  = file_flen,
        }
    }
};

static void file_dec_set_output_channel(struct file_dec_hdl *dec)
{
    int state;
    enum audio_channel ch_type = AUDIO_CH_DIFF;
    u8 channel_num = audio_output_channel_num();
    if (channel_num == 2) {
        ch_type = AUDIO_CH_LR;
    }
#if TCFG_USER_TWS_ENABLE
    state = tws_api_get_tws_state();
    if (state & TWS_STA_SIBLING_CONNECTED) {
        if (channel_num == 2) {
            ch_type = tws_api_get_local_channel() == 'L' ? AUDIO_CH_DUAL_L : AUDIO_CH_DUAL_R;
        } else {
            ch_type = tws_api_get_local_channel() == 'L' ? AUDIO_CH_L : AUDIO_CH_R;
        }
    }
#endif

    if (ch_type != dec->ch_type) {
        printf("set_channel: %d\n", ch_type);
        audio_decoder_set_output_channel(&dec->decoder, ch_type);
        dec->ch_type = ch_type;
        dec->ch_num = AUDIO_CH_IS_MONO(ch_type) ? 1 : 2;
    }
}
#if MUSIC_EQ_SUPPORT_32BIT
void music_eq_32bit_out(struct file_dec_hdl *dec)
{
    int wlen = 0;

	if (dec->hw_src) {
		wlen = audio_src_resample_write(dec->hw_src, &dec->eq_out_buf[dec->eq_out_points], (dec->eq_out_total-dec->eq_out_points)*2);
	} else{
		wlen = audio_mixer_ch_write(&dec->mix_ch, &dec->eq_out_buf[dec->eq_out_points], (dec->eq_out_total-dec->eq_out_points)*2);
	}
	dec->eq_out_points += wlen/2;
}
#endif /*MUSIC_EQ_SUPPORT_32BIT*/

static int file_eq_output(void *priv, s16 *data, u32 len)
{
    int wlen = 0;
    int rlen = len;
    struct file_dec_hdl *dec = priv;
#if MUSIC_EQ_SUPPORT_ASYNC
    if (!dec->eq_remain) {
#if MUSIC_EQ_SUPPORT_32BIT
		if (dec->eq_out_buf && (dec->eq_out_points < dec->eq_out_total)) {
			music_eq_32bit_out(dec);
			if (dec->eq_out_points < dec->eq_out_total) {
				return 0;
			}
		}

#endif /*MUSIC_EQ_SUPPORT_32BIT*/

#if TCFG_MUSIC_MODE_DRC_ENABLE
        if (dec->p_drc) {
            audio_drc_run(dec->p_drc, data, len);
        }
#endif
    }

#if MUSIC_EQ_SUPPORT_32BIT
		if ((!dec->eq_out_buf) || (dec->eq_out_buf_len < len/2)) {
			if (dec->eq_out_buf) {
				free(dec->eq_out_buf);
			}
			dec->eq_out_buf_len = len/2;
			dec->eq_out_buf = malloc(dec->eq_out_buf_len);
			ASSERT(dec->eq_out_buf);
		}
		s32 *idat = data;
		s16 *odat = dec->eq_out_buf;
		for (int i=0; i<len/4; i++) {
			s32 outdat = *idat++;
			if (outdat > 32767) {
				outdat = 32767;
			} else if (outdat < -32768) {
				outdat = -32768;
			}
			*odat++ = outdat;
		}
		dec->eq_out_points = 0;
	   	dec->eq_out_total = len/4;

		music_eq_32bit_out(dec);
		return len;
#endif /*MUSIC_EQ_SUPPORT_32BIT*/



		if (dec->hw_src) {
			wlen = audio_src_resample_write(dec->hw_src, data, len);
		} else {
            wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
		}

    if (wlen == len) {
        dec->eq_remain = 0;
    } else {
        dec->eq_remain = 1;
    }
	return wlen;
#endif
    return len;
}



static int file_dec_probe_handler(struct audio_decoder *decoder)
{
    struct file_dec_hdl *dec = container_of(decoder, struct file_dec_hdl, decoder);
#if FILE_DEC_ONCE_OUT_NUM
    dec->once_out_cnt = 0;
#endif
    return 0;
}

static int file_dec_src_output_handler(struct audio_decoder *decoder, s16 *data, int len)
{
    int wlen = 0;
    int rlen = len;
    struct file_dec_hdl *dec = container_of(decoder, struct file_dec_hdl, decoder);

    if (!file_dec || (file_dec->status != FILE_DEC_STATUS_PLAY)) {
        /* putchar('O'); */
        return 0;
    }
#if TCFG_PCM_ENC2TWS_ENABLE
    if (dec->pcm_enc_flag) {
        return pcm2tws_enc_output(NULL, data, len);
    }
#endif


        wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
    return wlen;
}

static int file_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    int wlen = 0;
    int rlen = len;
    struct file_dec_hdl *dec = container_of(decoder, struct file_dec_hdl, decoder);
    if (dec->status != FILE_DEC_STATUS_PLAY) {
        return 0;
    }
#if TEST_POINTS
    {
        static u32 points = 0;//
        points += len / 2;
        if (points >= 16384) {
            points -= 16384;
            JL_PORTC->DIR &= ~BIT(3);
            JL_PORTC->OUT ^= BIT(3);
        }
        return len;
    }
#endif

#if FILE_DEC_ONCE_OUT_NUM
    if (dec->once_out_cnt >= FILE_DEC_ONCE_OUT_NUM) {
        return 0;
    }
    if (!dec->remain) {
        dec->once_out_cnt += len;
    }
#endif

    /* putchar('O'); */

    /* put_buf(data,10);	 */
#if TCFG_DEC2TWS_ENABLE
    if (dec->pick_flag) { // tws
        return local_tws_output(&dec->decoder.fmt, data, len);
    }
#endif
    if (!dec->remain) {
#if ((defined(DEC_OUT_OTHER_DATA) && (DEC_OUT_OTHER_DATA)))
        other_audio_dec_output(decoder, data, len, dec->ch_num, dec->decoder.fmt.sample_rate);
#endif

#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
        if (dec->user_hdl) {
            u8 ch_num = dec->ch_num;
            user_audio_process_handler_run(dec->user_hdl, data, len, ch_num);
        }
#endif
    }


#if TCFG_SPEED_PITCH_ENABLE
    if (dec->remain == 0) {
        picth_run(dec->pitch_hdl, data, data, len, dec->ch_num);
    }
#endif

#if MUSIC_EQ_SUPPORT_ASYNC

#if TCFG_MUSIC_MODE_EQ_ENABLE
    if (dec->p_eq) {
        int eqlen = audio_eq_run(dec->p_eq, data, len);
        len -= eqlen;
        if (len == 0) {
            dec->remain = 0;
        } else {
            dec->remain = 1;
        }
        return eqlen;
    }
#endif
#endif



    if (!dec->remain) {
#if TCFG_MUSIC_MODE_EQ_ENABLE
        if (dec->p_eq) {
            audio_eq_run(dec->p_eq, data, len);
        }
#endif
#if TCFG_MUSIC_MODE_DRC_ENABLE
        if (dec->p_drc) {
            audio_drc_run(dec->p_drc, data, len);
        }
#endif
    }

    do {
        if (dec->hw_src) {
            wlen = audio_src_resample_write(dec->hw_src, data, len);
#if TCFG_PCM_ENC2TWS_ENABLE
        } else if (dec->pcm_enc_flag) {
            wlen = pcm2tws_enc_output(NULL, data, len);
#endif
        } else {
            wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);//3M  64点
        }
        if (!wlen) {
            /* putchar('z'); */
            /* printf("wlen 0 break\n"); */
            break;
        }

        data += wlen / 2;
        len -= wlen;
    } while (len);

    if (len == 0) {
        dec->remain = 0;
    } else {
        dec->remain = 1;
    }

    return rlen - len;
}

extern void put_u16hex(u16 dat);

static int file_dec_post_handler(struct audio_decoder *decoder)
{
    struct file_dec_hdl *dec = container_of(decoder, struct file_dec_hdl, decoder);
    if (dec->status) {
        dec->dec_cur_time = audio_decoder_get_play_time(&dec->decoder);
    }
#if FILE_DEC_ONCE_OUT_NUM
    if (dec->once_out_cnt >= FILE_DEC_ONCE_OUT_NUM) {
        /* put_u16hex(dec->once_out_cnt); */
        audio_decoder_resume(&dec->decoder);
        return 0;
    }
#endif
    return 0;
}

static const struct audio_dec_handler file_dec_handler = {
    .dec_probe  = file_dec_probe_handler,
    .dec_output = file_dec_output_handler,
    .dec_post   = file_dec_post_handler,
};

static void file_dec_priv_func_close()
{
    struct file_dec_hdl *dec = file_dec;
#if TCFG_PCM_ENC2TWS_ENABLE
    if (dec->pcm_enc_flag) {
        dec->pcm_enc_flag = 0;
        pcm2tws_enc_close();
        encfile_tws_dec_close();
    }
#endif
#if TCFG_DEC2TWS_ENABLE || TCFG_PCM_ENC2TWS_ENABLE
    dec->pick_flag = 0;
    if (dec->dec_no_out_sound) {
        dec->dec_no_out_sound = 0;
        local_tws_stop();
    }
#endif
    file_eq_drc_close(file_dec);

#if TCFG_DEC_ID3_V1_ENABLE
    if (dec->p_mp3_id3_v1) {
        id3_obj_post(&dec->p_mp3_id3_v1);
    }
#endif
#if TCFG_DEC_ID3_V2_ENABLE
    if (dec->p_mp3_id3_v2) {
        id3_obj_post(&dec->p_mp3_id3_v2);
    }
#endif
}

void file_dec_close();
void file_dec_resume(void);

static void file_dec_release()
{

    file_dec_priv_func_close();

    audio_decoder_task_del_wait(&decode_task, &file_dec->wait);

    if (file_dec->decoder.fmt.coding_type) {
        dec_clock_remove(file_dec->decoder.fmt.coding_type);
    }

    local_irq_disable();
    free(file_dec);
    file_dec = NULL;
    local_irq_enable();

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
    audio_dec_bt_emitter_check_empty_en(0);
#endif
}

static void file_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        puts("AUDIO_DEC_EVENT_END\n");
        printf("arg1:%d, maigc:%d \n", (u8)argv[1], (u8)(file_dec_maigc - 1));
        if ((u8)argv[1] != (u8)(file_dec_maigc - 1)) {
            log_i("maigc err, %s\n", __FUNCTION__);
            break;
        }
        if (!file_dec) {
            log_i("file_dec handle err ");
            break;
        }

        if (file_dec->evt_cb) {
            /* file_dec->evt_cb(file_dec->evt_priv, argc, argv); */
            int msg[2];
            msg[0] = argv[0];
            msg[1] = file_dec->read_err;
            /* printf("read err0:%d ", file_dec->read_err); */
            file_dec->evt_cb(file_dec->evt_priv, 2, msg);
        } else {
            // 有回调，让上层close，避免close后上层读不到断点等
            file_dec_close();
        }
        //audio_decoder_resume_all(&decode_task);
        break;
    }
}

#if FILE_DEC_REPEAT_EN
// 循环播放返回0，结束循环返回非0
static int file_dec_repeat_cb(void *priv)
{
    struct file_dec_hdl *dec = priv;
	y_printf("file_dec_repeat_cb\n");
	if (dec->repeat_num) {
		dec->repeat_num--;
	} else {
		y_printf("file_dec_repeat_cb end\n");
		return -1;
	}
	return 0;
}
int file_dec_repeat_set(u8 repeat_num)
{
    struct file_dec_hdl *dec = file_dec;
	if (!dec || !dec->decoder.dec_ops) {
		return false;
	}
    switch (dec->decoder.dec_ops->coding_type) {
	case AUDIO_CODING_MP3:
	case AUDIO_CODING_WAV:
		{
			dec->repeat_num = repeat_num;
			struct audio_repeat_mode_param rep = {0};
			rep.flag = 1; //使能
			rep.headcut_frame = 2; //依据需求砍掉前面几帧，仅mp3格式有效
			rep.tailcut_frame = 2; //依据需求砍掉后面几帧，仅mp3格式有效
			rep.repeat_callback = file_dec_repeat_cb;
			rep.callback_priv = dec;
			rep.repair_buf = &dec->repair_buf;
			audio_decoder_ioctrl(&dec->decoder, AUDIO_IOCTRL_CMD_REPEAT_PLAY, &rep);
		}
		return true;
	}
	return false;
}
#endif

u8 file_dec_start_pause = 0;
static int file_dec_start()
{
    int err;
    struct audio_fmt *fmt;
    struct file_dec_hdl *dec = file_dec;
    struct audio_fmt enc_fmt = {0};
    u8 pick_flag = 0;
    u8 pcm_enc_flag = 0;

    if (!dec) {
        return -EINVAL;
    }

    puts("file_dec_start: in\n");

    err = audio_decoder_open(&dec->decoder, &file_input, &decode_task);
    if (err) {
        goto __err1;
    }

    dec->ch_type = AUDIO_CH_MAX;

    audio_decoder_set_handler(&dec->decoder, &file_dec_handler);
    audio_decoder_set_event_handler(&dec->decoder, file_dec_event_handler, file_dec_maigc++);

    audio_decoder_set_breakpoint(&dec->decoder, dec->bp);

#if TCFG_DEC2TWS_ENABLE
#if (FILE_DEC_TWS_USE_PCM == 0)
    if (dec2tws_check_enable() == true) {
        pick_flag = 1;
    }
#endif
#endif

    audio_decoder_set_pick_stu(&dec->decoder, pick_flag);

    err = audio_decoder_get_fmt(&dec->decoder, &fmt);
    if (err) {
#if TCFG_DEC2TWS_ENABLE
        if (pick_flag) {
            pick_flag = 0;
            audio_decoder_set_pick_stu(&dec->decoder, pick_flag);
            err = audio_decoder_get_fmt(&dec->decoder, &fmt);
        }
#endif
        if (err) {
            goto __err2;
        }
    }

#if TCFG_DEC2TWS_ENABLE
    if (dec2tws_check_enable() == true) {
        pcm_enc_flag = !pick_flag;	// 没有拆包就是输出pcm
    }
#endif

    if (dec->decoder.dec_ops->coding_type == AUDIO_CODING_MP3) {
#if TCFG_DEC_ID3_V1_ENABLE
        if (file_dec->p_mp3_id3_v1) {
            id3_obj_post(&file_dec->p_mp3_id3_v1);
        }
        file_dec->p_mp3_id3_v1 = id3_v1_obj_get(dec->file);
#endif
#if TCFG_DEC_ID3_V2_ENABLE
        if (file_dec->p_mp3_id3_v2) {
            id3_obj_post(&file_dec->p_mp3_id3_v2);
        }
        file_dec->p_mp3_id3_v2 = id3_v2_obj_get(dec->file);
#endif
    }

    enc_fmt.sample_rate = dec->decoder.fmt.sample_rate;
#if TCFG_PCM_ENC2TWS_ENABLE
    dec->pcm_enc_flag = 0;
    if (pcm_enc_flag && (dec->decoder.dec_ops->coding_type & FILE_DEC_PCM_DEC_TYPE_MASK)) {
        audio_decoder_set_output_channel(&dec->decoder, FILE_DEC_PCM_ENC_CHANNEL);
        dec->ch_type = FILE_DEC_PCM_ENC_CHANNEL;
        dec->ch_num = AUDIO_CH_IS_MONO(dec->ch_type) ? 1 : 2;
        enc_fmt.coding_type = FILE_DEC_PCM_ENC_TYPE;
        enc_fmt.bit_rate = 128;
        enc_fmt.channel = dec->ch_num;
        enc_fmt.sample_rate = dec->decoder.fmt.sample_rate;
#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
        enc_fmt.sample_rate = pcm2tws_sbc_sample_rate_select(enc_fmt.sample_rate);
#endif
        int ret = pcm2tws_enc_open(&enc_fmt);
        if (ret == 0) {
#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
            pcm2tws_enc_set_output_handler(local_tws_output);
#else
            pcm2tws_enc_set_output_handler(encfile_tws_wfile);
            ret = encfile_tws_dec_open(&enc_fmt);
#endif
            if (ret == 0) {
                dec->pcm_enc_flag = 1;
                pcm2tws_enc_set_resume_handler(file_dec_resume);
#if FILE_DEC_ONCE_OUT_NUM
                audio_decoder_set_run_max(&dec->decoder, 20);
#endif
            } else {
                pcm2tws_enc_close();
            }
        }
    }
#endif

#if TCFG_DEC2TWS_ENABLE
    dec->pick_flag = pick_flag;
    if (dec->pick_flag) {
        dec->ch_type = AUDIO_CH_LR;
    }
#endif

#if TCFG_DEC2TWS_ENABLE || TCFG_PCM_ENC2TWS_ENABLE
    if (dec->pick_flag || dec->pcm_enc_flag) {
        dec->dec_no_out_sound = 1;
        audio_decoder_task_del_wait(&decode_task, &dec->wait);

        local_tws_sync_no_check_data_buf(1);
        if (dec->pcm_enc_flag) {
            local_tws_start(&enc_fmt);
        } else {
            local_tws_start(&dec->decoder.fmt);
        }
        dec->src_out_sr = enc_fmt.sample_rate;
        goto __dec_start;
    }
#endif


    file_dec_set_output_channel(dec);

    audio_mixer_ch_open(&dec->mix_ch, &mixer);
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, audio_output_rate(fmt->sample_rate));

    dec->src_out_sr = audio_output_rate(fmt->sample_rate);

#if TCFG_SPEED_PITCH_ENABLE
    PITCH_SHIFT_PARM *temp_parm = get_effect_parm();
    temp_parm->sr = fmt->sample_rate;
    update_audio_effect_parm(temp_parm);
#endif

    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);

    file_eq_drc_open(dec, fmt);


#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
    struct user_audio_digital_parm  vol_parm = {0};
#if (defined(USER_DIGITAL_VOLUME_ADJUST_ENABLE) && (USER_DIGITAL_VOLUME_ADJUST_ENABLE != 0))
    vol_parm.en  = true;
    vol_parm.vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
    vol_parm.vol_max = app_audio_get_max_volume();
    vol_parm.fade_step = 2;
#endif
    dec->user_hdl = user_audio_process_open((void *)&vol_parm, NULL, NULL);
#endif

    /* dec->src_out_sr = audio_output_rate(fmt->sample_rate); */
    /* dec->src_out_sr = audio_output_rate(audio_mixer_get_sample_rate(&mixer)); */

__dec_start:

    if (fmt->sample_rate != dec->src_out_sr) {
        u8 ch_num = dec->ch_num;
        printf("file dec sr:%d, or:%d, ch:%d \n", fmt->sample_rate, file_dec->src_out_sr, ch_num);
        dec->hw_src = audio_hw_resample_open(&dec->decoder, file_dec_src_output_handler,
                                             ch_num, fmt->sample_rate, dec->src_out_sr);
    }

    dec_clock_add(dec->decoder.dec_ops->coding_type);
#if (defined(TCFG_DEC_MIDI_ENABLE) && (TCFG_DEC_MIDI_ENABLE))
    if (dec->decoder.dec_ops->coding_type == AUDIO_CODING_MIDI) {
        extern int midi_init(void *info);
        memset(&file_dec->midi_init_info_val, 0, sizeof(MIDI_INIT_STRUCT));
        int ret = midi_init(&file_dec->midi_init_info_val);
        if (ret) {
            goto __err3;
        }
        audio_decoder_ioctrl(&file_dec->decoder, CMD_INIT_CONFIG, &file_dec->midi_init_info_val);
    }
#endif

    dec->dec_cur_time = 0;
    dec->dec_total_time = audio_decoder_get_total_time(&dec->decoder);
    printf("total_time : %d \n", dec->dec_total_time);

    if (file_dec_start_pause) {
        printf("file_dec_start_pause\n");
        file_dec_start_pause = 0;
        dec->status = FILE_DEC_STATUS_PAUSE;
        return 0;
    }

#if FILE_DEC_REPEAT_EN
	file_dec_repeat_set(3);
#endif

    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);
    err = audio_decoder_start(&dec->decoder);
    if (err) {
        goto __err3;
    }

    clock_set_cur();
    dec->status = FILE_DEC_STATUS_PLAY;

    /* os_time_dly(500); */
    /* log_i("total_time:%d ", audio_decoder_get_total_time(&dec->decoder)); */
    /* log_i("play_time:%d ", audio_decoder_get_play_time(&dec->decoder)); */

    return 0;

__err3:
    if (dec->hw_src) {
        audio_hw_resample_close(dec->hw_src);
        dec->hw_src = NULL;
    }
    if (!dec->dec_no_out_sound) {
        audio_mixer_ch_close(&dec->mix_ch);
    }
#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
    if (dec->user_hdl) {
        user_audio_process_close(dec->user_hdl);
        dec->user_hdl = NULL;
    }
#endif

__err2:
    audio_decoder_close(&dec->decoder);
__err1:
    file_dec_release();

    return err;
}

static int file_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;

#if 0
    if (event == AUDIO_RES_GET) {
        err = file_dec_start();
    } else if (event == AUDIO_RES_PUT) {
        if (file_dec->status) {
            file_dec->status = FILE_DEC_STATUS_STOP;
            if (file_dec->bp) {
                audio_decoder_get_breakpoint(&file_dec->decoder, file_dec->bp);
            }
            audio_decoder_close(&file_dec->decoder);

#if (AUDIO_OUTPUT_WAY != AUDIO_OUTPUT_WAY_DAC)
            if (file_dec->hw_src) {
                printf("hw_src close begin\n");
                audio_hw_src_stop(file_dec->hw_src);
                audio_hw_src_close(file_dec->hw_src);
                local_irq_disable();
                free(file_dec->hw_src);
                file_dec->hw_src = NULL;
                local_irq_enable();
                printf("hw_src close out\n");
            }
#endif
            if (!file_dec->dec_no_out_sound) {
                audio_mixer_ch_close(&file_dec->mix_ch);
            }
            file_dec_priv_func_close();
        }
    }
#else
    log_i("file_wait_res_handler, event:%d, status:%d ", event, file_dec->status);
    if (event == AUDIO_RES_GET) {
        if (file_dec->status == 0) {
            err = file_dec_start();
        } else if (file_dec->tmp_pause) {
            file_dec->tmp_pause = 0;

            if (!file_dec->dec_no_out_sound) {
                audio_mixer_ch_open(&file_dec->mix_ch, &mixer);
                audio_mixer_ch_set_sample_rate(&file_dec->mix_ch, audio_output_rate(file_dec->src_out_sr));
            }

            audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);
            audio_output_start(file_dec->src_out_sr, 1);
            if (file_dec->status == FILE_DEC_STATUS_PLAY) {
                err = audio_decoder_start(&file_dec->decoder);
            }
        }
    } else if (event == AUDIO_RES_PUT) {
        if (file_dec->status) {
            if (file_dec->status == FILE_DEC_STATUS_PLAY || \
                file_dec->status == FILE_DEC_STATUS_PAUSE) {
                err = audio_decoder_pause(&file_dec->decoder);
                os_time_dly(2);
                audio_output_stop();

                if (!file_dec->dec_no_out_sound) {
                    audio_mixer_ch_close(&file_dec->mix_ch);
                }
            }
            file_dec->tmp_pause = 1;
        }
    }
#endif

    return err;
}

bool file_dec_is_stop(void)
{
    local_irq_disable();
    if ((!file_dec) || (file_dec->status == FILE_DEC_STATUS_STOP)) {
        local_irq_enable();
        return true;
    }
    local_irq_enable();
    return false;
}
bool file_dec_is_play(void)
{
    local_irq_disable();
    if ((file_dec) && (file_dec->status == FILE_DEC_STATUS_PLAY)) {
        local_irq_enable();
        return true;
    }
    local_irq_enable();
    return false;
}
bool file_dec_is_pause(void)
{
    local_irq_disable();
    if ((file_dec) && (file_dec->status == FILE_DEC_STATUS_PAUSE)) {
        local_irq_enable();
        return true;
    }
    local_irq_enable();
    return false;
}

int file_dec_pp(void)
{
    int ret;
    if (!file_dec) {
        return -EPERM;
    }
    if (file_dec->tmp_pause) {
        return -EPERM;
    }
    if (file_dec->status == FILE_DEC_STATUS_PLAY) {
        ret = audio_decoder_pause(&file_dec->decoder);
        file_dec->status = FILE_DEC_STATUS_PAUSE;
#if TCFG_DEC2TWS_ENABLE || TCFG_PCM_ENC2TWS_ENABLE
        if (file_dec->dec_no_out_sound) {
            local_tws_decoder_pause();
        } else
#endif
        {
            audio_mixer_ch_pause(&file_dec->mix_ch, 1);
            //audio_decoder_resume_all(&decode_task);
        }
        if (audio_mixer_get_active_ch_num(&mixer) == 0) {
            clock_pause_play(1);
        }
    } else if (file_dec->status == FILE_DEC_STATUS_PAUSE) {
        clock_pause_play(0);
        if (!file_dec->dec_no_out_sound) {
            audio_mixer_ch_pause(&file_dec->mix_ch, 0);
        }
        ret = audio_decoder_start(&file_dec->decoder);
        file_dec->status = FILE_DEC_STATUS_PLAY;
    } else {
        return -EPERM;
    }
    return ret;
}

int file_dec_FF(int step)
{
    int ret = 0;
    if (!file_dec) {
        return -EPERM;
    }
    if (file_dec->tmp_pause) {
        return -EPERM;
    }

    if (file_dec->status == FILE_DEC_STATUS_PAUSE) {
        ret = file_dec_pp();
    }
    if (!ret) {
        if (file_dec->status == FILE_DEC_STATUS_PLAY) {
            ret = audio_decoder_forward(&file_dec->decoder, step);
        }
    }
    return ret;
}

int file_dec_FR(int step)
{
    int ret = 0;
    if (!file_dec) {
        return -EPERM;
    }
    if (file_dec->tmp_pause) {
        return -EPERM;
    }

    if (file_dec->status == FILE_DEC_STATUS_PAUSE) {
        ret = file_dec_pp();
    }
    if (!ret) {
        if (file_dec->status == FILE_DEC_STATUS_PLAY) {
            ret = audio_decoder_rewind(&file_dec->decoder, step);
        }
    }

    return ret;
}

int file_dec_get_breakpoint(struct audio_dec_breakpoint *bp)
{
    int ret;
    if (!file_dec) {
        return -EPERM;
    }
    if (file_dec->status == FILE_DEC_STATUS_STOP) {
        return -EPERM;
    }

    ret = audio_decoder_get_breakpoint(&file_dec->decoder, bp);
    return ret;
}

int file_dec_get_total_time(void)
{
    int ret;
    if (!file_dec) {
        return 0;
    }

    local_irq_disable();
    if (file_dec->status == FILE_DEC_STATUS_STOP) {
        local_irq_enable();
        return -EPERM;
    }

    /* ret = audio_decoder_get_total_time(&file_dec->decoder); */
    ret = file_dec->dec_total_time;
    local_irq_enable();
    return ret;
}

int file_dec_get_cur_time(void)
{
    int ret;
    if (!file_dec) {
        return 0;
    }

    local_irq_disable();
    if (file_dec->status == FILE_DEC_STATUS_STOP) {
        local_irq_enable();
        return -EPERM;
    }

    /* ret = audio_decoder_get_play_time(&file_dec->decoder); */
    ret = file_dec->dec_cur_time;
    local_irq_enable();
    return ret;
}


int file_dec_get_decoder_type(void)
{
    int ret;
    if (!file_dec) {
        return -EINVAL;
    }

    local_irq_disable();
    if (file_dec->status == FILE_DEC_STATUS_STOP) {
        local_irq_enable();
        return -EPERM;
    }

    ret = file_dec->decoder.dec_ops->coding_type;
    local_irq_enable();
    return ret;
}

int file_dec_create(void *priv, void (*handler)(void *, int argc, int *argv))
{
    struct file_dec_hdl *dec;
    dec = zalloc(sizeof(*dec));
    if (!dec) {
        return -ENOMEM;
    }
    if (file_dec) {
        file_dec_close();
    }

    file_dec = dec;
    file_dec->evt_cb = handler;
    file_dec->evt_priv = priv;

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
    audio_dec_bt_emitter_check_empty_en(1);
#endif

    return 0;
}

int file_dec_open(void *file, struct audio_dec_breakpoint *bp)
{
    int err;
    struct file_dec_hdl *dec = file_dec;

    printf("file_dec_open: in, 0x%x \n", file);

    if ((!dec) || (!file)) {
        return -EPERM;
    }
    dec->file = file;
    dec->bp = bp;

#if TCFG_DEC_DECRYPT_ENABLE
    cipher_init(&file_dec->mply_cipher, TCFG_DEC_DECRYPT_KEY);
    cipher_check_decode_file(&file_dec->mply_cipher, file);
#endif

#if TCFG_SPEED_PITCH_ENABLE
    file_dec->pitch_hdl = open_pitch(get_effect_parm());
#endif

    dec->wait.priority = 1;
    dec->wait.preemption = 1;
    dec->wait.handler = file_wait_res_handler;
    err = audio_decoder_task_add_wait(&decode_task, &dec->wait);

    return err;
}

void file_dec_close()
{
    if (!file_dec) {
        return;
    }

    if (file_dec->status) {
        file_dec->status = 0;
        audio_decoder_close(&file_dec->decoder);


        if (file_dec->hw_src) {
            audio_hw_resample_close(file_dec->hw_src);
            file_dec->hw_src = NULL;
        }
        if (!file_dec->dec_no_out_sound) {
            audio_mixer_ch_close(&file_dec->mix_ch);
        }
    }
#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
    if (file_dec->user_hdl) {
        user_audio_process_close(file_dec->user_hdl);
        file_dec->user_hdl = NULL;
    }
#endif

#if TCFG_SPEED_PITCH_ENABLE
    close_pitch(file_dec->pitch_hdl);
#endif

    file_dec_release();

    clock_set_cur();
    puts("file_dec_close: exit\n");
}

int file_dec_restart(int magic)
{
    if ((!file_dec) || (magic != file_dec_maigc)) {
        return -1;
    }
    if (file_dec->status && file_dec->bp) {
        audio_decoder_get_breakpoint(&file_dec->decoder, file_dec->bp);
    }

    void *file = file_dec->file;
    void *bp = file_dec->bp;
    void *evt_cb = file_dec->evt_cb;
    void *evt_priv = file_dec->evt_priv;
    int err;

    file_dec_close();
    err = file_dec_create(evt_priv, evt_cb);
    if (!err) {
        err = file_dec_open(file, bp);
    }
    return err;
}

int file_dec_push_restart(void)
{
    if (!file_dec) {
        return false;
    }
    int argv[3];
    argv[0] = (int)file_dec_restart;
    argv[1] = 1;
    argv[2] = (int)file_dec_maigc;
    os_taskq_post_type(os_current_task(), Q_CALLBACK, ARRAY_SIZE(argv), argv);
    return true;
}

void file_dec_resume(void)
{
    if (file_dec && file_dec->dec_no_out_sound && (file_dec->status == FILE_DEC_STATUS_PLAY)) {
        audio_decoder_resume(&file_dec->decoder);
    }
}
int file_dec_no_out_sound(void)
{
    if (file_dec && file_dec->dec_no_out_sound) {
        return true;
    }
    return false;
}


#if TCFG_SPEED_PITCH_ENABLE

void update_audio_effect_parm(PITCH_SHIFT_PARM *parm)
{
    if (file_dec) {
        if (file_dec->pitch_hdl) {
            file_dec->pitch_hdl->ops->init(file_dec->pitch_hdl->databuf, get_effect_parm());
        }
    }
}
#endif


#if (defined(TCFG_DEC_MIDI_ENABLE) && (TCFG_DEC_MIDI_ENABLE))
u32 tmark_trigger(void *priv, u8 *val, u8 len)
{
    return 0;
}

u32 melody_trigger(void *priv, u8 key, u8 vel)
{
    return 0;
}

u32 timDiv_trigger(void *priv)
{
    return 0;
}

u32 beat_trigger(void *priv, u8 val1/*一节多少拍*/, u8 val2/*每拍多少分音符*/)
{
    return 0;
}

void init_midi_info_val(MIDI_INIT_STRUCT  *midi_init_info_v, u8 *addr)
{
    //midi初始化表
    midi_init_info_v->init_info.player_t = 8;
    midi_init_info_v->init_info.sample_rate = 4;
    midi_init_info_v->init_info.spi_pos = addr;

    //midi的模式初始化
    midi_init_info_v->mode_info.mode = 0; //CMD_MIDI_CTRL_MODE_2;
    /* midi_init_info_v->mode_info.mode = 1;//CMD_MIDI_CTRL_MODE_2; */

    //midi节奏初始化
    midi_init_info_v->tempo_info.tempo_val = 1042;

    midi_init_info_v->tempo_info.decay_val = ((u16)31 << 11) | 1024;
    midi_init_info_v->tempo_info.mute_threshold = (u16)1L << 29;

    //midi主轨道初始化
    midi_init_info_v->mainTrack_info.chn = 17; //把哪个轨道当成主轨道

    //midi外部音量初始化
    {
        u32 tmp_i;
        for (tmp_i = 0; tmp_i < 16; tmp_i++) {
            midi_init_info_v->vol_info.cc_vol[tmp_i] = 4096; //4096即原来的音量
        }
    }

    //midi的主轨道乐器设置
    midi_init_info_v->prog_info.prog = 0;
    midi_init_info_v->prog_info.ex_vol = 1024;
    midi_init_info_v->prog_info.replace_mode = 0;


    //midi的mark控制初始化
    midi_init_info_v->mark_info.priv = NULL; //&file_mark;
    midi_init_info_v->mark_info.mark_trigger = tmark_trigger;

    //midi的melody控制初始化
    midi_init_info_v->moledy_info.priv = NULL; //&file_melody;
    midi_init_info_v->moledy_info.melody_trigger = melody_trigger;

    //midi的小节回调控制初始化
    midi_init_info_v->tmDiv_info.priv = NULL;
    midi_init_info_v->tmDiv_info.timeDiv_trigger = timDiv_trigger;

    //midi的小拍回调控制初始化
    midi_init_info_v->beat_info.priv = NULL;
    midi_init_info_v->beat_info.beat_trigger = beat_trigger;

    //使能位控制
    midi_init_info_v->switch_info = MELODY_PLAY_ENABLE | MELODY_ENABLE | EX_VOL_ENABLE;            //主轨道播放使能

    return;
}

int midi_get_cfg_addr(u8 **addr)
{
    //获取音色文件
    FILE  *file = fopen(SDFILE_RES_ROOT_PATH"MIDI.bin\0", "r");
    if (!file) {
        log_e("MIDI.bin open err\n");
        return -1;
    }
    struct vfs_attr attr = {0};
    fget_attrs(file, &attr);
    *addr = (u8 *)attr.sclust;
    fclose(file);
    return 0;
}

int midi_init(void *info)
{
    u8 *cache_addr;
    if (midi_get_cfg_addr(&cache_addr)) {
        log_e("get midi addr err\n");
        return -1;
    }
    //初始化midi参数
    init_midi_info_val(info, cache_addr);  //需要外部写
    return 0;
}
/*
 *cmd:
    CMD_MIDI_SEEK_BACK_N = 0x00,
	CMD_MIDI_SET_CHN_PROG,
	CMD_MIDI_CTRL_TEMPO,
	CMD_MIDI_GOON,
	CMD_MIDI_CTRL_MODE,
	CMD_MIDI_SET_SWITCH,
	CMD_MIDI_SET_EX_VOL,
	CMD_INIT_CONFIG
 *
 * */
void midi_ioctrl(u32 cmd, void *priv)
{
    if (!file_dec) {
        return ;
    }
    if (file_dec->status == FILE_DEC_STATUS_STOP) {
        return ;
    }

    log_i("midi cmd %x", cmd);
    audio_decoder_ioctrl(&file_dec->decoder, cmd, priv);
}

void *ex_vol_test()
{
    static int val = 4096;
    EX_CH_VOL_PARM ex_vol;
    for (int test_ci = 0; test_ci < CTRL_CHANNEL_NUM; test_ci++) {
        ex_vol.cc_vol[test_ci] = val;
    }
    val -= 64;
    if (val <= 0) {
        val = 4096;
    }
    midi_ioctrl(CMD_MIDI_SET_EX_VOL, &ex_vol);
    return NULL;
}
#endif


#if (defined(USER_DIGITAL_VOLUME_ADJUST_ENABLE) && (USER_DIGITAL_VOLUME_ADJUST_ENABLE != 0))
/*
 *插卡插u歌数字音量调节
 * */
void file_user_digital_volume_set(u8 vol)
{
    if (file_dec && file_dec->user_hdl && file_dec->user_hdl->dvol_hdl) {
        user_audio_digital_volume_set(file_dec->user_hdl->dvol_hdl, vol);
    }
}

u8 file_user_audio_digital_volume_get()
{
    if (!file_dec) {
        return 0;
    }
    if (!file_dec->user_hdl) {
        return 0;
    }
    if (!file_dec->user_hdl->dvol_hdl) {
        return 0;
    }
    return user_audio_digital_volume_get(file_dec->user_hdl->dvol_hdl);
}

/*
 *user_vol_max:音量级数
 *user_vol_tab:自定义音量表,自定义表长user_vol_max+1
 *注意：如需自定义音量表，须在volume_set前调用 ,否则会在下次volume_set时生效
 */
void file_user_digital_volume_tab_set(u16 *user_vol_tab, u8 user_vol_max)
{
    if (file_dec && file_dec->user_hdl && file_dec->user_hdl->dvol_hdl) {
        user_audio_digital_set_volume_tab(file_dec->user_hdl->dvol_hdl, user_vol_tab, user_vol_max);
    }
}
#endif


void file_eq_drc_open(struct file_dec_hdl *dec, struct audio_fmt *fmt)
{
    if (!dec) {
        log_e("file_eq_open err\n");
        return;
    }
#if TCFG_MUSIC_MODE_EQ_ENABLE
    dec->p_eq = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
    if (dec->p_eq) {
        dec->p_eq->eq_ch = (struct hw_eq_ch *)((int)dec->p_eq + sizeof(struct audio_eq));
        struct audio_eq_param file_eq_param = {0};
        file_eq_param.channels = dec->ch_num;
        file_eq_param.online_en = 1;
        file_eq_param.mode_en = 1;
        file_eq_param.remain_en = 1;
        file_eq_param.max_nsection = EQ_SECTION_MAX;
        file_eq_param.cb = eq_get_filter_info;
#if MUSIC_EQ_SUPPORT_ASYNC
        file_eq_param.no_wait = 1;//异步
#endif
        file_eq_param.eq_name = 0;
        audio_eq_open(dec->p_eq, &file_eq_param);
        audio_eq_set_samplerate(dec->p_eq, fmt->sample_rate);
        audio_eq_set_output_handle(dec->p_eq, file_eq_output, dec);
#if MUSIC_EQ_SUPPORT_32BIT
		audio_eq_set_info(dec->p_eq, file_eq_param.channels, 1);
#endif
        audio_eq_start(dec->p_eq);
    }
#endif

#if TCFG_MUSIC_MODE_DRC_ENABLE
    dec->p_drc = malloc(sizeof(struct audio_drc));
    if (dec->p_drc) {
        struct audio_drc_param drc_param = {0};
        drc_param.channels = dec->ch_num;
        drc_param.online_en = 1;
        drc_param.remain_en = 1;
        drc_param.cb = drc_get_filter_info;
        drc_param.stero_div = 0;
        drc_param.drc_name = 0;
        audio_drc_open(dec->p_drc, &drc_param);
        audio_drc_set_samplerate(dec->p_drc, fmt->sample_rate);
#if MUSIC_EQ_SUPPORT_32BIT
		audio_drc_set_32bit_mode(dec->p_drc, 1);
#endif
        audio_drc_set_output_handle(dec->p_drc, NULL, NULL);
        audio_drc_start(dec->p_drc);
    }

#endif

}


void file_eq_drc_close(struct file_dec_hdl *dec)
{
    if (!dec) {
        return;
    }
#if TCFG_MUSIC_MODE_EQ_ENABLE
    if (dec->p_eq) {
        audio_eq_close(dec->p_eq);
        local_irq_disable();
        free(dec->p_eq);
        dec->p_eq = NULL;
        local_irq_enable();
    }
#endif
#if TCFG_MUSIC_MODE_DRC_ENABLE
    if (dec->p_drc) {
        audio_drc_close(dec->p_drc);
        local_irq_disable();
        free(dec->p_drc);
        dec->p_drc = NULL;
        local_irq_enable();
    }
#endif
#if MUSIC_EQ_SUPPORT_32BIT
    if (dec->eq_out_buf) {
        free(dec->eq_out_buf);
        dec->eq_out_buf = NULL;
    }
#endif

}


#endif /*TCFG_APP_MUSIC_EN*/

