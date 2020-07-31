
#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_online_cfg.h"

#include "audio_eq.h"
#include "audio_eq_tab.h"

#include "audio_drc.h"

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

extern const u8 audio_eq_sdk_name[16];
extern const u8 audio_eq_ver[4];

#define TCFG_EQ_FILE_ENABLE		1
#define EQ_FILE_NAME 			SDFILE_RES_ROOT_PATH"eq_cfg_hw.bin"

#define EQ_CH_NUM               1
/*-----------------------------------------------------------*/
/*eq magic*/
typedef enum {
    MAGIC_EQ_COEFF = 0xA5A0,
    MAGIC_EQ_LIMITER,
    MAGIC_EQ_SOFT_SEC,
    MAGIC_EQ_SEG = 0xA6A1,
    MAGIC_DRC,
    MAGIC_EQ_MAX,
} EQ_MAGIC;

/*eq online cmd*/
typedef enum {
    EQ_ONLINE_CMD_SECTION       = 1,
    EQ_ONLINE_CMD_GLOBAL_GAIN,
    EQ_ONLINE_CMD_LIMITER,
    EQ_ONLINE_CMD_INQUIRE,
    EQ_ONLINE_CMD_GETVER,
    EQ_ONLINE_CMD_GET_SOFT_SECTION,//br22专用
    EQ_ONLINE_CMD_GET_SECTION_NUM = 0x7,//工具查询 小机需要的eq段数
	EQ_ONLINE_CMD_GLOBAL_GAIN_SUPPORT_FLOAT = 0x8,

    EQ_ONLINE_CMD_PARAMETER_SEG = 0x11,
    EQ_ONLINE_CMD_PARAMETER_TOTAL_GAIN,
    EQ_ONLINE_CMD_PARAMETER_LIMITER,
    EQ_ONLINE_CMD_PARAMETER_DRC,
    EQ_ONLINE_CMD_PARAMETER_CHANNEL,//通道切换
} EQ_ONLINE_CMD;

/*eq file seg head*/
typedef struct {
    unsigned short crc;
    unsigned short seg_num;
    float global_gain;
    int enable_section;
} _GNU_PACKED_ EQ_FILE_SEG_HEAD;

/*eq file limiter head*/
typedef struct {
    unsigned short crc;
    unsigned short re;
    float AttackTime;
    float ReleaseTime;
    float Threshold;
    int Enable;
} _GNU_PACKED_ LIMITER_CFG_HEAD;

/*eq online packet*/
typedef struct {
    int cmd;     			///<EQ_ONLINE_CMD
    int data[45];       	///<data
} _GNU_PACKED_ EQ_ONLINE_PACKET;


/*EQ_ONLINE_CMD_PARAMETER_SEG*/
typedef struct eq_seg_info EQ_ONLINE_PARAMETER_SEG;

/*EQ_ONLINE_CMD_PARAMETER_TOTAL_GAIN*/
typedef struct {
	/* int gain; */
	float gain;
} _GNU_PACKED_ EQ_ONLINE_PARAMETER_TOTAL_GAIN;

/*EQ_ONLINE_CMD_PARAMETER_LIMITER*/
typedef struct {
    int enable;
    int attackTime;
    int releaseTime;
    int threshold;
} _GNU_PACKED_ EQ_ONLINE_PARAMETER_LIMITER;


/*-----------------------------------------------------------*/
typedef struct eq_seg_info EQ_CFG_SEG;
typedef struct {
    float global_gain;
    EQ_CFG_SEG seg[EQ_SECTION_MAX];
#if TCFG_DRC_ENABLE
    struct drc_ch drc;
#endif
} EQ_CFG_PARAMETER;
typedef struct {
    u32 eq_type : 3;
    u32 seg_num : 6;
    u32 online_need_updata : 1;
    u32 mode_updata : 1;
    u32 reserved : 19;
    u32 online_updata[EQ_CH_NUM];
    u32 drc_updata[EQ_CH_NUM];
    u16 cur_sr[EQ_CH_NUM];
    u32 design_mask[EQ_CH_NUM];
    spinlock_t lock;
    EQ_CFG_PARAMETER param[EQ_CH_NUM];
#ifndef CONFIG_EQ_NO_USE_COEFF_TABLE
    int EQ_Coeff_table[EQ_CH_NUM][EQ_SECTION_MAX * 5];
#endif

#if (TCFG_EQ_ONLINE_ENABLE && TCFG_USER_TWS_ENABLE)
    void *tws_ci;
    int tws_tmr;
#endif

    u8 eq_ch;
} EQ_CFG;


/*EQ_ONLINE_CMD_PARAMETER_CHANNEL*/
typedef struct {
    int channel;
} _GNU_PACKED_ EQ_ONLINE_CMD_PARAMETER_CAHNNEL;


/*-----------------------------------------------------------*/
static u8 eq_mode = 0;
static EQ_CFG *p_eq_cfg = NULL;

static const EQ_CFG_SEG eq_seg_nor = {
    0, 2, 1000, 0 << 20, (int)(0.7f * (1 << 24))
};

/*-----------------------------------------------------------*/
extern u16 crc_get_16bit(const void *src, u32 len);

