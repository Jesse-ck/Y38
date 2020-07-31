#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_online_cfg.h"
#include "audio_drc.h"
#include "common/Resample_api.h"
#include "audio_reverb.h"
#include "clock_cfg.h"
#include "audio_config.h"
#define LOG_TAG     "[APP-REVERB]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"

#define REVERB_RUN_POINT_NUM		160	//固定160个点

#define HOWLING_ENABLE    			0 //啸叫抑制



extern void matching_dec_not_set_clock(u8 not_set_clock);

extern u32 audio_output_rate(int input_rate);
extern u32 audio_output_channel_num(void);
extern int audio_output_set_start_volume(u8 state);

extern struct audio_src_handle *audio_hw_resample_open(void *priv, int (*output_handler)(void *, void *, int),
        u8 channel, u16 input_sample_rate, u16 output_sample_rate);
extern void audio_hw_resample_close(struct audio_src_handle *hdl);
extern void audio_adc_set_mic_gain(struct audio_adc_hdl *adc, u8 gain);

extern struct audio_adc_hdl adc_hdl;

extern struct audio_decoder_task decode_task;
extern struct audio_mixer mixer;

//*******************混响接口h***********************//
REVERB_API_STRUCT *open_reverb(REVERB_PARM_SET *reverb_seting, u16 sample_rate)
{
    REVERB_API_STRUCT *reverb_api_obj;

    int buf_lent;

    reverb_api_obj = zalloc(sizeof(REVERB_API_STRUCT));
    if (!reverb_api_obj) {
        return NULL;
    }
    reverb_api_obj->func_api = get_reverb_func_api();

    //初始化混响参数
    if (reverb_seting) {
        memcpy(&reverb_api_obj->parm, reverb_seting, sizeof(REVERB_PARM_SET));
    } else {
        reverb_api_obj->parm.decayval = 2000;	//衰减系数 [0:4096]
        reverb_api_obj->parm.deepval = 0;//4096;	  //调节混响深度，影响pre-delay;[0-4096],4096 代表max_ms
        reverb_api_obj->parm.filtsize = 0;//4096;	//[0-4096],如果要回声效果 置0；混响效果建议4096
        reverb_api_obj->parm.wetgain = 4096;	//湿声增益：[0:4096]
        reverb_api_obj->parm.drygain = 4096;	//干声增益: [0:4096]
        reverb_api_obj->parm.sr = 16000; //配置输入的采样率，影响need_buf 大小
        reverb_api_obj->parm.max_ms = 120;	//所需要的最大延时，影响 need_buf 大小
        reverb_api_obj->parm.centerfreq_bandQ = 0;	//留声机音效，不需要置0
    }
    if (sample_rate) {
        reverb_api_obj->parm.sr = sample_rate;
    }

    //申请混响空间，初始
    buf_lent = reverb_api_obj->func_api->need_buf(reverb_api_obj->ptr, &reverb_api_obj->parm);
    reverb_api_obj->ptr = zalloc(buf_lent);
    if (!reverb_api_obj->ptr) {
        free(reverb_api_obj);
        return NULL;
    }

    clock_add_set(REVERB_CLK);
    reverb_api_obj->func_api->open(reverb_api_obj->ptr, &reverb_api_obj->parm);
    return reverb_api_obj;
}


void  close_reverb(REVERB_API_STRUCT *reverb_api_obj)
{
    if (reverb_api_obj) {
        if (reverb_api_obj->ptr) {
            free(reverb_api_obj->ptr);
            reverb_api_obj->ptr = NULL;
        }
        clock_remove_set(REVERB_CLK);
        free(reverb_api_obj);
    }
}

void update_reverb_parm(REVERB_API_STRUCT *reverb_api_obj, REVERB_PARM_SET *reverb_seting)
{
    reverb_api_obj->func_api->init(reverb_api_obj->ptr, reverb_seting);
}

