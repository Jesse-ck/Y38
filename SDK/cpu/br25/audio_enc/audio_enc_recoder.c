
#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "asm/audio_src.h"
#include "audio_enc.h"
#include "app_main.h"
#include "app_action.h"
#include "encode/encode_write.h"
#include "audio_reverb.h"
#include "clock_cfg.h"
#include "audio_pitch.h"
#include "btstack/avctp_user.h"
#define LADC_MIC_BUF_NUM        2
#define LADC_MIC_CH_NUM         1
#define LADC_MIC_IRQ_POINTS     256
#define LADC_MIC_BUFS_SIZE      (LADC_MIC_CH_NUM * LADC_MIC_BUF_NUM * LADC_MIC_IRQ_POINTS)


struct ladc_mic_demo {
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
    s16 adc_buf[LADC_MIC_BUFS_SIZE];    //align 4Bytes
    int cut_timer;
};
struct ladc_mic_demo *ladc_mic = NULL;

static REVERB_API_STRUCT *p_reverb_obj = NULL;

extern struct audio_adc_hdl adc_hdl;

static void adc_mic_demo_output(void *priv, s16 *data, int len)
{
    struct audio_adc_hdl *hdl = priv;
    //putchar('o');
    //printf("adc:%x,len:%d",data,len);
    int wlen = app_audio_output_write(data, len * hdl->channel);
    if (wlen != len) {
        //printf("wlen:%d-%d",wlen,len);
    }
}

static u8 mic_demo_idle_query()
{
    return (ladc_mic ? 0 : 1);
}
REGISTER_LP_TARGET(mic_demo_lp_target) = {
    .name = "mic_demo",
    .is_idle = mic_demo_idle_query,
};