int eq_seg_design(struct eq_seg_info *seg, int sample_rate, int *coeff)
{
    /* printf("seg:0x%x, coeff:0x%x, rate:%d, ", seg, coeff, sample_rate); */
    /* printf("idx:%d, iir:%d, freq:%d, gain:%d, q:%d ", seg->index, seg->iir_type, seg->freq, seg->gain, seg->q); */
    if (seg->freq >= (((u32)sample_rate / 2 * 29491) >> 15)) {
        if (seg->freq != 96000) {
            log_error("sample_rate %d, freq:%d err ",sample_rate, seg->freq);
        }
        eq_get_AllpassCoeff(coeff);
        return false;
    }
    switch (seg->iir_type) {
    case EQ_IIR_TYPE_HIGH_PASS:
        design_hp(seg->freq, sample_rate, seg->q, coeff);
        break;
    case EQ_IIR_TYPE_LOW_PASS:
        design_lp(seg->freq, sample_rate, seg->q, coeff);
        break;
    case EQ_IIR_TYPE_BAND_PASS:
        design_pe(seg->freq, sample_rate, seg->gain, seg->q, coeff);
        break;
    case EQ_IIR_TYPE_HIGH_SHELF:
        design_hs(seg->freq, sample_rate, seg->gain, seg->q, coeff);
        break;
    case EQ_IIR_TYPE_LOW_SHELF:
        design_ls(seg->freq, sample_rate, seg->gain, seg->q, coeff);
        break;
    }
    int status = eq_stable_check(coeff);
    if (status) {
        log_error("eq_stable_check err:%d ", status);
        log_info("%d %d %d %d %d", coeff[0], coeff[1], coeff[2], coeff[3], coeff[4]);
        eq_get_AllpassCoeff(coeff);
        return false;
    }
    return true;
}


static void eq_coeff_set(int sr, u8 ch)
{
#ifndef CONFIG_EQ_NO_USE_COEFF_TABLE
    int i;
    if (!sr) {
        sr = 44100;
        log_error("sr is zero");
    }
    //printf("-----------ch %d\n", ch);
    //printf("p_eq_cfg->EQ_Coeff_table[ch] %x\n", p_eq_cfg->EQ_Coeff_table[ch]);
    for (i = 0; i < p_eq_cfg->seg_num; i++) {
        if (p_eq_cfg->design_mask[ch] & BIT(i)) {
            p_eq_cfg->design_mask[ch] &= ~BIT(i);
            eq_seg_design(&p_eq_cfg->param[ch].seg[i], sr, &p_eq_cfg->EQ_Coeff_table[ch][5 * i]);

            /* printf("\np_eq_cfg->param[ch].seg[i].index    %d\n p_eq_cfg->param[ch].seg[i].iir_type %d\n p_eq_cfg->param[ch].seg[i].freq %d\n p_eq_cfg->param[ch].seg[i].gain %d\n p_eq_cfg->param[ch].seg[i].q    %d\n", */
            /* p_eq_cfg->param[ch].seg[i].index,  */
            /* p_eq_cfg->param[ch].seg[i].iir_type,  */
            /* p_eq_cfg->param[ch].seg[i].freq,  */
            /* 10*p_eq_cfg->param[ch].seg[i].gain/(1<<20), */
            /* 10*p_eq_cfg->param[ch].seg[i].q/(1<<24)); */

            /* printf("cf0:%d, cf1:%d, cf2:%d, cf3:%d, cf4:%d ", p_eq_cfg->EQ_Coeff_table[ch][5*i] */
            /* , p_eq_cfg->EQ_Coeff_table[ch][5*i + 1] */
            /* , p_eq_cfg->EQ_Coeff_table[ch][5*i + 2] */
            /* , p_eq_cfg->EQ_Coeff_table[ch][5*i + 3] */
            /* , p_eq_cfg->EQ_Coeff_table[ch][5*i + 4] */
            /* ); */
        }
    }
#endif
}