//*******************啸叫抑制***********************//
/* #if HOWLING_ENABLE */
HOWLING_API_STRUCT *open_howling(HOWLING_PARM_SET *howl_para, u16 sample_rate, u8 channel)
{
    HOWLING_API_STRUCT *howling_hdl = zalloc(sizeof(HOWLING_API_STRUCT));
    if (!howling_hdl) {
        return NULL;
    }
    howling_hdl->ptr = zalloc(get_howling_buf());
    if (!howling_hdl->ptr) {
        free(howling_hdl);
        return NULL;
    }

    if (howl_para) {
        memcpy(&howling_hdl->parm, howl_para, sizeof(HOWLING_PARM_SET));
    } else {
        howling_hdl->parm.threshold = 13;
        howling_hdl->parm.depth  = 15;
        howling_hdl->parm.bandwidth = 40;
        howling_hdl->parm.attack_time = 100;
        howling_hdl->parm.release_time = 5;
        howling_hdl->parm.noise_threshold = -50000;
        howling_hdl->parm.low_th_gain = 0;
        howling_hdl->parm.sample_rate = 16000;
        howling_hdl->parm.channel = 1;
    }
    if (sample_rate) {
        howling_hdl->parm.sample_rate = sample_rate;
    }
    if (channel) {
        howling_hdl->parm.channel = channel;
    }

    howling_init(howling_hdl->ptr,
                 howling_hdl->parm.threshold,
                 howling_hdl->parm.depth,
                 howling_hdl->parm.bandwidth,
                 howling_hdl->parm.attack_time,
                 howling_hdl->parm.release_time,
                 howling_hdl->parm.noise_threshold,
                 howling_hdl->parm.low_th_gain,
                 howling_hdl->parm.sample_rate,
                 howling_hdl->parm.channel);
    clock_add_set(REVERB_HOWLING_CLK);
    return howling_hdl;
}

void close_howling(HOWLING_API_STRUCT *holing_hdl)
{
    if (holing_hdl) {
        if (holing_hdl->ptr) {
            free(holing_hdl->ptr);
            holing_hdl->ptr = NULL;
        }
        free(holing_hdl);
        clock_remove_set(REVERB_HOWLING_CLK);
    }
}
/* #endif */

/******************************************************************/
//************************* MIC+reverb  to DAC API *****************************//
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
#define ADC_BUF_NUM        	2
#define ADC_CH_NUM         	1
#define ADC_IRQ_POINTS     	256
#define ADC_BUFS_SIZE      	(ADC_BUF_NUM *ADC_CH_NUM* ADC_IRQ_POINTS)
#define PCM_BUF_LEN  		(1*1024+256)

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
#define PCM_RATE_MAX_STEP		100
#else
#define PCM_RATE_MAX_STEP		50
#endif
#define PCM_RATE_INC_STEP       5
#define PCM_RATE_DEC_STEP       5

enum {
    REVERB_STATUS_STOP = 0,
    REVERB_STATUS_START,
    REVERB_STATUS_PAUSE,
};

struct s_reverb_hdl {
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;

    s16 adc_buf[ADC_BUFS_SIZE];

    int mic_gain;
    u16 mic_sr;
    u16 src_out_sr;
    u16 src_out_sr_n;
    int begin_size;
    int top_size;
    int bottom_size;
    u16 audio_new_rate;
    u16 audio_max_speed;
    u16 audio_min_speed;
    u8 sync_start;
    struct audio_src_handle *src_sync;

    u8 pcm_buf[PCM_BUF_LEN * 2];
    cbuffer_t pcm_cbuf;

    u8  run_buf[REVERB_RUN_POINT_NUM * 2 * 2];
    u16 run_r;
    u16 run_len;

    /* u8 stop; */
    /* u8 dec_start; */
    /* u8 dec_pause; */

    u32 status : 2;
    u32 out_ch_num : 2;
    u32 source_ch_num : 2;
    u8 first_start;
    struct audio_decoder decoder;
    struct audio_res_wait wait;
    struct audio_mixer_ch mix_ch;

    REVERB_API_STRUCT *p_reverb_obj;
    u8 first_tone;
#if HOWLING_ENABLE
    HOWLING_API_STRUCT *p_howling_obj;
#endif

    u8 output_aec_en;
};


extern void audio_aec_inbuf(s16 *buf, u16 len);
static struct s_reverb_hdl *reverb_hdl = NULL;
static u8 pcm_dec_maigc = 0;

static void adc_output_to_buf(void *priv, s16 *data, int len)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    int wlen = cbuf_write(&reverb_hdl->pcm_cbuf, data, len);
    if (!wlen) {
        putchar('W');
    }
    if (reverb_hdl->output_aec_en) {
        audio_aec_inbuf(data, len);
    }
    audio_decoder_resume(&reverb_hdl->decoder);
}

