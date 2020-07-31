
#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "asm/audio_src.h"
#include "audio_enc.h"
#include "app_main.h"
#include "app_action.h"
#include "clock_cfg.h"

#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))

#define SBC_ENC_IN_SIZE			512
#define SBC_ENC_OUT_SIZE		256

#define SBC_ENC_IN_CBUF_SIZE	(SBC_ENC_IN_SIZE * 4)
#define SBC_ENC_OUT_CBUF_SIZE	(SBC_ENC_OUT_SIZE * 4)

#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
#define SBC_PACKET_HEADER_LEN   (14) /*包头Max Length*/
#define SBC_FRAME_SUM           (4)	 /*一包多少帧*/
static unsigned char pcm2tws_sbc_packet_head_buf[SBC_PACKET_HEADER_LEN] = {
    0x80, 0x60,     		//Version:2 Playload Type:0x60
    0x00, 0x64,   			//Sequencenumber
    0x00, 0x00, 0x03, 0x20, //Timestamp
    0x00, 0x00, 0x00, 0x00  //Synchronization Source
    //SBC_FRAME_SUM			//defined for Media Payload
};
#endif /* (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE)) */

#define PCM2TWS_ENC_TASK			encode_task
#define PCM2TWS_ENC_TASK_NAME		"audio_enc"

#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
#define PCM_ENC2TWS_OUTBUF_LEN				(SBC_ENC_IN_SIZE * 2)
#else
#define PCM_ENC2TWS_OUTBUF_LEN				(4 * 1024)
#endif

struct pcm2tws_enc_hdl {
    struct audio_encoder encoder;
    /* OS_SEM pcm_frame_sem; */
#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
    s16 output_frame[SBC_ENC_OUT_SIZE / 2];               //align 4Bytes
#else
    s16 output_frame[2304 / 2];               //align 4Bytes
#endif
    int pcm_frame[SBC_ENC_IN_SIZE / 4];       //align 4Bytes
    u8 output_buf[PCM_ENC2TWS_OUTBUF_LEN];
    cbuffer_t output_cbuf;
#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
    u8 sbc_out_buf[SBC_PACKET_HEADER_LEN + SBC_ENC_OUT_CBUF_SIZE];
    u8 sbc_fame_cnt;
    u16 sbc_fame_len;
    u16 sbc_fame_sn;
#endif
    void (*resume)(void);
    int (*output)(struct audio_fmt *, s16 *, int);
    u32 status : 3;
    u32 reserved: 29;
};
struct pcm2tws_enc_hdl *pcm2tws_enc = NULL;

extern struct audio_encoder_task *encode_task;

static void pcm2tws_encoder_resume(struct pcm2tws_enc_hdl *enc)
{
    /* os_sem_set(&enc->pcm_frame_sem, 0); */
    /* os_sem_post(&enc->pcm_frame_sem); */
    if (enc->status) {
        audio_encoder_resume(&enc->encoder);
    }
}

int pcm2tws_enc_output(void *priv, s16 *data, int len)
{
    if (!pcm2tws_enc || !pcm2tws_enc->status) {
        return 0;
    }
    u16 wlen = cbuf_write(&pcm2tws_enc->output_cbuf, data, len);
    if (!wlen) {
        /* putchar(','); */
        if (len > (PCM_ENC2TWS_OUTBUF_LEN / 2)) {
            wlen = cbuf_write(&pcm2tws_enc->output_cbuf, data, (PCM_ENC2TWS_OUTBUF_LEN / 2));
        }
    }
    /* printf("wl:%d ", wlen); */
    if (cbuf_get_data_len(&pcm2tws_enc->output_cbuf) >= SBC_ENC_IN_SIZE) {
        pcm2tws_encoder_resume(pcm2tws_enc);
    }
    return wlen;
}

void pcm2tws_enc_set_resume_handler(void (*resume)(void))
{
    if (pcm2tws_enc) {
        pcm2tws_enc->resume = resume;
    }
}

