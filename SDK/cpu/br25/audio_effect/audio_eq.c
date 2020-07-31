#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_online_cfg.h"
#include "audio_eq.h"
#include "audio_drc.h"
#include "clock_cfg.h"

#define LOG_TAG     "[APP-EQ]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#include "debug.h"

#ifdef CONFIG_EQ_APP_SEG_ENABLE
#pragma const_seg(	".eq_app_codec_const")
#pragma code_seg(	".eq_app_codec_code")
#endif

#if (TCFG_EQ_ENABLE == 1)

#define AUDIO_EQ_SUPPORT_32BIT		1
const u8 audio_eq_sdk_name[16] 		= "AC696X";

const u8 audio_eq_ver[4] 			= {0, 6, 0, 2};

static struct hw_eq hw_eq_hdl;

#ifdef CONFIG_EQ_SUPPORT_ASYNC
static void audio_eq_async_output(struct audio_eq *eq, u8 toggle)
{
    struct hw_eq_ch *hw_eq = eq->eq_ch;
    struct audio_eq_async *async = &eq->async[toggle];

    if (async->clear) {
        async->clear = 0;
        async->ptr = async->len;
        return ;
    }
    if (eq->output_source) {
        eq->output_source(eq->output_priv, &async->buf_bk[async->ptr], async->len - async->ptr);
    }
    int ret = eq->output(eq->output_priv, &async->buf[async->ptr], async->len - async->ptr);
    async->ptr += ret;
    if (async->ptr >= async->len) {
        /* putchar('1'); */
    } else {
        /* putchar('2'); */
    }
    /* printf("asout, toggle:%d, ptr:%d, len:%d ", toggle, async->ptr, async->len); */
}
#endif

static int audio_eq_probe(struct hw_eq_ch *ch)
{
    return 0;
}
static int audio_eq_output(struct hw_eq_ch *ch, s16 *buf, u16 len)
{
    struct audio_eq *eq = (struct audio_eq *)ch->priv;
    struct hw_eq_ch *hw_eq = eq->eq_ch;
#ifdef CONFIG_EQ_SUPPORT_ASYNC
    if (eq->output && hw_eq->no_wait) {
        int toggle = 0;
        for (; toggle < 2; toggle++) {
            if ((int)buf == (int)eq->async[toggle].buf) {
                break;
            }
        }
        ASSERT(toggle < 2);
        {
            struct audio_eq_async *async = &eq->async[!eq->async_toggle];
            if (async->ptr < async->len) {
                toggle = !eq->async_toggle;
            }
        }
        audio_eq_async_output(eq, toggle);
    }
#endif
    return 0;
}
static int audio_eq_post(struct hw_eq_ch *ch)
{
    return 0;
}

static struct hw_eq_handler audio_eq_handler = {
    .eq_probe 	= audio_eq_probe,
    .eq_output 	= audio_eq_output,
    .eq_post 	= audio_eq_post,
};

/*-----------------------------------------------------------*/


// eq app
int audio_eq_open(struct audio_eq *eq, struct audio_eq_param *param)
{
    struct hw_eq_ch *hw_eq = eq->eq_ch;
    struct audio_eq_filter_info info = {0};
    if (!eq || !param || !param->cb) {
        return -EPERM;
    }
    eq->max_nsection = param->max_nsection;

    if (!param->drc_en) {
#if TCFG_BT_MUSIC_EQ_NUM
        if (eq->max_nsection > TCFG_BT_MUSIC_EQ_NUM) {
            eq->max_nsection = TCFG_BT_MUSIC_EQ_NUM;
        }
#endif
    }
    int *eq_LRmem = zalloc(eq->max_nsection * (sizeof(int) * 2 * 2));
    if (!eq_LRmem) {
        return -EPERM;
    }
    audio_hw_eq_ch_open(hw_eq, &hw_eq_hdl);
    hw_eq->channels = param->channels;
    hw_eq->out_32bit = 0;
    hw_eq->no_wait = param->no_wait;
#ifndef CONFIG_EQ_SUPPORT_ASYNC
    ASSERT(!hw_eq->no_wait);
#endif
    hw_eq->priv = eq;
    hw_eq->eq_handler = &audio_eq_handler;
    hw_eq->eq_LRmem = eq_LRmem;

    eq->online_en = param->online_en;
    eq->mode_en = param->mode_en;
    eq->remain_en = param->remain_en;
    eq->sr = 0;
    eq->cb = param->cb;
    eq->eq_name = param->eq_name;
    if (param->drc_en) {
        info.nsection = param->nsection = param->max_nsection;
    }

#if (defined(TCFG_ONLINE_ENABLE) && (TCFG_ONLINE_ENABLE != 0))
    clock_add(EQ_ONLINE_CLK);
#endif

    if (param->drc_en) {
        clock_add(EQ_DRC_CLK);
    } else {
        clock_add(EQ_CLK);
    }



    eq->cb(eq->sr, &info);
    return 0;
}