static int reverb_src_output_handler(void *priv, void *buf, int len)
{
    int wlen = 0;
    int rlen = len;
    struct s_reverb_hdl *dec = (struct s_reverb_hdl *) priv;
    s16 *data = (s16 *)buf;
    /* return len;		 */
    do {
        wlen = audio_mixer_ch_write(&dec->mix_ch, data, rlen);
        if (!wlen) {
            break;
        }
        data += wlen / 2 ;
        rlen -= wlen;
    } while (rlen);
    /* printf("src,l:%d, wl:%d \n", len, len-rlen); */
    return (len - rlen);
}

void pcm_dec_relaese()
{
    audio_decoder_task_del_wait(&decode_task, &reverb_hdl->wait);
}
static void pcm_dec_close(void)
{
    audio_decoder_close(&reverb_hdl->decoder);
    if (reverb_hdl->src_sync) {
        audio_hw_resample_close(reverb_hdl->src_sync);
        reverb_hdl->src_sync = NULL;
    }
    audio_mixer_ch_close(&reverb_hdl->mix_ch);
}

void stop_reverb_mic2dac(void)
{
    if (!reverb_hdl) {
        return;
    }
    printf("\n--func=%s\n", __FUNCTION__);

    reverb_hdl->status = REVERB_STATUS_STOP;

    audio_adc_del_output_handler(&adc_hdl, &reverb_hdl->adc_output);
    audio_adc_mic_close(&reverb_hdl->mic_ch);

    pcm_dec_close();

    close_reverb(reverb_hdl->p_reverb_obj);

#if HOWLING_ENABLE
    close_howling(reverb_hdl->p_howling_obj);
#endif

    pcm_dec_relaese();

    free(reverb_hdl);
    reverb_hdl = NULL;
}

void restart_reverb_mic2dac()
{
    if (reverb_hdl) {
        return;
    }

}

static inline void audio_pcm_mono_to_dual(s16 *dual_pcm, s16 *mono_pcm, int points)
{
    s16 *mono = mono_pcm;
    int i = 0;
    u8 j = 0;

    for (i = 0; i < points; i++, mono++) {
        *dual_pcm++ = *mono;
        *dual_pcm++ = *mono;
    }
}

static int pcm_fread(struct audio_decoder *decoder, void *buf, u32 len)
{
    int rlen = 0;
    struct s_reverb_hdl *dec = container_of(decoder, struct s_reverb_hdl, decoder);
    // 固定输出单声道
    if (dec->source_ch_num == 2) {
        rlen = cbuf_read(&dec->pcm_cbuf, (void *)((int)buf + (len / 2)), len / 2);
        audio_pcm_mono_to_dual(buf, (void *)((int)buf + (len / 2)), rlen / 2);
        rlen <<= 1;
    } else {
        rlen = cbuf_read(&dec->pcm_cbuf, buf, len);
    }
    if (rlen == 0) {
        if (dec->first_start < 10) { //打开时mic未出数填0，避免挡住mixer混合，造成DAC没数据推
            dec->first_start++;
            memset(buf, 0, len);
            rlen = len;
            return rlen;
        }
        return -1;
    } else {
        dec->first_start = 100;
    }
    /* printf("fread len %d %d\n",len,rlen); */
    return rlen;
}
static const struct audio_dec_input pcm_input = {
    .coding_type = AUDIO_CODING_PCM,
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
#if VFS_ENABLE == 0
#undef fread
#undef fseek
#undef flen
#endif
            .fread = pcm_fread,
            /* .fseek = file_fseek, */
            /* .flen  = linein_flen, */
        }
    }
};

