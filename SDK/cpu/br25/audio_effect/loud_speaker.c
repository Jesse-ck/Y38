#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_online_cfg.h"
#include "audio_drc.h"
#include "audio_reverb.h"
#include "clock_cfg.h"
#include "audio_config.h"
#include "storage_dev/storage_dev.h"
#include "loud_speaker.h"
#define LOG_TAG     "[APP-SPEAKER]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"

#define REVERB_RUN_POINT_NUM		160	//固定160个点
#define REVERB_ENABLE    			0 //混响
#define HOWLING_ENABLE    			1 //啸叫抑制


#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))


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


//************************* MIC to DAC API *****************************//
#define ADC_BUF_NUM        	2
#define ADC_CH_NUM         	1
#define ADC_IRQ_POINTS     	32//256
#define ADC_BUFS_SIZE      	(ADC_BUF_NUM *ADC_CH_NUM* ADC_IRQ_POINTS)
#define PCM_BUF_LEN  		(1024)

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
#define PCM_RATE_MAX_STEP		100
#else
#define PCM_RATE_MAX_STEP		100
#endif
#define PCM_RATE_INC_STEP       10
#define PCM_RATE_DEC_STEP       10

enum {
    REVERB_STATUS_STOP = 0,
    REVERB_STATUS_START,
    REVERB_STATUS_PAUSE,
};

struct s_speaker_hdl {
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

    u8 pcm_buf[PCM_BUF_LEN ];
    cbuffer_t pcm_cbuf;

    u8  run_buf[REVERB_RUN_POINT_NUM * 2 * 2];
    u16 run_r;
    u16 run_len;


    u32 status : 2;
    u32 out_ch_num : 2;
    u32 source_ch_num : 2;
    u32 reverb_en : 2;
    u32 howling_en : 2;
    u8 first_start;
	u8 speaker_pause;
    struct audio_decoder decoder;
    struct audio_res_wait wait;
    struct audio_mixer_ch mix_ch;

#if REVERB_ENABLE
    REVERB_API_STRUCT *p_reverb_obj;
#endif
#if HOWLING_ENABLE
    HOWLING_API_STRUCT *p_howling_obj;
#endif
};


static struct s_speaker_hdl *speaker_hdl = NULL;
static u8 pcm_dec_maigc = 0;

static void adc_output_to_buf(void *priv, s16 *data, int len)
{
    if ((!speaker_hdl) || (speaker_hdl->status != REVERB_STATUS_START)) {
        return;
    }
	if(speaker_hdl->speaker_pause)
        return;
    int wlen = cbuf_write(&speaker_hdl->pcm_cbuf, data, len);
    if (!wlen) {
        putchar('W');
    }
    audio_decoder_resume(&speaker_hdl->decoder);
}

static int speaker_src_output_handler(void *priv, void *buf, int len)
{
    int wlen = 0;
    int rlen = len;
    struct s_speaker_hdl *dec = (struct s_speaker_hdl *) priv;
    s16 *data = (s16 *)buf;
    /* return len;		 */
    return audio_mixer_ch_write(&dec->mix_ch, data, rlen);
}

static void pcm_dec_relaese()
{
    audio_decoder_task_del_wait(&decode_task, &speaker_hdl->wait);
}

static void pcm_dec_close(void)
{
    audio_decoder_close(&speaker_hdl->decoder);
    if (speaker_hdl->src_sync) {
        audio_hw_resample_close(speaker_hdl->src_sync);
        speaker_hdl->src_sync = NULL;
    }
    audio_mixer_ch_close(&speaker_hdl->mix_ch);
}

void stop_loud_speaker(void)
{
    if (!speaker_hdl) {
        return;
    }
    printf("\n--func=%s\n", __FUNCTION__);

    speaker_hdl->status = REVERB_STATUS_STOP;

    audio_adc_del_output_handler(&adc_hdl, &speaker_hdl->adc_output);
    audio_adc_mic_close(&speaker_hdl->mic_ch);

    pcm_dec_close();

#if REVERB_ENABLE
    close_reverb(speaker_hdl->p_reverb_obj);
#endif

#if HOWLING_ENABLE
    close_howling(speaker_hdl->p_howling_obj);
#endif

    pcm_dec_relaese();

    free(speaker_hdl);
    speaker_hdl = NULL;
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
    struct s_speaker_hdl *dec = container_of(decoder, struct s_speaker_hdl, decoder);
    // 固定输出单声道
    if (dec->source_ch_num == 2) {
        rlen = cbuf_read(&dec->pcm_cbuf, (void *)((int)buf + (len / 2)), len / 2);
        audio_pcm_mono_to_dual(buf, (void *)((int)buf + (len / 2)), rlen / 2);
        rlen <<= 1;
    } else {
		if(len > REVERB_RUN_POINT_NUM*2)
        	rlen = cbuf_read(&dec->pcm_cbuf, buf, REVERB_RUN_POINT_NUM*2);
		else
        	rlen = cbuf_read(&dec->pcm_cbuf, buf, len);

    }
    if (rlen == 0) {
        return -1;
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
            /* .fseek = pcm_fseek, */
            /* .flen  = pcm_flen, */
        }
    }
};