int audio_eq_set_info(struct audio_eq *eq, u8 channels, u8 out_32bit)
{
    struct hw_eq_ch *hw_eq = eq->eq_ch;
    audio_hw_eq_ch_set_info(hw_eq, channels, out_32bit);
    return 0;
}
int audio_eq_start(struct audio_eq *eq)
{
    int err;
    if (!eq || !eq->cb) {
        return -EPERM;
    }
    struct hw_eq_ch *hw_eq = eq->eq_ch;
    struct audio_eq_filter_info info = {0};
    err = eq->cb(eq->sr, &info);
    if (err) {
        return err;
    }
    if (info.nsection > eq->max_nsection) {
        info.nsection = eq->max_nsection;
    }
    err = audio_hw_eq_ch_set_coeff(hw_eq, &info);
    if (err) {
        return err;
    }
    eq->start = 1;
    return 0;
}
int audio_eq_close(struct audio_eq *eq)
{
    if (!eq) {
        return -EPERM;
    }
    struct hw_eq_ch *hw_eq = eq->eq_ch;
    audio_hw_eq_ch_close(hw_eq);
    if (hw_eq->eq_LRmem) {
        free(hw_eq->eq_LRmem);
        hw_eq->eq_LRmem = NULL;
    }
#ifdef CONFIG_EQ_SUPPORT_ASYNC
    for (int i = 0; i < 2; i++) {
        if (eq->async[i].buf) {
            free(eq->async[i].buf);
            eq->async[i].buf = NULL;
        }
    }

    for (int i = 0; i < 2; i++) {
        if (eq->async[i].buf_bk) {
            free(eq->async[i].buf_bk);
            eq->async[i].buf_bk = NULL;
        }
    }

#endif

    clock_remove(EQ_CLK);
    clock_remove(EQ_DRC_CLK);
    clock_remove(EQ_ONLINE_CLK);

    eq->start = 0;
    return 0;
}

void audio_eq_set_samplerate(struct audio_eq *eq, int sr)
{
    if (!eq) {
        return ;
    }
    if (eq->sr != sr) {
        eq->sr = sr;
        eq->updata = 1;
    }
}

void audio_eq_set_channel(struct audio_eq *eq, u8 channel)
{
    if (!eq) {
        return ;
    }
    struct hw_eq_ch *hw_eq = eq->eq_ch;
    if (hw_eq->channels != channel) {
        hw_eq->channels = channel;
        eq->updata = 1;
    }
}

void audio_eq_set_output_handle(struct audio_eq *eq, int (*output)(void *priv, void *data, u32 len), void *output_priv)
{
    if (!eq) {
        return ;
    }
    eq->output = output;
    eq->output_priv = output_priv;
}

void audio_eq_set_output_source_handle(struct audio_eq *eq, int (*output)(void *priv, void *data, u32 len), void *output_priv)
{
    if (!eq) {
        return ;
    }
    eq->output_source = output;
    //eq->output_priv = output_priv;
}


