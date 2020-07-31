/*****************************************************************
>file name : apps/earphone/bt_media.c
>author : lichao
>create time : Sat 27 Apr 2019 07:33:28 PM CST
*****************************************************************/
#include "system/includes.h"
#include "media/includes.h"
#include "btstack/avctp_user.h"
#include "app_config.h"
#include "audio_config.h"
#include "aec_user.h"
#include "bt_tws.h"
#include "classic/tws_api.h"
#include "app_online_cfg.h"
#include "app_main.h"
#include "app_online_cfg.h"

#define LOG_TAG             "[BT_MEIDA]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

#define BT_MUSIC_DECODE
#define BT_PHONE_DECODE

/*u8 audio_dma_buffer[AUDIO_FIXED_SIZE] ALIGNED(4) sec(.audio_buf);*/

#if 0

static struct server *audio_dec = NULL;

extern int sbc_play_sync_open(struct server *server, int buffer_size);
extern int phone_call_sync_open(struct server *server, int buffer_size);
extern void *a2dp_media_get_packet(int *len, int *error_code);
extern void *a2dp_media_fetch_packet(int *len, void *prev_packet);
extern void a2dp_media_free_packet(void *_packet);
extern int a2dp_media_get_total_data_len();
extern int a2dp_media_get_total_buffer_size();
extern int a2dp_media_get_remain_buffer_size();
extern void a2dp_media_suspend_resume(u8 flag);
extern u32 lmp_private_fetch_sco_packet();
#ifdef BT_MUSIC_DECODE

void earphone_a2dp_audio_codec_close();
static void sbc_dec_event_handler(void *priv, int argc, int *argv)
{
    union audio_dec_req req = {0};
    switch (argv[0]) {
    case AUDIO_PLAY_EVENT_END:
        if (audio_dec) {
            earphone_a2dp_audio_codec_close();
        }
        break;
    default:

        break;
    }
}


static int sbc_get_packet(void *priv, struct audio_packet_buffer *pkt)
{
    int read_len = 0;
    int error_code = 0;

    if (!audio_dec) {
        return 0;
    }

    pkt->baddr = (u32)a2dp_media_get_packet(&read_len, &error_code);
    if (pkt->baddr == 0) {
        pkt->len = 0;
        if (error_code == -1) {
            return -ENOMEM;
        }
        return 0;
    }

    pkt->len = read_len;

    return 0;
}

static int sbc_put_packet(void *priv, struct audio_packet_buffer *pkt)
{
    if (pkt->baddr) {
        a2dp_media_free_packet((void *)pkt->baddr);
    }

    return 0;
}

static int sbc_fetch_packet(void *priv, struct audio_packet_buffer *pkt)
{
    int len = 0;
    pkt->baddr = (u32)a2dp_media_fetch_packet(&len, (void *)pkt->baddr);
    if (pkt->baddr) {
        pkt->len = len;
        return 0;
    }
    return -ENOMEM;
}

static int sbc_packet_buffer_query(void *priv, struct audio_packet_stats *stats)
{
    /*stats->total_size = a2dp_media_get_total_buffer_size();*/
    stats->remain_size = a2dp_media_get_remain_buffer_size();
    stats->data_size = a2dp_media_get_total_data_len();
    return 0;
}

static int sbc_packet_dec_event_handler(void *priv, int event)
{
    switch (event) {
    case PACKET_EVENT_DEC_SUSPEND:
        log_info("PACKET DEC EVENT : suspend\n");
        a2dp_media_suspend_resume(1);
        break;
    case PACKET_EVENT_DEC_RESUME:
        log_info("PACKET DEC EVENT : resume\n");
        a2dp_media_suspend_resume(2);
        break;
    default:
        break;
    }

    return 0;
}

static const struct audio_packet_ops sbc_packet_ops = {
    .get_packet = sbc_get_packet,
    .put_packet = sbc_put_packet,
    .query      = sbc_packet_buffer_query,
    .fetch      = sbc_fetch_packet,
    .event_handler = sbc_packet_dec_event_handler,
};

#ifndef AUDIO_OUTPUT_MODE
#define AUDIO_OUTPUT_MODE   AUDIO_OUTPUT_MONO_LR_CH
#endif

