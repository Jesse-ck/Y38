#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "btctrler/lmp_api.h"
#include "aec_user.h"
#include "asm/audio_src.h"
#include "audio_enc.h"
#include "app_main.h"
#include "app_action.h"
#include "clock_cfg.h"
#include "audio_reverb.h"

/* #include "encode/encode_write.h" */
extern struct adc_platform_data adc_data;

extern int sbc_encoder_init();
extern int msbc_encoder_init();
extern int cvsd_encoder_init();
extern int mp3_encoder_init();
extern int adpcm_encoder_init();
extern int opus_encoder_preinit();

struct audio_adc_hdl adc_hdl;
struct esco_enc_hdl *esco_enc = NULL;
struct audio_encoder_task *encode_task = NULL;
u8 enc_run_cnt = 0;

#define ESCO_ADC_BUF_NUM        3
#define ESCO_ADC_IRQ_POINTS     256
#define ESCO_ADC_BUFS_SIZE      (ESCO_ADC_BUF_NUM * ESCO_ADC_IRQ_POINTS)


struct esco_enc_hdl {
    struct audio_encoder encoder;
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
    //OS_SEM pcm_frame_sem;
    s16 output_frame[30];               //align 4Bytes
    int pcm_frame[60];                 //align 4Bytes
    s16 adc_buf[ESCO_ADC_BUFS_SIZE];    //align 4Bytes
};

static void adc_mic_output_handler(void *priv, s16 *data, int len)
{
    //printf("buf:%x,data:%x,len:%d",esco_enc->adc_buf,data,len);
    audio_aec_inbuf(data, len);
}

__attribute__((weak)) int audio_aec_output_read(s16 *buf, u16 len)
{
    return 0;
}

void esco_enc_resume(void)
{
    if (esco_enc) {
        //os_sem_post(&esco_enc->pcm_frame_sem);
        audio_encoder_resume(esco_enc);
    }
}

static int esco_enc_pcm_get(struct audio_encoder *encoder, s16 **frame, u16 frame_len)
{
    int rlen = 0;
    if (encoder == NULL) {
        r_printf("encoder NULL");
    }
    struct esco_enc_hdl *enc = container_of(encoder, struct esco_enc_hdl, encoder);

    if (enc == NULL) {
        r_printf("enc NULL");
    }

    while (1) {
        rlen = audio_aec_output_read(enc->pcm_frame, frame_len);
        if (rlen == frame_len) {
            /*esco编码读取数据正常*/
            void record_player_pcm2file_write_mix_pcm(s16 * data, int len);
            record_player_pcm2file_write_mix_pcm(enc->pcm_frame, frame_len);
            break;
        } else if (rlen == 0) {
            /*esco编码读不到数,返回0*/
            return 0;
            /*esco编码读不到数，pend住*/
            /* int ret = os_sem_pend(&enc->pcm_frame_sem, 100);
            if (ret == OS_TIMEOUT) {
                r_printf("esco_enc pend timeout\n");
                break;
            } */
        } else {
            /*通话结束，aec已经释放*/
            printf("audio_enc end:%d\n", rlen);
            rlen = 0;
            break;
        }
    }

    *frame = enc->pcm_frame;
    return rlen;
}
static void esco_enc_pcm_put(struct audio_encoder *encoder, s16 *frame)
{
}

static const struct audio_enc_input esco_enc_input = {
    .fget = esco_enc_pcm_get,
    .fput = esco_enc_pcm_put,
};

static int esco_enc_probe_handler(struct audio_encoder *encoder)
{
    return 0;
}

static int esco_enc_output_handler(struct audio_encoder *encoder, u8 *frame, int len)
{
    lmp_private_send_esco_packet(NULL, frame, len);
    //printf("frame:%x,out:%d\n",frame, len);

    return len;
}

const static struct audio_enc_handler esco_enc_handler = {
    .enc_probe = esco_enc_probe_handler,
    .enc_output = esco_enc_output_handler,
};

static void esco_enc_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    printf("esco_enc_event_handler:0x%x,%d\n", argv[0], argv[0]);
    switch (argv[0]) {
    case AUDIO_ENC_EVENT_END:
        puts("AUDIO_ENC_EVENT_END\n");
        break;
    }
}