static u32 hw_eq_run(struct audio_eq *eq, void *in_buf, u32 len)
{
    int err;
    eq_app_run_check(eq);
    if (eq->updata) {
        err = audio_eq_start(eq);
        if (err) {
            return 0;
        }
        eq->updata = 0;
    }
    struct hw_eq_ch *hw_eq = eq->eq_ch;
    void *buf = in_buf;

#ifdef CONFIG_EQ_SUPPORT_ASYNC
    struct audio_eq_async *async;
    if (hw_eq->no_wait) {
		int buf_start = 0;
		int buf_len = len;
#if AUDIO_EQ_SUPPORT_32BIT
		if (hw_eq->out_32bit) {
			buf_start = len;
			buf_len = len * 2;
		}
#endif
        eq->async_toggle = !eq->async_toggle;
        async = &eq->async[eq->async_toggle];
        if (async->buf) {
            if (async->buf_len < buf_len) {
                free(async->buf);
                async->buf = NULL;
            }
        }
        if (!async->buf) {
            async->buf = malloc(buf_len);
            ASSERT(async->buf);
            async->buf_len = buf_len;
            /* printf("toggle:%d, buf:0x%x, len:%d ", eq->async_toggle, async->buf, len); */
        }
        memcpy(&async->buf[buf_start], buf, len);
        async->len = buf_len;
        async->ptr = 0;
        async->clear = 0; // new EQ start
		buf = async->buf;
		err = audio_hw_eq_ch_start(hw_eq, &async->buf[buf_start], buf, len);
	} else {
		err = audio_hw_eq_ch_start(hw_eq, buf, buf, len);
    }
    if (err == 0) {
        if (hw_eq->no_wait) {
            eq->async[eq->async_toggle].len = 0;
            eq->async_toggle = !eq->async_toggle;
        }
        return 0;
    }

#else

    err = audio_hw_eq_ch_start(hw_eq, buf, buf, len);
    if (err == 0) {
        return 0;
    }
#endif

    if (!hw_eq->no_wait) {
        if (eq->output) {
            return eq->output(eq->output_priv, buf, len);
        }
    }
    return len;
}

static u32 hw_eq_run_inOut(struct audio_eq *eq, void *in_buf, void *out_buf, u32 len)
{
    int err;
    eq_app_run_check(eq);
    if (eq->updata) {
        err = audio_eq_start(eq);
        if (err) {
            return 0;
        }
        eq->updata = 0;
    }
    struct hw_eq_ch *hw_eq = eq->eq_ch;
    void *buf = in_buf;
    void *outbuf = out_buf;

#ifdef CONFIG_EQ_SUPPORT_ASYNC
    struct audio_eq_async *async;
    if (hw_eq->no_wait) {
        eq->async_toggle = !eq->async_toggle;
        async = &eq->async[eq->async_toggle];
        if (async->buf) {
            if (async->buf_len < len) {
                free(async->buf);
                async->buf = NULL;

                if (async->buf_bk) {
                    free(async->buf_bk);
                    async->buf_bk = NULL;
                }
            }
        }
        if (!async->buf) {
            async->buf = malloc(len);
            ASSERT(async->buf);
            async->buf_len = len;

            if (out_buf == 0xaa) {
                async->buf_bk = malloc(len);
                ASSERT(async->buf_bk);
            }

            /* printf("toggle:%d, buf:0x%x, len:%d ", eq->async_toggle, async->buf, len); */
        }
        if (async->buf_bk) { //备份数据源，用于输出回调中嵌套调用
            memcpy(async->buf_bk, buf, len);
        }
        memcpy(async->buf, buf, len);
        async->len = len;
        async->ptr = 0;
        async->clear = 0; // new EQ start
        buf = async->buf;
        outbuf = buf;
    }
    err = audio_hw_eq_ch_start(hw_eq, buf, outbuf, len);
    if (err == 0) {
        if (hw_eq->no_wait) {
            eq->async[eq->async_toggle].len = 0;
            eq->async_toggle = !eq->async_toggle;
        }
        return 0;
    }

#else

    err = audio_hw_eq_ch_start(hw_eq, buf, outbuf, len);
    if (err == 0) {
        return 0;
    }
#endif

    if (!hw_eq->no_wait) {
        if (eq->output) {
            return eq->output(eq->output_priv, outbuf, len);
        }
    }
    return len;
}


