/*
 ****************************************************************
 *File : audio_local_tws.c
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
#include "audio_enc.h"
#include "clock_cfg.h"

#include "classic/tws_api.h"
#include "classic/tws_local_media_sync.h"

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))

u8 is_tws_active_device(void);

#define DEC2TWS_MEDIA_ALLOC_EN		1	// 使用空间申请方式

#define DEC2TWS_MEDIA_BUF_LEN		(12 * 1024)
#define DEC2TWS_MEDIA_LIMIT_SIZE	(DEC2TWS_MEDIA_BUF_LEN*7/10)	// 填了多少数据才开始传输

#define DEC2TWS_ENCFILE_BUF_LEN		(2 * 1024)

#define DEC2TWS_MEDIA_INFO_LEN		(1)
#define DEC2TWS_MEDIA_FMT_LEN		(DEC2TWS_MEDIA_INFO_LEN+sizeof(u16))	// B0:INFO, B[1-2]:SEQN
#define DEC2TWS_MEDIA_TO_MS			(6000)

#define DEC2TWS_MEDIA_INFO_STOP		(0)		// 特殊包，停止
#define DEC2TWS_MEDIA_INFO_PAUSE	(0xff)	// 特殊包，暂停

#define AUDIO_LOCAL_TWS_SUPPORT_SYNC	1

#define AUDIO_LOCAL_TWS_SUPPORT_SOUND_EFFECT		1

#define AUDIO_LOCAL_TWS_EQ_SUPPORT_ASYNC			1 	// 异步eq

#ifndef CONFIG_EQ_SUPPORT_ASYNC
#undef AUDIO_LOCAL_TWS_EQ_SUPPORT_ASYNC
#define AUDIO_LOCAL_TWS_EQ_SUPPORT_ASYNC			0
#endif


#if AUDIO_LOCAL_TWS_EQ_SUPPORT_ASYNC &&(TCFG_MUSIC_MODE_EQ_ENABLE && AUDIO_LOCAL_TWS_SUPPORT_SOUND_EFFECT)
#define LOCAL_TWS_EQ_SUPPORT_32BIT		1
#else
#define LOCAL_TWS_EQ_SUPPORT_32BIT		0
#endif


#define AUDIO_LOCAL_TWS_VOL_FADE_EN					0 // 淡入，过滤掉同步开始时的快速对齐产生的噪音
#define AUDIO_LOCAL_TWS_VOL_FADE_CNT_TIME_MS		(2000)	// 淡入时长

extern void *local_tws_play_sync_open(struct audio_decoder *dec, u8 channel, u32 sample_rate, u32 output_rate);
extern void bt_drop_a2dp_frame_start(void);
extern int a2dp_media_clear_packet();

extern void local_tws_sync_no_check_data_buf(u8 no_check);
extern u8 local_tws_sync_no_check_data_buf_status(void);

void dec2tws_master_dec_resume(void);

enum {
    DEC2TWS_MEDIA_TYPE_UNKNOW = 0,
    DEC2TWS_MEDIA_TYPE_MP3,
    DEC2TWS_MEDIA_TYPE_WMA,
    DEC2TWS_MEDIA_TYPE_SBC,
};
static const u16 dec2tws_sample_rate_tbl[] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000
};

struct local_tws_dec_hdl {
    struct audio_decoder decoder;
    struct audio_res_wait wait;
    struct audio_mixer_ch mix_ch;
    enum audio_channel ch_type;
    u32 status : 3;
    u32 tws_flag : 1;
    u32 read_en : 1;
    u32 ch_num : 4;
    u32 tmp_pause : 1;
    u32 dec_mix : 1;
    u32 remain : 1;
    u32 eq_remain : 1;
    u32 need_get_fmt : 1;
    u32 fade_en : 1;
#if AUDIO_LOCAL_TWS_SUPPORT_SYNC
    u32 sync_start : 1;
#endif

#if AUDIO_LOCAL_TWS_VOL_FADE_EN
    u8  fade_idx;
    u16 fade_cnt;
    u16 fade_cnt_max;
#endif

    u32 dec_type;
    u16 sample_rate;
    u32 media_value;

#if TCFG_MUSIC_MODE_EQ_ENABLE && AUDIO_LOCAL_TWS_SUPPORT_SOUND_EFFECT
    struct audio_eq *p_eq;
#endif

#if TCFG_MUSIC_MODE_DRC_ENABLE && AUDIO_LOCAL_TWS_SUPPORT_SOUND_EFFECT
    struct audio_drc *p_drc;
#endif

    u8 *tws_ptr;
    u32 tws_len;
    u32 tws_ulen;

    u16 sbc_header_len;

    u16 src_out_sr;
    struct audio_src_handle *hw_src;

#if LOCAL_TWS_EQ_SUPPORT_32BIT 
	s16 *eq_out_buf;
	int eq_out_buf_len;
	int eq_out_points;
	int eq_out_total;
#endif

};
static struct local_tws_dec_hdl *local_tws_dec = NULL;

struct dec2tws_hdl {
    u16 tmrout;
    u16 seqn;
    u32 media_value;
    u8 drop_frame_start;
    u8 tws_send_pause;
};
static struct dec2tws_hdl g_dec2tws = {0};

#if DEC2TWS_MEDIA_ALLOC_EN
static u8 *dec2tws_media_buf = NULL;
#else
static u8 dec2tws_media_buf[DEC2TWS_MEDIA_BUF_LEN] sec(.dec2tws_mem);
#endif

extern void sys_auto_shut_down_disable(void);

///////////////////////////////////////////////////////////////////////////////////
#if AUDIO_LOCAL_TWS_VOL_FADE_EN
const u16 local_tws_dig_vol_table[] = {
    0,    93,   111,  132,  158,  189,  226,  270,
    323,  386,  462,  552,  660,  789,  943,  1127,
    1347, 1610, 1925, 2301, 2751, 3288, 3930, 4698,
    5616, 6713, 8025, 9592, 11466, 15200, 16000, 16384
};

void local_tws_fade_run(struct local_tws_dec_hdl *dec, s16 *buf, u16 len)
{
    if (!dec->fade_en) {
        return ;
    }
    s32 valuetemp;
    for (u16 i = 0; i < len / 2; i++) {
        valuetemp = buf[i];
        valuetemp = (valuetemp * local_tws_dig_vol_table[dec->fade_idx]) >> 14 ;
        if (valuetemp < -32768) {
            valuetemp = -32768;
        } else if (valuetemp > 32767) {
            valuetemp = 32767;
        }
        buf[i] = (s16)valuetemp;
        dec->fade_cnt += 2;
        if (dec->fade_cnt >= dec->fade_cnt_max) {
            dec->fade_cnt = 0;
            dec->fade_idx ++;
            if (dec->fade_idx >= (ARRAY_SIZE(local_tws_dig_vol_table) - 1)) {
                dec->fade_en = 0;
                printf("fade end \n");
                return ;
            }
        }
    }
}

void local_tws_fade_init(struct local_tws_dec_hdl *dec)
{
    dec->fade_en = 1;
    dec->fade_idx = 0;
    dec->fade_cnt = 0;
    dec->fade_cnt_max = ((u32)dec->sample_rate * dec->ch_num * 2 * AUDIO_LOCAL_TWS_VOL_FADE_CNT_TIME_MS / 1000) / ARRAY_SIZE(local_tws_dig_vol_table);
    dec->fade_cnt_max = (dec->fade_cnt_max * 4) / 4;
    printf("sr:%d, ch:%d, num:%d \n", dec->sample_rate, dec->ch_num, ARRAY_SIZE(local_tws_dig_vol_table));
    printf("fade_cnt_max : %d \n", dec->fade_cnt_max);
}
#endif /*AUDIO_LOCAL_TWS_VOL_FADE_EN*/