void audio_adc_mic_demo(u16 sr)
{
    r_printf("audio_adc_mic_open:%d\n", sr);
    ladc_mic = zalloc(sizeof(struct ladc_mic_demo));
    if (ladc_mic) {
        audio_adc_mic_open(&ladc_mic->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
        audio_adc_mic_set_sample_rate(&ladc_mic->mic_ch, sr);
        audio_adc_mic_set_gain(&ladc_mic->mic_ch, 20);
        audio_adc_mic_set_buffs(&ladc_mic->mic_ch, ladc_mic->adc_buf, LADC_MIC_IRQ_POINTS * 2, LADC_MIC_BUF_NUM);
        ladc_mic->adc_output.handler = adc_mic_demo_output;
        audio_adc_add_output_handler(&adc_hdl, &ladc_mic->adc_output);
        audio_adc_mic_start(&ladc_mic->mic_ch);

        app_audio_output_samplerate_set(sr);
        app_audio_output_start();
    }
}

int audio_adc_mic_init(u16 sr)
{
    //printf("ladc_mic_open:%d\n",sr);
    ASSERT(ladc_mic == NULL);
    ladc_mic = zalloc(sizeof(struct ladc_mic_demo));
    if (ladc_mic) {
        audio_adc_mic_open(&ladc_mic->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
        audio_adc_mic_set_sample_rate(&ladc_mic->mic_ch, sr);
        audio_adc_mic_set_gain(&ladc_mic->mic_ch, 5);
        audio_adc_mic_set_buffs(&ladc_mic->mic_ch, ladc_mic->adc_buf, LADC_MIC_IRQ_POINTS * 2, LADC_MIC_BUF_NUM);
        audio_adc_mic_start(&ladc_mic->mic_ch);
        return 0;
    } else {
        return -1;
    }
}

void audio_adc_mic_exit(void)
{
    //printf("ladc_mic_close\n");
    if (ladc_mic) {
        audio_adc_mic_close(&ladc_mic->mic_ch);
        free(ladc_mic);
        ladc_mic = NULL;
    }
}

/******************************************************/
#define LADC_LINEIN_BUF_NUM        2
#define LADC_LINEIN_CH_NUM         1
#define LADC_LINEIN_IRQ_POINTS     256
#define LADC_LINEIN_BUFS_SIZE      (LADC_LINEIN_CH_NUM * LADC_LINEIN_BUF_NUM * LADC_LINEIN_IRQ_POINTS)
struct audio_adc_var {
    struct audio_adc_output_hdl adc_output;
    struct audio_adc_ch ch;
    s16 adc_buf[LADC_LINEIN_BUFS_SIZE];    //align 4Bytes
};
struct audio_adc_var *ladc_linein = NULL;

void audio_adc_linein_demo(void)
{
    u16 ladc_linein_sr = 44100;
    r_printf("audio_adc_linein_demo...");
    ladc_linein = zalloc(sizeof(*ladc_linein));
    if (ladc_linein) {
        audio_adc_linein_open(&ladc_linein->ch, AUDIO_ADC_LINE0_L, &adc_hdl);
        audio_adc_linein_set_sample_rate(&ladc_linein->ch, ladc_linein_sr);
        audio_adc_linein_set_gain(&ladc_linein->ch, 5);
        printf("adc_buf_size:%d", sizeof(ladc_linein->adc_buf));
        audio_adc_set_buffs(&ladc_linein->ch, ladc_linein->adc_buf, LADC_LINEIN_CH_NUM * LADC_LINEIN_IRQ_POINTS * 2, LADC_LINEIN_BUF_NUM);
        ladc_linein->adc_output.handler = adc_mic_demo_output;
        ladc_linein->adc_output.priv = &adc_hdl;
        audio_adc_add_output_handler(&adc_hdl, &ladc_linein->adc_output);
        audio_adc_linein_start(&ladc_linein->ch);

        app_audio_output_samplerate_set(ladc_linein_sr);
        app_audio_output_start();
    }
}

#define ADC_BUF_NUM        2
#define ADC_CH_NUM         1
#define ADC_IRQ_POINTS     256
#define ADC_BUFS_SIZE      (ADC_BUF_NUM *ADC_CH_NUM* ADC_IRQ_POINTS)

#define ADC_STORE_PCM_SIZE	(ADC_BUFS_SIZE * 14)

struct linein_sample_hdl {
    OS_SEM sem;
    struct audio_adc_ch linein_ch;
    struct audio_adc_output_hdl sample_output;
    s16 adc_buf[ADC_BUFS_SIZE];
    /* s16 *store_pcm_buf[ADC_STORE_PCM_SIZE]; */
    s16 *store_pcm_buf;
    cbuffer_t cbuf;
    void (*resume)(void);
    u8 channel_num;
};

/* struct linein_sample_hdl g_linein_sample_hdl sec(.linein_pcm_mem); */
/* static s16 linein_store_pcm_buf[ADC_STORE_PCM_SIZE] sec(.linein_pcm_mem); */

void linein_sample_set_resume_handler(void *priv, void (*resume)(void))
{
    struct linein_sample_hdl *linein = (struct linein_sample_hdl *)priv;

    if (linein) {
        linein->resume = resume;
    }
}

AT(.fm_code)
void fm_inside_output_handler(void *priv, s16 *data, int len)
{
    struct linein_sample_hdl *linein = (struct linein_sample_hdl *)priv;

    int wlen = cbuf_write(&linein->cbuf, data, len);
    os_sem_post(&linein->sem);
    if (wlen != len) {
        putchar('W');
    }
    if (linein->resume) {
        linein->resume();
    }
}

static void linein_sample_output_handler(void *priv, s16 *data, int len)
{
    struct linein_sample_hdl *linein = (struct linein_sample_hdl *)priv;

    int wlen = cbuf_write(&linein->cbuf, data, len * linein->channel_num);
    os_sem_post(&linein->sem);
    if (wlen != len * linein->channel_num) {
        putchar('W');
    }
    if (linein->resume) {
        linein->resume();
    }
}

int linein_sample_read(void *hdl, void *data, int len)
{
    struct linein_sample_hdl *linein = (struct linein_sample_hdl *)hdl;
#if 0
// no wait
    u8 count = 0;
__try:
        if (cbuf_get_data_size(&linein->cbuf) < len) {
            local_irq_disable();
            os_sem_set(&linein->sem, 0);
            local_irq_enable();
            os_sem_pend(&linein->sem, 2);
            if (cbuf_get_data_size(&linein->cbuf) < len) {
                if (++count > 4) {
                    return 0;
                }
                goto __try;
            }

        }
#endif
    return cbuf_read(&linein->cbuf, data, len);
}

int linein_sample_size(void *hdl)
{
    struct linein_sample_hdl *linein = (struct linein_sample_hdl *)hdl;
    return cbuf_get_data_size(&linein->cbuf);
}

int linein_sample_total(void *hdl)
{
    struct linein_sample_hdl *linein = (struct linein_sample_hdl *)hdl;
    return linein->cbuf.total_len;
}

void *linein_sample_open(u8 source, u16 sample_rate)
{
    struct linein_sample_hdl *linein = NULL;

    /* linein = &g_linein_sample_hdl;// zalloc(sizeof(struct linein_sample_hdl)); */
    linein =  zalloc(sizeof(struct linein_sample_hdl));
    if (!linein) {
        return NULL;
    }

    memset(linein, 0x0, sizeof(struct linein_sample_hdl));

    linein->store_pcm_buf = malloc(ADC_STORE_PCM_SIZE);
    if (!linein->store_pcm_buf) {
        return NULL;
    }

    if (source != 0xff) {
        audio_adc_linein_open(&linein->linein_ch, (source << 2), &adc_hdl);
    }

    cbuf_init(&linein->cbuf, linein->store_pcm_buf, ADC_STORE_PCM_SIZE);
    os_sem_create(&linein->sem, 0);
    if (source == 0xff) {
        return linein;
    }
    linein->channel_num = adc_hdl.channel;
    audio_adc_linein_set_sample_rate(&linein->linein_ch, sample_rate);
    audio_adc_linein_set_gain(&linein->linein_ch, 3);

    audio_adc_set_buffs(&linein->linein_ch, linein->adc_buf, ADC_CH_NUM * ADC_IRQ_POINTS * 2, ADC_BUF_NUM);

    linein->sample_output.handler = linein_sample_output_handler;
    linein->sample_output.priv = linein;

    audio_adc_add_output_handler(&adc_hdl, &linein->sample_output);

    audio_adc_linein_start(&linein->linein_ch);

    return linein;
}

void linein_sample_close(void *hdl)
{
    struct linein_sample_hdl *linein = (struct linein_sample_hdl *)hdl;

    if (!linein) {
        return;
    }
    audio_adc_linein_close(&linein->linein_ch);
    audio_adc_del_output_handler(&adc_hdl, &linein->sample_output);

    free(linein->store_pcm_buf);
    free(linein);
}


void *fm_sample_open(u8 source, u16 sample_rate)
{

    struct linein_sample_hdl *linein = NULL;
    /* linein = &g_linein_sample_hdl;// zalloc(sizeof(struct linein_sample_hdl)); */
    linein =  zalloc(sizeof(struct linein_sample_hdl));
    if (!linein) {
        return NULL;
    }
    memset(linein, 0x0, sizeof(struct linein_sample_hdl));

    linein->store_pcm_buf = zalloc(ADC_STORE_PCM_SIZE);
    if (!linein->store_pcm_buf) {
        return NULL;
    }

    cbuf_init(&linein->cbuf, linein->store_pcm_buf, ADC_STORE_PCM_SIZE);
    os_sem_create(&linein->sem, 0);


    if (source == 1) { //line1
        audio_adc_linein_open(&linein->linein_ch, AUDIO_ADC_LINE1_LR, &adc_hdl);
    } else if (source == 0) { //linein0
        audio_adc_linein_open(&linein->linein_ch, AUDIO_ADC_LINE0_LR, &adc_hdl);
    } else if (source == 2) {
        audio_adc_linein_open(&linein->linein_ch, AUDIO_ADC_LINE2_LR, &adc_hdl);
    } else {
        return linein;
    }

    audio_adc_linein_set_sample_rate(&linein->linein_ch, sample_rate);
    audio_adc_linein_set_gain(&linein->linein_ch, 1);

    audio_adc_set_buffs(&linein->linein_ch, linein->adc_buf, ADC_CH_NUM * ADC_IRQ_POINTS * 2, ADC_BUF_NUM);

    linein->sample_output.handler = linein_sample_output_handler;
    linein->sample_output.priv = linein;

    audio_adc_add_output_handler(&adc_hdl, &linein->sample_output);
    audio_adc_linein_start(&linein->linein_ch);
    return linein;
}

void fm_sample_close(void *hdl, u8 source)
{
    struct linein_sample_hdl *linein = (struct linein_sample_hdl *)hdl;

    if (!linein) {
        return;
    }

    if (source <= 2) {
        audio_adc_linein_close(&linein->linein_ch);
        audio_adc_del_output_handler(&adc_hdl, &linein->sample_output);
    }

    free(linein->store_pcm_buf);
    free(linein);
}


////>>>>>>>>>>>>>>record_player api录音接口<<<<<<<<<<<<<<<<<<<<<///
#if 1

/* #define RECORD_PLAYER_DEFULT_SAMPLERATE	(44100L) */
#define RECORD_PLAYER_DEFULT_SAMPLERATE	(16000L)
#define RECORD_PLAYER_DEFULT_BITRATE	(128L)
#define RECORD_PLAYER_DEFULT_ADPCM_BLOCKSIZE (1024L) //256/512/1024/2048
#define RECORD_PLAYER_MIXER_CHANNELS		 1     //mixer record single channel


struct record_hdl {
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch    mic_ch;
    struct audio_adc_ch linein_ch;
    struct storage_dev *dev;
    void *file;
    /* s16 adc_buf[ADC_BUFS_SIZE]; */
	s16* adc_buf; 
    u32 magic;
    u32 start_sys_tick;
    enum enc_source source;
    u16	sample_rate;
    u16 cut_head_timer;
    u8  cut_head_flag;
    u8  stop_req;
    u8  nchl;
    u8  coding_type;

#if (defined(TCFG_MIC_REC_PITCH_EN) && (TCFG_MIC_REC_PITCH_EN))
    s_pitch_hdl *pitch_hdl;
#endif
};

extern u32 timer_get_ms(void);
extern u32 audio_output_channel_num(void);

extern struct audio_mixer mixer;

static struct record_hdl *rec_hdl = NULL;

static void dual_to_single(void *out, void *in, u16 len)
{
    s16 *outbuf = out;
    s16 *inbuf = in;
    s32 tmp = 0;
    len >>= 2;
    while (len--) {
        tmp = inbuf[0]/2 + inbuf[1]/2;
        if (tmp > 32767) {
            tmp = 32767;
        } else if (tmp < -32768) {
            tmp = -32768;
        }

        /* *outbuf++ = (inbuf[0] + inbuf[1]) >> 1; */
        *outbuf++ = tmp;
        inbuf += 2;
    }
}

void record_player_pcm2file_write_mix_pcm(s16 *data, int len)
{
	if (rec_hdl) {
		if (false == pcm2file_enc_is_work(rec_hdl->file)) {
			return ;
		}
		pcm2file_enc_write_mix_pcm(rec_hdl->file, data, len);
	}

}

void record_player_pcm2file_write_pcm_ex(s16 *data, int len)
{
    if (rec_hdl) {
        if (false == pcm2file_enc_is_work(rec_hdl->file)) {
            return ;
        }

        u16 cur_sr = audio_mixer_get_sample_rate(&mixer);
        if (rec_hdl->sample_rate != cur_sr) {
            ///采样率不一样， 应该停止录音，发消息停止mixer录音
            if (rec_hdl->stop_req == 0) {
                rec_hdl->stop_req = 1;
                app_task_msg_post(USER_MSG_SYS_MIXER_RECORD_STOP, 0, 0);
            }
        } else {
            int wlen = len;
            u32 chls = audio_output_channel_num();
            if (chls > 2) {
                if (rec_hdl->stop_req == 0) {
                    rec_hdl->stop_req = 1;
                    app_task_msg_post(USER_MSG_SYS_MIXER_RECORD_STOP, 0, 0);
                }
                return;
            } else if (chls == 2) {
                if (rec_hdl->nchl == 1) {
                    dual_to_single(data, data, len);
                    wlen = len >> 1;
                }
            }
            pcm2file_enc_write_pcm(rec_hdl->file, data, wlen);
        }
    }
}


void record_player_encode_stop(void)
{
    if (rec_hdl) {
        switch (rec_hdl->source) {
        case ENCODE_SOURCE_MIX:
            break;
        case ENCODE_SOURCE_MIC:
            audio_adc_mic_close(&rec_hdl->mic_ch);
            audio_adc_del_output_handler(&adc_hdl, &rec_hdl->adc_output);
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE)&&(TCFG_MIC_REC_REVERB_EN))
            close_reverb(p_reverb_obj);
#endif
#if (defined(TCFG_MIC_REC_PITCH_EN) && (TCFG_MIC_REC_PITCH_EN))
            close_pitch(rec_hdl->pitch_hdl);
#endif
            break;
        case ENCODE_SOURCE_LINE0_LR:
        case ENCODE_SOURCE_LINE1_LR:
        case ENCODE_SOURCE_LINE2_LR:
            audio_adc_linein_close(&rec_hdl->linein_ch);
            audio_adc_del_output_handler(&adc_hdl, &rec_hdl->adc_output);
            break;
        default:
            break;
        }
        rec_hdl->magic++;
        pcm2file_enc_close(rec_hdl->file);

        if (rec_hdl->coding_type == AUDIO_CODING_WAV) {
            clock_remove(ENC_WAV_CLK);
        } else if (rec_hdl->coding_type == AUDIO_CODING_G726) {
            clock_remove(ENC_G726_CLK);
        } else if (rec_hdl->coding_type == AUDIO_CODING_MP3) {
            clock_remove(ENC_MP3_CLK);
        }

        rec_hdl->file = NULL;
        sys_timeout_del(rec_hdl->cut_head_timer);
		if(rec_hdl->adc_buf)
			free(rec_hdl->adc_buf);
        free(rec_hdl);
        rec_hdl = NULL;
        clock_set_cur();
    }
}

