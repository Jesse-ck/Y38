/*
 ****************************************************************
 *File : audio_linein.c
 *Note :
 *
 ****************************************************************
 */

/***********************linein pcm enc******************************/

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

#if ((defined(TCFG_LINEIN_ENABLE) && (TCFG_LINEIN_ENABLE))|| (TCFG_FM_ENABLE))//外部收音走linein

#if (defined(TCFG_LINEIN_REC_EN) && (TCFG_LINEIN_REC_EN))

static u32 linein_enc_magic = 0;

#endif

#if (defined(TCFG_PCM2TWS_SBC_ENABLE) && (TCFG_PCM2TWS_SBC_ENABLE))
#define LINEIN_DEC_PCM_ENC_TYPE		AUDIO_CODING_SBC
#define LINEIN_DEC_PCM_ENC_CHANNEL	AUDIO_CH_LR
#else
#define LINEIN_DEC_PCM_ENC_TYPE		AUDIO_CODING_MP3
#define LINEIN_DEC_PCM_ENC_CHANNEL	AUDIO_CH_LR
#endif

/***********************linein dec ******************************/

#define LINEIN_RATE_MAX_STEP       50
#define LINEIN_RATE_INC_STEP       5
#define LINEIN_RATE_DEC_STEP       5

#define LINEIN_EQ_SUPPORT_ASYNC		1

#ifndef CONFIG_EQ_SUPPORT_ASYNC
#undef LINEIN_EQ_SUPPORT_ASYNC
#define LINEIN_EQ_SUPPORT_ASYNC		0
#endif


#if LINEIN_EQ_SUPPORT_ASYNC && TCFG_LINEIN_MODE_EQ_ENABLE
#define LINEIN_EQ_SUPPORT_32BIT		1
#else
#define LINEIN_EQ_SUPPORT_32BIT		0
#endif

struct linein_dec_hdl {
    struct audio_decoder decoder;
    struct audio_res_wait wait;
    struct audio_mixer_ch mix_ch;
    u32 coding_type;
    u16 sample_rate;
    u8 source;
    u8 channel;
    u8 start;
    u16 src_out_sr;

    int begin_size;
    int top_size;
    int bottom_size;
    u16 audio_new_rate;
    u16 audio_max_speed;
    u16 audio_min_speed;
    u8 sync_start;
    struct audio_src_handle *src_sync;

    void *linein;

    u32 ch_num : 4;
    u32 dec_mix : 1;
    u32 remain : 1;
    u32 dec_no_out_sound : 1;	// 解码不直接输出声音（用于TWS转发）

    u32 eq_remain : 1;
#if TCFG_LINEIN_MODE_EQ_ENABLE
    struct audio_eq *p_eq;
#endif
#if TCFG_LINEIN_MODE_DRC_ENABLE
    struct audio_drc *p_drc;
#endif
    u32 quad_pcm_point_offset;
    struct user_audio_parm *user_hdl;

#if  LINEIN_EQ_SUPPORT_32BIT
	s16 *eq_out_buf;
	int eq_out_buf_len;
	int eq_out_points;
	int eq_out_total;
#endif


};



void linein_eq_drc_open(struct linein_dec_hdl *dec, struct audio_fmt *fmt);
void linein_eq_drc_close(struct linein_dec_hdl *dec);

struct linein_dec_hdl *linein_dec = NULL;
static u8 linein_dec_maigc = 0;

#if (defined(USER_DIGITAL_VOLUME_ADJUST_ENABLE) && (USER_DIGITAL_VOLUME_ADJUST_ENABLE != 0))
/*
 *AUX播歌数字音量调节
 * */
void linein_user_digital_volume_set(u8 vol)
{
    if (linein_dec  && linein_dec->user_hdl && linein_dec->user_hdl->dvol_hdl) {
        user_audio_digital_volume_set(linein_dec->user_hdl->dvol_hdl, vol);
    }
}