///////////////////////////////////////////////////////////////////////////////////
struct local_dec_type {
    u32 type;
    u32 clk;
};

const struct local_dec_type local_dec_clk_tb[] = {
    {AUDIO_CODING_SBC,  DEC_TWS_SBC_CLK},
    {AUDIO_CODING_MP3,  DEC_MP3_CLK},
    {AUDIO_CODING_WMA,  DEC_WMA_CLK},
};

static void local_dec_clock_add(u32 type)
{
    printf("local_dec_clock_add : 0x%x \n", type);
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(local_dec_clk_tb); i++) {
        if (type == local_dec_clk_tb[i].type) {
            clock_add(local_dec_clk_tb[i].clk);
            return;
        }
    }
}

static void local_dec_clock_remove(u32 type)
{
    printf("local_dec_clock_remove : 0x%x \n", type);
    int i = 0;
    for (i = 0; i < ARRAY_SIZE(local_dec_clk_tb); i++) {
        if (type == local_dec_clk_tb[i].type) {
            clock_remove(local_dec_clk_tb[i].clk);
            return;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////
#if AUDIO_LOCAL_TWS_SUPPORT_SYNC

static void *local_tws_audio_sync;
static void *local_tws_audio_sync_open(void *priv, int sample_rate, int out_sample_rate)
{
    u8 channel = audio_output_channel_num();

    if (!local_tws_audio_sync) {
        /*音频同步声道配置为最后一级输出的声道数*/
        local_tws_audio_sync = local_tws_play_sync_open(priv, channel, sample_rate, out_sample_rate);
    }

    if (local_tws_audio_sync) {
        audio_wireless_sync_info_init(local_tws_audio_sync, sample_rate, out_sample_rate, channel);
    }

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
    if (!audio_fm_src) {
        audio_fm_src = fm_emitter_sync_src_open(channel, sample_rate, out_sample_rate);
    }

    if (!fm_sync_buffer) {
        fm_sync_buffer = zalloc(FM_SYNC_OUTPUT_SIZE);
    }
#endif

    clock_add(SYNC_CLK);
    return local_tws_audio_sync;
}

#endif


///////////////////////////////////////////////////////////////////////////////////
static void dec2tws_media_init(void)
{
#if DEC2TWS_MEDIA_ALLOC_EN
    dec2tws_media_buf = malloc(DEC2TWS_MEDIA_BUF_LEN);
    ASSERT(dec2tws_media_buf);
#endif
    tws_api_local_media_trans_start();
    tws_api_local_media_trans_set_buf(dec2tws_media_buf, DEC2TWS_MEDIA_BUF_LEN);
}
__initcall(dec2tws_media_init);

void dec2tws_media_enable(void)
{
#if DEC2TWS_MEDIA_ALLOC_EN
    if (dec2tws_media_buf) {
        return ;
    }
    dec2tws_media_buf = malloc(DEC2TWS_MEDIA_BUF_LEN);
    ASSERT(dec2tws_media_buf);
    tws_api_local_media_trans_set_buf(dec2tws_media_buf, DEC2TWS_MEDIA_BUF_LEN);
    tws_api_local_media_trans_start();
#endif
}

void dec2tws_media_disable(void)
{
#if DEC2TWS_MEDIA_ALLOC_EN
    if (dec2tws_media_buf) {
        tws_api_local_media_trans_stop();
        free(dec2tws_media_buf);
        dec2tws_media_buf = NULL;
    }
#endif
}

static void local_tws_dec_resume(void)
{
    if (local_tws_dec && local_tws_dec->status) {
        audio_decoder_resume(&local_tws_dec->decoder);
    }
}
u8 is_local_tws_dec_open()
{
    if (local_tws_dec) {
        return 1;
    }
    return 0;

}

static void tws_media_dec_event(u8 event_type, u32 value)
{
    struct sys_event event;
    event.type = SYS_BT_EVENT;
    event.arg = (void *)SYS_BT_EVENT_FROM_TWS;
    event.u.bt.event = event_type;
    event.u.bt.value = value;
    sys_event_notify(&event);
}
static void tws2dec_timeout_func(void *priv)
{
    g_dec2tws.tmrout = 0;
    log_i("tws2dec_timeout_func");
    if (g_dec2tws.media_value) {
        tws_media_dec_event(TWS_EVENT_LOCAL_MEDIA_STOP, 0);
    }
}

void set_wait_a2dp_start(u8 flag)
{
    if (local_tws_dec) {
        g_dec2tws.drop_frame_start = flag;
    }
}
int tws_api_local_media_sync_rx_handler_notify(u8 *data, int len, u8 rx)
{
    u32 value = 0;

    /* putchar('r'); */
    if ((local_tws_dec) && (local_tws_dec->status) && (local_tws_dec->tmp_pause)) {
        // 从机，暂停，抛弃中间数据包
        goto __del_pkt;
    }
    memcpy(&value, data, DEC2TWS_MEDIA_INFO_LEN);
    if (g_dec2tws.drop_frame_start) {
        r_printf("local_tws_wait_drop_over\n");
        goto __del_pkt;
    }
    if ((value != g_dec2tws.media_value) && (value != DEC2TWS_MEDIA_INFO_PAUSE)) {
        if (g_dec2tws.drop_frame_start) {
            r_printf("local_tws_wait_a2dp_start\n");
            goto __del_pkt;
        } else {
            if (!is_tws_active_device()) {
                if (value) {
                    tws_media_dec_event(TWS_EVENT_LOCAL_MEDIA_START, value);
                } else {
                    tws_media_dec_event(TWS_EVENT_LOCAL_MEDIA_STOP, 0);
                }
                printf("++++++ dec2tws media chage, value:0x%x  ", value);
                g_dec2tws.media_value = value;
            } else {
                putchar('%');
                /* goto __del_pkt; */
            }

        }
    }
    if (value == 0) {
__del_pkt:
#if 0
        // 删除当前包
        tws_api_local_media_trans_packet_del(data);
#else
        // 删除所有的数据
        while (1) {
            int len = 0;
            void *ptr = tws_api_local_media_trans_pop(&len);
            if (!ptr) {
                break;
            }
            tws_api_local_media_trans_free(ptr);
        }
#endif
        return -EINVAL;
    } else if (rx) {
        /* extern int rx_media_buf_free_space(); */
#if 0
        int free_space_size = DEC2TWS_MEDIA_BUF_LEN - tws_api_local_media_trans_check_total(1);
        /* y_printf("^%d ",free_space_size ); */
        if (free_space_size < (len * 4)) {
            void *del_ptr = tws_api_local_media_trans_pop(&len);
            if (del_ptr) {
                tws_api_local_media_trans_free(del_ptr);
                y_printf("rx_lbuf_free_space full=%d,%d", free_space_size, len);
            }
        }
#endif
    }

    if (!is_tws_active_device()) {
        if (value == DEC2TWS_MEDIA_INFO_PAUSE) {
            if (g_dec2tws.tmrout) {
                sys_hi_timeout_del(g_dec2tws.tmrout);
                g_dec2tws.tmrout = 0;
            }
        } else if (g_dec2tws.tmrout) {
            sys_hi_timeout_modify(g_dec2tws.tmrout, DEC2TWS_MEDIA_TO_MS);
        } else {
            g_dec2tws.tmrout = sys_hi_timeout_add(NULL, tws2dec_timeout_func, DEC2TWS_MEDIA_TO_MS);
        }
    }

    /* putchar('I'); */
    local_tws_dec_resume();
    return 0;
}

int dec2tws_media_set(u8 *data, struct audio_fmt *pfmt)
{
    u8 type, ch, sr;
    *data = 0;
    if (!pfmt) {
        return false;
    }
    if (pfmt->coding_type & AUDIO_CODING_MP3) {
        type = DEC2TWS_MEDIA_TYPE_MP3;
    } else if (pfmt->coding_type & AUDIO_CODING_WMA) {
        type = DEC2TWS_MEDIA_TYPE_WMA;
    } else if (pfmt->coding_type & AUDIO_CODING_SBC) {
        type = DEC2TWS_MEDIA_TYPE_SBC;
    } else {
        log_e("type err:0x%x ", pfmt->coding_type);
        return false;
    }
    for (sr = 0; sr < ARRAY_SIZE(dec2tws_sample_rate_tbl); sr++) {
        if (pfmt->sample_rate == dec2tws_sample_rate_tbl[sr]) {
            break;
        }
    }
    if (sr == ARRAY_SIZE(dec2tws_sample_rate_tbl)) {
        log_e("sample_rate err:0x%x ", pfmt->sample_rate);
        return false;
    }
    ch = pfmt->channel == 2 ? 1 : 0;
    *data = (sr << 4) | (ch << 3) | (type << 0);
    return true;
}

int dec2tws_media_get(u8 *data, struct audio_fmt *pfmt)
{
    u8 type, ch, sr;
    if (*data == 0) {
        return false;
    }
    type = ((*data) >> 0) & 0x07;
    ch = ((*data) >> 3) & 0x01;
    sr = ((*data) >> 4) & 0x0f;
    if (type == DEC2TWS_MEDIA_TYPE_MP3) {
        pfmt->coding_type = AUDIO_CODING_MP3;
    } else if (type == DEC2TWS_MEDIA_TYPE_WMA) {
        pfmt->coding_type = AUDIO_CODING_WMA;
    } else if (type == DEC2TWS_MEDIA_TYPE_SBC) {
        pfmt->coding_type = AUDIO_CODING_SBC;
    } else {
        pfmt->coding_type = AUDIO_CODING_UNKNOW;
    }
    if (sr >= ARRAY_SIZE(dec2tws_sample_rate_tbl)) {
        log_e("sr err:0x%x ", sr);
        return false;
    }
    pfmt->channel = ch == 1 ? 2 : 1;
    pfmt->sample_rate = dec2tws_sample_rate_tbl[sr];
    return true;
}

static int local_tws_fread(struct audio_decoder *decoder, void *buf, u32 len)
{
    u32 rlen = len;
    struct local_tws_dec_hdl *dec = container_of(decoder, struct local_tws_dec_hdl, decoder);
    /* putchar('r'); */
    if (dec->tws_len) {
        rlen = dec->tws_len;
        if (rlen > len) {
            rlen = len;
        }
        memcpy(buf, (void *)((u32)dec->tws_ptr + dec->tws_ulen), rlen);
        dec->tws_ulen += rlen;
        dec->tws_len -= rlen;
        if (dec->tws_len == 0) {
            tws_api_local_media_trans_free(dec->tws_ptr);
            dec2tws_master_dec_resume();
        }
        return rlen;
    }
    return 0;
}
static int local_tws_fseek(struct audio_decoder *decoder, u32 offset, int seek_mode)
{
    return 0;
}
static int local_tws_flen(struct audio_decoder *decoder)
{
    return 0x7fffffff;
}

static const struct audio_dec_input local_file_tws_input = {
    .coding_type = AUDIO_CODING_MP3,
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
            .fread = local_tws_fread,
            .fseek = local_tws_fseek,
            .flen  = local_tws_flen,
        }
    }
};

#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
void printf_buf(u8 *buf, u32 len);

static int get_sbc_header_len(u8 *buf, int len)
{
    int ext, csrc;
    int byte_len;
    int header_len = 0;
    u8 *data = buf;

    csrc = buf[0] & 0x0f;
    ext  = buf[0] & 0x10;

    byte_len = 12 + 4 * csrc;
    buf += byte_len;
    if (ext) {
        ext = (RB16(buf + 2) + 1) << 2;
    }

    header_len = byte_len + ext + 1;
    while (data[header_len] != 0x9c) {
        if (++header_len > len) {
            return len;
        }
    }

    return header_len;
}

static int local_fame_dec_get_frame(struct audio_decoder *decoder, u8 **frame)
{
    struct local_tws_dec_hdl *dec = container_of(decoder, struct local_tws_dec_hdl, decoder);

    if (!dec->tws_len) {
        return 0;
    }
    /* putchar('R'); */
    int len = 0;
    /* *frame = tws_api_local_media_trans_pop(&len); */
    dec->sbc_header_len = get_sbc_header_len(dec->tws_ptr + DEC2TWS_MEDIA_FMT_LEN, dec->tws_len);
    *frame = dec->tws_ptr + DEC2TWS_MEDIA_FMT_LEN + dec->sbc_header_len;
    len = dec->tws_len - dec->sbc_header_len;
    dec2tws_master_dec_resume();
    return len;
}

static void local_fame_dec_put_frame(struct audio_decoder *decoder, u8 *frame)
{
    struct local_tws_dec_hdl *dec = container_of(decoder, struct local_tws_dec_hdl, decoder);

    if (frame) {
        dec->tws_len = 0;
        tws_api_local_media_trans_free(frame - DEC2TWS_MEDIA_FMT_LEN - dec->sbc_header_len);
    }
    dec2tws_master_dec_resume();
}

static int local_fame_dec_fetch_frame(struct audio_decoder *decoder, u8 **frame)
{
    int len = 0;

    *frame = tws_api_local_media_trans_fetch(NULL, &len);
    if (len < DEC2TWS_MEDIA_FMT_LEN) {
        return 0;
    }

    u8 *ptr = *frame;
    ptr += DEC2TWS_MEDIA_FMT_LEN;
    *frame = ptr;

    len -= DEC2TWS_MEDIA_FMT_LEN;

    return len;
}

static const struct audio_dec_input local_fame_tws_input = {
    .coding_type = AUDIO_CODING_SBC,
    .data_type   = AUDIO_INPUT_FRAME,
    .ops = {
        .frame = {
            .fget = local_fame_dec_get_frame,
            .fput = local_fame_dec_put_frame,
            .ffetch = local_fame_dec_fetch_frame,
        }
    }
};
#endif

static void local_tws_dec_set_output_channel(struct local_tws_dec_hdl *dec)
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
#if LOCAL_TWS_EQ_SUPPORT_32BIT
void local_tws_eq_32bit_out(struct local_tws_dec_hdl *dec)
{
    int wlen = 0;

	if (dec->hw_src) {
		wlen = audio_src_resample_write(dec->src_sync, &dec->eq_out_buf[dec->eq_out_points], (dec->eq_out_total-dec->eq_out_points)*2);
	} else{
		wlen = audio_mixer_ch_write(&dec->mix_ch, &dec->eq_out_buf[dec->eq_out_points], (dec->eq_out_total-dec->eq_out_points)*2);
	}
	dec->eq_out_points += wlen/2;
}
#endif /*LOCAL_TWS_EQ_SUPPORT_32BIT*/

static int local_tws_eq_output(void *priv, s16 *data, u32 len)
{
#if AUDIO_LOCAL_TWS_EQ_SUPPORT_ASYNC
    int wlen = 0;
    int rlen = len;
    struct local_tws_dec_hdl *dec = priv;

    if (!dec->eq_remain) {

#if LOCAL_TWS_EQ_SUPPORT_32BIT
		if (dec->eq_out_buf && (dec->eq_out_points < dec->eq_out_total)) {
			local_tws_eq_32bit_out(dec);
			if (dec->eq_out_points < dec->eq_out_total) {
				return 0;
			}
		}
#endif /*LOCAL_TWS_EQ_SUPPORT_32BIT*/


#if TCFG_MUSIC_MODE_DRC_ENABLE && AUDIO_LOCAL_TWS_SUPPORT_SOUND_EFFECT
        if (dec->p_drc) {
            audio_drc_run(dec->p_drc, data, len);
        }
#endif

    }
#if LOCAL_TWS_EQ_SUPPORT_32BIT
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

		local_tws_eq_32bit_out(dec);
		return len;
#endif /*LOCAL_TWS_EQ_SUPPORT_32BIT*/

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

#if AUDIO_LOCAL_TWS_SUPPORT_SYNC
static int local_tws_dec_stop_and_restart(struct audio_decoder *decoder)
{
    struct local_tws_dec_hdl *dec = container_of(decoder, struct local_tws_dec_hdl, decoder);

    if (local_tws_audio_sync) {
#if AUDIO_LOCAL_TWS_EQ_SUPPORT_ASYNC
#if TCFG_MUSIC_MODE_EQ_ENABLE && AUDIO_LOCAL_TWS_SUPPORT_SOUND_EFFECT
        if (dec->p_eq) {
            audio_eq_async_data_clear(dec->p_eq);
            dec->remain = 0;
            dec->eq_remain = 0;
        }
#endif
#endif

        audio_mixer_ch_reset(&dec->mix_ch);
        audio_wireless_sync_stop(local_tws_audio_sync);
        app_audio_output_reset(300);
        while (1) {
            int len = 0;
            void *ptr = tws_api_local_media_trans_pop(&len);
            if (!ptr) {
                break;
            }
            tws_api_local_media_trans_free(ptr);
        }
#if 1
        audio_wireless_sync_close(local_tws_audio_sync);
        local_tws_audio_sync = NULL;
        local_tws_audio_sync_open(&dec->decoder, dec->sample_rate, dec->src_out_sr);
#endif
    }
    return 0;
}

extern int tws_api_local_media_trans_get_total_buffer_size(void);
static int local_tws_data_rx_monitor(struct audio_decoder *decoder, struct rt_stream_info *info)
{
    struct local_tws_dec_hdl *dec = container_of(decoder, struct local_tws_dec_hdl, decoder);
    int total_size = tws_api_local_media_trans_get_total_buffer_size();

    if (!dec->sync_start) {
        int start_size = total_size / 2;
        if (info->data_len < start_size) {
            return -EAGAIN;
        }
        dec->sync_start = 1;
        app_audio_output_reset(350);
    }

    info->rx_delay = RX_DELAY_NULL;
    if (local_tws_sync_no_check_data_buf_status() == 0) {
        if (info->data_len <= (total_size * 50 / 100)) {
            info->rx_delay = RX_DELAY_DOWN;
        } else if (info->data_len >= (total_size * 60 / 100)) {
            info->rx_delay = RX_DELAY_UP;
        }

        if (info->remain_len < 768) {
            info->rx_delay = RX_DELAY_UP;
        }
    }
    return 0;
}
#endif

extern void put_u16hex(u16 dat);
extern void put_u32hex(u32 dat);

extern int tws_api_local_media_trans_get_total_buffer_size(void);
extern int tws_api_local_media_trans_check_total(u8 head);
static int local_tws_dec_probe_handler(struct audio_decoder *decoder)
{
    struct local_tws_dec_hdl *dec = container_of(decoder, struct local_tws_dec_hdl, decoder);
    if (dec->tws_len) {
        // 还有数据没读完
        return 0;
    }
    int pkt_len = 0;
    u8 *pkt_addr = NULL;
    int total_len = tws_api_local_media_trans_check_ready_total();
    int total_size = tws_api_local_media_trans_get_total_buffer_size();

    if (0) { ; }
#if AUDIO_LOCAL_TWS_SUPPORT_SYNC
    else if (local_tws_audio_sync) {
        pkt_addr = tws_api_local_media_trans_fetch(NULL, &pkt_len);
        if (!pkt_addr || (pkt_len < DEC2TWS_MEDIA_FMT_LEN)) {
            /* log_e("pkt addr:0x%x, len:%d \n", pkt_addr, pkt_len); */
            audio_decoder_suspend(&dec->decoder, 0);
            /* if (audio_decoder_running_number(&decode_task) <= 1) { */
            /* os_time_dly(2); */
            /* } */
            return -EAGAIN;
        }
        if (pkt_addr[0] == DEC2TWS_MEDIA_INFO_PAUSE) {
            log_i("local tws pause \n");
            pkt_addr = tws_api_local_media_trans_pop(&pkt_len);
            if (pkt_addr) {
                tws_api_local_media_trans_free(pkt_addr);
            }
            // reset
            dec->read_en = 0;
            audio_wireless_sync_close(local_tws_audio_sync);
            local_tws_audio_sync = NULL;
            dec->sync_start = 0;
            local_tws_audio_sync_open(&dec->decoder, dec->sample_rate, audio_output_rate(dec->sample_rate));

            app_audio_output_reset(300);
            return -EAGAIN;
        }

        struct rt_stream_info rts_info = {0};
        int total_len_all = tws_api_local_media_trans_check_total(1);
        rts_info.remain_len = total_size - total_len_all;
#if 1
        if (dec->read_en == 0) {
            rts_info.remain_len = total_size;
        }
#endif
#if 1
        if (dec->read_en && (local_tws_sync_no_check_data_buf_status() == 0)) {
            // 需要对齐的都是有自己的时钟，正常情况下输出到tws的数据都是固定的，
            // 可以使用这个总数据长度，避免无线传输时快时慢的影响
            total_len = total_len_all;
        }
#endif
        rts_info.data_len = total_len;
        if (rts_info.data_len < 2 * 1024) {
            putchar('!');
            /* printf("<%d>\n", rts_info.data_len); */
            /* put_u16hex(rts_info.data_len); */
        }
        int err = local_tws_data_rx_monitor(decoder, &rts_info);
        if (err) {
            audio_decoder_suspend(decoder, 0);
            return -EAGAIN;
        }
        memcpy(&rts_info.seqn, pkt_addr + DEC2TWS_MEDIA_INFO_LEN, sizeof(u16));
        err = audio_wireless_sync_probe(local_tws_audio_sync, &rts_info);
        if (err) {
            /*printf("AE:%d\n", err);*/
            if (err == SYNC_ERR_STREAM_RESET) {
                dec->read_en = 0;
                if (g_dec2tws.tws_send_pause) {
                    // 暂停过程中刚好数据也没了，先不要restart
                    audio_decoder_suspend(&dec->decoder, 0);
                    return -EAGAIN;
                }
                local_tws_dec_stop_and_restart(decoder);
                /*dec2tws_master_dec_resume();*/
                tws_api_local_media_set_limit_size(DEC2TWS_MEDIA_LIMIT_SIZE);
            }
            return -EAGAIN;
        }

    }
#endif
    else {
        pkt_addr = tws_api_local_media_trans_fetch(NULL, &pkt_len);
        if (pkt_addr && (pkt_addr[0] == DEC2TWS_MEDIA_INFO_PAUSE)) {
            log_i("local tws pause \n");
            pkt_addr = tws_api_local_media_trans_pop(&pkt_len);
            if (pkt_addr) {
                tws_api_local_media_trans_free(pkt_addr);
            }
            // reset
            dec->read_en = 0;

            app_audio_output_reset(300);
            return -EAGAIN;
        }

        if (!dec->read_en) {
            // 数据足够才开始解码
            putchar('E');
            /* printf("total_len : %d ", total_len); */
            if (total_len >= (DEC2TWS_MEDIA_BUF_LEN / 5)) {
                dec->read_en = 1;
            }
            dec2tws_master_dec_resume();
            audio_decoder_suspend(&dec->decoder, 0);
            return -EINVAL;
        }
    }

    if (dec->read_en == 0) {
#if AUDIO_LOCAL_TWS_VOL_FADE_EN
        local_tws_fade_init(dec);
#endif
    }
    dec->read_en = 1;
#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
    if (dec->need_get_fmt) {
        dec->need_get_fmt = 0;
        audio_decoder_get_fmt_info(decoder, &decoder->fmt);
    }
#endif

    dec->tws_ulen = 0;
    /* putchar('d'); */
    dec->tws_ptr = tws_api_local_media_trans_pop(&dec->tws_len);
    if (!dec->tws_len) {
        putchar('e');
        audio_decoder_suspend(&dec->decoder, 0);
        return -EINVAL;
    }
    dec->tws_ulen += DEC2TWS_MEDIA_FMT_LEN;
    dec->tws_len -= DEC2TWS_MEDIA_FMT_LEN;
    if (dec->tws_len == 0) {
        tws_api_local_media_trans_free(dec->tws_ptr);
//        audio_decoder_suspend(&dec->decoder, 0);
        dec2tws_master_dec_resume();
        return -EINVAL;
    }

    return 0;
}

static int local_tws_dec_src_output_handler(struct audio_decoder *decoder, s16 *data, int len)
{
    int wlen = 0;
    int rlen = len;
    struct local_tws_dec_hdl *dec = container_of(decoder, struct local_tws_dec_hdl, decoder);

    if (!local_tws_dec || (!local_tws_dec->status)) {
        /* putchar('O'); */
        return 0;
    }

	wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
	return wlen;
}

static int local_tws_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len)
{
    int wlen = 0;
    int rlen = len;
    int in_len = len;
    struct local_tws_dec_hdl *dec = container_of(decoder, struct local_tws_dec_hdl, decoder);

    if (!dec->remain) {
#if AUDIO_LOCAL_TWS_VOL_FADE_EN
        local_tws_fade_run(dec, data, len);
#endif

#if AUDIO_LOCAL_TWS_SUPPORT_SYNC
        if (local_tws_audio_sync) {
            audio_wireless_sync_after_dec(local_tws_audio_sync, data, len);
        }
#endif


    }

#if AUDIO_LOCAL_TWS_EQ_SUPPORT_ASYNC
#if TCFG_MUSIC_MODE_EQ_ENABLE && AUDIO_LOCAL_TWS_SUPPORT_SOUND_EFFECT
    if (dec->p_eq) {
        int eqlen = audio_eq_run(dec->p_eq, data, len);
        if (len == eqlen) {
            dec->remain = 0;
        } else {
            dec->remain = 1;
        }
        return eqlen;
    }
#endif
#endif

    if (!dec->remain) {

#if TCFG_MUSIC_MODE_EQ_ENABLE && AUDIO_LOCAL_TWS_SUPPORT_SOUND_EFFECT
        if (dec->p_eq) {
            audio_eq_run(dec->p_eq, data, len);
        }
#endif

#if TCFG_MUSIC_MODE_DRC_ENABLE && AUDIO_LOCAL_TWS_SUPPORT_SOUND_EFFECT
        if (dec->p_drc) {
            audio_drc_run(dec->p_drc, data, len);
        }
#endif
    }

    do {
        if (dec->hw_src) {
            wlen = audio_src_resample_write(dec->hw_src, data, len);
        } else {
            wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
        }
        if (!wlen) {
            /* putchar('z'); */
            /* printf("wlen 0 break\n"); */
            break;
        }

        /*printf("music_write: %d, %d\n", wlen, len);*/
        if (wlen < len) {
            /*y_printf("pend: %d, %d, %d\n", wlen, len, rlen);*/
            /*audio_decoder_wait(decoder, 10);*/
            /*os_sem_pend(&dac_sem, 0);*/
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

static int local_tws_dec_post_handler(struct audio_decoder *decoder)
{
    return 0;
}

static const struct audio_dec_handler local_tws_dec_handler = {
    .dec_probe  = local_tws_dec_probe_handler,
    .dec_output = local_tws_dec_output_handler,
    .dec_post   = local_tws_dec_post_handler,
};

static void local_tws_dec_priv_func_close()
{
    struct local_tws_dec_hdl *dec = local_tws_dec;
    if (dec->tws_flag) {
        dec->tws_flag = 0;
    }
#if AUDIO_LOCAL_TWS_SUPPORT_SYNC
    if (local_tws_audio_sync) {
        audio_wireless_sync_close(local_tws_audio_sync);
        local_tws_audio_sync = NULL;
        clock_remove(SYNC_CLK);
        dec->sync_start = 0;
    }
    local_tws_sync_no_check_data_buf(0);
#endif
#if TCFG_MUSIC_MODE_EQ_ENABLE && AUDIO_LOCAL_TWS_SUPPORT_SOUND_EFFECT
    if (dec->p_eq) {
        audio_eq_close(dec->p_eq);
        local_irq_disable();
        free(dec->p_eq);
        dec->p_eq = NULL;
        local_irq_enable();
    }
#endif
#if TCFG_MUSIC_MODE_DRC_ENABLE && AUDIO_LOCAL_TWS_SUPPORT_SOUND_EFFECT
    if (dec->p_drc) {
        audio_drc_close(dec->p_drc);
        local_irq_disable();
        free(dec->p_drc);
        dec->p_drc = NULL;
        local_irq_enable();
    }
#endif

#if LOCAL_TWS_EQ_SUPPORT_32BIT
    if (dec->eq_out_buf) {
        free(dec->eq_out_buf);
        dec->eq_out_buf = NULL;
    }
#endif

}

int local_tws_dec_close(u8 drop_frame_start);

static void local_tws_dec_release()
{
    local_tws_dec_priv_func_close();
    audio_decoder_task_del_wait(&decode_task, &local_tws_dec->wait);

    local_dec_clock_remove(local_tws_dec->decoder.dec_ops->coding_type);

    clock_remove(DEC_TWS_SBC_CLK);

    local_irq_disable();
    free(local_tws_dec);
    local_tws_dec = NULL;
    g_dec2tws.tws_send_pause = 0;
    local_irq_enable();
}

static void local_tws_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        puts("AUDIO_DEC_EVENT_END\n");
        local_tws_dec_close(1);
        //audio_decoder_resume_all(&decode_task);
        break;
    }
}


static int local_tws_dec_start()
{
    int err;
    struct audio_fmt *fmt;
    struct audio_fmt f;
    struct local_tws_dec_hdl *dec = local_tws_dec;

    if (!dec) {
        return -EINVAL;
    }

    printf("local_tws_dec_start: in, type:0x%x \n", dec->dec_type);

#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
    if (dec->dec_type & AUDIO_CODING_FAME_MASK) {
        dec->need_get_fmt = 1;
        err = audio_decoder_open(&dec->decoder, &local_fame_tws_input, &decode_task);
    } else {
        err = audio_decoder_open(&dec->decoder, &local_file_tws_input, &decode_task);
    }
#else

    ASSERT(!(dec->dec_type & AUDIO_CODING_FAME_MASK));
    err = audio_decoder_open(&dec->decoder, &local_file_tws_input, &decode_task);
#endif
    if (err) {
        goto __err1;
    }

    dec->ch_type = AUDIO_CH_MAX;

    audio_decoder_set_handler(&dec->decoder, &local_tws_dec_handler);
    audio_decoder_set_event_handler(&dec->decoder, local_tws_dec_event_handler, 0);

    audio_decoder_set_tws_stu(&dec->decoder, 1);

    f.coding_type = dec->dec_type;
    f.sample_rate = dec->sample_rate;
    f.channel = 2;
    fmt = &f;

    err = audio_decoder_set_fmt(&dec->decoder, &f);
    if (err) {
        goto __err2;
    }

    dec->tws_flag = 1;

    local_tws_dec_set_output_channel(dec);

    audio_mixer_ch_open(&dec->mix_ch, &mixer);
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, audio_output_rate(fmt->sample_rate));

    dec->src_out_sr = audio_output_rate(fmt->sample_rate);

    if (fmt->sample_rate != dec->src_out_sr) {
        printf("file dec sr:%d, or:%d ", fmt->sample_rate, dec->src_out_sr);
        dec->hw_src = audio_hw_resample_open(&dec->decoder, local_tws_dec_src_output_handler,
                                             dec->ch_num, fmt->sample_rate, dec->src_out_sr);
    }

    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);

#if TCFG_MUSIC_MODE_EQ_ENABLE && AUDIO_LOCAL_TWS_SUPPORT_SOUND_EFFECT
    dec->p_eq = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
    if (dec->p_eq) {
        dec->p_eq->eq_ch = (struct hw_eq_ch *)((int)dec->p_eq + sizeof(struct audio_eq));
        struct audio_eq_param local_tws_eq_param = {0};
        local_tws_eq_param.channels = dec->ch_num;
        local_tws_eq_param.online_en = 1;
        local_tws_eq_param.mode_en = 1;
        local_tws_eq_param.remain_en = 1;
        local_tws_eq_param.max_nsection = EQ_SECTION_MAX;
        local_tws_eq_param.cb = eq_get_filter_info;
#if AUDIO_LOCAL_TWS_EQ_SUPPORT_ASYNC
        local_tws_eq_param.no_wait = 1;//异步
#endif

        audio_eq_open(dec->p_eq, &local_tws_eq_param);
        audio_eq_set_samplerate(dec->p_eq, fmt->sample_rate);
        audio_eq_set_output_handle(dec->p_eq, local_tws_eq_output, dec);
#if LOCAL_TWS_EQ_SUPPORT_32BIT
		audio_eq_set_info(dec->p_eq, file_eq_param.channels, 1);
#endif

        audio_eq_start(dec->p_eq);
    }
#endif
#if TCFG_MUSIC_MODE_DRC_ENABLE && AUDIO_LOCAL_TWS_SUPPORT_SOUND_EFFECT
    dec->p_drc = malloc(sizeof(struct audio_drc));
    if (dec->p_drc) {
        struct audio_drc_param drc_param = {0};
        drc_param.channels = dec->ch_num;
        drc_param.online_en = 1;
        drc_param.remain_en = 1;
        drc_param.cb = drc_get_filter_info;
        audio_drc_open(dec->p_drc, &drc_param);
        audio_drc_set_samplerate(dec->p_drc, fmt->sample_rate);
        audio_drc_set_output_handle(dec->p_drc, NULL, NULL);
#if LOCAL_TWS_EQ_SUPPORT_32BIT
		audio_drc_set_32bit_mode(dec->p_drc, 1);
#endif

        audio_drc_start(dec->p_drc);
    }
#endif

    local_dec_clock_add(dec->decoder.dec_ops->coding_type);

#if AUDIO_LOCAL_TWS_SUPPORT_SYNC
    local_tws_audio_sync_open(&dec->decoder, fmt->sample_rate, audio_output_rate(fmt->sample_rate));
#endif

    err = audio_decoder_start(&dec->decoder);
    if (err) {
        goto __err3;
    }

    local_tws_drop_frame_stop();
    bt_drop_a2dp_frame_start();
    clock_set_cur();
    dec->status = 1;

    /* os_time_dly(500); */
    /* log_i("total_time:%d ", audio_decoder_get_total_time(&dec->decoder)); */
    /* log_i("play_time:%d ", audio_decoder_get_play_time(&dec->decoder)); */

    return 0;

__err3:
    if (dec->hw_src) {
        audio_hw_resample_close(dec->hw_src);
        dec->hw_src = NULL;
    }
    audio_mixer_ch_close(&dec->mix_ch);
__err2:
    audio_decoder_close(&dec->decoder);
__err1:
    local_tws_dec_release();

    return err;
}