static int pcm_output(struct s_speaker_hdl *dec, s16 *data, u16 len)
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
 
    }while (0);
    return wlen;
}

static int pcm_dec_output(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    struct s_speaker_hdl *dec = container_of(decoder, struct s_speaker_hdl, decoder);
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
			if(dec->howling_en){
				howling_run(dec->p_howling_obj->ptr, &dec->run_buf[REVERB_RUN_POINT_NUM * 2], &dec->run_buf[REVERB_RUN_POINT_NUM * 2], REVERB_RUN_POINT_NUM);
			}
		}
#endif

#if REVERB_ENABLE
		if (dec->p_reverb_obj) {
			if(dec->reverb_en)
				dec->p_reverb_obj->func_api->run(dec->p_reverb_obj->ptr, &dec->run_buf[REVERB_RUN_POINT_NUM * 2], REVERB_RUN_POINT_NUM);
		}
#endif
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
 
    return pcm_dec_output(decoder, data, len, priv);
}

static int pcm_dec_stream_sync(struct s_speaker_hdl *dec, int data_size)
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
    struct s_speaker_hdl *dec = container_of(decoder, struct s_speaker_hdl, decoder);

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

static int pcm_dec_sync_init(struct s_speaker_hdl *dec)
{
    dec->sync_start = 0;
    dec->begin_size = dec->pcm_cbuf.total_len * 40 / 100;
    dec->top_size = dec->pcm_cbuf.total_len * 80 / 100;
    dec->bottom_size = dec->pcm_cbuf.total_len * 30 / 100;

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

    audio_src_set_output_handler(dec->src_sync, dec, speaker_src_output_handler);
    return 0;
}

static int pcm_dec_start(void)
{
    int err;
    struct audio_fmt f;
    struct s_speaker_hdl *dec = speaker_hdl;

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
    if (!speaker_hdl) {
        log_e("speaker_hdl err \n");
        return -1;
    }
    log_i("pcmdec_wait_res_handler, event:%d;status:%d; \n", event, speaker_hdl->status);
    /* printf("\n--func=%s\n", __FUNCTION__); */
    if (event == AUDIO_RES_GET) {
        if (speaker_hdl->status == REVERB_STATUS_STOP) {
            err = pcm_dec_start();
        } else if (speaker_hdl->status == REVERB_STATUS_PAUSE) {
            speaker_hdl->status = REVERB_STATUS_START;
        }
    } else if (event == AUDIO_RES_PUT) {
        if (speaker_hdl->status == REVERB_STATUS_START) {
            /* reverb_hdl->status = REVERB_STATUS_PAUSE; */
        }
    }

    return err;
}