u8 linein_user_audio_digital_volume_get()
{
    if (!linein_dec) {
        return 0;
    }
    if (!linein_dec->user_hdl) {
        return 0;
    }
    if (!linein_dec->user_hdl->dvol_hdl) {
        return 0;
    }
    return user_audio_digital_volume_get(linein_dec->user_hdl->dvol_hdl);
}

/*
 *user_vol_max:音量级数
 *user_vol_tab:自定义音量表,自定义表长user_vol_max+1
 *注意：如需自定义音量表，须在volume_set前调用 ,否则会在下次volume_set时生效
 */

void linein_user_digital_volume_tab_set(u16 *user_vol_tab, u8 user_vol_max)
{
    if (linein_dec && linein_dec->user_hdl->dvol_hdl) {
        user_audio_digital_set_volume_tab(linein_dec->user_hdl->dvol_hdl, user_vol_tab, user_vol_max);
    }
}


#endif

void linein_dec_relaese()
{
    audio_decoder_task_del_wait(&decode_task, &linein_dec->wait);
    if (linein_dec) {
        clock_remove(linein_dec->coding_type);
        local_irq_disable();
        free(linein_dec);
        linein_dec = NULL;
        local_irq_enable();
    }
}

static int linein_fread(struct audio_decoder *decoder, void *buf, u32 len)
{
    int rlen = 0;
    struct linein_dec_hdl *dec = container_of(decoder, struct linein_dec_hdl, decoder);
    if (!dec->sync_start) {
        if (linein_sample_size(dec->linein) < dec->begin_size) {
            return -1;
        }
        dec->sync_start = 1;
    }
    u8 ch_o = dec->ch_num;//audio_output_channel_num();
    /* printf("ch_o:%d, ch:%d ", ch_o, dec->channel); */
    if ((ch_o == 2) && (dec->channel == 1)) {
        rlen = linein_sample_read(dec->linein, (void *)((int)buf + (len / 2)), len / 2);
        audio_pcm_mono_to_dual(buf, (void *)((int)buf + (len / 2)), rlen / 2);
        rlen <<= 1;
    } else if ((ch_o == 1) && (dec->channel == 2)) {
        rlen = linein_sample_read(dec->linein, buf, len);
        audio_decoder_dual_switch(AUDIO_CH_DIFF, 1, buf, rlen);
        rlen >>= 1;
    } else {
        rlen = linein_sample_read(dec->linein, buf, len);
    }
    if (rlen == 0) {
        /* memset(buf, 0, len); */
        /* rlen = len; */
        return -1;
    }
    /* printf("fread len %d %d\n",len,rlen); */
    return rlen;
}
static int linein_flen(struct audio_decoder *decoder)
{
    struct linein_dec_hdl *dec = container_of(decoder, struct linein_dec_hdl, decoder);
    return 0xffffffff;
}

static const struct audio_dec_input linein_input = {
    .coding_type = AUDIO_CODING_PCM,
    .data_type   = AUDIO_INPUT_FILE,
    .ops = {
        .file = {
#if VFS_ENABLE == 0
#undef fread
#undef fseek
#undef flen
#endif
            .fread = linein_fread,
            /* .fseek = file_fseek, */
            /* .flen  = linein_flen, */
        }
    }
};


void linein_dec_close();

static void linein_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        if ((u8)argv[1] != (u8)(linein_dec_maigc - 1)) {
            log_i("maigc err, %s\n", __FUNCTION__);
            break;
        }
        linein_dec_close();
        //audio_decoder_resume_all(&decode_task);
        break;
    }
}

static int linein_src_output_handler(void *priv, void *buf, int len)
{
    int wlen = 0;
    int rlen = len;
    struct linein_dec_hdl *dec = (struct linein_dec_hdl *) priv;
    s16 *data = (s16 *)buf;

    if (!linein_dec || (!linein_dec->start)) {
        /* putchar('O'); */
        return 0;
    }

#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))
    if (dec->dec_no_out_sound) {
        return pcm2tws_enc_output(NULL, data, len);
    }
#endif

	wlen = audio_mixer_ch_write(&dec->mix_ch, data, len);
	return wlen;
}