int audio_eq_run(struct audio_eq *eq, s16 *data, u32 len)
{
    int wlen = len;
    if (!eq || !eq->cb) {
        return 0;
    }
    if (!len) {
        return 0;
    }
    struct hw_eq_ch *hw_eq = eq->eq_ch;
#ifdef CONFIG_EQ_SUPPORT_ASYNC
    if (eq->output && hw_eq->no_wait) {
        u8 toggle = !eq->async_toggle;
        struct audio_eq_async *async = &eq->async[toggle];
        if (async->ptr < async->len) {
            audio_eq_async_output(eq, toggle);
        }
        if (async->ptr < async->len) {
            return 0;
        }

        if (len <= 0) {
            return 0;
        }
        return hw_eq_run(eq, data, len);
    }
#endif
    if (eq->remain_en && eq->remain_flag && eq->output) {
        wlen = eq->output(eq->output_priv, data, len);
    } else {
        wlen = hw_eq_run(eq, data, len);
        /* printf("out, wl:%d, l:%d ", wlen, len); */
    }
    eq->remain_flag = len == wlen ? 0 : 1;
    return wlen;
}

int audio_eq_data_len(struct audio_eq *eq)
{
#ifdef CONFIG_EQ_SUPPORT_ASYNC
    struct hw_eq_ch *hw_eq = eq->eq_ch;
    if (eq->output && hw_eq->no_wait) {
        u8 toggle = !eq->async_toggle;
        struct audio_eq_async *async = &eq->async[toggle];
        return async->len - async->ptr;
    }
#endif
    return 0;
}


int audio_eq_run_inOut(struct audio_eq *eq, s16 *indata, s16 *data, u32 len)
{
    int wlen = len;
    if (!eq || !eq->cb) {
        return 0;
    }

    if (!len) {
        return 0;
    }

    struct hw_eq_ch *hw_eq = eq->eq_ch;
#ifdef CONFIG_EQ_SUPPORT_ASYNC
    if (eq->output && hw_eq->no_wait) {
        u8 toggle = !eq->async_toggle;
        struct audio_eq_async *async = &eq->async[toggle];
        if (async->ptr < async->len) {
            audio_eq_async_output(eq, toggle);
        }
        if (async->ptr < async->len) {
            return 0;
        }
        return hw_eq_run_inOut(eq, indata, data, len);
    }
#endif
    if (eq->remain_en && eq->remain_flag && eq->output) {
        wlen = eq->output(eq->output_priv, data, len);
    } else {
        wlen = hw_eq_run_inOut(eq, indata, data, len);
        /* printf("out, wl:%d, l:%d ", wlen, len); */
    }
    eq->remain_flag = len == wlen ? 0 : 1;
    return wlen;
}

#ifdef CONFIG_EQ_SUPPORT_ASYNC
void audio_eq_async_data_clear(struct audio_eq *eq)
{
    if (eq) {
        eq->async[0].clear = 1;
        eq->async[1].clear = 1;
    }
}
#endif

void audio_eq_flush_out(void)
{
	audio_hw_eq_flush_out(&hw_eq_hdl);
}

___interrupt
static void _eq_isr(void)
{
    if (JL_EQ->CON0 & BIT(15)) {
        JL_EQ->CON0 |= BIT(13);
        /* putchar('e'); */
        audio_hw_eq_irq_handler(&hw_eq_hdl);
    }
}

void audio_eq_init(void)
{
    request_irq(IRQ_EQ_IDX, 1, _eq_isr, 0);
    audio_hw_eq_init(&hw_eq_hdl);
}


#if TCFG_DRC_ENABLE

#include "asm/sw_drc.h"


static int L_coeff_tmp[2][3] = {0};
static int R_coeff_tmp[2][3] = {0};
static int drc_nsection[2][1] = {0};
int crossover_low_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    info->L_coeff = (void *)L_coeff_tmp[0][0];
    info->R_coeff = (void *)R_coeff_tmp[0][0];
    info->L_gain = 0;
    info->R_gain = 0;
    info->nsection = drc_nsection[0][0];

    /* printf("%s\n", __FUNCTION__); */
    /* printf("xx info->nsection %d\n", info->nsection); */
    /* for (int i = 0; i < info->nsection; i++)	{ */
    /* printf("cf0:%d, cf1:%d, cf2:%d, cf3:%d, cf4:%d ", info->L_coeff[5*i] */
    /* , info->L_coeff[5*i + 1] */
    /* , info->L_coeff[5*i + 2] */
    /* , info->L_coeff[5*i + 3] */
    /* , info->L_coeff[5*i + 4] */
    /* ); */
    /* } */

    return 0;
}