/*-----------------------------------------------------------*/
// eq file
#if TCFG_EQ_FILE_ENABLE
enum {
    MAGIC_FLAG_SEG = 0,
    MAGIC_FLAG_DRC = 1,
};
static s32 eq_file_get_cfg(EQ_CFG *eq_cfg)
{
    u8 magic_flag = 0;
    int magic;
    int ret = 0;
    int read_size;
    FILE *file = NULL;
    u8 *file_data = NULL;

    if (eq_cfg == NULL) {
        return  -EINVAL;
    }
    file = fopen(EQ_FILE_NAME, "r");
    if (file == NULL) {
        log_error("eq file open err\n");
        return  -ENOENT;
    }
    log_info("eq file open ok \n");

    {
        // eq ver
        u8 ver[4] = {0};
        if (4 != fread(file, ver, 4)) {
            ret = -EIO;
            goto err_exit;
        }
        if (memcmp(ver, audio_eq_ver, sizeof(audio_eq_ver))) {
            log_info("eq ver err \n");
            log_info_hexdump(ver, 4);
            /* ret = -EINVAL; */
            /* goto err_exit; */
            fseek(file, 0, SEEK_SET);
        }
    }

    u8 eq_ch = 0;
    u8 drc_ch_n = 0;
    while (1) {
        if (sizeof(int) != fread(file, &magic, sizeof(int))) {
            ret = 0;
            break;
        }
        log_info("eq magic 0x%x\n", magic);
        if ((magic >= MAGIC_EQ_COEFF) && (magic < MAGIC_EQ_MAX)) {
            if (magic == MAGIC_EQ_SOFT_SEC) {
                fseek(file, sizeof(int), SEEK_CUR);
            }
            if (magic == MAGIC_EQ_SEG) {
                EQ_FILE_SEG_HEAD *eq_file_h;
                int cfg_zone_size = sizeof(EQ_FILE_SEG_HEAD) + EQ_SECTION_MAX * sizeof(EQ_CFG_SEG);
                file_data = malloc(cfg_zone_size);
                if (file_data == NULL) {
                    ret = -ENOMEM;
                    break;
                }

                if (sizeof(EQ_FILE_SEG_HEAD) != fread(file, file_data, sizeof(EQ_FILE_SEG_HEAD))) {
                    ret = -EIO;
                    break;
                }
                eq_file_h = (EQ_FILE_SEG_HEAD *)file_data;
                log_info("cfg_seg_num:%d\n", eq_file_h->seg_num);
                log_info("cfg_global_gain:%d\n", (u32)(eq_file_h->global_gain));
                log_info("cfg_enable_section:%x\n", eq_file_h->enable_section);

                if (eq_file_h->seg_num > EQ_SECTION_MAX) {
                    ret = -EINVAL;
                    break;
                }

                read_size = eq_file_h->seg_num * sizeof(EQ_CFG_SEG);
                if (read_size != fread(file, file_data + sizeof(EQ_FILE_SEG_HEAD), read_size)) {
                    ret = -EIO;
                    break;
                }

                if (eq_file_h->crc == crc_get_16bit(&eq_file_h->seg_num, read_size + sizeof(EQ_FILE_SEG_HEAD) - 2)) {
                    log_info("eq_cfg_file crc ok\n");
                    spin_lock(&eq_cfg->lock);
                    eq_cfg->seg_num = eq_file_h->seg_num;
                    eq_cfg->param[eq_ch].global_gain = eq_file_h->global_gain;
                    memcpy(eq_cfg->param[eq_ch].seg, file_data + sizeof(EQ_FILE_SEG_HEAD), sizeof(EQ_CFG_SEG) * eq_file_h->seg_num);
                    eq_cfg->design_mask[eq_ch] = (u32) - 1;
                    eq_coeff_set(eq_cfg->cur_sr[eq_ch], eq_ch);
                    spin_unlock(&eq_cfg->lock);
                    log_info("sr %d head size %d\n", eq_cfg->cur_sr[eq_ch], sizeof(EQ_FILE_SEG_HEAD));
                    /* printf_buf(eq_cfg->param.seg, sizeof(EQ_CFG_SEG) * eq_file_h->seg_num); */
                    ret = 0;
                    log_info("eq_ch %d\n", eq_ch);
#if (EQ_CH_NUM == 4)
                    eq_ch++;
#else
                    eq_ch = 0;
#endif
                } else {
                    log_error("eq_cfg_info crc err\n");
                    ret = -ENOEXEC;
                }

                free(file_data);
                file_data = NULL;
                magic_flag |= BIT(MAGIC_FLAG_SEG);
            }

            if (magic == MAGIC_EQ_COEFF) {
                EQ_FILE_SEG_HEAD eq_file;
                if (sizeof(EQ_FILE_SEG_HEAD) != fread(file, &eq_file, sizeof(EQ_FILE_SEG_HEAD))) {
                    ret = -EIO;
                    break;
                }
                if (eq_file.seg_num > EQ_SECTION_MAX) {
                    ret = -EINVAL;
                    break;
                }
                fseek(file, eq_file.seg_num * 5 * sizeof(int) * 9, SEEK_CUR);
            }
            if (magic == MAGIC_EQ_LIMITER) {
                fseek(file, sizeof(LIMITER_CFG_HEAD), SEEK_CUR);
            }
            if (magic == MAGIC_DRC) {
#if TCFG_DRC_ENABLE
                u16 crc16;
                u32 pam = 0;
                struct drc_ch drc = {0};
                if (sizeof(pam) != fread(file, &pam, sizeof(pam))) {
                    ret = -EIO;
                    break;
                }
                crc16 = pam;
                if (sizeof(struct drc_ch) != fread(file, &drc, sizeof(struct drc_ch))) {
                    ret = -EIO;
                    break;
                }
                if (crc16 == crc_get_16bit(&drc, sizeof(struct drc_ch))) {
                    memcpy(&eq_cfg->param[drc_ch_n].drc, &drc, sizeof(struct drc_ch));
                    magic_flag |= BIT(MAGIC_FLAG_DRC);
                    /* ret = 0; */
                }
#else
                fseek(file, sizeof(struct drc_ch) + 4, SEEK_CUR);
#endif

                log_info("drc_ch_n %d\n", drc_ch_n);
#if (EQ_CH_NUM == 4)
                drc_ch_n++;
#else
                drc_ch_n = 0;
#endif

            }
        } else {
            log_error("cfg_info magic err\n");
            /* ret = -EINVAL; */
            ret = 0;
            break;
        }
    }
err_exit:
    if (file_data) {
        free(file_data);
    }
    fclose(file);
    if (ret == 0) {
        if (!(magic_flag & BIT(MAGIC_FLAG_SEG))) {
            log_error("cfg_info coeff err\n");
            ret = -EINVAL;
        }
#if TCFG_DRC_ENABLE
        if (!(magic_flag & BIT(MAGIC_FLAG_DRC))) {
            log_error("cfg_info drc err\n");
            /* ret = -EINVAL; */
        }
#endif
    }
    if (ret == 0) {
        log_info("cfg_info ok \n");
    }
    return ret;
}