static int linein_dec_output_handler(struct audio_decoder *decoder, s16 *data, int len, void *priv)
{
    char err = 0;
    int wlen = 0;
    int rlen = len;
    struct linein_dec_hdl *dec = container_of(decoder, struct linein_dec_hdl, decoder);

    if (!dec->remain) {
#if (defined(TCFG_LINEIN_REC_EN) && (TCFG_LINEIN_REC_EN))
        pcm2file_enc_write_pcm(dec->enc, data, len);
#endif

#if ((defined(DEC_OUT_OTHER_DATA) && (DEC_OUT_OTHER_DATA)))
        other_audio_dec_output(decoder, data, len, dec->ch_num, dec->sample_rate);
#endif

#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
        if (dec->user_hdl) {
            u8 ch_num = dec->ch_num;
            user_audio_process_handler_run(dec->user_hdl, data, len, ch_num);
        }
#endif
    }

#if LINEIN_EQ_SUPPORT_ASYNC
#if TCFG_LINEIN_MODE_EQ_ENABLE
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
#if TCFG_LINEIN_MODE_EQ_ENABLE
        if (dec->p_eq) {
            audio_eq_run(dec->p_eq, data, len);
        }
#endif
#if TCFG_LINEIN_MODE_DRC_ENABLE
        if (dec->p_drc) {
            audio_drc_run(dec->p_drc, data, len);
        }
#endif
    }


    do {
        if (dec->src_sync) {
            wlen = audio_src_resample_write(dec->src_sync, data, rlen);
#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))
        } else if (dec->dec_no_out_sound) {
            wlen = pcm2tws_enc_output(NULL, data, rlen);
#endif
        } else {
            wlen = audio_mixer_ch_write(&dec->mix_ch, data, rlen);
        }
        if (!wlen) {
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

    if (rlen == 0) {
        dec->remain = 0;
    } else {
        dec->remain = 1;
    }

    return len - rlen;
}

static int linein_audio_stream_sync(struct linein_dec_hdl *dec, int data_size)
{
    if (!dec->src_sync) {
        return 0;
    }
    u16 sr = dec->audio_new_rate;

    if (data_size < dec->bottom_size) {
        dec->audio_new_rate += LINEIN_RATE_INC_STEP;
        /*printf("rate inc\n");*/
    }

    if (data_size > dec->top_size) {
        dec->audio_new_rate -= LINEIN_RATE_DEC_STEP;
        /*printf("rate dec : %d\n", __this->audio_new_rate);*/
    }

    if (dec->audio_new_rate < dec->audio_min_speed) {
        dec->audio_new_rate = dec->audio_min_speed;
    } else if (dec->audio_new_rate > dec->audio_max_speed) {
        dec->audio_new_rate = dec->audio_max_speed;
    }

    if (sr != dec->audio_new_rate) {
        audio_hw_src_set_rate(dec->src_sync, dec->sample_rate, dec->audio_new_rate);
    }

    return 0;
}

static int linein_dec_probe_handler(struct audio_decoder *decoder)
{
    struct linein_dec_hdl *dec = container_of(decoder, struct linein_dec_hdl, decoder);
    int err = 0;

#if 0
    if (!dec->sync_start) {
        if (linein_sample_size(dec->linein) > dec->begin_size) {
            dec->sync_start = 1;
            return 0;
        } else {
            /* os_time_dly(2); */
            audio_decoder_suspend(&dec->decoder, 0);
            /* audio_decoder_resume(&dec->decoder); */
            /* return 0;//临时 */
            return -EINVAL;
        }
    }
#else
    if (!dec->sync_start) {
        return 0;
    }
#endif

#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))
    if (dec->dec_no_out_sound) {
        // localtws同步有微调处理
        return 0;
    }
#endif

    linein_audio_stream_sync(dec, linein_sample_size(dec->linein));
    return err;
}

static const struct audio_dec_handler linein_dec_handler = {
    .dec_probe = linein_dec_probe_handler,
    .dec_output = linein_dec_output_handler,
    /* .dec_post   = linein_dec_post_handler, */
};

static int linein_audio_sync_init(struct linein_dec_hdl *dec)
{
    // printf("total:%d, size:%d \n", linein_sample_total(dec->linein), linein_sample_size(dec->linein));
    dec->sync_start = 0;
    dec->begin_size = linein_sample_total(dec->linein) * 60 / 100;
    dec->top_size = linein_sample_total(dec->linein) * 70 / 100;
    dec->bottom_size = linein_sample_total(dec->linein) * 40 / 100;

    u16 out_sr = dec->src_out_sr;//audio_output_rate(dec->sample_rate);
    printf("out_sr:%d, dsr:%d, dch:%d \n", out_sr, dec->sample_rate, dec->ch_num);
    dec->audio_new_rate = out_sr;
    dec->audio_max_speed = out_sr + LINEIN_RATE_MAX_STEP;
    dec->audio_min_speed = out_sr - LINEIN_RATE_MAX_STEP;

    /* #if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE)) */
    /* if (dec->dec_no_out_sound) { */
    if (dec->audio_new_rate == dec->sample_rate) {
        return -ENODEV;
    }
    /* } */
    /* #endif */

    dec->src_sync = zalloc(sizeof(struct audio_src_handle));
    if (!dec->src_sync) {
        return -ENODEV;
    }
#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_FRONT_LR_REAR_LR)
    u8 ch_num = 4;
#else
    u8 ch_num = dec->ch_num;
#endif

    audio_hw_src_open(dec->src_sync, ch_num, SRC_TYPE_AUDIO_SYNC);

    audio_hw_src_set_rate(dec->src_sync, dec->sample_rate, dec->audio_new_rate);

    audio_src_set_output_handler(dec->src_sync, dec, linein_src_output_handler);
    return 0;
}
#if LINEIN_EQ_SUPPORT_32BIT
void linein_eq_32bit_out(struct linein_dec_hdl *dec)
{
    int wlen = 0;

	if (dec->src_sync) {
		wlen = audio_src_resample_write(dec->src_sync, &dec->eq_out_buf[dec->eq_out_points], (dec->eq_out_total-dec->eq_out_points)*2);
	} else{
		wlen = audio_mixer_ch_write(&dec->mix_ch, &dec->eq_out_buf[dec->eq_out_points], (dec->eq_out_total-dec->eq_out_points)*2);
	}
	dec->eq_out_points += wlen/2;
}
#endif /*LINEIN_EQ_SUPPORT_32BIT*/

static int linein_eq_output(void *priv, s16 *data, u32 len)
{
    int wlen = 0;
    int rlen = len;
    struct linein_dec_hdl *dec = priv;
#if LINEIN_EQ_SUPPORT_ASYNC


    if (!dec->eq_remain) {
#if LINEIN_EQ_SUPPORT_32BIT
		if (dec->eq_out_buf && (dec->eq_out_points < dec->eq_out_total)) {
			linein_eq_32bit_out(dec);
			if (dec->eq_out_points < dec->eq_out_total) {
				return 0;
			}
		}
#endif /*LINEIN_EQ_SUPPORT_32BIT*/

#if TCFG_LINEIN_MODE_DRC_ENABLE
        if (dec->p_drc) {
            audio_drc_run(dec->p_drc, data, len);
        }
#endif
    }

#if LINEIN_EQ_SUPPORT_32BIT
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

		linein_eq_32bit_out(dec);
		return len;
#endif /*LINEIN_EQ_SUPPORT_32BIT*/



        if (dec->src_sync) {
            wlen = audio_src_resample_write(dec->src_sync, data, len);
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

int linein_dec_start()
{
    int err;
    struct audio_fmt f;
    struct linein_dec_hdl *dec = linein_dec;

    if (!linein_dec) {
        return -EINVAL;
    }

    err = audio_decoder_open(&dec->decoder, &linein_input, &decode_task);
    printf("\n--func=%s\n", __FUNCTION__);
    if (err) {
        goto __err1;
    }
    dec->ch_num = audio_output_channel_num();//AUDIO_CH_MAX;
    audio_decoder_set_handler(&dec->decoder, &linein_dec_handler);
    audio_decoder_set_event_handler(&dec->decoder, linein_dec_event_handler, linein_dec_maigc++);

    f.coding_type = AUDIO_CODING_PCM;
    f.sample_rate = dec->sample_rate;
    f.channel = dec->channel;

    err = audio_decoder_set_fmt(&dec->decoder, &f);
    if (err) {
        goto __err2;
    }

#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))
    dec->dec_no_out_sound = 0;
    if (dec2tws_check_enable() == true) {
        audio_decoder_set_output_channel(&dec->decoder, LINEIN_DEC_PCM_ENC_CHANNEL);
        dec->ch_num = AUDIO_CH_IS_MONO(LINEIN_DEC_PCM_ENC_CHANNEL) ? 1 : 2;;
        struct audio_fmt enc_fmt = {0};
        enc_fmt.coding_type = LINEIN_DEC_PCM_ENC_TYPE;
        enc_fmt.bit_rate = 128;
        enc_fmt.channel = dec->ch_num;
        enc_fmt.sample_rate = audio_output_rate(dec->sample_rate);//dec->decoder.fmt.sample_rate;
        enc_fmt.sample_rate = app_audio_output_samplerate_select(enc_fmt.sample_rate, 1);
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
                dec->dec_no_out_sound = 1;
                pcm2tws_enc_set_resume_handler(linein_dec_resume);
                audio_decoder_task_del_wait(&decode_task, &dec->wait);
                local_tws_start(&enc_fmt);
                audio_decoder_set_run_max(&dec->decoder, 20);
                dec->src_out_sr = enc_fmt.sample_rate;
                goto __dec_start;
            } else {
                pcm2tws_enc_close();
            }
        }
    }
#endif

    audio_mixer_ch_open(&dec->mix_ch, &mixer);
    audio_mixer_ch_set_sample_rate(&dec->mix_ch, audio_output_rate(f.sample_rate));

    dec->src_out_sr = audio_output_rate(f.sample_rate);

    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);

    linein_eq_drc_open(dec, &f);

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



__dec_start:

    dec->linein = linein_sample_open(dec->source, dec->sample_rate);

    linein_sample_set_resume_handler(dec->linein, linein_dec_resume);

    linein_audio_sync_init(dec);

    err = audio_decoder_start(&dec->decoder);
    if (err) {
        goto __err3;
    }
    clock_set_cur();
    dec->start = 1;
    return 0;
__err3:
    if (dec->linein) {
        linein_sample_close(dec->linein);
        dec->linein = NULL;
    }
    if (dec->src_sync) {
        audio_hw_resample_close(dec->src_sync);
        dec->src_sync = NULL;
    }
    if (!dec->dec_no_out_sound) {
        audio_mixer_ch_close(&dec->mix_ch);
    }
__err2:
    audio_decoder_close(&dec->decoder);
    linein_eq_drc_close(dec);
__err1:
#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
    if (dec->user_hdl) {
        user_audio_process_close(dec->user_hdl);
        dec->user_hdl = NULL;
    }
#endif

    linein_dec_relaese();
    return err;
}