static int local_tws_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;

    struct local_tws_dec_hdl *dec = local_tws_dec;

    if (event == AUDIO_RES_GET) {
        if (dec->status == 0) {
            tws_api_local_media_set_limit_size(DEC2TWS_MEDIA_LIMIT_SIZE);
            err = local_tws_dec_start();
        } else if (dec->tmp_pause) {
            tws_api_local_media_set_limit_size(DEC2TWS_MEDIA_LIMIT_SIZE);
            local_tws_drop_frame_stop();
            dec->tmp_pause = 0;

            audio_mixer_ch_open(&dec->mix_ch, &mixer);
            audio_mixer_ch_set_sample_rate(&dec->mix_ch, audio_output_rate(dec->src_out_sr));

            audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);
            /*audio_output_start(dec->src_out_sr, 1);*/
#if AUDIO_LOCAL_TWS_SUPPORT_SYNC
            local_tws_audio_sync_open(&dec->decoder, dec->sample_rate, audio_output_rate(dec->sample_rate));
#endif
            if (dec->status) {
                err = audio_decoder_start(&dec->decoder);
            }
        }
    } else if (event == AUDIO_RES_PUT) {
        /* puts("localtws AUDIO_RES_PUT\n"); */
        if (dec->status) {
            dec->tmp_pause = 1;
            err = audio_decoder_pause(&dec->decoder);
            os_time_dly(2);
            // 先暂停再清数据，避免清了数据后dec还在output
            tws_api_local_media_trans_clear();
            local_tws_drop_frame_start();
#if AUDIO_LOCAL_TWS_SUPPORT_SYNC
            if (local_tws_audio_sync) {
                audio_wireless_sync_close(local_tws_audio_sync);
                dec->sync_start = 0;
                local_tws_audio_sync = NULL;
                clock_remove(SYNC_CLK);
            }
#endif
            audio_mixer_ch_close(&dec->mix_ch);
        }
    }

    return err;
}

