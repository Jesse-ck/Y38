
#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "encode/encode_write.h"
#include "asm/audio_src.h"
#include "audio_enc.h"
#include "app_main.h"
#include "app_action.h"
#include "clock_cfg.h"

#if (defined(ENC_WRITE_FILE_ENABLE) && (ENC_WRITE_FILE_ENABLE))

#define RECORD_MIX_ENABLE                   1

#define UPDATA_WAV_HEAD_TIME                (5000)

#define PCM_ENC2FILE_PCM_LEN				(1*1024+128)
#define PCM_ENC2FILE_FILE_LEN				(1*1024+256)//(4 * 1024)
#define WAV_FILE_HEAD_LEN                   90

#define PCM2FILE_ENC_BUF_COUNT				0

extern struct audio_encoder_task *encode_task;
extern u8 enc_run_cnt;

void updata_wav_head_info(struct audio_encoder *encoder, void *priv);

#define MP3_OUTPUT_FRAME_LEN                (1152 / 2 * 2)
#define WAV_OUTPUT_FRAME_LEN                (512 / 8 * 2)
s16 *output_frame;

struct pcm2file_enc_hdl {
    struct audio_encoder encoder;
    /* s16 output_frame[512 / 8];             //align 4Bytes */
    int pcm_frame[64];                 //align 4Bytes
    u8 file_head_frame[128];
    u8 file_head_len;
    u8 pcm_buf[PCM_ENC2FILE_PCM_LEN];
    cbuffer_t pcm_cbuf;
#if RECORD_MIX_ENABLE
    u8 mix_pcm_buf[PCM_ENC2FILE_PCM_LEN];//mix单声道数据减半
    cbuffer_t mix_pcm_cbuf;
    int mix_pcm_frame[64];                 //align 4Bytes
#endif
#ifdef TEST_PCM_S
    u8  out_file_tmp_flag;
    s16 out_file_tmp[256];
#endif

    /* int out_file_frame[512 / 4]; */
    int out_file_frame[512 / 8];
    u8 out_file_buf[PCM_ENC2FILE_FILE_LEN];
    cbuffer_t out_file_cbuf;

    void *whdl;
    OS_SEM sem_wfile;

    volatile u32 status : 1;
        volatile u32 enc_err : 1;
        volatile u32 encoding : 1;

        u32 lost;

#if PCM2FILE_ENC_BUF_COUNT
        u16 pcm_buf_max;
        u16 out_file_max;
#endif

    };

    static void pcm2file_enc_resume(struct pcm2file_enc_hdl *enc)
{
    audio_encoder_resume(&enc->encoder);
}

static void pcm2file_wfile_resume(struct pcm2file_enc_hdl *enc)
{
    os_sem_set(&enc->sem_wfile, 0);
    os_sem_post(&enc->sem_wfile);
    enc_write_file_resume(enc->whdl);
}

// 写pcm数据
int pcm2file_enc_write_pcm(void *priv, s16 *data, int len)
{
    struct pcm2file_enc_hdl *enc = (struct pcm2file_enc_hdl *)priv;
    if (!enc || !enc->status || enc->enc_err) {
        return 0;
    }
    enc->encoding = 1;
    u16 wlen = cbuf_write(&enc->pcm_cbuf, data, len);
    if (!wlen) {
        enc->lost++;
        putchar('~');
    } else {
        /* putchar('G');//__G__ */
    }
#if PCM2FILE_ENC_BUF_COUNT
    if (enc->pcm_buf_max < enc->pcm_cbuf.data_len) {
        enc->pcm_buf_max = enc->pcm_cbuf.data_len;
    }
#endif
    /* printf("wl:%d ", wlen); */
    // 激活录音编码器
    pcm2file_enc_resume(enc);
    enc->encoding = 0;
    return wlen;
}