static void record_player_encode_event_handler(struct audio_encoder *hdl, int argc, int *argv)
{
    if (rec_hdl == NULL) {
        return ;
    }

    struct audio_encoder *enc = get_pcm2file_encoder_hdl(rec_hdl->file);
    printf("%s, argv[]:%d, %d , hdl = %x, enc = %x", __FUNCTION__,  argv[0], argv[1], hdl, enc);
    if ((hdl != NULL) && (hdl == enc)) {
        record_player_encode_stop();
    } else {
        printf("err enc handle !!\n");
    }
}



static void adc_output_to_enc(void *priv, s16 *data, int len)
{
    /* putchar('o'); */
    if (rec_hdl == NULL) {
        return ;
    }

    if (rec_hdl->cut_head_flag) {
        return ;
    }

    switch (rec_hdl->source) {
    case ENCODE_SOURCE_MIC:

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE)&&(TCFG_MIC_REC_REVERB_EN))
        if (p_reverb_obj) {
            p_reverb_obj->func_api->run(p_reverb_obj->ptr, data, len / 2);
        }
#endif
#if (defined(TCFG_MIC_REC_PITCH_EN) && (TCFG_MIC_REC_PITCH_EN))
        if (rec_hdl->pitch_hdl) {
            picth_run(rec_hdl->pitch_hdl, data, data, len, 1);
        }
