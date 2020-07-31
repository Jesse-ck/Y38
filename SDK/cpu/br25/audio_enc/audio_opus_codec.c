#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "btctrler/lmp_api.h"
#include "aec_user.h"
#include "asm/audio_src.h"
#include "audio_enc.h"
#include "app_main.h"
#include "btstack/avctp_user.h"
#include "clock_cfg.h"

#if TCFG_ENC_OPUS_ENABLE

extern struct audio_encoder_task *encode_task;
//static struct audio_encoder_task *encode_task = NULL;

#define ESCO_ADC_BUF_NUM        2
#define ESCO_ADC_IRQ_POINTS     256
#define ESCO_ADC_BUFS_SIZE      (ESCO_ADC_BUF_NUM * ESCO_ADC_IRQ_POINTS)

#define OPUS_USE_MIC_CHANNEL    1
#define OPUS_ENC_IN_SIZE		512
#define OPUS_ENC_OUT_SIZE		256
struct opus_enc_hdl {
    struct audio_encoder encoder;
    OS_SEM pcm_frame_sem;
    u8 output_frame[OPUS_ENC_OUT_SIZE];
    u8  pcm_frame[OPUS_ENC_IN_SIZE];
    u8 frame_size;
    u8 out_cbuf_buf[OPUS_ENC_OUT_SIZE * 8];
    cbuffer_t output_cbuf;
    u8 in_cbuf_buf[OPUS_ENC_IN_SIZE * 4];
    cbuffer_t pcm_in_cbuf;
#if opus_ENC_PACK_ENABLE
    u16 cp_type;
    u16 packet_head_sn;
#endif
#if OPUS_USE_MIC_CHANNEL
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
    s16 adc_buf[ESCO_ADC_BUFS_SIZE];    //align 4Bytes
#endif
};

static struct opus_enc_hdl *opus_enc = NULL;

void opus_enc_resume(void)
{
    if (opus_enc) {
        os_sem_post(&opus_enc->pcm_frame_sem);
    }
}

static int opus_enc_pcm_get(struct audio_encoder *encoder, s16 **frame, u16 frame_len)
{
    int pcm_len = 0;
    if (encoder == NULL) {
        r_printf("encoder NULL");
    }
    struct opus_enc_hdl *enc = container_of(encoder, struct opus_enc_hdl, encoder);

    if (enc == NULL) {
        r_printf("enc NULL");
    }
    if (cbuf_get_data_size(&enc->pcm_in_cbuf) < frame_len) {
        os_sem_pend(&opus_enc->pcm_frame_sem, 0);
    }

    //memset(enc->pcm_frame, 0, sizeof(enc->pcm_frame));
    pcm_len = cbuf_read(&enc->pcm_in_cbuf, enc->pcm_frame, frame_len);
    if (pcm_len != frame_len) {
        putchar('L');
    }

    *frame = enc->pcm_frame;
    return pcm_len;
}

static void opus_enc_pcm_put(struct audio_encoder *encoder, s16 *frame)
{
}

static const struct audio_enc_input opus_enc_input = {
    .fget = opus_enc_pcm_get,
    .fput = opus_enc_pcm_put,
};

static int opus_enc_probe_handler(struct audio_encoder *encoder)
{
    return 0;
}




static int opus_enc_output_handler(struct audio_encoder *encoder, u8 *frame, int len)
{
    if (encoder == NULL) {
        r_printf("encoder NULL");
    }
    put_buf(frame, 16);
    return len;
}

const static struct audio_enc_handler opus_enc_handler = {
    .enc_probe = opus_enc_probe_handler,
    .enc_output = opus_enc_output_handler,
};

static void opus_enc_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    printf("opus_enc_event_handler:0x%x,%d\n", argv[0], argv[0]);
    switch (argv[0]) {
    case AUDIO_ENC_EVENT_END:
        puts("AUDIO_ENC_EVENT_END\n");
        break;
    }
}



static void adc_mic_output_handler(void *priv, s16 *data, int len)
{
    if (opus_enc) {
        u16 wlen = cbuf_write(&opus_enc->pcm_in_cbuf, data, len);
        if (wlen != len) {
            putchar('@');
        }
        audio_encoder_resume(&opus_enc->encoder);
        opus_enc_resume();
    }
}




extern struct audio_adc_hdl adc_hdl;
int audio_opus_enc_open()
{
    int err;
    struct audio_fmt fmt;
    u16 frame_len = 512;

    if (encode_task) {
        return -1;
    }
    fmt.quality = 2;
    fmt.sample_rate = 16000;
    fmt.coding_type = AUDIO_CODING_OPUS;

    if (!encode_task) {
        encode_task = zalloc(sizeof(*encode_task));
        audio_encoder_task_create(encode_task, "audio_enc");
    }
    if (!opus_enc) {
        opus_enc = zalloc(sizeof(*opus_enc));
    }
    cbuf_init(&opus_enc->output_cbuf, opus_enc->out_cbuf_buf, OPUS_ENC_OUT_SIZE * 8);
    cbuf_init(&opus_enc->pcm_in_cbuf, opus_enc->in_cbuf_buf, OPUS_ENC_IN_SIZE * 3);
    os_sem_create(&opus_enc->pcm_frame_sem, 0);
    audio_encoder_open(&opus_enc->encoder, &opus_enc_input, encode_task);
    audio_encoder_set_handler(&opus_enc->encoder, &opus_enc_handler);
    audio_encoder_set_fmt(&opus_enc->encoder, &fmt);
    audio_encoder_set_event_handler(&opus_enc->encoder, opus_enc_event_handler, 0);
    audio_encoder_set_output_buffs(&opus_enc->encoder, opus_enc->output_frame,
                                   sizeof(opus_enc->output_frame), 1);
    int start_err = audio_encoder_start(&opus_enc->encoder);
    printf("opus_enc_open ok %d\n", start_err);

#if OPUS_USE_MIC_CHANNEL
    fmt.sample_rate = 16000;
    audio_adc_mic_open(&opus_enc->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
    audio_adc_mic_set_sample_rate(&opus_enc->mic_ch, fmt.sample_rate);
    audio_adc_mic_set_gain(&opus_enc->mic_ch, app_var.aec_mic_gain);
    audio_adc_mic_set_buffs(&opus_enc->mic_ch, opus_enc->adc_buf,
                            ESCO_ADC_IRQ_POINTS * 2, ESCO_ADC_BUF_NUM);
    opus_enc->adc_output.handler = adc_mic_output_handler;
    audio_adc_add_output_handler(&adc_hdl, &opus_enc->adc_output);


    //app_audio_output_samplerate_set(44100);
    //app_audio_output_start();
    audio_adc_mic_start(&opus_enc->mic_ch);
#endif

    clock_add(ENC_MP3_CLK);
    clock_set_cur();
    return 0;
}




int audio_opus_enc_close()
{
    if (!opus_enc) {
        return -1;
    }
    printf("audio_opus_enc_close\n");
#if OPUS_USE_MIC_CHANNEL
    audio_adc_mic_close(&opus_enc->mic_ch);
    audio_adc_del_output_handler(&adc_hdl, &opus_enc->adc_output);
#endif
    opus_enc_resume();
    audio_encoder_close(&opus_enc->encoder);
    free(opus_enc);
    opus_enc = NULL;

    if (encode_task) {
        audio_encoder_task_del(encode_task);
        free(encode_task);
        encode_task = NULL;
    }

    clock_remove(ENC_MP3_CLK);
    clock_set_cur();
    printf("audio_opus_enc_close end\n");
    return 0;
}

#endif