static int eq_file_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    int *coeff = NULL;
    if (!sr) {
        return -1;
    }
    ASSERT(p_eq_cfg);
    if (sr != p_eq_cfg->cur_sr[0]) {
        //更新coeff
        /* if (eq_file_get_cfg(p_eq_cfg)) { */
        /* p_eq_cfg->cur_sr = 0; */
        /* return -1; */
        /* } */
        spin_lock(&p_eq_cfg->lock);

#if (EQ_CH_NUM == 4)
        for (int ch = 0; ch < 2/* EQ_CH_NUM */; ch++) {
#else
        for (int ch = 0; ch < EQ_CH_NUM; ch++) {
#endif
            p_eq_cfg->cur_sr[ch] = sr;
            p_eq_cfg->design_mask[ch] = (u32) - 1;
            eq_coeff_set(sr, ch);
        }

        spin_unlock(&p_eq_cfg->lock);
    }

#ifdef CONFIG_EQ_NO_USE_COEFF_TABLE
    info->no_coeff = 1;
    info->sr = sr;

#if (EQ_CH_NUM == 4)
    info->L_coeff = (void *)p_eq_cfg->param[0].seg;
    info->R_coeff = (void *)p_eq_cfg->param[1].seg;
    info->L_gain = p_eq_cfg->param[0].global_gain;
    info->R_gain = p_eq_cfg->param[1].global_gain;
#else
    coeff = p_eq_cfg->param[0].seg;
    info->L_coeff = info->R_coeff = (void *)coeff;
    info->L_gain = info->R_gain = p_eq_cfg->param[0].global_gain;
#endif

#else

#if (EQ_CH_NUM == 4)
    info->L_coeff = (void *)p_eq_cfg->EQ_Coeff_table[0];
    info->R_coeff = (void *)p_eq_cfg->EQ_Coeff_table[1];
    info->L_gain = p_eq_cfg->param[0].global_gain;
    info->R_gain = p_eq_cfg->param[1].global_gain;
#else
    coeff = p_eq_cfg->EQ_Coeff_table[0];
    info->L_coeff = info->R_coeff = (void *)coeff;
    info->L_gain = info->R_gain = p_eq_cfg->param[0].global_gain;
#endif

#endif
    info->nsection = p_eq_cfg->seg_num;
    /* log_info("info->nsection %d\n",info->nsection); */
    return 0;
}



#endif

/*-----------------------------------------------------------*/
// eq mode
static const EQ_CFG_SEG *eq_mode_tab[EQ_MODE_MAX] = {
    eq_tab_normal, eq_tab_rock, eq_tab_pop, eq_tab_classic, eq_tab_jazz, eq_tab_country, eq_tab_custom
};

static int eq_mode_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    int *coeff = NULL;
    if (!sr) {
        return -1;
    }
    ASSERT(p_eq_cfg);