#endif
    case ENCODE_SOURCE_LINE0_LR:
    case ENCODE_SOURCE_LINE1_LR:
    case ENCODE_SOURCE_LINE2_LR:
        pcm2file_enc_write_pcm(rec_hdl->file, data, len);
        break;
    default:
        break;
    }
}

void record_cut_head_timeout(void *priv)
{
    if (rec_hdl) {
        rec_hdl->cut_head_flag = 0;
        printf("record_cut_head_timeout \n");
    }
}
///cut_head_time:单位 1ms
///cut_tail_time:单位 1ms
int record_player_encode_start(struct audio_fmt *f, struct storage_dev *dev, enum enc_source source, u32 cut_head_time, u32 cut_tail_time, u32 limit_size)
{
    struct audio_fmt fmt;
    u32 cut_tail_size = 0;

    struct record_hdl *rec = NULL;

    if (rec_hdl) {
        record_player_encode_stop();
    }

    printf("struct record_hdl:%d ", sizeof(struct record_hdl));	
    rec = zalloc(sizeof(struct record_hdl));
    ASSERT(rec);

    rec->source = source;
    rec->dev = dev;

    if (f) {
        memcpy((u8 *)&fmt, (u8 *)f, sizeof(struct audio_fmt));
    } else {
        printf("use defult fmt=====\n");
        if (rec->source == ENCODE_SOURCE_MIC) {
            fmt.channel     = 1;
        } else {
            fmt.channel     = 2;
        }
        // AUDIO_CODING_G726 仅支持单声道 8k或16k采样
        /* fmt.coding_type = AUDIO_CODING_G726;;//AUDIO_CODING_MP3;// */
        fmt.coding_type = AUDIO_CODING_MP3;//AUDIO_CODING_WAV;//AUDIO_CODING_G726;;//
#if TCFG_NOR_FS_ENABLE || FLASH_INSIDE_REC_ENABLE
        fmt.coding_type = AUDIO_CODING_WAV;//外挂flash录音暂时只支持wav
#endif
        fmt.sample_rate = RECORD_PLAYER_DEFULT_SAMPLERATE;
        //coding_type为AUDIO_CODING_MP3, bit_rate为实际的码率， 当coding_type为AUDIO_CODING_WAV, bit_rate为ADPCM_BLOCKSIZE
        if ((fmt.coding_type == AUDIO_CODING_WAV) || fmt.coding_type == AUDIO_CODING_G726) {
            fmt.bit_rate    = RECORD_PLAYER_DEFULT_ADPCM_BLOCKSIZE;
        } else {
            fmt.bit_rate    = RECORD_PLAYER_DEFULT_BITRATE;
        }
    }
    rec->sample_rate = fmt.sample_rate;
    rec->nchl = fmt.channel;
    rec->coding_type = fmt.coding_type;

    printf("rec->source = %d\n", rec->source);
    printf("\n ch:%d,sm:%d,br:%d,type:%d \n ", fmt.channel, fmt.sample_rate, fmt.bit_rate, fmt.coding_type);

    rec->file = pcm2file_enc_open(&fmt, dev);

    if (!rec->file) {
        printf("mic_record_start %d\n", __LINE__);
        free(rec);
        return -1;
    }

    rec->cut_head_flag = 1;
    rec->cut_head_timer = sys_timeout_add(NULL, record_cut_head_timeout, cut_head_time);

    if ((fmt.coding_type == AUDIO_CODING_WAV) || (fmt.coding_type == AUDIO_CODING_G726)) {
        cut_tail_size = (4 * fmt.sample_rate * fmt.channel) * cut_tail_time / 1000 / 8;
        cut_tail_size = ((cut_tail_size + fmt.bit_rate - 1) / fmt.bit_rate) * fmt.bit_rate;
    } else {
        cut_tail_size = fmt.bit_rate * cut_tail_time / 8;
    }

    if (fmt.coding_type == AUDIO_CODING_WAV) {
        clock_add(ENC_WAV_CLK);
    } else if (fmt.coding_type == AUDIO_CODING_G726) {
        clock_add(ENC_G726_CLK);
    } else if (fmt.coding_type == AUDIO_CODING_MP3) {
        clock_add(ENC_MP3_CLK);
    }


    printf("cut_tail_size = %d\n", cut_tail_size);

    last_enc_file_codeing_type_save(fmt.coding_type);

    pcm2file_enc_set_evt_handler(rec->file, record_player_encode_event_handler, rec->magic);
    pcm2file_enc_write_file_set_limit(rec->file, cut_tail_size, limit_size);
    pcm2file_enc_start(rec->file);

    switch (rec->source) {
    case ENCODE_SOURCE_MIX:
        break;
    case ENCODE_SOURCE_MIC:

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE)&&(TCFG_MIC_REC_REVERB_EN))
        p_reverb_obj = open_reverb(NULL, fmt.sample_rate);