static int local_tws_dec_open_mem(void)
{
    struct local_tws_dec_hdl *dec;
    dec = zalloc(sizeof(*dec));
    if (!dec) {
        return -ENOMEM;
    }
    if (local_tws_dec) {
        local_tws_dec_close(0);
    }

    local_tws_dec = dec;

    return 0;
}

int local_tws_dec_create(void)
{
    sys_auto_shut_down_disable();
    return 0;
}
int local_tws_timer = 0;
static void local_tws_drop_frame(void *p)
{
    int len;
    /* r_printf("local_tws_drop_frame=%d\n", g_dec2tws.drop_frame_start); */
    putchar('L');
    tws_api_local_media_trans_clear();
    if (g_dec2tws.drop_frame_start) {
        g_dec2tws.drop_frame_start = 0;
        putchar('l');
        /* puts("drop_frame_start return\n"); */
        sys_timeout_del(local_tws_timer);
        local_tws_timer = 0;
        /* tws_api_local_media_trans_clear(); */
        return;
    }
    /* int num = tws_api_local_media_packet_cnt(NULL); */
    /* if (num > 1) { */
    /* for (int i = 0; i < (num - 1); i++) { */
    /* void *ptr = tws_api_local_media_trans_pop(&len); */
    /* if (!ptr) { */
    /* break; */
    /* } */
    /* tws_api_local_media_trans_free(ptr); */
    /* } */
    /* dec2tws_master_dec_resume(); */

    /* } */
    extern int tone_get_status();
    extern u8 is_a2dp_dec_open();
    if (tone_get_status() || is_a2dp_dec_open()) {
        local_tws_timer = sys_timeout_add(NULL, local_tws_drop_frame, 10);
    }
    /* local_tws_timer = sys_timeout_add(NULL, local_tws_drop_frame, 100); */

}
void local_tws_drop_frame_start()
{
    r_printf("local_tws_drop_frame_start %d\n", local_tws_timer);
    local_irq_disable();
    if (local_tws_timer == 0) {
        local_tws_timer = sys_timeout_add(NULL, local_tws_drop_frame, 500);
        printf("local_tws_timer=0x%x\n", local_tws_timer);
    }
    local_irq_enable();

}