#if (EQ_CH_NUM == 4)
    for (int i = 0; i < 2/* EQ_CH_NUM */; i++) {
#else
    for (int i = 0; i < EQ_CH_NUM; i++) {
#endif
        memcpy(p_eq_cfg->param[i].seg, eq_mode_tab[eq_mode], sizeof(EQ_CFG_SEG) * p_eq_cfg->seg_num/* EQ_SECTION_MAX */);
    }

    spin_lock(&p_eq_cfg->lock);
#if (EQ_CH_NUM == 4)
    for (int ch = 0; ch < 2/* EQ_CH_NUM */; ch++) {
#else
    for (int ch = 0; ch < EQ_CH_NUM; ch++) {
#endif
        p_eq_cfg->design_mask[ch] = (u32) - 1;
        eq_coeff_set(sr, ch);
        p_eq_cfg->cur_sr[ch] = 0;
    }
    spin_unlock(&p_eq_cfg->lock);
    /* printf("sr %d\n", sr); */



#ifdef CONFIG_EQ_NO_USE_COEFF_TABLE
    info->no_coeff = 1;
    info->sr = sr;

#if (EQ_CH_NUM == 4)
    info->L_coeff = (void *)p_eq_cfg->param[0].seg;
    info->R_coeff = (void *)p_eq_cfg->param[1].seg;
    info->L_gain = p_eq_cfg->param[0].global_gain;
    info->R_gain = p_eq_cfg->param[1].global_gain;
#else
    coeff = p_eq_cfg->param[0].seg;
    info->L_coeff = info->R_coeff = (void *)coeff;
    info->L_gain = info->R_gain = p_eq_cfg->param[0].global_gain;
#endif

#else

#if (EQ_CH_NUM == 4)
    info->L_coeff = (void *)p_eq_cfg->EQ_Coeff_table[0];
    info->R_coeff = (void *)p_eq_cfg->EQ_Coeff_table[1];
    info->L_gain = p_eq_cfg->param[0].global_gain;
    info->R_gain = p_eq_cfg->param[1].global_gain;
#else
    coeff = p_eq_cfg->EQ_Coeff_table[0];
    info->L_coeff = info->R_coeff = (void *)coeff;
    info->L_gain = info->R_gain = p_eq_cfg->param[0].global_gain;
#endif

#endif
    info->nsection = p_eq_cfg->seg_num;
    return 0;
}



int eq_mode_set(u8 mode)
{
    if (mode >= EQ_MODE_MAX) {
        mode = 0;
    }
    eq_mode = mode;
    log_info("set eq mode %d\n", eq_mode);

    if (p_eq_cfg && (p_eq_cfg->eq_type == EQ_TYPE_MODE_TAB)) {
        p_eq_cfg->mode_updata = 1;
    }
    return 0;
}

int eq_mode_sw(void)
{
    eq_mode++;
    if (eq_mode >= EQ_MODE_MAX) {
        eq_mode = 0;
    }
    log_info("sw eq mode %d\n", eq_mode);

    if (p_eq_cfg && (p_eq_cfg->eq_type == EQ_TYPE_MODE_TAB)) {
        p_eq_cfg->mode_updata = 1;
    }
    return 0;
}

int eq_mode_get_cur(void)
{
    return  eq_mode;
}

/*-----------------------------------------------------------*/
// eq online
#if TCFG_EQ_ONLINE_ENABLE
#include "config/config_interface.h"

#if TCFG_USER_TWS_ENABLE
extern void *tws_ci_sync_open(void *priv, void (*rx_handler)(void *, void *, int));
extern int tws_ci_data_sync(void *priv, void *data, int len, u32 time);

static void eq_online_tws_send_tmr(void *priv)
{
    if (!p_eq_cfg) {
        return ;
    }
    /* printf("send:"); */
    /* printf_buf(&p_eq_cfg->param, sizeof(EQ_CFG_PARAMETER)); */
    /*同步串口收到数据到对耳*/
    tws_ci_data_sync(p_eq_cfg->tws_ci, &p_eq_cfg->param[0], sizeof(EQ_CFG_PARAMETER), 0/*ms*/);
}
static void eq_online_tws_updata(void *priv)
{
    if (!p_eq_cfg) {
        return ;
    }
    eq_online_tws_send_tmr(NULL);
    p_eq_cfg->online_updata[0] = 1;
    p_eq_cfg->drc_updata[0] = 1;
}
static void eq_online_tws_send(void)
{
    if (!p_eq_cfg) {
        return ;
    }
    spin_lock(&p_eq_cfg->lock);
    if (!p_eq_cfg->tws_tmr) {
        p_eq_cfg->tws_tmr = sys_timer_add(NULL, eq_online_tws_send_tmr, 500);
    }
    spin_unlock(&p_eq_cfg->lock);
}
static void eq_online_tws_ci_data_rx_handler(void *priv, void *data, int len)
{
    if (!p_eq_cfg) {
        return ;
    }
    /* printf("%s,%d", __func__, __LINE__); */
    /* printf_buf(data, len); */
    spin_lock(&p_eq_cfg->lock);
    if (p_eq_cfg->tws_tmr) { // pc调试端
        if (0 == memcmp(data, &p_eq_cfg->param[0], sizeof(EQ_CFG_PARAMETER))) {
            // 数据相同，结束
            sys_timer_del(p_eq_cfg->tws_tmr);
            p_eq_cfg->tws_tmr = 0;
        }
    } else { // tws接受端
        memcpy(&p_eq_cfg->param[0], data, sizeof(EQ_CFG_PARAMETER));
        p_eq_cfg->design_mask[0] = (u32) - 1;
        sys_timeout_add(NULL, eq_online_tws_updata, 10);
    }
    spin_unlock(&p_eq_cfg->lock);
}
static void eq_online_tws_open(void)
{
    p_eq_cfg->tws_ci = tws_ci_sync_open(NULL, eq_online_tws_ci_data_rx_handler);
}
static void eq_online_tws_close(void)
{
    spin_lock(&p_eq_cfg->lock);
    if (p_eq_cfg->tws_tmr) { // pc调试端
        sys_timer_del(p_eq_cfg->tws_tmr);
        p_eq_cfg->tws_tmr = 0;
    }
    spin_unlock(&p_eq_cfg->lock);
    tws_ci_sync_open(NULL, NULL);
}
#endif

static s32 eq_online_update(EQ_ONLINE_PACKET *packet)
{
    EQ_ONLINE_PARAMETER_SEG seg;
    EQ_ONLINE_PARAMETER_TOTAL_GAIN gain;
    EQ_ONLINE_CMD_PARAMETER_CAHNNEL channel = {0};
    if (packet->cmd != 0x4) {
        printf("eq_cmd:0x%x ", packet->cmd);
    }
    /* printf_buf(packet->data, sizeof(packet->data)); */
    if (p_eq_cfg->eq_type != EQ_TYPE_ONLINE) {
        return -EPERM;
    }
    switch (packet->cmd) {
    case EQ_ONLINE_CMD_PARAMETER_CHANNEL:
#if (EQ_CH_NUM == 4)
        spin_lock(&p_eq_cfg->lock);
        memcpy(&channel, packet->data, sizeof(EQ_ONLINE_CMD_PARAMETER_CAHNNEL));
        p_eq_cfg->eq_ch = channel.channel;
        spin_unlock(&p_eq_cfg->lock);
#else
        p_eq_cfg->eq_ch = 0;
#endif
        log_info(">>>>>>>>>>>>>>>>>>>>>>>p_eq_cfg->eq_ch %d<<<<<<<<<<<<<<<<<<<<<\n", p_eq_cfg->eq_ch);
        break;
    case EQ_ONLINE_CMD_PARAMETER_SEG:
        spin_lock(&p_eq_cfg->lock);
        memcpy(&seg, packet->data, sizeof(EQ_ONLINE_PARAMETER_SEG));
        if (seg.index >= EQ_SECTION_MAX) {
            log_error("index:%d ", seg.index);
            spin_unlock(&p_eq_cfg->lock);
            /* return -EINVAL; */
            break;
        }
        memcpy(&p_eq_cfg->param[p_eq_cfg->eq_ch].seg[seg.index], &seg, sizeof(EQ_ONLINE_PARAMETER_SEG));
        p_eq_cfg->design_mask[p_eq_cfg->eq_ch] |= BIT(seg.index);
        spin_unlock(&p_eq_cfg->lock);

        log_info("idx:%d, iir:%d, frq:%d, gain:%d, q:%d \n", seg.index, seg.iir_type, seg.freq, seg.gain, seg.q);
        log_info("idx:%d, iir:%d, frq:%d, gain:%d, q:%d \n", seg.index, seg.iir_type, seg.freq, 10 * seg.gain / (1 << 20), 10 * seg.q / (1 << 24));
        break;
    case EQ_ONLINE_CMD_PARAMETER_TOTAL_GAIN:
        spin_lock(&p_eq_cfg->lock);
        memcpy(&gain, packet->data, sizeof(EQ_ONLINE_PARAMETER_TOTAL_GAIN));
        p_eq_cfg->param[p_eq_cfg->eq_ch].global_gain = gain.gain;
        spin_unlock(&p_eq_cfg->lock);
        log_info("global_gain:%f\n", p_eq_cfg->param[p_eq_cfg->eq_ch].global_gain);
        break;
    case EQ_ONLINE_CMD_INQUIRE:
        if (p_eq_cfg->online_need_updata) {
            p_eq_cfg->online_need_updata = 0;
            log_info("updata eq table\n");
            return 0;
        }
        return -EINVAL;
    case EQ_ONLINE_CMD_PARAMETER_LIMITER:
        log_info("EQ_ONLINE_CMD_PARAMETER_LIMITER");
        break;
    case EQ_ONLINE_CMD_PARAMETER_DRC:
        log_info("EQ_ONLINE_CMD_PARAMETER_DRC");
#if TCFG_DRC_ENABLE
        spin_lock(&p_eq_cfg->lock);
        memcpy(&p_eq_cfg->param[p_eq_cfg->eq_ch].drc, packet->data, sizeof(struct drc_ch));
        /* log_info("p_eq_cfg->param[p_eq_cfg->eq_ch].drc.limiter[0].threshold[0] %d\n", p_eq_cfg->param[p_eq_cfg->eq_ch].drc._p.limiter[0].threshold[0] ); */
        spin_unlock(&p_eq_cfg->lock);
#endif
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static int eq_online_nor_cmd(EQ_ONLINE_PACKET *packet)
{
    if (p_eq_cfg->eq_type != EQ_TYPE_ONLINE) {
        return -EPERM;
    }
    if (packet->cmd == EQ_ONLINE_CMD_GETVER) {
        struct eq_ver_info {
            char sdkname[16];
            u8 eqver[4];
        };
        struct eq_ver_info eq_ver_info = {0};
        memcpy(eq_ver_info.sdkname, audio_eq_sdk_name, sizeof(audio_eq_sdk_name));
        memcpy(eq_ver_info.eqver, audio_eq_ver, sizeof(audio_eq_ver));
        ci_send_packet(EQ_CONFIG_ID, (u8 *)&eq_ver_info, sizeof(struct eq_ver_info));
        return 0;
	}else if (packet->cmd == EQ_ONLINE_CMD_GET_SECTION_NUM) {
		uint8_t hw_section = EQ_SECTION_MAX;
		u32 id = 0x7;
		ci_send_packet(id, (u8 *)&hw_section, sizeof(uint8_t));
		//log_i("hw_section %d\n", hw_section);
		return 0;
	}else if (packet->cmd == EQ_ONLINE_CMD_GLOBAL_GAIN_SUPPORT_FLOAT){
		uint8_t support_float = 1;
		u32 id = 0x8;
		ci_send_packet(id, (u8 *)&support_float, sizeof(uint8_t));
		//log_i("support_float %d\n", support_float);
		return 0;
	}

    return -EINVAL;
}

static void eq_online_callback(uint8_t *packet, uint16_t size)
{
    s32 res;
    if (!p_eq_cfg) {
        return ;
    }

    ASSERT(((int)packet & 0x3) == 0, "buf %x size %d\n", packet, size);
    res = eq_online_update((EQ_ONLINE_PACKET *)packet);
    /* log_info("EQ payload"); */
    /* log_info_hexdump(packet, sizeof(EQ_ONLINE_PACKET)); */

    u32 id = EQ_CONFIG_ID;

    if (res == 0) {
        log_info("Ack");
        ci_send_packet(id, (u8 *)"OK", 2);
#if TCFG_USER_TWS_ENABLE
        eq_online_tws_send();
#endif
#if TCFG_DRC_ENABLE
        {
            EQ_ONLINE_PACKET *pkt = (EQ_ONLINE_PACKET *)packet;
            if (pkt->cmd == EQ_ONLINE_CMD_PARAMETER_DRC) {
                p_eq_cfg->drc_updata[p_eq_cfg->eq_ch] = 1;
                return ;
            }
        }
#endif
        spin_lock(&p_eq_cfg->lock);
        p_eq_cfg->online_updata[p_eq_cfg->eq_ch] = 1;
        spin_unlock(&p_eq_cfg->lock);
    } else {
        res = eq_online_nor_cmd((EQ_ONLINE_PACKET *)packet);
        if (res == 0) {
            return ;
        }
        /* log_info("Nack"); */
        ci_send_packet(id, (u8 *)"ER", 2);
    }
}

static int eq_online_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    int *coeff = NULL;
    if (!sr) {
        return -1;
    }
    ASSERT(p_eq_cfg);
    /* if (p_eq_cfg->eq_type != EQ_TYPE_ONLINE) { */
    /* return -1; */
    /* } */
    if ((sr != p_eq_cfg->cur_sr[p_eq_cfg->eq_ch]) || (p_eq_cfg->online_updata[p_eq_cfg->eq_ch])) {
        //在线请求coeff
        spin_lock(&p_eq_cfg->lock);
        if (sr != p_eq_cfg->cur_sr[p_eq_cfg->eq_ch]) {
            p_eq_cfg->design_mask[p_eq_cfg->eq_ch] = (u32) - 1;
        }
        eq_coeff_set(sr, p_eq_cfg->eq_ch);
        p_eq_cfg->cur_sr[p_eq_cfg->eq_ch] = sr;
        p_eq_cfg->online_updata[p_eq_cfg->eq_ch] = 0;
        spin_unlock(&p_eq_cfg->lock);
    }
#ifdef CONFIG_EQ_NO_USE_COEFF_TABLE
    info->no_coeff = 1;
    info->sr = sr;

#if (EQ_CH_NUM == 4)
    info->L_coeff = (void *)p_eq_cfg->param[0].seg;
    info->R_coeff = (void *)p_eq_cfg->param[1].seg;
    info->L_gain = p_eq_cfg->param[0].global_gain;
    info->R_gain = p_eq_cfg->param[1].global_gain;
#else
    coeff = p_eq_cfg->param[0].seg;
    info->L_coeff = info->R_coeff = (void *)coeff;
    info->L_gain = info->R_gain = p_eq_cfg->param[0].global_gain;
#endif

#else

#if (EQ_CH_NUM == 4)
    info->L_coeff = (void *)p_eq_cfg->EQ_Coeff_table[0];
    info->R_coeff = (void *)p_eq_cfg->EQ_Coeff_table[1];
    info->L_gain = p_eq_cfg->param[0].global_gain;
    info->R_gain = p_eq_cfg->param[1].global_gain;
#else
    coeff = p_eq_cfg->EQ_Coeff_table[0];
    info->L_coeff = info->R_coeff = (void *)coeff;
    info->L_gain = info->R_gain = p_eq_cfg->param[0].global_gain;
#endif

#endif
    info->nsection = p_eq_cfg->seg_num;
    return 0;
}

static int eq_online_open(void)
{
    int i;
    int ch;
    spin_lock(&p_eq_cfg->lock);
    p_eq_cfg->eq_type = EQ_TYPE_ONLINE;
    p_eq_cfg->seg_num = EQ_SECTION_MAX;
    for (ch = 0; ch < EQ_CH_NUM; ch++) {
        p_eq_cfg->param[ch].global_gain = 0;
        for (i = 0; i < EQ_SECTION_MAX; i++) {
            memcpy(&p_eq_cfg->param[ch].seg[i], &eq_seg_nor, sizeof(EQ_CFG_SEG));
        }
    }
    for (ch = 0; ch < EQ_CH_NUM; ch++) {
        p_eq_cfg->design_mask[ch] = (u32) - 1;
        eq_coeff_set(p_eq_cfg->cur_sr[ch], ch);
    }
    spin_unlock(&p_eq_cfg->lock);

#if TCFG_USER_TWS_ENABLE
    eq_online_tws_open();
#endif
    return 0;
}
static void eq_online_close(void)
{
#if TCFG_USER_TWS_ENABLE
    eq_online_tws_close();
#endif
}

REGISTER_CONFIG_TARGET(eq_config_target) = {
    .id         = EQ_CONFIG_ID,
    .callback   = eq_online_callback,
};

//EQ在线调试不进power down
static u8 eq_online_idle_query(void)
{
    if (!p_eq_cfg) {
        return 1;
    }
    return 0;
}

REGISTER_LP_TARGET(eq_online_lp_target) = {
    .name = "eq_online",
    .is_idle = eq_online_idle_query,
};
#endif /*TCFG_EQ_ONLINE_ENABLE*/


/*-----------------------------------------------------------*/
int eq_get_filter_info(int sr, struct audio_eq_filter_info *info)
{
    /* log_info("%s, sr:%d, cur sr:%d, type:%d \n", __func__, sr, p_eq_cfg->cur_sr, p_eq_cfg->eq_type); */
    switch (p_eq_cfg->eq_type) {
    case EQ_TYPE_MODE_TAB:
        return eq_mode_get_filter_info(sr, info);
#if TCFG_EQ_FILE_ENABLE
    case EQ_TYPE_FILE:
        return eq_file_get_filter_info(sr, info);
#endif
#if TCFG_EQ_ONLINE_ENABLE
    case EQ_TYPE_ONLINE:
        return eq_online_get_filter_info(sr, info);
#endif
    }
    return -1;
}


void eq_get_cfg_info(struct audio_eq_cfg_info *cfg)
{
    ASSERT(p_eq_cfg);
}

/*-----------------------------------------------------------*/
int drc_get_filter_info(struct audio_drc_filter_info *info)
{
#if TCFG_DRC_ENABLE
    ASSERT(p_eq_cfg);

#if (EQ_CH_NUM == 4)
    for (int ch = 0; ch < 2/* EQ_CH_NUM */; ch++) {
#else
    for (int ch = 0; ch < EQ_CH_NUM ; ch++) {
#endif
        p_eq_cfg->drc_updata[ch] = 0;
    }

#if (EQ_CH_NUM == 4)
    info->pch = &p_eq_cfg->param[0].drc;
    info->R_pch = &p_eq_cfg->param[1].drc;
#else
    info->pch = info->R_pch = &p_eq_cfg->param[0].drc;
#endif
#endif
    return 0;
}



/*-----------------------------------------------------------*/
// app
void eq_app_run_check(struct audio_eq *eq)
{
    ASSERT(p_eq_cfg);
#if TCFG_EQ_ONLINE_ENABLE
    if (p_eq_cfg->online_updata[p_eq_cfg->eq_ch] && eq->online_en) {
        if ((eq->eq_name == 1) && (p_eq_cfg->eq_ch >= 2)) {
#if (EQ_CH_NUM == 4)
            eq->cb = eq_online_get_filter_info2;
            eq->updata = 1;
#endif
        } else if ((eq->eq_name == 0) && (p_eq_cfg->eq_ch < 2)) {
            eq->cb = eq_online_get_filter_info;
            eq->updata = 1;
        }
    }
#endif
    if (p_eq_cfg->mode_updata && eq->mode_en) {
        if (eq->eq_name == 1) {
#if (EQ_CH_NUM == 4)
            eq->cb = eq_mode_get_filter_info2;
            eq->updata = 1;
#endif
        } else {
            eq->cb = eq_mode_get_filter_info;
            eq->updata = 1;
        }

#if (EQ_CH_NUM == 4)
        static u8 cnt = 0;
        cnt++;
        if (cnt >= 2) {
            cnt = 0;
            p_eq_cfg->mode_updata = 0;
        }
#else
        p_eq_cfg->mode_updata = 0;
#endif
    }
}

void drc_app_run_check(struct audio_drc *drc)
{
#if TCFG_DRC_ENABLE
    ASSERT(p_eq_cfg);

    if (p_eq_cfg->drc_updata[p_eq_cfg->eq_ch] && drc->online_en) {
        log_info("drc->drc_name %d, p_eq_cfg->eq_ch %d\n", drc->drc_name, p_eq_cfg->eq_ch);
        if ((drc->drc_name == 1) && (p_eq_cfg->eq_ch >= 2)) {
#if (EQ_CH_NUM == 4)
            p_eq_cfg->drc_updata[p_eq_cfg->eq_ch] = 0;
            drc->cb = drc_get_filter_info2;
            drc->updata = 1;
#endif
        } else if ((drc->drc_name == 0) && (p_eq_cfg->eq_ch < 2)) {
            p_eq_cfg->drc_updata[p_eq_cfg->eq_ch] = 0;
            drc->cb = drc_get_filter_info;
            drc->updata = 1;
        }
    }
#endif
}

/*-----------------------------------------------------------*/
void eq_cfg_open(void)
{
    if (p_eq_cfg == NULL) {
        p_eq_cfg = malloc(sizeof(EQ_CFG));
#ifndef CONFIG_EQ_NO_USE_COEFF_TABLE
        ASSERT(p_eq_cfg->EQ_Coeff_table);
#endif
    }
    memset(p_eq_cfg, 0, sizeof(EQ_CFG));
    for (int ch = 0; ch < EQ_CH_NUM; ch++) {
        p_eq_cfg->design_mask[ch] = (u32) - 1;
    }

    spin_lock_init(&p_eq_cfg->lock);

#if TCFG_EQ_FILE_ENABLE
    p_eq_cfg->eq_type = EQ_TYPE_FILE;
    for (int ch = 0; ch < EQ_CH_NUM; ch++) {
        p_eq_cfg->cur_sr[ch] = 44100;
    }

    if (eq_file_get_cfg(p_eq_cfg)) //获取EQ文件失败
#endif
    {
        p_eq_cfg->eq_type = EQ_TYPE_MODE_TAB;
        if (EQ_SECTION_MAX > 10) {
            p_eq_cfg->seg_num = 10;
        } else {
            p_eq_cfg->seg_num = EQ_SECTION_MAX;
        }

        for (int i = 0; i < EQ_CH_NUM; i++) {
            p_eq_cfg->param[i].global_gain = 0;
        }
    }

#if TCFG_EQ_ONLINE_ENABLE
    eq_online_open();
#endif
}

void eq_cfg_close(void)
{
    if (!p_eq_cfg) {
        return ;
    }

#if TCFG_EQ_ONLINE_ENABLE
    eq_online_close();
#endif

    void *ptr = p_eq_cfg;
    p_eq_cfg = NULL;
    free(ptr);
}

int eq_init(void)
{
    audio_eq_init();

    eq_cfg_open();

    return 0;
}
__initcall(eq_init);

#endif