#endif

#if (defined(TCFG_MIC_REC_PITCH_EN) && (TCFG_MIC_REC_PITCH_EN))
        rec->pitch_hdl = open_pitch(NULL);
#endif
        audio_adc_mic_open(&rec->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
        audio_adc_mic_set_sample_rate(&rec->mic_ch, fmt.sample_rate);
        audio_adc_mic_set_gain(&rec->mic_ch, 2);
		rec->adc_buf = (s16*)malloc(LADC_MIC_IRQ_POINTS*2*LADC_MIC_BUF_NUM);
		ASSERT(rec->adc_buf);
		audio_adc_mic_set_buffs(&rec->mic_ch, rec->adc_buf, LADC_MIC_IRQ_POINTS * 2, LADC_MIC_BUF_NUM);
		rec->adc_output.handler = adc_output_to_enc;
        audio_adc_add_output_handler(&adc_hdl, &rec->adc_output);
        audio_adc_mic_start(&rec->mic_ch);
        break;
    case ENCODE_SOURCE_LINE0_LR:
    case ENCODE_SOURCE_LINE1_LR:
    case ENCODE_SOURCE_LINE2_LR:
        audio_adc_linein_open(&rec->linein_ch, AUDIO_ADC_LINE0_LR + (rec->source - ENCODE_SOURCE_LINE0_LR), &adc_hdl);
        audio_adc_linein_set_sample_rate(&rec->linein_ch, fmt.sample_rate);
        audio_adc_linein_set_gain(&rec->linein_ch, 1);
		rec->adc_buf = (s16*)malloc(ADC_CH_NUM * ADC_IRQ_POINTS * 2*ADC_BUF_NUM);
		ASSERT(rec->adc_buf);
		audio_adc_set_buffs(&rec->linein_ch, rec->adc_buf, ADC_CH_NUM * ADC_IRQ_POINTS * 2, ADC_BUF_NUM);
		rec->adc_output.handler = adc_output_to_enc;
        rec->adc_output.priv = &adc_hdl;
        audio_adc_add_output_handler(&adc_hdl, &rec->adc_output);
        audio_adc_linein_start(&rec->linein_ch);
        break;
    default:
        break;
    }

    clock_set_cur();
    rec->start_sys_tick = timer_get_ms();
    rec_hdl = rec;

    return 0;
}