s16 audio_dac_unmute_energy(void)
{
    return 128;
}

static u8 mute_L = 0;
static u8 mute_R = 0;
static void sbc_auto_mute_handler(u8 event, u8 channel)
{
    switch (event) {
    case DEC_EVENT_AUTO_MUTE:
#if (AUDIO_OUTPUT_MODE == AUDIO_OUTPUT_STEREO)
        if (channel == 0) {
            if (mute_R) {
                break;
            }
            app_audio_mute(AUDIO_MUTE_L_CH);
            mute_L = 1;
        } else if (channel == 1) {
            if (mute_L) {
                break;
            }
            app_audio_mute(AUDIO_MUTE_R_CH);
            mute_R = 1;
        } else if (channel == 2) {
            mute_L = 1;
            mute_R = 1;
            /*声道分离 : 两个声道能量都变低的情况下不用mute全部声道,修复iphone按键音的问题*/
            /*app_audio_mute(AUDIO_MUTE_DEFAULT);*/
        }
#else
        app_audio_mute(AUDIO_MUTE_DEFAULT);
#endif
        /*log_info(">>>auto mute %d\n", channel);*/
        break;
    case DEC_EVENT_AUTO_UNMUTE:
#if (AUDIO_OUTPUT_MODE == AUDIO_OUTPUT_STEREO)
        if (channel == 0) {
            mute_L = 0;
            app_audio_mute(AUDIO_UNMUTE_L_CH);
        } else if (channel == 1) {
            app_audio_mute(AUDIO_UNMUTE_R_CH);
            mute_R = 0;
        } else if (channel == 2) {
            mute_L = 0;
            mute_R = 0;
            app_audio_mute(AUDIO_UNMUTE_DEFAULT);
        }
#else
        app_audio_mute(AUDIO_UNMUTE_DEFAULT);
#endif
        /*log_info(">>>auto unmute %d\n", channel);*/
        break;
    }
}

#define A2DP_CODEC_SBC			0x00
#define A2DP_CODEC_MPEG12		0x01
#define A2DP_CODEC_MPEG24		0x02

int earphone_a2dp_audio_codec_open(int media_type)
{
    int err = 0;
    union audio_dec_req req = {0};

    audio_dec = server_open("audio_dec", NULL);
    if (!audio_dec) {
        log_info("audio decode server open error\n");
        return -EFAULT;
    }

    /*播放事件注册(这个可以先不用加,根据需要添加)*/
    server_register_event_handler(audio_dec, NULL, sbc_dec_event_handler);
    switch (media_type) {
    case A2DP_CODEC_SBC:
        req.play.format = "sbc";
        break;
    case A2DP_CODEC_MPEG24:
        break;
    }
    req.play.cmd = AUDIO_DEC_OPEN;
    req.play.auto_dec = 1;
    /*req.play.sample_rate = 44100;*/
    req.play.priority = MUSIC_PRIORITY;
    req.play.volume = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);//100;//音量百分比

#if TCFG_BT_MUSIC_EQ_ENABLE
#if TCFG_EQ_ONLINE_ENABLE
    clk_set("sys", BT_A2DP_ONLINE_EQ_HZ);
#elif (AUDIO_OUTPUT_MODE == AUDIO_OUTPUT_STEREO)
    clk_set("sys", BT_A2DP_STEREO_EQ_HZ);
#else
    clk_set("sys", BT_A2DP_MONO_EQ_HZ);
#endif
#endif

#if (AUDIO_OUTPUT_MODE == AUDIO_OUTPUT_STEREO)
    req.play.channel = 2; //配置播放声道：1 - 单声道, 2 - 双声道(立体声)
#else
    req.play.channel = 1; //配置播放声道：1 - 单声道, 2 - 双声道(立体声)
#endif

#if TCFG_USER_TWS_ENABLE
    req.play.ch_mode = tws_api_get_local_channel() == 'L' ? AUDIO_OUTPUT_L_CH : AUDIO_OUTPUT_R_CH;//;
#else
    req.play.ch_mode = AUDIO_OUTPUT_MODE;