static int pcm_output(struct s_reverb_hdl *dec, s16 *data, u16 len)
{
    char err = 0;
    int wlen = 0;
    int rlen = len;
    /* return rlen;//for test */
    do {
        if (dec->src_sync) {
            if (dec->src_out_sr_n != dec->src_out_sr) {
                dec->src_out_sr = dec->src_out_sr_n;
                dec->audio_new_rate = dec->src_out_sr;
                dec->audio_max_speed = dec->audio_new_rate + PCM_RATE_MAX_STEP;
                dec->audio_min_speed = dec->audio_new_rate - PCM_RATE_MAX_STEP;
                audio_hw_src_set_rate(dec->src_sync, dec->mic_sr, dec->src_out_sr_n);
                printf(" set reverb src[%d] [%d] \n", dec->mic_sr, dec->src_out_sr_n);
            }
            wlen = audio_src_resample_write(dec->src_sync, data, rlen);
        } else {
            wlen = audio_mixer_ch_write(&dec->mix_ch, data, rlen);
        }
        if (!wlen) {
            /* putchar('p'); */
            err++;
            if (err < 2) {
                continue;
            }
            break;
        }
        err = 0;
        data += wlen / 2;
        rlen -= wlen;
    } while (rlen > 0);
    /* putchar('A'); */
    return len - rlen;
}

static int pcm_dec_output(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    struct s_reverb_hdl *dec = container_of(decoder, struct s_reverb_hdl, decoder);
    int wlen = 0;
    if (dec->run_len >= (REVERB_RUN_POINT_NUM * 2)) {
        wlen = pcm_output(dec, &dec->run_buf[dec->run_r], dec->run_len - dec->run_r);
        dec->run_r += wlen;
        /* printf("wl:%d, r:%d, \n", wlen, dec->run_r); */
        if (dec->run_r < dec->run_len) {
            // 没有输出完
            return 0;
        }
        // 输出完毕
        dec->run_len = 0;
    }

    wlen = len;
    if (wlen > (REVERB_RUN_POINT_NUM * 2) - dec->run_len) {
        wlen = (REVERB_RUN_POINT_NUM * 2) - dec->run_len;
    }
    // 保存在后半部中
    memcpy(&dec->run_buf[dec->run_len + (REVERB_RUN_POINT_NUM * 2)], data, wlen);
    dec->run_len += wlen;
    if (dec->run_len >= (REVERB_RUN_POINT_NUM * 2)) {
#if HOWLING_ENABLE
        if (dec->p_howling_obj) {
            howling_run(dec->p_howling_obj->ptr, &dec->run_buf[REVERB_RUN_POINT_NUM * 2], &dec->run_buf[REVERB_RUN_POINT_NUM * 2], REVERB_RUN_POINT_NUM);
        }
#endif
        if (dec->p_reverb_obj) {
            if (dec->first_tone >= 20) {
                dec->p_reverb_obj->func_api->run(dec->p_reverb_obj->ptr, &dec->run_buf[REVERB_RUN_POINT_NUM * 2], REVERB_RUN_POINT_NUM);
            } else {
                dec->first_tone++;
            }
        }
        dec->run_len <<= 1;
        if (dec->out_ch_num == 1) {
            // 单声道，播放后半部
            dec->run_r = (REVERB_RUN_POINT_NUM * 2);
        } else {
            // 双声道，扩充
            dec->run_r = 0;
            audio_pcm_mono_to_dual(dec->run_buf, &dec->run_buf[REVERB_RUN_POINT_NUM * 2], REVERB_RUN_POINT_NUM);
        }
    }
    /* printf("wl:%d, rl:%d \n", wlen, dec->run_len); */
    return wlen;
}
static int pcm_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    int wlen = 0;
    int rlen = len;
    do {
        wlen = pcm_dec_output(decoder, data, rlen, priv);
        if (!wlen) {
            break;
        }
        data += wlen / 2 ;
        rlen -= wlen;
    } while (rlen);
    /* printf("dec,l:%d, wl:%d \n", len, len-rlen); */
    return (len - rlen);
}

static int pcm_dec_stream_sync(struct s_reverb_hdl *dec, int data_size)
{
    if (!dec->src_sync) {
        return 0;
    }
    u16 sr = dec->audio_new_rate;

    if (data_size < dec->bottom_size) {
        dec->audio_new_rate += PCM_RATE_INC_STEP;
        /*printf("rate inc\n");*/
    }

    if (data_size > dec->top_size) {
        dec->audio_new_rate -= PCM_RATE_DEC_STEP;
        /*printf("rate dec : %d\n", __this->audio_new_rate);*/
    }

    if (dec->audio_new_rate < dec->audio_min_speed) {
        dec->audio_new_rate = dec->audio_min_speed;
    } else if (dec->audio_new_rate > dec->audio_max_speed) {
        dec->audio_new_rate = dec->audio_max_speed;
    }

    if (sr != dec->audio_new_rate) {
        /* printf(" set reverb sr[%d] [%d] \n",dec->mic_sr,dec->audio_new_rate); */
        audio_hw_src_set_rate(dec->src_sync, dec->mic_sr, dec->audio_new_rate);
    }
    return 0;
}