// 写入录音待混合PCM数据
int pcm2file_enc_write_mix_pcm(void *priv, s16 *data, int len)
{

#if RECORD_MIX_ENABLE
    struct pcm2file_enc_hdl *enc = (struct pcm2file_enc_hdl *)priv;
    if (!enc || !enc->status || enc->enc_err) {
        return 0;
    }
    int wlen = cbuf_write(&enc->mix_pcm_cbuf, data, len);
    if (!wlen) {
        putchar('M');
    }
    return wlen;
#else
    return len;
#endif
}
// 编码器获取数据
static int pcm2file_enc_pcm_get(struct audio_encoder *encoder, s16 **frame, u16 frame_len)
{
    int i, rlen = 0;
    int rlen_mix = 0;
    int dlen = 0;
    s32 temp = 0;
    if (encoder == NULL) {
        r_printf("encoder NULL");
    }
    struct pcm2file_enc_hdl *enc = container_of(encoder, struct pcm2file_enc_hdl, encoder);

    if (enc == NULL) {
        r_printf("enc NULL");
    }

    /* printf("l:%d", frame_len); */

    if (!enc->status) {
        return 0;
    }
    if (enc->enc_err) {
        return 0;
    }

    dlen = cbuf_get_data_len(&enc->pcm_cbuf);
    if (dlen < frame_len) {
        /* putchar('T');//__T__ */
        return 0;
    }

    rlen = cbuf_read(&enc->pcm_cbuf, enc->pcm_frame, frame_len);
    /* #if 0 */
#if RECORD_MIX_ENABLE
    if (rlen) {
        rlen_mix = cbuf_read(&enc->mix_pcm_cbuf, enc->mix_pcm_frame, rlen);//默认录单声道数据
        /* printf("r mixlen[%d]",rlen_mix); */
        if (rlen_mix) {
            s16 *p_data = (s16 *)enc->pcm_frame;
            s16 *p_mix_data = (s16 *)enc->mix_pcm_frame;
#if 1

            for (i = 0; i < rlen_mix / 2; i++) {
                temp = 	p_data[i];
                temp += p_mix_data[i];
                if (temp < -32768) {
                    temp = -32768;
                } else if (temp > 32767) {
                    temp = 32767;
                }
                p_data[i] = temp;

            }
#else

            for (i = 0; i < rlen_mix / 2; i++) {
                temp = 	p_data[i * 2];
                temp += p_mix_data[i];
                if (temp < -32768) {
                    temp = -32768;
                } else if (temp > 32767) {
                    temp = 32767;
                }
                p_data[i * 2] = temp;

                temp = 	p_data[i * 2 + 1];
                temp += p_mix_data[i];
                if (temp < -32768) {
                    temp = -32768;
                } else if (temp > 32767) {
                    temp = 32767;
                }
                p_data[i * 2 + 1] = temp;
            }
#endif
        }
    }
#endif
    *frame = enc->pcm_frame;

    return rlen;
}

static void pcm2file_enc_pcm_put(struct audio_encoder *encoder, s16 *frame)
{
}

static const struct audio_enc_input pcm2file_enc_input = {
    .fget = pcm2file_enc_pcm_get,
    .fput = pcm2file_enc_pcm_put,
};
#ifdef TEST_PCM_S
static int pcm2file_enc_output_handler(struct audio_encoder *encoder, u8 *frame, int len);
#endif
static int pcm2file_enc_probe_handler(struct audio_encoder *encoder)
{
#ifdef TEST_PCM_S
    struct pcm2file_enc_hdl *pcm2file = container_of(encoder, struct pcm2file_enc_hdl, encoder);

    if (pcm2file->out_file_tmp_flag) {
        int wlen = pcm2file_enc_output_handler(encoder, pcm2file->out_file_tmp, sizeof(pcm2file->out_file_tmp) / 2);
        if (wlen != sizeof(pcm2file->out_file_tmp) / 2) {
            return -1;
        }
        pcm2file->out_file_tmp_flag = 0;
    }
    while (cbuf_read(&pcm2file->pcm_cbuf, pcm2file->out_file_tmp, sizeof(pcm2file->out_file_tmp))) {
        s16 *idat = (s16 *)pcm2file->out_file_tmp;
        s16 *odat = (s16 *)pcm2file->out_file_tmp;
        for (int i = 0; i < sizeof(pcm2file->out_file_tmp) / 4; i++) {
            *odat++ = *idat++;
            idat++;
        }
        int wlen = pcm2file_enc_output_handler(encoder, pcm2file->out_file_tmp, sizeof(pcm2file->out_file_tmp) / 2);
        if (wlen != sizeof(pcm2file->out_file_tmp) / 2) {
            pcm2file->out_file_tmp_flag = 1;
            break;
        }
    }
    return -1;
#endif
    return 0;
}
// 编码器输出
static int pcm2file_enc_output_handler(struct audio_encoder *encoder, u8 *frame, int len)
{
    struct pcm2file_enc_hdl *enc = container_of(encoder, struct pcm2file_enc_hdl, encoder);

    int wlen = cbuf_write(&enc->out_file_cbuf, frame, len);
#if PCM2FILE_ENC_BUF_COUNT
    if (enc->out_file_max < enc->out_file_cbuf.data_len) {
        enc->out_file_max = enc->out_file_cbuf.data_len;
    }
#endif
    pcm2file_wfile_resume(enc);
    /* if (wlen != len) { */
    /* printf("X"); */
    /* } */
    /* if (!enc->status) { */
    /* return 0; */
    /* } */
    if (enc->enc_err) {
        return 0;
    }

    return wlen;
}