#endif

    req.play.output_buf_len = AUDIO_FIXED_SIZE;
    req.play.output_buf = audio_dma_buffer;
    req.play.stereo_sync = 1;
    req.play.fade_en    = 1;
#if (TCFG_BT_MUSIC_EQ_ENABLE == 1)
    req.play.eq_en    = 1;
#endif
    req.play.pkt_ops = &sbc_packet_ops;
    err = server_request(audio_dec, AUDIO_REQ_DEC, &req);

    sbc_play_sync_open(audio_dec, a2dp_media_get_total_buffer_size());

    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());

#if AUDIO_OUTPUT_MODE == AUDIO_OUTPUT_STEREO
    req.auto_mute.state = AUTO_MUTE_OPEN;
    req.auto_mute.mute_energy = 8;
    req.auto_mute.filt_points = 200;
    req.auto_mute.filt_number = 50;
    req.auto_mute.event_handler = sbc_auto_mute_handler;
    /*req.auto_mute.pcm_mute = 1;*/
    err = server_request(audio_dec, AUDIO_REQ_DEC_AUTO_MUTE, &req);
#endif
    /*start*/
    req.play.cmd = AUDIO_DEC_START;
    req.play.auto_dec = 1;
    err = server_request(audio_dec, AUDIO_REQ_DEC, &req);
#if (TCFG_BT_MUSIC_EQ_ENABLE == 1)
    eq_cfg_sync(MUSIC_PRIORITY);
#endif

    return err;
}

void earphone_a2dp_audio_codec_close()
{
    union audio_dec_req req = {0};

    if (!audio_dec) {
        return;
    }

    log_info("audio decode server close\n");
    req.play.cmd = AUDIO_DEC_STOP;
    server_request(audio_dec, AUDIO_REQ_DEC, &req);

    server_close(audio_dec);

    app_audio_state_exit(APP_AUDIO_STATE_MUSIC);

    audio_dec = NULL;
}
#endif

#ifdef BT_PHONE_DECODE

struct server *phone_dec = NULL;

struct server *phone_rec_sever = NULL;
extern int lmp_private_send_esco_packet(void *priv, u8 *packet, int len);
int (*put_packet)(void *priv, void *buff, int len);
static const struct as_packet_ops rec_packet_ops = {
    .put_packet = (int (*)(void *, void *, int))lmp_private_send_esco_packet,
};
#endif

int phone_call_end(void *priv);

extern int lmp_private_get_esco_data_len();
extern void lmp_private_free_esco_packet(void *packet);
extern void *lmp_private_get_esco_packet(int *len, u32 *hash);
extern int lmp_private_esco_suspend_resume(int flag);
static void phone_call_dec_event_handler(void *priv, int argc, int *argv)
{
    union audio_dec_req req = {0};
    switch (argv[0]) {
    case AUDIO_PLAY_EVENT_END:
        if (phone_dec) {
            phone_call_end(NULL);
        }
        break;
    default:
        break;
    }
}
static int phone_call_fetch_packet(void *priv, struct audio_packet_buffer *pkt)
{
    int len = 0;
    if (!phone_dec) {
        pkt->baddr = 0;
        return -ENOMEM;
    }
    pkt->baddr = lmp_private_fetch_sco_packet();
    if (pkt->baddr) {
        pkt->len = len;
        return 0;
    }
    return -ENOMEM;
}


static int phone_call_get_packet(void *priv, struct audio_packet_buffer *pkt)
{
    int read_len;
    u32 hash = 0;
    if (!phone_dec) {
        return 0;
    }

    pkt->baddr = (u32)lmp_private_get_esco_packet(&read_len, &hash);
    pkt->slot_time = hash;
    if (pkt->baddr == 0) {
        pkt->len = 0;
        if (read_len == -1) {
            return -ENOMEM;
        }
        return 0;
    }

    /*播题示音(inband_ringtone == 1但是蓝牙不播)的时候清掉收到的sco数据*/
#if (BT_INBAND_RINGTONE == 0)
    extern u8 get_device_inband_ringtone_flag(void);
    if (bt_user_priv_var.phone_income_flag && get_device_inband_ringtone_flag()) {
        memset((void *)pkt->baddr, 0xAA, read_len);
        //putchar('d');
    }
#endif

    pkt->len = read_len;
    return 0;
}