int esco_enc_open(u32 coding_type, u8 frame_len)
{
    int err;
    struct audio_fmt fmt;

    printf("esco_enc_open: %d,frame_len:%d\n", coding_type, frame_len);

    fmt.channel = 1;
    fmt.frame_len = frame_len;
    if (coding_type == AUDIO_CODING_MSBC) {
        fmt.sample_rate = 16000;
        fmt.coding_type = AUDIO_CODING_MSBC;
        clock_add(ENC_MSBC_CLK);
    } else if (coding_type == AUDIO_CODING_CVSD) {
        fmt.sample_rate = 8000;
        fmt.coding_type = AUDIO_CODING_CVSD;
        clock_add(ENC_CVSD_CLK);
    } else {
        /*Unsupoport eSCO Air Mode*/
    }

    if (!encode_task) {
        encode_task = zalloc(sizeof(*encode_task));
        audio_encoder_task_create(encode_task, "audio_enc");
    }
    enc_run_cnt++;
    if (!esco_enc) {
        esco_enc = zalloc(sizeof(*esco_enc));
    }
    //os_sem_create(&esco_enc->pcm_frame_sem, 0);

    audio_encoder_open(&esco_enc->encoder, &esco_enc_input, encode_task);
    audio_encoder_set_handler(&esco_enc->encoder, &esco_enc_handler);
    audio_encoder_set_fmt(&esco_enc->encoder, &fmt);
    audio_encoder_set_event_handler(&esco_enc->encoder, esco_enc_event_handler, 0);
    audio_encoder_set_output_buffs(&esco_enc->encoder, esco_enc->output_frame,
                                   sizeof(esco_enc->output_frame), 1);

    audio_encoder_start(&esco_enc->encoder);

    printf("esco sample_rate: %d,mic_gain:%d\n", fmt.sample_rate, app_var.aec_mic_gain);
#if (TCFG_REVERB_ENABLE && TCFG_CALLING_EN_REVERB)
    if (reverb_if_working()) {
        reverb_en_mic2aec(1);
    } else {
        audio_adc_mic_open(&esco_enc->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
        audio_adc_mic_set_sample_rate(&esco_enc->mic_ch, fmt.sample_rate);
        audio_adc_mic_set_gain(&esco_enc->mic_ch, app_var.aec_mic_gain);
        audio_adc_mic_set_buffs(&esco_enc->mic_ch, esco_enc->adc_buf,
                                ESCO_ADC_IRQ_POINTS * 2, ESCO_ADC_BUF_NUM);
        esco_enc->adc_output.handler = adc_mic_output_handler;
        audio_adc_add_output_handler(&adc_hdl, &esco_enc->adc_output);

        audio_adc_mic_start(&esco_enc->mic_ch);
    }
#else
    audio_adc_mic_open(&esco_enc->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
    audio_adc_mic_set_sample_rate(&esco_enc->mic_ch, fmt.sample_rate);
    audio_adc_mic_set_gain(&esco_enc->mic_ch, app_var.aec_mic_gain);
    audio_adc_mic_set_buffs(&esco_enc->mic_ch, esco_enc->adc_buf,
                            ESCO_ADC_IRQ_POINTS * 2, ESCO_ADC_BUF_NUM);
    esco_enc->adc_output.handler = adc_mic_output_handler;
    audio_adc_add_output_handler(&adc_hdl, &esco_enc->adc_output);

    audio_adc_mic_start(&esco_enc->mic_ch);
#endif
    clock_set_cur();

    return 0;
}

void esco_enc_close()
{
    printf("esco_enc_close\n");
    if (!esco_enc) {
        printf("esco_enc NULL\n");
        return;
    }

    if (esco_enc->encoder.fmt.coding_type == AUDIO_CODING_MSBC) {
        clock_remove(ENC_MSBC_CLK);
    } else if (esco_enc->encoder.fmt.coding_type == AUDIO_CODING_CVSD) {
        clock_remove(ENC_CVSD_CLK);
    }
#if (TCFG_REVERB_ENABLE && TCFG_CALLING_EN_REVERB)
    if (reverb_if_working()) {
        reverb_en_mic2aec(0);
    } else {
        audio_adc_mic_close(&esco_enc->mic_ch);
        //os_sem_post(&esco_enc->pcm_frame_sem);
        audio_encoder_close(&esco_enc->encoder);
        audio_adc_del_output_handler(&adc_hdl, &esco_enc->adc_output);
    }
#else
    audio_adc_mic_close(&esco_enc->mic_ch);
    //os_sem_post(&esco_enc->pcm_frame_sem);
    audio_encoder_close(&esco_enc->encoder);
    audio_adc_del_output_handler(&adc_hdl, &esco_enc->adc_output);

#endif
    free(esco_enc);
    esco_enc = NULL;

    enc_run_cnt--;
    if (enc_run_cnt == 0 && encode_task) {
        audio_encoder_task_del(encode_task);
        free(encode_task);
        encode_task = NULL;
    }
    clock_set_cur();
}


//////////////////////////////////////////////////////////////////////////////
int g726_encoder_init();
int audio_enc_init()
{
    printf("audio_enc_init\n");

#if TCFG_ENC_CVSD_ENABLE
    cvsd_encoder_init();
#endif
#if TCFG_ENC_MSBC_ENABLE
    msbc_encoder_init();
#endif
#if TCFG_ENC_MP3_ENABLE
    mp3_encoder_init();
#endif
#if TCFG_ENC_ADPCM_ENABLE
    adpcm_encoder_init();
#endif
#if TCFG_ENC_OPUS_ENABLE
    opus_encoder_preinit();
#endif

    audio_adc_init(&adc_hdl, &adc_data);
#if TCFG_ENC_SBC_ENABLE
    sbc_encoder_init();
#endif
    return 0;
}