int crossover_band_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    info->L_coeff = (void *)L_coeff_tmp[0][1];
    info->R_coeff = (void *)R_coeff_tmp[0][1];
    info->L_gain = 0;
    info->R_gain = 0;
    info->nsection = drc_nsection[0][0];
    /* printf("%s\n", __FUNCTION__); */
    /* printf("xx info->nsection %d\n", info->nsection); */
    /* for (int i = 0; i < info->nsection; i++)	{ */
    /* printf("cf0:%d, cf1:%d, cf2:%d, cf3:%d, cf4:%d ", info->L_coeff[5*i] */
    /* , info->L_coeff[5*i + 1] */
    /* , info->L_coeff[5*i + 2] */
    /* , info->L_coeff[5*i + 3] */
    /* , info->L_coeff[5*i + 4] */
    /* ); */
    /* } */


    return 0;
}

int crossover_high_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    info->L_coeff = (void *)L_coeff_tmp[0][2];
    info->R_coeff = (void *)R_coeff_tmp[0][2];
    info->L_gain = 0;
    info->R_gain = 0;
    info->nsection = drc_nsection[0][0];

    /* printf("%s\n", __FUNCTION__); */
    /* printf("xx info->nsection %d\n", info->nsection); */
    /* for (int i = 0; i < info->nsection; i++)	{ */
    /* printf("cf0:%d, cf1:%d, cf2:%d, cf3:%d, cf4:%d ", info->L_coeff[5*i] */
    /* , info->L_coeff[5*i + 1] */
    /* , info->L_coeff[5*i + 2] */
    /* , info->L_coeff[5*i + 3] */
    /* , info->L_coeff[5*i + 4] */
    /* ); */
    /* } */

    return 0;
}



int crossover_low_get_filter_info2(int sr, struct audio_eq_filter_info *info)
{
    info->L_coeff = (void *)L_coeff_tmp[1][0];
    info->R_coeff = (void *)R_coeff_tmp[1][0];
    info->L_gain = 0;
    info->R_gain = 0;
    info->nsection = drc_nsection[1][0];
    /* printf("%s\n", __FUNCTION__); */
    return 0;
}

int crossover_band_get_filter_info2(int sr, struct audio_eq_filter_info *info)
{
    info->L_coeff = (void *)L_coeff_tmp[1][1];
    info->R_coeff = (void *)R_coeff_tmp[1][1];
    info->L_gain = 0;
    info->R_gain = 0;
    info->nsection = drc_nsection[1][0];
    /* printf("%s\n", __FUNCTION__); */

    return 0;
}

int crossover_high_get_filter_info2(int sr, struct audio_eq_filter_info *info)
{
    info->L_coeff = (void *)L_coeff_tmp[1][2];
    info->R_coeff = (void *)R_coeff_tmp[1][2];
    info->L_gain = 0;
    info->R_gain = 0;
    info->nsection = drc_nsection[1][0];
    /* printf("%s\n", __FUNCTION__); */
    return 0;
}


static int crossover_eq_output(void *priv, void *data, u32 len)
{
    return len;
}