void local_tws_drop_frame_stop()
{
    r_printf("local_tws_drop_frame_stop\n");
    local_irq_disable();
    if (local_tws_timer) {
        sys_timeout_del(local_tws_timer);
        local_tws_timer = 0;
    }
    g_dec2tws.drop_frame_start = 0;
    local_irq_enable();
}

int local_tws_dec_open(u32 value)
{
    struct audio_fmt fmt = {0};
    dec2tws_media_get(&value, &fmt);
    if (local_tws_dec && (local_tws_dec->media_value != value)) {
        local_tws_dec_close(0);
    }
    if (!local_tws_dec) {
        local_tws_dec_open_mem();
    } else {
        return 0;
    }

    int err;
    struct local_tws_dec_hdl *dec = local_tws_dec;

    printf(" ******  local_tws_dec_open: in, \n");

    if (!dec) {
        return -EPERM;
    }

#if 1
    a2dp_dec_close();
    esco_dec_close();
    g_dec2tws.drop_frame_start = 0;
#endif

    dec->media_value = value;
    dec->dec_type = fmt.coding_type;
    dec->sample_rate = fmt.sample_rate;

    dec->wait.priority = 1;
    dec->wait.preemption = 1;
    dec->wait.handler = local_tws_wait_res_handler;
    err = audio_decoder_task_add_wait(&decode_task, &dec->wait);

    return err;
}