static void __linein_dec_close(void)
{
    if (linein_dec && linein_dec->start) {
        linein_dec->start = 0;

#if (defined(TCFG_LINEIN_REC_EN) && (TCFG_LINEIN_REC_EN))
        linein_pcm_enc_stop();
#endif
        audio_decoder_close(&linein_dec->decoder);
        linein_sample_close(linein_dec->linein);
        linein_dec->linein = NULL;

        if (linein_dec->src_sync) {
            audio_hw_resample_close(linein_dec->src_sync);
            linein_dec->src_sync = NULL;
        }
        if (!linein_dec->dec_no_out_sound) {
            audio_mixer_ch_close(&linein_dec->mix_ch);
        }
    }
#if (defined(TCFG_PCM_ENC2TWS_ENABLE) && (TCFG_PCM_ENC2TWS_ENABLE))
    if (linein_dec->dec_no_out_sound) {
        linein_dec->dec_no_out_sound = 0;
        pcm2tws_enc_close();
        encfile_tws_dec_close();
        local_tws_stop();
    }
#endif


    linein_eq_drc_close(linein_dec);

#if (defined(USER_AUDIO_PROCESS_ENABLE) && (USER_AUDIO_PROCESS_ENABLE != 0))
    if (linein_dec->user_hdl) {
        user_audio_process_close(linein_dec->user_hdl);
        linein_dec->user_hdl = NULL;
    }

#endif
}