static void pcm2file_enc_get_head_info(struct audio_encoder *encoder)
{
    struct pcm2file_enc_hdl *pcm2file = container_of(encoder, struct pcm2file_enc_hdl, encoder);
    u16 len = 0;
    u8 *ptr = (u8 *)audio_encoder_ioctrl(&pcm2file->encoder, 2, AUDIO_ENCODER_IOCTRL_CMD_GET_HEAD_INFO, &len);
    printf("%s, ptr = %x, len = %d\n", __FUNCTION__, ptr, len);
    if (ptr) {
        if (len > sizeof(pcm2file->file_head_frame)) {
            printf("file_head_frame buf not enough\n");
            return;
        }
        memcpy(pcm2file->file_head_frame, ptr, len);
        /* put_buf(pcm2file->file_head_frame, len); */
        pcm2file->file_head_len = len;
    }
}

static int pcm2file_enc_close_handler(struct audio_encoder *encoder)
{
    //做一些编码关闭前的操作， 例如：adpcm写头操作
    pcm2file_enc_get_head_info(encoder);//写编码头部信息
    return 0;
}

const static struct audio_enc_handler pcm2file_enc_handler = {
    .enc_probe = pcm2file_enc_probe_handler,
    .enc_output = pcm2file_enc_output_handler,
    .enc_close = pcm2file_enc_close_handler,
};

static void pcm2file_enc_w_evt(void *hdl, int evt, int parm)
{
    struct pcm2file_enc_hdl *enc = hdl;
    printf("evt: %d ", evt);
    if (evt == ENC_WRITE_FILE_EVT_WRITE_ERR) {
        enc->enc_err = 1;
        pcm2file_wfile_resume(enc);
        pcm2file_enc_resume(enc);
        audio_encoder_stop(&enc->encoder);
    } else if (evt == ENC_WRITE_FILE_EVT_FILE_CLOSE) {
        printf("sclust: %d ", parm);
    }
}

static int pcm2file_enc_w_get(void *hdl, s16 **frame, u16 frame_len)
{
    int rlen;
    struct pcm2file_enc_hdl *enc = hdl;
    os_sem_set(&enc->sem_wfile, 0);
    /* printf("r:%d", frame_len); */
    do {
        rlen = cbuf_read(&enc->out_file_cbuf, enc->out_file_frame, frame_len);
        if (rlen == frame_len) {
            break;
        }

        if (!enc->status) {
            rlen = cbuf_get_data_len(&enc->out_file_cbuf);
            rlen = cbuf_read(&enc->out_file_cbuf, enc->out_file_frame, rlen);
            break;
        }

        if (enc->enc_err) {
            return 0;
        }
        pcm2file_enc_resume(enc);
        os_sem_pend(&enc->sem_wfile, 2);
    } while (1);


    *frame = enc->out_file_frame;
    return rlen;
}
static void pcm2file_enc_w_put(void *hdl, s16 *frame)
{
#if TCFG_NOR_FS_ENABLE || FLASH_INSIDE_REC_ENABLE
    return ;
#endif
    struct pcm2file_enc_hdl *enc = (struct pcm2file_enc_hdl *)hdl;
    static u32 cur_sys_time = 0, last_sys_time = 0;

    extern u32 timer_get_ms(void);
    cur_sys_time = timer_get_ms();
    if (cur_sys_time - last_sys_time >= UPDATA_WAV_HEAD_TIME) {
        updata_wav_head_info(&(enc->encoder), enc);
        last_sys_time = cur_sys_time;
    }
}