void pcm2tws_enc_set_output_handler(int (*output)(struct audio_fmt *, s16 *, int))
{
    if (pcm2tws_enc) {
        pcm2tws_enc->output = output;
    }
}

static void pcm2tws_enc_need_data(void)
{
    if (pcm2tws_enc && pcm2tws_enc->resume) {
        pcm2tws_enc->resume();
    }
}

static int pcm2tws_enc_pcm_get(struct audio_encoder *encoder, s16 **frame, u16 frame_len)
{
    int rlen = 0;
    int dlen = 0;
    if (encoder == NULL) {
        r_printf("encoder NULL");
    }
    struct pcm2tws_enc_hdl *enc = container_of(encoder, struct pcm2tws_enc_hdl, encoder);

    if (enc == NULL) {
        r_printf("enc NULL");
    }
    /* os_sem_set(&pcm2tws_enc->pcm_frame_sem, 0); */
    /* printf("l:%d", frame_len); */


    if (!pcm2tws_enc->status) {
        return 0;
    }

#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
    if (encoder->fmt.coding_type & AUDIO_CODING_FAME_MASK) {
        frame_len = SBC_ENC_IN_SIZE;
        if (encoder->fmt.channel == 1) {
            frame_len = SBC_ENC_IN_SIZE / 2;
        }
    }
#endif

    pcm2tws_enc_need_data();

    dlen = cbuf_get_data_len(&pcm2tws_enc->output_cbuf);
    if (dlen < frame_len) {
        return 0;
    }

    rlen = cbuf_read(&pcm2tws_enc->output_cbuf, enc->pcm_frame, frame_len);

    *frame = enc->pcm_frame;

    return rlen;

#if 0
    do {
        rlen = cbuf_read(&pcm2tws_enc->output_cbuf, enc->pcm_frame, frame_len);
        if (rlen == frame_len) {
            break;
        }
        if (rlen == -EINVAL) {
            return 0;
        }
        if (!pcm2tws_enc->status) {
            return 0;
        }
        pcm2tws_enc_need_data();
        os_sem_pend(&pcm2tws_enc->pcm_frame_sem, 2);
    } while (1);

    *frame = enc->pcm_frame;
    return rlen;
#endif
}
static void pcm2tws_enc_pcm_put(struct audio_encoder *encoder, s16 *frame)
{
}

static const struct audio_enc_input pcm2tws_enc_input = {
    .fget = pcm2tws_enc_pcm_get,
    .fput = pcm2tws_enc_pcm_put,
};

static int pcm2tws_enc_probe_handler(struct audio_encoder *encoder)
{
    return 0;
}

#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
#include "sbc_enc.h"
static sbc_t pcm2tws_sbc_param = {
    .frequency = SBC_FREQ_44100,
    .blocks = SBC_BLK_16,
    .subbands = SBC_SB_8,
    .mode = SBC_MODE_STEREO,
    .allocation = 0,
    .endian = SBC_LE,
    .bitpool = 53,
};

int pcm2tws_sbc_sample_rate_select(int rate)
{
    if (rate >= 48000) {
        return 48000;
    } else if (rate >= 44100) {
        return 44100;
    } else if (rate >= 32000) {
        return 32000;
    }
    return 16000;
}

static void pcm2tws_sbc_enc_packet_pack(struct pcm2tws_enc_hdl *hdl)
{
    unsigned int TS; /*TimeStamp*/
    /*SequenceNumber*/
    hdl->sbc_fame_sn++;
    /*Timestamp*/
    TS = hdl->sbc_fame_sn * (128 * SBC_FRAME_SUM);

    pcm2tws_sbc_packet_head_buf[2] = hdl->sbc_fame_sn >> 8;
    pcm2tws_sbc_packet_head_buf[3] = hdl->sbc_fame_sn & 0xff;
    pcm2tws_sbc_packet_head_buf[4] = TS >> 24;
    pcm2tws_sbc_packet_head_buf[5] = (TS >> 16) & 0xff;
    pcm2tws_sbc_packet_head_buf[6] = (TS >> 8) & 0xff;
    pcm2tws_sbc_packet_head_buf[7] = (TS) & 0xff;
#if 0
    if (hdl->cp_type == 0x0002) {
        /*
         *Content Protection Header
         *L-bit	:bit0
         *Cp-bit:bit1
         *RFA	:bit2 to bit7
         */
        pcm2tws_sbc_packet_head_buf[12] = 0;
        pcm2tws_sbc_packet_head_buf[13] = SBC_FRAME_SUM;
    } else
#endif
    {
        pcm2tws_sbc_packet_head_buf[12] = SBC_FRAME_SUM;
    }
}
#endif