static int linein_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;
    log_i("linein_wait_res_handler, event:%d\n", event);
    if (event == AUDIO_RES_GET) {
        err = linein_dec_start();
    } else if (event == AUDIO_RES_PUT) {
        __linein_dec_close();
    }

    return err;
}

int linein_dec_open(u8 source, u32 sample_rate)
{
    int err;
    struct linein_dec_hdl *dec;
    dec = zalloc(sizeof(*dec));
    if (!dec) {
        return -ENOMEM;
    }
    linein_dec = dec;
    switch (source) {
    case BIT(0):
    case BIT(1):
        dec->channel = 1;
        break;
    case BIT(2):
    case BIT(3):
        dec->channel = 1;
        break;
    default:
        log_e("Linein can only select a single channel\n");
        ASSERT(0, "err\n");
        break;
    }
    dec->source = source;


    dec->sample_rate = sample_rate;

    dec->coding_type = AUDIO_CODING_PCM;
    dec->wait.priority = 2;
    dec->wait.preemption = 1;
    dec->wait.handler = linein_wait_res_handler;
    clock_add(dec->coding_type);

    err = audio_decoder_task_add_wait(&decode_task, &dec->wait);
    return err;
}

void linein_dec_close(void)
{
    if (!linein_dec) {
        return;
    }

    __linein_dec_close();
    linein_dec_relaese();
    clock_set_cur();
    puts("linein dec close \n\n ");
}