static int pcm_dec_probe_handler(struct audio_decoder *decoder)
{
    struct s_reverb_hdl *dec = container_of(decoder, struct s_reverb_hdl, decoder);

    if (!dec->sync_start) {
        if (cbuf_get_data_len(&dec->pcm_cbuf) > dec->begin_size) {
            dec->sync_start = 1;
            return 0;
        } else {
            audio_decoder_suspend(&dec->decoder, 0);
            return -EINVAL;
        }
    }

    pcm_dec_stream_sync(dec, cbuf_get_data_len(&dec->pcm_cbuf));
    return 0;
}

static const struct audio_dec_handler pcm_dec_handler = {
    .dec_probe = pcm_dec_probe_handler,
    .dec_output = pcm_dec_output_handler,
    /* .dec_post   = linein_dec_post_handler, */
};
static void pcm_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        if ((u8)argv[1] != (u8)(pcm_dec_maigc - 1)) {
            log_i("maigc err, %s\n", __FUNCTION__);
            break;
        }
        /* pcm_dec_close(); */
        break;
    }
}

static int pcm_dec_sync_init(struct s_reverb_hdl *dec)
{
    dec->sync_start = 0;
    dec->begin_size = dec->pcm_cbuf.total_len * 60 / 100;
    dec->top_size = dec->pcm_cbuf.total_len * 70 / 100;
    dec->bottom_size = dec->pcm_cbuf.total_len * 40 / 100;

    u16 out_sr = dec->src_out_sr;
    printf("out_sr:%d, dsr:%d, dch:%d \n", out_sr, dec->mic_sr, dec->out_ch_num);
    dec->audio_new_rate = out_sr;
    dec->audio_max_speed = out_sr + PCM_RATE_MAX_STEP;
    dec->audio_min_speed = out_sr - PCM_RATE_MAX_STEP;

    dec->src_sync = zalloc(sizeof(struct audio_src_handle));
    if (!dec->src_sync) {
        return -ENODEV;
    }
    audio_hw_src_open(dec->src_sync, dec->out_ch_num, SRC_TYPE_AUDIO_SYNC);

    audio_hw_src_set_rate(dec->src_sync, dec->mic_sr, dec->audio_new_rate);

    audio_src_set_output_handler(dec->src_sync, dec, reverb_src_output_handler);
    return 0;
}

static int pcm_dec_start(void)
{
    int err;
    struct audio_fmt f;
    struct s_reverb_hdl *dec = reverb_hdl;

    printf("\n--func=%s\n", __FUNCTION__);
    err = audio_decoder_open(&dec->decoder, &pcm_input, &decode_task);
    if (err) {
        goto __err1;
    }
    dec->out_ch_num = audio_output_channel_num();//AUDIO_CH_MAX;

    audio_decoder_set_handler(&dec->decoder, &pcm_dec_handler);
    audio_decoder_set_event_handler(&dec->decoder, pcm_dec_event_handler, pcm_dec_maigc++);


    f.coding_type = AUDIO_CODING_PCM;
    f.sample_rate = dec->mic_sr;
    f.channel = 1;//dec->channel;

    err = audio_decoder_set_fmt(&dec->decoder, &f);
    if (err) {
        goto __err2;
    }

    audio_mixer_ch_open(&dec->mix_ch, &mixer);
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, audio_output_rate(f.sample_rate));

    dec->src_out_sr = audio_output_rate(f.sample_rate);
    dec->src_out_sr_n = dec->src_out_sr;
    pcm_dec_sync_init(dec);

    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);

    /* audio_adc_mic_start(&dec->mic_ch); */
    printf("\n\n audio decoder start \n");
    audio_decoder_set_run_max(&dec->decoder, 20);
    err = audio_decoder_start(&dec->decoder);
    if (err) {
        goto __err3;
    }
    dec->status = REVERB_STATUS_START;
    printf("\n\n audio mic start  1 \n");
    return 0;