int eq_get_filter_info(int sr, struct audio_eq_filter_info *info);
void audio_hw_crossover_open(struct sw_drc *drc, int (*L_coeff)[3], u8 nsection, u8 drc_name)
{
    for (int i = 0; i < drc->nband ; i++) {
        log_info("audio_hw_cross_open i = %d, nband %d, nsection %d\n", i, drc->nband, nsection);
        struct audio_eq *crossover_eq = zalloc(sizeof(struct audio_eq) + sizeof(struct hw_eq_ch));
        if (crossover_eq) {
            crossover_eq->eq_ch = (struct hw_eq_ch *)((int)crossover_eq + sizeof(struct audio_eq));
            struct audio_eq_param eq_param = {0};
            eq_param.channels = drc->channel;//fmt->channel;
            /* log_info("drc->channel %d\n", drc->channel); */
            eq_param.online_en = 0;
            eq_param.mode_en = 1;
            eq_param.remain_en = 1;
            eq_param.max_nsection = nsection;
            eq_param.drc_en = 1;
            if (!drc_name) {
                if (drc->nband  > 2) {
                    if (i == 0) {
                        L_coeff_tmp[0][0] = L_coeff[0][0];
                        R_coeff_tmp[0][0] = L_coeff[0][0] +  nsection * 5 * sizeof(int);
                        eq_param.cb = crossover_low_get_filter_info;
                    } else if (i == 1) {
                        L_coeff_tmp[0][1] = L_coeff[0][1];
                        R_coeff_tmp[0][1] = L_coeff[0][1] +  nsection * 5 * sizeof(int);
                        eq_param.cb = crossover_band_get_filter_info;
                    } else if (i == 2) {
                        L_coeff_tmp[0][2] = L_coeff[0][2];
                        R_coeff_tmp[0][2] = L_coeff[0][2] +  nsection * 5 * sizeof(int);
                        eq_param.cb = crossover_high_get_filter_info;
                    } else {
                        printf("nband err %d\n", i);
                    }
                } else {
                    if (i == 0) {
                        L_coeff_tmp[0][0] = L_coeff[0][0];
                        R_coeff_tmp[0][0] = L_coeff[0][0] +  nsection * 5 * sizeof(int);
                        eq_param.cb = crossover_low_get_filter_info;
                    } else if (i == 1) {
                        L_coeff_tmp[0][2] = L_coeff[0][2];
                        R_coeff_tmp[0][2] = L_coeff[0][2] +  nsection * 5 * sizeof(int);
                        eq_param.cb = crossover_high_get_filter_info;
                    } else {
                        printf("nband err %d\n", i);
                    }

                }
                drc_nsection[0][0] = nsection;
            } else {
                if (drc->nband  > 2) {
                    if (i == 0) {
                        L_coeff_tmp[1][0] = L_coeff[0][0];
                        R_coeff_tmp[1][0] = L_coeff[0][0] +  nsection * 5 * sizeof(int);
                        eq_param.cb = crossover_low_get_filter_info2;
                    } else if (i == 1) {
                        L_coeff_tmp[1][1] = L_coeff[0][1];
                        R_coeff_tmp[1][1] = L_coeff[0][1] +  nsection * 5 * sizeof(int);
                        eq_param.cb = crossover_band_get_filter_info2;
                    } else if (i == 2) {
                        L_coeff_tmp[1][2] = L_coeff[0][2];
                        R_coeff_tmp[1][2] = L_coeff[0][2] +  nsection * 5 * sizeof(int);
                        eq_param.cb = crossover_high_get_filter_info2;
                    } else {
                        printf("nband err %d\n", i);
                    }
                } else {
                    if (i == 0) {
                        L_coeff_tmp[1][0] = L_coeff[0][0];
                        R_coeff_tmp[1][0] = L_coeff[0][0] +  nsection * 5 * sizeof(int);
                        eq_param.cb = crossover_low_get_filter_info2;
                    } else if (i == 1) {
                        L_coeff_tmp[1][2] = L_coeff[0][2];
                        R_coeff_tmp[1][2] = L_coeff[0][2] +  nsection * 5 * sizeof(int);
                        eq_param.cb = crossover_high_get_filter_info2;
                    } else {
                        printf("nband err %d\n", i);
                    }
                }
                drc_nsection[1][0] = nsection;
            }
            eq_param.no_wait = 0;
            eq_param.eq_switch = 0;
            audio_eq_open(crossover_eq, &eq_param);

            /* audio_eq_set_info(crossover_eq, eq_param.channels, 1); */
            /* log_info(" ixdrc->sample_rate %d\n", drc->sample_rate); */
            audio_eq_set_samplerate(crossover_eq, drc->sample_rate);
            audio_eq_set_output_handle(crossover_eq, crossover_eq_output, NULL);
            audio_eq_start(crossover_eq);
        }
        drc->crossover[i] = crossover_eq;
        drc->nsection = nsection;
    }
}


void audio_hw_crossover_close(struct sw_drc *drc)
{
    for (int i = 0; i < 3/* drc->nband */ ; i++) {
        if (drc->crossover[i]) {
            audio_eq_close(drc->crossover[i]);
            free(drc->crossover[i]);
            drc->crossover[i] = NULL;
        }
    }
}


void audio_hw_crossover_run(struct sw_drc *drc, s16 *data, int len)
{
    /* printf("drc->nband %d\n",drc->nband); */
    int i;
    for (i = 0; i < drc->nband ; i++) {
        audio_eq_run_inOut(drc->crossover[i], data, drc->run_tmp[i], len);
    }
}
#endif

#endif /*TCFG_EQ_ENABLE*/