int linein_dec_restart(int magic)
{
    if ((!linein_dec) || (magic != linein_dec_maigc)) {
        return -1;
    }
    u8 source = linein_dec->source;
    u32 sample_rate = linein_dec->sample_rate;
    linein_dec_close();
    int err = linein_dec_open(source, sample_rate);
    return err;
}

int linein_dec_push_restart(void)
{
    if (!linein_dec) {
        return false;
    }
    int argv[3];
    argv[0] = (int)linein_dec_restart;
    argv[1] = 1;
    argv[2] = (int)linein_dec_maigc;
    os_taskq_post_type(os_current_task(), Q_CALLBACK, ARRAY_SIZE(argv), argv);
    return true;
}

void linein_dec_resume(void)
{
    if (linein_dec && (linein_dec->start)) {
        audio_decoder_resume(&linein_dec->decoder);
    }
}
int linein_dec_no_out_sound(void)
{
    if (linein_dec && linein_dec->dec_no_out_sound) {
        return true;
    }
    return false;
}

/***********************linein pcm enc******************************/
#if (defined(TCFG_LINEIN_REC_EN) && (TCFG_LINEIN_REC_EN))

static void linein_pcm_enc_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    printf("linein_pcm_enc_event_handler, argv[]:%d, %d ", argv[0], argv[1]);
    linein_pcm_enc_stop();
    /* switch (argv[0]) { */
    /* case AUDIO_DEC_EVENT_END: */
    /* linein_dec_close(); */
    /* break; */
    /* } */
}