int local_tws_dec_close(u8 drop_frame_start)
{
    /* puts("local_tws_dec_close start\n"); */
    if (!local_tws_dec) {
        return 0;
    }
    if (drop_frame_start) {
        g_dec2tws.drop_frame_start = 1;	// 关闭过程中可能刚好会有数据过来
    }

    if (local_tws_dec->status) {
        local_tws_dec->status = 0;
        audio_decoder_close(&local_tws_dec->decoder);
        if (local_tws_dec->hw_src) {
            audio_hw_resample_close(local_tws_dec->hw_src);
            local_tws_dec->hw_src = NULL;
        }
        audio_mixer_ch_close(&local_tws_dec->mix_ch);
    }

    g_dec2tws.media_value = 0;
    if (g_dec2tws.tmrout) {
        sys_hi_timeout_del(g_dec2tws.tmrout);
        g_dec2tws.tmrout = 0;
    }

    local_tws_dec_release();

    tws_api_local_media_trans_clear();
    if (drop_frame_start) {
        g_dec2tws.drop_frame_start = 1;
        local_tws_drop_frame_start();
    } else {
        local_tws_drop_frame_stop();
        g_dec2tws.drop_frame_start = 0;
    }
    clock_set_cur();
    puts("*****  local_tws_dec_close: exit\n");
    return 1;
}