static int pcm2tws_enc_output_handler(struct audio_encoder *encoder, u8 *frame, int len)
{
    struct pcm2tws_enc_hdl *enc = container_of(encoder, struct pcm2tws_enc_hdl, encoder);

    if (!pcm2tws_enc->status) {
        return 0;
    }

#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
    if (encoder->fmt.coding_type & AUDIO_CODING_FAME_MASK) {
        /* printf("fc:%d, fl:%d, len:%d,, cnt:%d, \n", enc->sbc_fame_cnt, enc->sbc_fame_len, len, ccccccccccc); */
        /* printf_buf(frame, len); */
        if (enc->sbc_fame_cnt < SBC_FRAME_SUM) {
            if (enc->sbc_fame_cnt == 0) {
                pcm2tws_sbc_enc_packet_pack(enc);
                memcpy(enc->sbc_out_buf, pcm2tws_sbc_packet_head_buf, SBC_PACKET_HEADER_LEN);
                enc->sbc_fame_len = SBC_PACKET_HEADER_LEN;
            }
            memcpy(&enc->sbc_out_buf[enc->sbc_fame_len], frame, len);
            enc->sbc_fame_len += len;
            enc->sbc_fame_cnt ++;
        }
        if (enc->sbc_fame_cnt >= SBC_FRAME_SUM) {
            int wlen = pcm2tws_enc->output(&encoder->fmt, enc->sbc_out_buf, enc->sbc_fame_len);
            if (wlen == enc->sbc_fame_len) {
                enc->sbc_fame_cnt = 0;
                return len;
            }
            return 0;
        }
        return len;
    }
#endif

    int wlen = pcm2tws_enc->output(&encoder->fmt, frame, len);

    if (wlen == -EINVAL) {
        return 0;
    }

    return wlen;

#if 0
    int olen = len;

    os_sem_set(&pcm2tws_enc->pcm_frame_sem, 0);
    do {
        int wlen = pcm2tws_enc->output(&encoder->fmt, frame, len);
        if (wlen == len) {
            break;
        }
        if (wlen == -EINVAL) {
            return 0;
        }
        if (!pcm2tws_enc->status) {
            return 0;
        }
        frame += wlen;
        len -= wlen;
        os_sem_pend(&pcm2tws_enc->pcm_frame_sem, 2);
    } while (1);
    return olen;
#endif
}

const static struct audio_enc_handler pcm2tws_enc_handler = {
    .enc_probe = pcm2tws_enc_probe_handler,
    .enc_output = pcm2tws_enc_output_handler,
};