static int phone_call_put_packet(void *priv, struct audio_packet_buffer *pkt)
{
    if (pkt->baddr) {
        lmp_private_free_esco_packet((void *)pkt->baddr);
    }
    return 0;
}

static int phone_call_packet_buffer_query(void *priv, struct audio_packet_stats *stats)
{
    /*stats->total_size = 60 * 50;//3 * 1024;*/
    stats->data_size  = lmp_private_get_esco_data_len();
    stats->remain_size = 512;
    return 0;
}

extern void aec_suspend(u8 en);
static int phone_packet_dec_event_handler(void *priv, int event)
{
    switch (event) {
    case PACKET_EVENT_DEC_SUSPEND:
        lmp_private_esco_suspend_resume(1);
        aec_suspend(1);
        log_info("PACKET DEC EVENT :call suspend\n");
        break;
    case PACKET_EVENT_DEC_RESUME:
        lmp_private_esco_suspend_resume(2);
        if (aec_param.wideband) {
            if (aec_param.enablebit == AEC_MODE_ADVANCE) {
                clk_set("sys", BT_CALL_16k_ADVANCE_HZ);
            } else {
                clk_set("sys", BT_CALL_16k_HZ);
            }
        } else {
            if (aec_param.enablebit == AEC_MODE_ADVANCE) {
                clk_set("sys", BT_CALL_ADVANCE_HZ);
            } else {
                clk_set("sys", BT_CALL_HZ);
            }
        }
        aec_suspend(0);
        log_info("PACKET DEC EVENT :call resume\n");
        break;
    default:
        break;
    }

    return 0;
}
static const struct audio_packet_ops phone_call_packet_ops = {
    .get_packet = phone_call_get_packet,
    .put_packet = phone_call_put_packet,
    .query      = phone_call_packet_buffer_query,
    .event_handler = phone_packet_dec_event_handler,
    /* .fetch      = phone_call_fetch_packet, */
};


static int phone_call_rec_mute(void)
{
    union audio_rec_req req = {0};
    if (!phone_rec_sever) {
        return -EINVAL;
    }

    req.rec.state = AUDIO_STATE_REC_MUTE;
    return server_request(phone_rec_sever, AUDIO_REQ_REC, &req);
}

static int phone_call_rec_unmute(void)
{
    union audio_rec_req req = {0};
    if (!phone_rec_sever) {
        return -EINVAL;
    }

    req.rec.state = AUDIO_STATE_REC_UNMUTE;
    return server_request(phone_rec_sever, AUDIO_REQ_REC, &req);
}

static int phone_decode_start(int format)
{
    union audio_dec_req req = {0};
    int error_code = 0;

    if (phone_dec) {
        return -EINVAL;
    }

    phone_dec = server_open("audio_dec", NULL);
    if (!phone_dec) {
        log_info("phone decode server open error\n");
        return -EINVAL;
    }

    server_register_event_handler(phone_dec, NULL, phone_call_dec_event_handler);

    log_info("phone call decode open type %d\n", format);
    req.play.cmd = AUDIO_DEC_OPEN;
    req.play.auto_dec = 0;
    req.play.fade_en    = 1;
    req.play.stereo_sync = 1;

    if (format == 3) {
        req.play.format = "msbc";
        req.play.channel = 1;
        req.play.sample_rate = 16000;
    } else if (format == 2) {
        req.play.format = "cvsd";
        req.play.channel = 1;
        req.play.sample_rate = 8000;
    }

    req.play.output_buf_len = AUDIO_CALL_SIZE;
    req.play.output_buf = audio_dma_buffer;
    req.play.priority = PHONE_CALL_PRIORITY;
    req.play.volume = app_var.call_volume;//app_var.aec_dac_gain;
    req.play.pkt_ops = &phone_call_packet_ops;
#if (TCFG_PHONE_EQ_ENABLE == 1)
    req.play.eq_en = 1;
#endif
    req.play.soft_limiter = 0;

#if TCFG_AEC_SIMPLEX
    req.play.noise_limiter = 1;
#endif

    error_code = server_request(phone_dec, AUDIO_REQ_DEC, &req);

#if CONFIG_TWS_SCO_ONLY_MASTER
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        app_audio_state_switch(APP_AUDIO_STATE_CALL, 0);
    } else {
        app_audio_state_switch(APP_AUDIO_STATE_CALL, app_var.aec_dac_gain);
    }