int local_tws_output(struct audio_fmt *pfmt, s16 *data, int len)
{
    if ((local_tws_dec) && (local_tws_dec->status) && (local_tws_dec->tmp_pause)) {
        // 被打断暂停后抛弃数据
        g_dec2tws.seqn = 0;
        return len;
    }
    u8 *tws_buf = tws_api_local_media_trans_alloc(len +  DEC2TWS_MEDIA_FMT_LEN);
    if (!tws_buf) {
        /* putchar('f'); */
        return 0;
    }
    if (g_dec2tws.tws_send_pause == 2) {
        g_dec2tws.tws_send_pause = 0;
        tws_api_local_media_set_limit_size(DEC2TWS_MEDIA_LIMIT_SIZE);
    }
    memcpy(tws_buf, &g_dec2tws.media_value, DEC2TWS_MEDIA_INFO_LEN);
    memcpy(tws_buf + DEC2TWS_MEDIA_INFO_LEN, &g_dec2tws.seqn, sizeof(u16));
    memcpy(tws_buf + DEC2TWS_MEDIA_FMT_LEN, data, len);
    tws_api_local_media_trans_push(tws_buf, len +  DEC2TWS_MEDIA_FMT_LEN);
    g_dec2tws.seqn ++;
    return len;
}

int local_tws_push_info(u8 info, int tmrout)
{
    int to = 0;
    u8 *buf;
    tws_api_local_media_set_limit_size(0);
    while (1) {
        buf = tws_api_local_media_trans_alloc(DEC2TWS_MEDIA_FMT_LEN);
        if (buf) {
            dec2tws_media_set(buf, NULL);
            buf[0] = info;
            tws_api_local_media_trans_push(buf, DEC2TWS_MEDIA_FMT_LEN);
            g_dec2tws.seqn = 0; // 重新开始计数
            os_time_dly(1);
            return true;
        }
        os_time_dly(1);
        if (tmrout) {
            to += 10;
            if (to >= tmrout) {
                break;
            }
        }
    }
    return false;
}

int local_tws_send_end(int tmrout)
{
    return local_tws_push_info(DEC2TWS_MEDIA_INFO_STOP, tmrout);
}
int local_tws_send_pause(int tmrout)
{
    return local_tws_push_info(DEC2TWS_MEDIA_INFO_PAUSE, tmrout);
}

void local_tws_decoder_pause(void)
{
    g_dec2tws.tws_send_pause = 1;
#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))
    pcm2tws_enc_reset();
#endif
    local_tws_send_pause(100);
    g_dec2tws.tws_send_pause = 2;
}

void local_tws_start(struct audio_fmt *pfmt)
{
    dec2tws_media_set(&g_dec2tws.media_value, pfmt);

    local_tws_dec_create();
    local_tws_dec_open(g_dec2tws.media_value);
}

extern void tws_api_local_media_trans_clear_no_ready(void);
void local_tws_stop(void)
{
    puts("local_tws_stop\n");
    // 清除还没有准备发射的
    tws_api_local_media_trans_clear_no_ready();
    // 发送一个结束包
    local_tws_send_end(200);
    int to_cnt = 0;
    // 超时等待结束包被取走
    while (tws_api_local_media_trans_check_total(0)) {
        dec2tws_master_dec_resume();
        os_time_dly(1);
        to_cnt += 10;
        if (to_cnt > 500) {
            printf("local tws send end timer out \n");
            break;
        }
    }

    local_tws_dec_close(0);
    /* int to_cnt = 0; */
    /* while (tws_api_local_media_trans_check_total(0)) { */
    /* os_time_dly(1); */
    /* to_cnt += 10; */
    /* if (to_cnt > 500) { */
    /* break; */
    /* } */
    /* } */
    /* tws_api_local_media_trans_clear(); */
}