const struct audio_enc_write_input pcm2file_enc_w_input = {
    .get = pcm2file_enc_w_get,
    .put = pcm2file_enc_w_put,
};

static int enc_wfile_set_head(void *hdl, char **head)
{
    struct pcm2file_enc_hdl *enc = hdl;
    /* struct enc_write_test *tst = hdl; */
    *head = enc->file_head_frame;
    return enc->file_head_len;
}

void updata_wav_head_info(struct audio_encoder *encoder, void *priv)
{
    int len;
    char *head;
    int cur_write_pos;
    struct audio_fmt *enc_fmt;
    struct pcm2file_enc_hdl *enc = (struct pcm2file_enc_hdl *)priv;

    extern int audio_encoder_get_fmt(struct audio_encoder * enc, struct audio_fmt **fmt);
    audio_encoder_get_fmt(encoder, &enc_fmt);
    if ((enc_fmt->coding_type != AUDIO_CODING_WAV) && (enc_fmt->coding_type != AUDIO_CODING_G726)) {
        return;
    }

    extern void *get_wfil_hdl(void *enc_whdl);
    extern FILE *get_wfil_file(void *enc_whdl);
    cur_write_pos = fpos(get_wfil_file(enc->whdl));
    pcm2file_enc_get_head_info(encoder);
    len = enc_wfile_set_head(get_wfil_hdl(enc->whdl), &head);
    if (len) {
        fseek(get_wfil_file(enc->whdl), 0, SEEK_SET);
        fwrite(get_wfil_file(enc->whdl), head, len);
    }
    fseek(get_wfil_file(enc->whdl), cur_write_pos, SEEK_SET);
}

void *pcm2file_enc_open(struct audio_fmt *pfmt, struct storage_dev *dev)
{
    int err;
    struct pcm2file_enc_hdl *pcm2file = NULL;

    printf("pcm2file_enc_open: %d\n", pfmt->coding_type);
    if (pfmt->coding_type != AUDIO_CODING_MP3 && pfmt->coding_type != AUDIO_CODING_WAV && pfmt->coding_type != AUDIO_CODING_G726) {
        return NULL;
    }
    if (!encode_task) {
        encode_task = zalloc(sizeof(*encode_task));
        audio_encoder_task_create(encode_task, "audio_enc");
    }
    enc_run_cnt++;
    printf("pcm2file len:%d ", sizeof(*pcm2file));
    pcm2file = zalloc(sizeof(*pcm2file));
    if (!pcm2file) {
        return NULL;
    }

    if (pfmt->coding_type == AUDIO_CODING_MP3) {
        output_frame = zalloc(MP3_OUTPUT_FRAME_LEN);
        if (!output_frame) {
            return NULL;
        }
    } else {
        output_frame = zalloc(WAV_OUTPUT_FRAME_LEN);
        if (!output_frame) {
            return NULL;
        }
    }

    os_sem_create(&pcm2file->sem_wfile, 0);
    cbuf_init(&pcm2file->out_file_cbuf, pcm2file->out_file_buf, PCM_ENC2FILE_FILE_LEN);
    if (pfmt->coding_type == AUDIO_CODING_G726) {
        pcm2file->whdl = enc_write_file_open(dev, "/JL_REC", "AC690000.wav");
    } else if (pfmt->coding_type == AUDIO_CODING_MP3) {
        pcm2file->whdl = enc_write_file_open(dev, "/JL_REC", "AC690000.MP3");
    } else {
        pcm2file->whdl = enc_write_file_open(dev, "/JL_REC", "AC690000.WAV");
    }
    if (!pcm2file->whdl) {
        free(pcm2file);
        pcm2file = NULL;
        return NULL;
    }

    enc_write_file_set_evt_handler(pcm2file->whdl, pcm2file_enc_w_evt, pcm2file);
    enc_write_file_set_input(pcm2file->whdl, &pcm2file_enc_w_input, pcm2file, sizeof(pcm2file->out_file_frame));
    if ((pfmt->coding_type == AUDIO_CODING_WAV) || (pfmt->coding_type == AUDIO_CODING_G726)) {
        pcm2file->file_head_len = WAV_FILE_HEAD_LEN;
        enc_write_file_set_head_handler(pcm2file->whdl, enc_wfile_set_head, pcm2file);
    }
    /* enc_write_file_set_limit(pcm2file->whdl, 123, 5000); */

    cbuf_init(&pcm2file->pcm_cbuf, pcm2file->pcm_buf, PCM_ENC2FILE_PCM_LEN);
#if RECORD_MIX_ENABLE
    cbuf_init(&pcm2file->mix_pcm_cbuf, pcm2file->mix_pcm_buf, PCM_ENC2FILE_PCM_LEN);
#endif
    audio_encoder_open(&pcm2file->encoder, &pcm2file_enc_input, encode_task);
    audio_encoder_set_handler(&pcm2file->encoder, &pcm2file_enc_handler);
    audio_encoder_set_fmt(&pcm2file->encoder, pfmt);
    /* audio_encoder_set_output_buffs(&pcm2file->encoder, pcm2file->output_frame, */
    /*                                sizeof(pcm2file->output_frame), 1); */
    if (pfmt->coding_type == AUDIO_CODING_MP3) {
        audio_encoder_set_output_buffs(&pcm2file->encoder, output_frame, MP3_OUTPUT_FRAME_LEN, 1);
    } else {
        audio_encoder_set_output_buffs(&pcm2file->encoder, output_frame, WAV_OUTPUT_FRAME_LEN, 1);
    }

    printf("sample_rate: %d\n", pfmt->sample_rate);

    return pcm2file;
}