void start_loud_speaker(struct audio_fmt *fmt)
{
    struct s_speaker_hdl *reverb = NULL;
    int err;
    if (speaker_hdl) {
        stop_loud_speaker();
    }
    reverb = zalloc(sizeof(struct s_speaker_hdl));
    printf("reverb hdl:%d", sizeof(struct s_speaker_hdl));
    ASSERT(reverb);

    struct audio_fmt f = {0};
    if (fmt) {
        f.sample_rate = fmt->sample_rate;
    }
    if (f.sample_rate == 0) {
        f.sample_rate = 16000;
        /* f.sample_rate = 44100; */
    }
    f.channel = 1;


    reverb->source_ch_num = f.channel;
    reverb->mic_sr = f.sample_rate;
    reverb->mic_gain = 0;

#if REVERB_ENABLE
    reverb->p_reverb_obj = open_reverb(NULL, f.sample_rate);
#endif
#if HOWLING_ENABLE
    reverb->p_howling_obj = open_howling(NULL, f.sample_rate, 1);
	reverb->howling_en =1;
#endif

    audio_adc_mic_open(&reverb->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
    audio_adc_mic_set_sample_rate(&reverb->mic_ch, f.sample_rate);
    audio_adc_mic_set_gain(&reverb->mic_ch, reverb->mic_gain);
    audio_adc_mic_set_buffs(&reverb->mic_ch, reverb->adc_buf, ADC_IRQ_POINTS * 2, ADC_BUF_NUM);
    reverb->adc_output.handler = adc_output_to_buf;
    audio_adc_add_output_handler(&adc_hdl, &reverb->adc_output);

    cbuf_init(&reverb->pcm_cbuf, reverb->pcm_buf, sizeof(reverb->pcm_buf));
	/* audio_mic_0dB_en(1); */
	SFR(JL_ANA->ADA_CON1,16,1,1);//mic 前置增益0db
    audio_adc_mic_start(&reverb->mic_ch);

    reverb->wait.priority = 0;
    reverb->wait.preemption = 0;
    reverb->wait.protect = 1;
    reverb->wait.handler = pcmdec_wait_res_handler;
    /* app_audio_mute(AUDIO_MUTE_DEFAULT); */
    /* os_time_dly(10); */
    speaker_hdl = reverb;
    err = audio_decoder_task_add_wait(&decode_task, &reverb->wait);
    if (err == 0) {
        return ;
    }
    printf("audio decoder task add wait err \n");

    audio_adc_mic_close(&reverb->mic_ch);
    audio_adc_del_output_handler(&adc_hdl, &reverb->adc_output);
	
#if REVERB_ENABLE
    close_reverb(reverb->p_reverb_obj);
#endif
#if HOWLING_ENABLE
    close_howling(reverb->p_howling_obj);
#endif
}

int speaker_if_working(void)
{
    if (speaker_hdl && (speaker_hdl->status == REVERB_STATUS_START)) {
        return 1;
    }
    return 0;
}


void set_speaker_gain_up(u8 value)
{
    if ((!speaker_hdl) || (speaker_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    //o-31
    speaker_hdl->mic_gain += value;
    if (speaker_hdl->mic_gain > 14) {
        speaker_hdl->mic_gain = 14;
    }
    audio_adc_set_mic_gain(speaker_hdl->mic_ch.hdl, speaker_hdl->mic_gain);
	printf("mic gain up [%d]\n",speaker_hdl->mic_gain);	
	/* printf("\n--func=%s\n", __FUNCTION__); */
}

void set_speaker_gain_down(u8 value)
{
    if ((!speaker_hdl) || (speaker_hdl->status != REVERB_STATUS_START)) {
        return;
    }
    //o-31
    /* speaker_hdl->mic_gain -= value; */
    if (speaker_hdl->mic_gain >= value) {
        /* speaker_hdl->mic_gain = 0; */
    	speaker_hdl->mic_gain -= value;
    }
    /* audio_adc_mic_set_gain(&speaker_hdl->mic_ch, speaker_hdl->mic_gain); */
    audio_adc_set_mic_gain(speaker_hdl->mic_ch.hdl, speaker_hdl->mic_gain);
	/* printf("\n--func=%s\n", __FUNCTION__); */
	printf("mic gain [%d]\n",speaker_hdl->mic_gain);	
}

void reset_speaker_src_out(u16 s_rate)
{
    if (speaker_hdl && speaker_hdl->src_sync) {
        if (speaker_hdl->src_out_sr_n != s_rate) {
            printf("reset reverb srcout[%d]", s_rate);
            speaker_hdl->src_out_sr_n = s_rate;
        }
    }
}

void switch_holwing_en(void)
{
	if(speaker_hdl){		
		speaker_hdl->howling_en ^= 1;		
	printf("howling_en [%d]",speaker_hdl->howling_en);
	}
}

void switch_echo_en(void)
{
	if(speaker_hdl){		
		speaker_hdl->reverb_en ^= 1;		
	printf("reverb_en [%d]",speaker_hdl->reverb_en);
	}
	
}

void loud_speaker_pause(void)
{	
	if(speaker_hdl){		
		speaker_hdl->speaker_pause= 1;	
		audio_mixer_ch_pause(&speaker_hdl->mix_ch,1);
		printf("speaker_pause [%d]",speaker_hdl->speaker_pause);
	}	
}
void loud_speaker_resume(void)
{	
	if(speaker_hdl){		
		audio_mixer_ch_pause(&speaker_hdl->mix_ch,0);
		speaker_hdl->speaker_pause= 0;		
		printf("speaker_pause [%d]",speaker_hdl->speaker_pause);
	}	
}

#endif /*TCFG_LOUDSPEAKER_ENABLE*/