#else
    app_audio_state_switch(APP_AUDIO_STATE_CALL, app_var.aec_dac_gain);
#endif
    /*app_audio_set_volume(APP_AUDIO_STATE_CALL, app_var.call_volume);*/

    phone_call_sync_open(phone_dec, 60 * 50);
    /*start*/
    req.play.cmd = AUDIO_DEC_START;
    req.play.auto_dec = 1;
    error_code = server_request(phone_dec, AUDIO_REQ_DEC, &req);

#if (TCFG_PHONE_EQ_ENABLE == 1)
    phone_call_eq_open();
#endif

    return error_code;
}

static int phone_decode_close(void)
{
    union audio_dec_req req = {0};

    if (phone_dec == NULL) {
        return -1;
    }

    req.play.cmd = AUDIO_DEC_STOP;
    server_request(phone_dec, AUDIO_REQ_DEC, &req);
    server_close(phone_dec);

    app_audio_state_exit(APP_AUDIO_STATE_CALL);
    phone_dec = NULL;

    return 0;
}


static int phone_speak_start(int format, int packet_len)
{
    union audio_rec_req rec_req = {0};
    struct aec_s_attr *aec_attr;

    phone_rec_sever = server_open("audio_rec", NULL);
    if (!phone_rec_sever) {
        log_info("audio recode server open error\n");
        return -1;
    }

    if (format == 3) {
        rec_req.rec.format = "msbc";
        rec_req.rec.channel = 1;
        rec_req.rec.sample_rate = 16000;
    } else if (format == 2) {
        rec_req.rec.format = "cvsd";
        rec_req.rec.channel = 1;
        rec_req.rec.sample_rate = 8000;
    }

    aec_attr = aec_param_init(rec_req.rec.sample_rate);
    rec_req.rec.state = AUDIO_STATE_REC_START;
    rec_req.rec.channel = 1;
    rec_req.rec.source = "mic";
    /* rec_req.rec.fb_size = AUDIO_REC_FRAME_SIZE; */
    rec_req.rec.db_size = AUDIO_REC_DEV_SIZE;
    /* rec_req.rec.frame_buffer = audio_dma_buffer + AUDIO_CALL_SIZE; */
    rec_req.rec.dev_buffer = audio_dma_buffer + AUDIO_CALL_SIZE + AUDIO_REC_FRAME_SIZE;
    rec_req.rec.volume  = app_var.aec_mic_gain;

    rec_req.rec.packet_len = packet_len;
    rec_req.rec.packet_ops = &rec_packet_ops;
    rec_req.rec.priv = NULL;
    /*update aec param here*/
    rec_req.rec.aec_attr = aec_attr;

    server_request(phone_rec_sever, AUDIO_REQ_REC, &rec_req);

    return 0;
}

static int phone_speak_close(void)
{
    union audio_rec_req rec_req = {0};
    if (phone_rec_sever == NULL) {
        return -1;
    }
    rec_req.rec.state = AUDIO_STATE_REC_STOP;
    server_request(phone_rec_sever, AUDIO_REQ_REC, &rec_req);
    server_close(phone_rec_sever);
    phone_rec_sever = NULL;

    return 0;
}