__err3:
    if (dec->src_sync) {
        audio_hw_resample_close(dec->src_sync);
        dec->src_sync = NULL;
    }
    audio_adc_mic_close(&dec->mic_ch);
__err2:
    audio_decoder_close(&dec->decoder);
__err1:
    audio_decoder_task_del_wait(&decode_task, &dec->wait);
    return err;
}



static int pcmdec_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;
    if (!reverb_hdl) {
        log_e("reverb_hdl err \n");
        return -1;
    }
    log_i("pcmdec_wait_res_handler, event:%d;status:%d; \n", event, reverb_hdl->status);
    /* printf("\n--func=%s\n", __FUNCTION__); */
    if (event == AUDIO_RES_GET) {
        if (reverb_hdl->status == REVERB_STATUS_STOP) {
            err = pcm_dec_start();
        } else if (reverb_hdl->status == REVERB_STATUS_PAUSE) {
            reverb_hdl->status = REVERB_STATUS_START;
        }
    } else if (event == AUDIO_RES_PUT) {
        if (reverb_hdl->status == REVERB_STATUS_START) {
            /* reverb_hdl->status = REVERB_STATUS_PAUSE; */
        }
    }

    return err;
}
static u8 reset_mark = 0;
void reverb_pause(void)
{
    printf("\n--func=%s\n", __FUNCTION__);
    if (reverb_hdl) {
        stop_reverb_mic2dac();
        reset_mark = 1;
    }
}
void reverb_resume(void)
{
    printf("\n--func=%s\n", __FUNCTION__);
    if (reset_mark) {
        if (!reverb_hdl) {
            start_reverb_mic2dac(NULL);
            /* os_time_dly(20);// */
        }
        reset_mark = 0;
    }

}
void reverb_restart(void)
{
    if (reverb_hdl->status) {
        pcmdec_wait_res_handler(NULL, 0);
    }
    /* err = audio_decoder_task_add_wait(&decode_task, &reverb->wait);	 */
}