u32 record_player_get_encoding_time()
{
    u32 time_sec = 0;
    if (rec_hdl) {
        time_sec = (timer_get_ms() - rec_hdl->start_sys_tick) / 1000;
        /* printf("rec time sec = %d\n",time_sec); */
    }
    return time_sec;
}

///检查录音是否正在进行
int record_player_is_encoding(void)
{
    if (rec_hdl) {
        return 1;
    }
    return 0;
}


void record_player_device_offline_check(char *logo)
{
    if (rec_hdl) {
        if (!strcmp(rec_hdl->dev->logo, logo)) {
            ///当前录音正在使用的设备掉线， 应该停掉录音
            printf("is the recording dev = %s\n", logo);
            record_player_encode_stop();
        }
    }
}

int mixer_recorder_encoding(void)
{
    return record_player_is_encoding();
}
int mixer_recorder_start(void)
{
    struct audio_fmt fmt = {0};
    if (audio_output_channel_num() > 2) {
        printf("chl is overlimit, record fail!!\n");
        return -1;
    }
    fmt.channel = RECORD_PLAYER_MIXER_CHANNELS;//audio_output_channel_num();//2;
    if (get_call_status() != BT_CALL_HANGUP) {
        fmt.coding_type = AUDIO_CODING_WAV;
    } else {
        fmt.coding_type = AUDIO_CODING_MP3;
    }
    fmt.sample_rate = audio_mixer_get_sample_rate(&mixer);//RECORD_PLAYER_DEFULT_SAMPLERATE;
    /*coding_type为AUDIO_CODING_MP3, bit_rate为实际的码率， 当coding_type为AUDIO_CODING_WAV, bit_rate为ADPCM_BLOCKSIZE*/
    /* fmt.bit_rate    = RECORD_PLAYER_DEFULT_ADPCM_BLOCKSIZE; */
    if ((fmt.coding_type == AUDIO_CODING_WAV) || fmt.coding_type == AUDIO_CODING_G726) {
        fmt.bit_rate    = RECORD_PLAYER_DEFULT_ADPCM_BLOCKSIZE;
    } else {
        fmt.bit_rate    = RECORD_PLAYER_DEFULT_BITRATE;
    }
    if (fmt.channel > 2) {
        return -1;
    }

    int ret = record_player_encode_start(
                  &fmt,
                  storage_dev_last(),
                  ENCODE_SOURCE_MIX,
                  0,
                  0,
                  3000L
              );
    return ret;
}

void mixer_recorder_stop(void)
{
    record_player_encode_stop();
}

#endif//