//////////////////////////////////////////////////////////////////////////////
#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))
// 把压缩后的数据转换为tws发送的数据

struct encfile_tws_dec_hdl {
    struct audio_decoder decoder;
    u8 file_buf[DEC2TWS_ENCFILE_BUF_LEN];
    cbuffer_t file_cbuf;
    u32 status : 1;
};

static struct encfile_tws_dec_hdl *encfile_tws_dec = NULL;

void encfile_tws_dec_close();

void encfile_tws_dec_resume(void)
{
    if (encfile_tws_dec && encfile_tws_dec->status) {
        audio_decoder_resume(&encfile_tws_dec->decoder);
    }
}

int encfile_tws_wfile(struct audio_fmt *pfmt, s16 *data, int len)
{
    if (!encfile_tws_dec) {
        return 0;
    }
    struct encfile_tws_dec_hdl *dec = encfile_tws_dec;
    int wlen = cbuf_write(&dec->file_cbuf, data, len);
    if (wlen != len) {
        if (dec->status == 0) {
            int err = audio_decoder_start(&dec->decoder);
            if (!err) {
                dec->status = 1;
            }
        }
    }
    encfile_tws_dec_resume();
    return wlen;
}

static int encfile_tws_read(struct audio_decoder *decoder, void *buf, u32 len)
{
    int rlen = 0;
    struct encfile_tws_dec_hdl *dec = container_of(decoder, struct encfile_tws_dec_hdl, decoder);
    if (len > dec->file_cbuf.data_len) {
        len = dec->file_cbuf.data_len;
    }
    rlen = cbuf_read(&dec->file_cbuf, buf, len);
    if (!rlen) {
        pcm2tws_enc_resume();
    }
    return rlen;
}
static int encfile_tws_len(struct audio_decoder *decoder)
{
    return 0x7fffffff;
}

static const struct audio_dec_input encfile_tws_input = {
    .coding_type = AUDIO_CODING_MP3,
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
            .fread = encfile_tws_read,
            /* .fseek = encfile_tws_seek, */
            .flen  = encfile_tws_len,
        }
    }
};

static int encfile_tws_dec_probe_handler(struct audio_decoder *decoder)
{
    struct encfile_tws_dec_hdl *dec = container_of(decoder, struct encfile_tws_dec_hdl, decoder);
    if (dec->status == 0) {
        return -EPERM;
    }
    return 0;
}

static int encfile_tws_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    struct encfile_tws_dec_hdl *dec = container_of(decoder, struct encfile_tws_dec_hdl, decoder);
    int ret = local_tws_output(&dec->decoder.fmt, data, len);
    /* printf("encfile tws out len:%d, ret:%d ", len, ret); */
    /* printf_buf(data, len); */
    return ret;
}

static int encfile_tws_dec_post_handler(struct audio_decoder *decoder)
{
    return 0;
}

static const struct audio_dec_handler encfile_tws_dec_handler = {
    .dec_probe  = encfile_tws_dec_probe_handler,
    .dec_output = encfile_tws_dec_output_handler,
    .dec_post   = encfile_tws_dec_post_handler,
};

static void encfile_tws_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        //audio_decoder_resume_all(&decode_task);
        break;
    }
}

static void encfile_tws_dec_release()
{
    local_irq_disable();
    free(encfile_tws_dec);
    encfile_tws_dec = NULL;
    local_irq_enable();
}

int encfile_tws_dec_open(struct audio_fmt *pfmt)
{
    int err;
    struct audio_fmt f = {0};
    struct encfile_tws_dec_hdl *dec;

    puts("encfile_tws_dec_start: in\n");

    if (encfile_tws_dec) {
        encfile_tws_dec_close();
    }
    encfile_tws_dec = zalloc(sizeof(*encfile_tws_dec));
    ASSERT(encfile_tws_dec);

    dec = encfile_tws_dec;

    cbuf_init(&dec->file_cbuf, dec->file_buf, sizeof(dec->file_buf));

    err = audio_decoder_open(&dec->decoder, &encfile_tws_input, &decode_task);
    if (err) {
        goto __err1;
    }

    audio_decoder_set_handler(&dec->decoder, &encfile_tws_dec_handler);
    audio_decoder_set_event_handler(&dec->decoder, encfile_tws_dec_event_handler, 0);

    audio_decoder_set_pick_stu(&dec->decoder, 1);
    audio_decoder_set_tws_stu(&dec->decoder, 1);

    f.coding_type = pfmt->coding_type | AUDIO_CODING_STU_PICK;
    f.sample_rate = pfmt->sample_rate;
    f.channel = pfmt->channel;

    err = audio_decoder_set_fmt(&dec->decoder, &f);
    if (err) {
        goto __err2;
    }

    return 0;

__err2:
    audio_decoder_close(&dec->decoder);
__err1:
    encfile_tws_dec_release();

    printf("encfile open err");
    return err;
}

void encfile_tws_dec_close()
{
    if (!encfile_tws_dec) {
        return;
    }
    struct encfile_tws_dec_hdl *dec = encfile_tws_dec;

    if (dec->status) {
        dec->status = 0;
        audio_decoder_close(&dec->decoder);
    }

    encfile_tws_dec_release();

    puts("encfile_tws_dec_close: exit\n");
}

#endif


//////////////////////////////////////////////////////////////////////////////
#include "app_action.h"


void dec2tws_master_dec_resume(void)
{
#if (defined(TCFG_APP_MUSIC_EN) && (TCFG_APP_MUSIC_EN))
    file_dec_resume();
#endif
#if (defined(TCFG_APP_LINEIN_EN) && (TCFG_APP_LINEIN_EN))
    linein_dec_resume();
#endif
#if (defined(TCFG_APP_FM_EN) && (TCFG_APP_FM_EN))
    fm_dec_resume();
#endif
#if (defined(TCFG_APP_PC_EN) && (TCFG_APP_PC_EN))
    uac_dec_resume();
#endif
#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))
    pcm2tws_enc_resume();
#endif
}

void dec2tws_dec_restart(void)
{
    if (0) {
        return ;
    }
#if (defined(TCFG_APP_MUSIC_EN) && (TCFG_APP_MUSIC_EN))
    else if (true == file_dec_push_restart()) {
        return ;
    }
#endif
#if (defined(TCFG_APP_LINEIN_EN) && (TCFG_APP_LINEIN_EN))
    else if (true == linein_dec_push_restart()) {
        return ;
    }
#endif
#if (defined(TCFG_APP_FM_EN) && (TCFG_APP_FM_EN))
    else if (true == fm_dec_push_restart()) {
        return ;
    }
#endif
#if (defined(TCFG_APP_PC_EN) && (TCFG_APP_PC_EN))
    else if (true == uac_dec_push_restart()) {
        return ;
    }
#endif
}

int dec2tws_check_enable(void)
{
    int state = tws_api_get_tws_state();
    if ((state & TWS_STA_SIBLING_CONNECTED)) {
        return true;
    }
    return false;
}

int dec2tws_tws_event_deal(struct bt_event *evt)
{
    /* int state = tws_api_get_tws_state(); */
    /* log_i(">>>>>>>>>>>>>>>>> %s, state:0x%x ", __func__, state); */

    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
    case TWS_EVENT_CONNECTION_DETACH:
        dec2tws_dec_restart();
        break;
    }
    return 0;
}

#endif /*TCFG_DEC2TWS_ENABLE*/

