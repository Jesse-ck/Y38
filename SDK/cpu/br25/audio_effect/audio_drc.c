#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_online_cfg.h"
#include "audio_drc.h"
#include "clock_cfg.h"

#define LOG_TAG     "[APP-DRC]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"

#if (TCFG_DRC_ENABLE == 1)

static void drc_updata(struct audio_drc *drc)
{
    struct audio_drc_filter_info info = {0};

    drc->cb(&info);
    memcpy(&drc->sw_drc[0], info.pch, sizeof(struct drc_ch));
    if (drc->stero_div) {
        memcpy(&drc->sw_drc[1], info.R_pch, sizeof(struct drc_ch));
    } else {
        memcpy(&drc->sw_drc[1], info.pch, sizeof(struct drc_ch));
    }
    if (drc->hdl) {
        audio_sw_drc_close(drc->hdl);
        drc->hdl = NULL;
    }
    drc->hdl = audio_sw_drc_open(&drc->sw_drc, drc->sr, drc->channels, drc->drc_name, drc->run32bit);
}

static u32 drc_run(struct audio_drc *drc, void *buf, u32 len)
{
    drc_app_run_check(drc);
    if (drc->updata) {
        drc->updata = 0;
        drc_updata(drc);
    }
	int runlen = len / 2 / drc->channels;
	if (drc->run32bit) {
		runlen /= 2;
	}
    audio_sw_drc_run(drc->hdl, buf, buf, runlen);
    if (drc->output) {
        return drc->output(drc->output_priv, buf, len);
    }
    return len;
}

static u32 drc_run_inOut(struct audio_drc *drc, void *inbuf, void *buf, u32 len, int *ret)
{
    drc_app_run_check(drc);
    if (drc->updata) {
        drc->updata = 0;
        drc_updata(drc);
    }
	int runlen = len / 2 / drc->channels;
	if (drc->run32bit) {
		runlen /= 2;
	}
    int ret_tmp = audio_sw_drc_run(drc->hdl, inbuf, buf, runlen);
    *ret = ret_tmp;
    if (drc->output) {
        return drc->output(drc->output_priv, buf, len);
    }
    return len;
}


/*-----------------------------------------------------------*/

u8 audio_sw_drc_get_xx(void *ch)
{
    struct audio_drc *drc = container_of(ch, struct audio_drc, sw_drc);
    if (drc) {
        return  drc->stero_div;
    }

    return 0;
}

// drc app
int audio_drc_open(struct audio_drc *drc, struct audio_drc_param *param)
{
    if (!drc || !param || !param->cb) {
        return -EPERM;
    }
    memset(drc, 0, sizeof(*drc));
    struct audio_drc_filter_info info = {0};
    drc->channels = param->channels;
    drc->online_en = param->online_en;
    drc->remain_en = param->remain_en;
    drc->sr = 0;
    drc->cb = param->cb;
    drc->drc_name = param->drc_name;
    drc->cb(&info);
    drc->stero_div = param->stero_div;

	clock_add(EQ_DRC_CLK);
    puts("drc_init\n");
    return 0;
}

int audio_drc_start(struct audio_drc *drc)
{
    int err;
    if (!drc || !drc->cb) {
        return -EPERM;
    }
    struct audio_drc_filter_info info = {0};
    err = drc->cb(&info);
    if (err) {
        return err;
    }

    memcpy(&drc->sw_drc[0], info.pch, sizeof(struct drc_ch));
    if (drc->stero_div) {
        memcpy(&drc->sw_drc[1], info.R_pch, sizeof(struct drc_ch));
    } else {
        memcpy(&drc->sw_drc[1], info.pch, sizeof(struct drc_ch));
    }

    return 0;
}

int audio_drc_close(struct audio_drc *drc)
{
    if (drc->hdl) {
        audio_sw_drc_close(drc->hdl);
        drc->hdl = NULL;
    }
    clock_remove(EQ_DRC_CLK);
    return 0;
}

void audio_drc_set_samplerate(struct audio_drc *drc, int sr)
{
    if (!drc) {
        return ;
    }
    if (drc->sr != sr) {
        drc->sr = sr;
        drc->updata = 1;
    }
}

int audio_drc_set_32bit_mode(struct audio_drc *drc, u8 run_32bit)
{
	if (drc) {
		drc->run32bit = run_32bit;
	}
	return 0;
}

void audio_drc_set_output_handle(struct audio_drc *drc, int (*output)(void *priv, void *data, u32 len), void *output_priv)
{
    if (!drc) {
        return ;
    }
    drc->output = output;
    drc->output_priv = output_priv;
}

int audio_drc_run(struct audio_drc *drc, s16 *data, u32 len)
{
    int wlen = len;

    if (drc->remain_en && drc->remain_flag && drc->output) {
        wlen = drc->output(drc->output_priv, data, len);
    } else {
        wlen = drc_run(drc, data, len);
        /* printf("out, wl:%d, l:%d ", wlen, len); */
    }
    drc->remain_flag = len == wlen ? 0 : 1;
    return wlen;
}

int audio_drc_run_inOut(struct audio_drc *drc, s16 *indata, s16 *data, u32 len)
{
    int wlen = len;
    int ret;

    if (drc->remain_en && drc->remain_flag && drc->output) {
        wlen = drc->output(drc->output_priv, data, len);
    } else {
        wlen = drc_run_inOut(drc, indata, data, len, &ret);
        /* printf("out, wl:%d, l:%d ", wlen, len); */
    }
    drc->remain_flag = len == wlen ? 0 : 1;

    return ret;
    /* return wlen; */
}


#endif /*TCFG_DRC_ENABLE*/