void pcm2file_enc_write_file_set_limit(void *hdl, u32 cut_size, u32 limit_size)
{
    struct pcm2file_enc_hdl *pcm2file = hdl;
    enc_write_file_set_limit(pcm2file->whdl, cut_size, limit_size);
}

void pcm2file_enc_set_evt_handler(void *hdl, void (*handler)(struct audio_encoder *, int, int *), u32 maigc)
{
    struct pcm2file_enc_hdl *pcm2file = hdl;
    audio_encoder_set_event_handler(&pcm2file->encoder, handler, maigc);
}

void pcm2file_enc_start(void *hdl)
{
    struct pcm2file_enc_hdl *pcm2file = hdl;

    pcm2file->status = 1;

    audio_encoder_start(&pcm2file->encoder);

    enc_write_file_start(pcm2file->whdl);
}


void pcm2file_enc_close(void *hdl)
{
    struct pcm2file_enc_hdl *pcm2file = hdl;
    if (!pcm2file) {
        return;
    }
    pcm2file->status = 0;
    while (pcm2file->encoding) {
        os_time_dly(2);
    }

    audio_encoder_close(&pcm2file->encoder);
    enc_write_file_stop(pcm2file->whdl, 1000);
    enc_write_file_close(pcm2file->whdl);

    printf("pcm2file_enc_close, lost:%d ", pcm2file->lost);

#if PCM2FILE_ENC_BUF_COUNT
    printf("pcm_buf_max:%d,%d/100; out_file_max:%d,%d/100 ",
           pcm2file->pcm_buf_max, pcm2file->pcm_buf_max * 100 / PCM_ENC2FILE_PCM_LEN,
           pcm2file->out_file_max, pcm2file->out_file_max * 100 / PCM_ENC2FILE_FILE_LEN
          );
#endif

    free(pcm2file);
    free(output_frame);
    output_frame = NULL;

    enc_run_cnt--;
    if (enc_run_cnt == 0 && encode_task) {
        audio_encoder_task_del(encode_task);
        free(encode_task);
        encode_task = NULL;
    }
}

int pcm2file_enc_is_work(void *hdl)
{
    struct pcm2file_enc_hdl *enc = hdl;
    if (!enc || !enc->status || enc->enc_err) {
        return false;
    }
    return true;
}

int get_pcm2file_enc_file_len(void *hdl)
{
    struct pcm2file_enc_hdl *pcm2file = hdl;
    return get_enc_file_len(pcm2file->whdl);
}

struct audio_encoder *get_pcm2file_encoder_hdl(void *hdl)
{
    struct pcm2file_enc_hdl *enc = hdl;
    return &(enc->encoder);
}

#endif