void linein_pcm_enc_stop(void)
{
    void *ptr;
    if (linein_dec && linein_dec->enc) {
        ptr = linein_dec->enc;
        linein_dec->enc = NULL;
        linein_enc_magic++;
        pcm2file_enc_close(ptr);
    }
}
int linein_pcm_enc_start(void)
{
    void *ptr;
    if (!linein_dec) {
        return -1;
    }
    linein_pcm_enc_stop();
    struct audio_fmt fmt = {0};
    fmt.coding_type = AUDIO_CODING_MP3;
    /* fmt.coding_type = AUDIO_CODING_WAV; */
    fmt.bit_rate = 128;
    fmt.channel = linein_dec->fmt.channel;
    fmt.sample_rate = linein_dec->fmt.sample_rate;
    linein_dec->enc = pcm2file_enc_open(&fmt, storage_dev_last());
    if (!linein_dec->enc) {
        return -1;
    }
    pcm2file_enc_set_evt_handler(linein_dec->enc, linein_pcm_enc_event_handler, linein_enc_magic);
    pcm2file_enc_start(linein_dec->enc);
    return 0;
}
bool linein_pcm_enc_check()
{
    if (linein_dec && linein_dec->enc) {
        return true;
    }
    return false;
}
#endif


void linein_eq_drc_open(struct linein_dec_hdl *dec, struct audio_fmt *fmt)
{
    if (!dec) {
        return;
    }
#if TCFG_LINEIN_MODE_EQ_ENABLE
    dec->p_eq = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
    if (dec->p_eq) {
        dec->p_eq->eq_ch = (struct hw_eq_ch *)((int)dec->p_eq + sizeof(struct audio_eq));
        struct audio_eq_param linein_eq_param = {0};
        linein_eq_param.channels = dec->ch_num;
        linein_eq_param.online_en = 1;
        linein_eq_param.mode_en = 1;
        linein_eq_param.remain_en = 1;
        linein_eq_param.max_nsection = EQ_SECTION_MAX;
        linein_eq_param.cb = eq_get_filter_info;
#if LINEIN_EQ_SUPPORT_ASYNC
        linein_eq_param.no_wait = 1;//异步
#endif
        linein_eq_param.eq_name = 0;
        audio_eq_open(dec->p_eq, &linein_eq_param);
        audio_eq_set_samplerate(dec->p_eq, fmt->sample_rate);

        audio_eq_set_output_handle(dec->p_eq, linein_eq_output, dec);

#if LINEIN_EQ_SUPPORT_32BIT
		audio_eq_set_info(dec->p_eq, linein_eq_param.channels, 1);
#endif
        audio_eq_start(dec->p_eq);
    }
#endif

#if TCFG_LINEIN_MODE_DRC_ENABLE
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
#if LINEIN_EQ_SUPPORT_32BIT
		audio_drc_set_32bit_mode(dec->p_drc, 1);
#endif

        audio_drc_set_output_handle(dec->p_drc, NULL, NULL);
        audio_drc_start(dec->p_drc);
    }

#endif


}

void linein_eq_drc_close(struct linein_dec_hdl *dec)
{
    if (!dec) {
        return;
    }
#if TCFG_LINEIN_MODE_EQ_ENABLE
    if (linein_dec->p_eq) {
        audio_eq_close(linein_dec->p_eq);
        free(linein_dec->p_eq);
        linein_dec->p_eq = NULL;
    }

#endif
#if TCFG_LINEIN_MODE_DRC_ENABLE
    if (linein_dec->p_drc) {
        audio_drc_close(linein_dec->p_drc);
        free(linein_dec->p_drc);
        linein_dec->p_drc = NULL;
    }
#endif

#if LINEIN_EQ_SUPPORT_32BIT
    if (dec->eq_out_buf) {
        free(dec->eq_out_buf);
        dec->eq_out_buf = NULL;
    }
#endif


}



#endif