void start_reverb_mic2dac(struct audio_fmt *fmt)
{
    struct s_reverb_hdl *reverb = NULL;
    int err;
    if (reverb_hdl) {
        stop_reverb_mic2dac();
    }
    reverb = zalloc(sizeof(struct s_reverb_hdl));
    printf("reverb hdl:%d", sizeof(struct s_reverb_hdl));
    ASSERT(reverb);

    struct audio_fmt f = {0};
    if (fmt) {
        f.sample_rate = fmt->sample_rate;
    }
    if (f.sample_rate == 0) {
        /* f.sample_rate = 8000; */
        f.sample_rate = 16000;
        /* f.sample_rate = 44100; */
        /* f.sample_rate = 32000; */
    }
    f.channel = 1;


    reverb->source_ch_num = f.channel;

    reverb->mic_sr = f.sample_rate;
    reverb->mic_gain = 6;
    reverb->p_reverb_obj = open_reverb(NULL, f.sample_rate);
#if HOWLING_ENABLE
    reverb->p_howling_obj = open_howling(NULL, f.sample_rate, 1);;
#endif
    audio_adc_mic_open(&reverb->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
    audio_adc_mic_set_sample_rate(&reverb->mic_ch, f.sample_rate);
    audio_adc_mic_set_gain(&reverb->mic_ch, reverb->mic_gain);
    audio_adc_mic_set_buffs(&reverb->mic_ch, reverb->adc_buf, ADC_IRQ_POINTS * 2, ADC_BUF_NUM);
    reverb->adc_output.handler = adc_output_to_buf;
    audio_adc_add_output_handler(&adc_hdl, &reverb->adc_output);

    cbuf_init(&reverb->pcm_cbuf, reverb->pcm_buf, sizeof(reverb->pcm_buf));

    audio_adc_mic_start(&reverb->mic_ch);

    reverb->wait.priority = 0;
    reverb->wait.preemption = 0;
    reverb->wait.protect = 1;
    reverb->wait.handler = pcmdec_wait_res_handler;
    /* app_audio_mute(AUDIO_MUTE_DEFAULT); */
    /* os_time_dly(10); */
    reverb_hdl = reverb;
    err = audio_decoder_task_add_wait(&decode_task, &reverb->wait);
    if (err == 0) {
        return ;
    }
    printf("audio decoder task add wait err \n");

    audio_adc_mic_close(&reverb->mic_ch);
    audio_adc_del_output_handler(&adc_hdl, &reverb->adc_output);
    close_reverb(reverb->p_reverb_obj);
#if HOWLING_ENABLE
    close_howling(reverb->p_howling_obj);
#endif
}

int reverb_if_working(void)
{
    if (reverb_hdl && (reverb_hdl->status == REVERB_STATUS_START)) {
        return 1;
    }
    return 0;
}
void reverb_en_mic2aec(u8 mark)
{
    if (reverb_hdl) {
        reverb_hdl->output_aec_en = mark ? 1 : 0;
    }
}


void set_mic_gain_up(u8 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    //o-31
    reverb_hdl->mic_gain += value;
    if (reverb_hdl->mic_gain > 31) {
        reverb_hdl->mic_gain = 31;
    }
    audio_adc_set_mic_gain(reverb_hdl->mic_ch.hdl, reverb_hdl->mic_gain);
}

void set_mic_gain_down(u8 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    //o-31
    reverb_hdl->mic_gain -= value;
    if (reverb_hdl->mic_gain < 0) {
        reverb_hdl->mic_gain = 0;
    }
    audio_adc_set_mic_gain(reverb_hdl->mic_ch.hdl, reverb_hdl->mic_gain);
}

void reset_reverb_src_out(u16 s_rate)
{
    if (reverb_hdl && reverb_hdl->src_sync) {
        if (reverb_hdl->src_out_sr_n != s_rate) {
            printf("reset reverb srcout[%d]", s_rate);
            reverb_hdl->src_out_sr_n = s_rate;
        }
    }
}

void set_reverb_deepval_up(u16 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    if (reverb_hdl->p_reverb_obj) {
        reverb_hdl->p_reverb_obj->parm.deepval += value;
        if (reverb_hdl->p_reverb_obj->parm.deepval > 4096) {
            reverb_hdl->p_reverb_obj->parm.deepval = 4096;
        }
        printf("deepval:%d", reverb_hdl->p_reverb_obj->parm.deepval);
        reverb_hdl->p_reverb_obj->func_api->init(reverb_hdl->p_reverb_obj->ptr, &reverb_hdl->p_reverb_obj->parm);
    }
}

void set_reverb_deepval_down(u16 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    if (reverb_hdl->p_reverb_obj) {
        if (reverb_hdl->p_reverb_obj->parm.deepval > value) {
            reverb_hdl->p_reverb_obj->parm.deepval -= value;
        } else {
            reverb_hdl->p_reverb_obj->parm.deepval = 0;
        }

        printf("deepval:%d", reverb_hdl->p_reverb_obj->parm.deepval);
        reverb_hdl->p_reverb_obj->func_api->init(reverb_hdl->p_reverb_obj->ptr, &reverb_hdl->p_reverb_obj->parm);
    }
}

void set_reverb_decayval_up(u16 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    if (reverb_hdl->p_reverb_obj) {
        reverb_hdl->p_reverb_obj->parm.decayval += value;
        if (reverb_hdl->p_reverb_obj->parm.decayval > 4096) {
            reverb_hdl->p_reverb_obj->parm.decayval = 4096;
        }
        printf("decayval:%d", reverb_hdl->p_reverb_obj->parm.deepval);
        reverb_hdl->p_reverb_obj->func_api->init(reverb_hdl->p_reverb_obj->ptr, &reverb_hdl->p_reverb_obj->parm);
    }
}

void set_reverb_decayval_down(u16 value)
{
    if ((!reverb_hdl) || (reverb_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    if (reverb_hdl->p_reverb_obj) {
        if (reverb_hdl->p_reverb_obj->parm.decayval > value) {
            reverb_hdl->p_reverb_obj->parm.decayval -= value;
        } else {
            reverb_hdl->p_reverb_obj->parm.decayval = 0;
        }

        printf("decayval:%d", reverb_hdl->p_reverb_obj->parm.decayval);
        reverb_hdl->p_reverb_obj->func_api->init(reverb_hdl->p_reverb_obj->ptr, &reverb_hdl->p_reverb_obj->parm);
    }
}

__attribute__((weak))void reverb_eq_cal_coef(u8 filtN, u32 gainN, u8 sw)
{
}


#endif