int phone_call_begin(void *priv)
{
    u32 esco_param = *(u32 *)priv;
    int esco_len = esco_param >> 16;
    int codec_type = esco_param & 0x000000ff;
    int err = 0;

    if (codec_type == 3) {
#if TCFG_AEC_SIMPLEX
        clk_set("sys", BT_CALL_16k_SIMPLEX_HZ);
#else
        if (aec_param.enablebit == AEC_MODE_ADVANCE) {
            clk_set("sys", BT_CALL_16k_ADVANCE_HZ);
        } else {
            clk_set("sys", BT_CALL_16k_HZ);
        }
#endif
        log_info(">>sco_format:msbc\n");
    } else if (codec_type == 2) {
#if TCFG_AEC_SIMPLEX
        clk_set("sys", BT_CALL_SIMPLEX_HZ);
#else
        if (aec_param.enablebit == AEC_MODE_ADVANCE) {
            clk_set("sys", BT_CALL_ADVANCE_HZ);
        } else {
            clk_set("sys", BT_CALL_HZ);
        }
#endif
        log_info(">>sco_format:cvsd\n");
    } else {
        log_info("sco_format:error->please check %d\n", codec_type);
        return -1;
    }

    err = phone_decode_start(codec_type);
    if (err) {
        goto __err;
    }

    err = phone_speak_start(codec_type, esco_len);
    if (err) {
        goto __err;
    }

    return 0;

__err:
    phone_call_end(NULL);

    return err;
}

int phone_call_end(void *priv)
{
    bt_user_priv_var.phone_income_flag = 0;

    phone_speak_close();
    phone_decode_close();

    clk_set("sys", BT_NORMAL_HZ);

    log_info("phone decode server close end\n");
    return 0;
}

u8 bt_audio_is_running(void)
{
    return (audio_dec || phone_dec);
}
u8 bt_media_is_running(void)
{
    return audio_dec ? 1 : 0;
}

u8 bt_phone_dec_is_running()
{
    return phone_dec != NULL;
}

#else

int phone_call_end(void *priv)
{
    return 0;
}

/* u8 bt_audio_is_running(void) */
/* { */
/* return 0; */
/* } */

/* u8 bt_media_is_running(void) */
/* { */
/* return 0; */
/* } */

/* u8 bt_phone_dec_is_running() */
/* { */
/* return 0; */
/* } */

int phone_call_begin(void *priv)
{
    return 0;
}

#if 0
#include "media/audio_decoder.h"
static int a2dp_get_frame(void *priv, u8 **frame)
{
    int rlen = 0;
    int error_code = 0;

    /* if (!audio_dec) {
        return 0;
    } */

    *frame = (u32)a2dp_media_get_packet(&rlen, &error_code);
    //printf("[i:%x-%d]",*frame,rlen);
    if (*frame == 0) {
        /* pkt->len = 0; */
        /* if (error_code == -1) { */
        /* return -ENOMEM; */
        /* } */
        //printf("error_code:%d\n",error_code);
        return 0;
    }
    return rlen;
}

static void a2dp_put_frame(void *priv, u8 *frame)
{
    //printf("[O:%x]",frame);
    if (frame) {
        a2dp_media_free_packet((void *)frame);
    }

    return;
}

static struct audio_input a2dp_input = {
    .coding_type = AUDIO_CODING_SBC,
    .data_type   = AUDIO_INPUT_FRAME,
    .ops = {
        .frame = {
            .fget = a2dp_get_frame,
            .fput = a2dp_put_frame,
        }
    }
};

struct audio_a2dp_decoder {
    void *dec;
};
struct audio_a2dp_decoder *a2dp_decoder;

extern int a2dp_dec_close(void *_dec);
int earphone_a2dp_audio_codec_open(int media_type)
{
    a2dp_decoder = malloc(sizeof(*a2dp_decoder));
    if (a2dp_decoder) {
        a2dp_decoder->dec = a2dp_dec_open(a2dp_input);
        if (a2dp_decoder->dec) {
            g_printf("a2dp_media_codec_open succ\n");
        } else {
            g_printf("a2dp_media_codec_open failed\n");
        }
    } else {
        g_printf("a2dp_codec malloc failed\n");
    }
    return 0;
}
void earphone_a2dp_audio_codec_close()
{
    g_printf("a2dp_media_codec_close\n");
    if (a2dp_decoder && a2dp_decoder->dec) {
        a2dp_dec_close(a2dp_decoder->dec);
        free(a2dp_decoder);
        a2dp_decoder = NULL;
        g_printf("a2dp_media_codec_close succ\n");
    }
}
#endif
#endif