int pcm2tws_enc_open(struct audio_fmt *pfmt)
{
    int err;
    struct audio_fmt fmt;

    printf("pcm2tws_enc_open: 0x%x, sr:%d, ch:%d \n", pfmt->coding_type, pfmt->sample_rate, pfmt->channel);

    if (!PCM2TWS_ENC_TASK) {
        PCM2TWS_ENC_TASK = zalloc(sizeof(*PCM2TWS_ENC_TASK));
        audio_encoder_task_create(PCM2TWS_ENC_TASK, PCM2TWS_ENC_TASK_NAME);
    }
    if (pcm2tws_enc) {
        pcm2tws_enc_close();
    }
    printf("pcm2tws_enc size:%d \n\n", sizeof(*pcm2tws_enc));
    pcm2tws_enc = zalloc(sizeof(*pcm2tws_enc));
    ASSERT(pcm2tws_enc);

    memcpy(&fmt, pfmt, sizeof(struct audio_fmt));

#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
    if (fmt.coding_type & AUDIO_CODING_SBC) {
        fmt.frame_len = SBC_ENC_IN_SIZE;
        fmt.priv = &pcm2tws_sbc_param;
        switch (fmt.sample_rate) {
        case 16000:
            pcm2tws_sbc_param.frequency = SBC_FREQ_16000;
            break;
        case 32000:
            pcm2tws_sbc_param.frequency = SBC_FREQ_32000;
            break;
        default:
        case 44100:
            pcm2tws_sbc_param.frequency = SBC_FREQ_44100;
            break;
        case 48000:
            pcm2tws_sbc_param.frequency = SBC_FREQ_48000;
            break;
        }
        if (fmt.channel == 1) {
            pcm2tws_sbc_param.mode = SBC_MODE_MONO;
        } else {
            pcm2tws_sbc_param.mode = SBC_MODE_STEREO;
        }
    }
#endif

    /* os_sem_create(&pcm2tws_enc->pcm_frame_sem, 0); */

#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
    if (fmt.coding_type & AUDIO_CODING_SBC) {
        clock_add(ENC_TWS_SBC_CLK);
    } else
#endif
    {
        clock_add(ENC_MP3_CLK);
    }

    cbuf_init(&pcm2tws_enc->output_cbuf, pcm2tws_enc->output_buf, PCM_ENC2TWS_OUTBUF_LEN);

    audio_encoder_open(&pcm2tws_enc->encoder, &pcm2tws_enc_input, PCM2TWS_ENC_TASK);
    audio_encoder_set_handler(&pcm2tws_enc->encoder, &pcm2tws_enc_handler);
    audio_encoder_set_fmt(&pcm2tws_enc->encoder, &fmt);
    audio_encoder_set_output_buffs(&pcm2tws_enc->encoder, pcm2tws_enc->output_frame,
                                   sizeof(pcm2tws_enc->output_frame), 1);

    pcm2tws_enc->status = 1;

    audio_encoder_start(&pcm2tws_enc->encoder);
    clock_set_cur();
    printf("sample_rate: %d\n", pcm2tws_enc->encoder.fmt.sample_rate);

    return 0;
}

void pcm2tws_enc_close()
{
    if (!pcm2tws_enc) {
        return;
    }

#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
    if (pcm2tws_enc->encoder.enc_ops->coding_type & AUDIO_CODING_SBC) {
        clock_remove(ENC_TWS_SBC_CLK);
    } else
#endif
    {
        clock_remove(ENC_MP3_CLK);
    }

    pcm2tws_enc->status = 0;
    printf("pcm2tws_enc_close");
    audio_encoder_close(&pcm2tws_enc->encoder);
    free(pcm2tws_enc);
    pcm2tws_enc = NULL;
    if (PCM2TWS_ENC_TASK) {
        audio_encoder_task_del(PCM2TWS_ENC_TASK);
        free(PCM2TWS_ENC_TASK);
        PCM2TWS_ENC_TASK = NULL;
    }
    clock_set_cur();
}

void pcm2tws_enc_resume(void)
{
    if (pcm2tws_enc) {
        pcm2tws_encoder_resume(pcm2tws_enc);
    }
}

int pcm2tws_enc_reset(void)
{
    if (!pcm2tws_enc) {
        return -1;
    }
    void (*resume)(void) = pcm2tws_enc->resume;
    int (*output)(struct audio_fmt *, s16 *, int) = pcm2tws_enc->output;
    struct audio_fmt fmt;
    memcpy(&fmt, &pcm2tws_enc->encoder.fmt, sizeof(struct audio_fmt));
    pcm2tws_enc_close();
    pcm2tws_enc_open(&fmt);
    pcm2tws_enc_set_output_handler(output);
    pcm2tws_enc_set_resume_handler(resume);
    return 0;
}

#endif





