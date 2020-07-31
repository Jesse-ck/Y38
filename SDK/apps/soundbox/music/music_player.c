
#include "app_config.h"
#include "music/music_player.h"
#include "app_action.h"
#include "audio_config.h"
#include "app_main.h"
#include "app_online_cfg.h"
#include "media/includes.h"
#include "app_api/vm_api.h"
#include "system/device/vm.h"
#include "clock_cfg.h"

#if TCFG_APP_MUSIC_EN

/* extern struct dac_platform_data dac_data; */

#define LOG_TAG_CONST       APP_MUSIC
#define LOG_TAG             "[APP_MUSIC_PLY]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

extern int file_dec_create(void *priv, void (*handler)(void *, int argc, int *argv));
extern int file_dec_open(void *file, struct audio_dec_breakpoint *bp);
extern void file_dec_close();

extern int file_dec_get_breakpoint(struct audio_dec_breakpoint *bp);

extern bool file_dec_is_stop(void);
extern bool file_dec_is_play(void);
extern bool file_dec_is_pause(void);
extern int file_dec_pp(void);

static void music_ply_save_last_active_dev_check(MUSIC_PLAYER *m_ply)
{
    if (m_ply == NULL) {
        return;
    }

    struct storage_dev *dev = file_opr_available_dev_get_last_active();
    if (dev) {
        if (strcmp(dev->logo, m_ply->fopr->dev->logo)) {
            //上一次活动设备跟当前选定设备不一致， 保存一下最新活动设备
            /* printf("_______fun = %s, _line = %d\n", __FUNCTION__, __LINE__); */
            u8 last_dev;
            if (!strcmp(m_ply->fopr->dev->logo, "sd0")) {
                last_dev = 0x01;
            } else if (!strcmp(m_ply->fopr->dev->logo, "sd1")) {
                last_dev = 0x02;
            } else if (!strcmp(m_ply->fopr->dev->logo, "udisk")) {
                last_dev = 0x03;
            } else {
                last_dev = 0x00;
            }
            log_info("MUSIC save last dev %s %d\n", m_ply->fopr->dev->logo, last_dev);
            syscfg_write(VM_MUSIC_LAST_DEV, &last_dev, 1);
        }
    }
    storage_dev_active_mark(m_ply->fopr->dev->logo);
}

static void _music_dec_server_close(MOPR_DEC *m_dec)
{
    if (m_dec) {
        if (m_dec->decoder) {
            file_dec_close();
            /* app_audio_state_switch(APP_AUDIO_STATE_IDLE, 0); */
            m_dec->decoder = NULL;
        }
    }
}
static int _music_dec_server_open(MUSIC_PLAYER *m_ply)
{
    if ((!m_ply) || (!m_ply->mdec)) {
        return MPLY_ERR_POINT;
    }
    _music_dec_server_close(m_ply->mdec);

    int ret = file_dec_create(m_ply->mdec->event_priv, m_ply->mdec->event_handler);
    if (ret) {
        return MPLY_ERR_POINT;
    }
    m_ply->mdec->decoder = (void *)1;

    return 0;
}

void music_ply_write_breakpoint(MUSIC_PLAYER *m_ply, u8 get_bp)
{
    if ((!m_ply) || (!m_ply->mbp) || (!m_ply->mdec) || (!m_ply->mdec->decoder) || (!m_ply->fopr) || (!m_ply->fopr->file)) {
        return ;
    }

    if (file_opr_api_get_recplay_status(m_ply->fopr)) {
        printf("rec play, no breakpoint save\n");
        return ;
    }

    u16 vm_id;
    if (!strcmp(m_ply->fopr->dev->logo, "sd0")) {
        vm_id = CFG_SD0_BREAKPOINT0;
    } else if (!strcmp(m_ply->fopr->dev->logo, "sd1")) {
        vm_id = CFG_SD1_BREAKPOINT0;
    } else if (!strcmp(m_ply->fopr->dev->logo, "udisk")) {
        vm_id = CFG_USB_BREAKPOINT0;
    } else {
        log_error("bp dev err \n");
        return ;
    }

    if ((!m_ply->mbp->fsize) || (!m_ply->mbp->sclust)) {
        return ;
    }

    if (get_bp) {
        log_info("bp datalen:%d ", m_ply->mbp->bp.data_len);
        int ret = file_dec_get_breakpoint(&m_ply->mbp->bp);
        if (ret) {
            return ;
        }
    } else {
        /* memset(&m_ply->mbp->bp, 0, sizeof(struct audio_dec_breakpoint)); */
        m_ply->mbp->bp.len = 0;
        m_ply->mbp->bp.fptr = 0;
    }

    /* log_info("save dec bp ,dev = %s\n", m_ply->fopr->dev->logo); */
    /* put_buf(m_ply->mbp, FILE_BP_LEN(m_ply->mbp)); */
    /* syscfg_write(vm_id, m_ply->mbp, FILE_BP_LEN(m_ply->mbp)); */
    vm_api_write_mult(vm_id, vm_id + 10, m_ply->mbp, FILE_BP_LEN(m_ply->mbp), 2);
}

void music_ply_save_bp(MUSIC_PLAYER *m_ply)
{
    music_ply_write_breakpoint(m_ply, 1);
}

void music_ply_clear_bp(MUSIC_PLAYER *m_ply)
{
    if ((!m_ply) || (!m_ply->mbp)) {
        return ;
    }

    u16 vm_id;
    if (!strcmp(m_ply->fopr->dev->logo, "sd0")) {
        vm_id = CFG_SD0_BREAKPOINT0;
    } else if (!strcmp(m_ply->fopr->dev->logo, "sd1")) {
        vm_id = CFG_SD1_BREAKPOINT0;
    } else if (!strcmp(m_ply->fopr->dev->logo, "udisk")) {
        vm_id = CFG_USB_BREAKPOINT0;
    } else {
        log_error("bp dev err \n");
        return ;
    }

    int bp_len = m_ply->mbp->bp.data_len;
    memset(m_ply->mbp, 0, sizeof(MOPR_BP));
    m_ply->mbp->bp.data_len = bp_len;

    log_info("clear dec bp ");
    /* syscfg_write(vm_id, m_ply->mbp, FILE_BP_LEN(m_ply->mbp)); */
    vm_api_write_mult(vm_id, vm_id + 10, m_ply->mbp, FILE_BP_LEN(m_ply->mbp), 2);
}

int music_ply_start(MUSIC_PLAYER *m_ply, struct audio_dec_breakpoint *bp)
{
    int ret = 0;

    if ((!m_ply) || (!m_ply->mdec)) {
        return MPLY_ERR_POINT;
    }
    music_ply_stop(m_ply);

    ret = _music_dec_server_open(m_ply);
    if (ret) {
        return ret;
    }

    if ((!m_ply->mdec->decoder) || (!m_ply->fopr) || (!m_ply->fopr->file)) {
        return MPLY_ERR_POINT;
    }

    struct vfs_attr attr = {0};
    ///file size check, filesize = 0?
    fget_attrs(m_ply->fopr->file, &attr);
    if (attr.fsize == 0) {
        printf("dec file size = 0!!!!!!\n");
        return MPLY_ERR_SERVER_CMD;
    }



///整理VM
    vm_check_all(0);

    ret = file_dec_open(m_ply->fopr->file, bp);
    if (ret) {
        ret = MPLY_ERR_SERVER_CMD;
    }


    if (!ret) {
        if ((m_ply->mbp)) {
            /* fget_attrs(m_ply->fopr->file, &attr); */
            m_ply->mbp->fsize = attr.fsize;
            m_ply->mbp->sclust = attr.sclust;
        }

        if (!bp) {
            music_ply_write_breakpoint(m_ply, 0);
        }
        /* printf("_______fun = %s, _line = %d\n", __FUNCTION__, __LINE__); */
        music_ply_save_last_active_dev_check(m_ply);
    }

    return ret;
}


int music_ply_stop(MUSIC_PLAYER *m_ply)
{
    int ret = 0;
    if (!m_ply) {
        return MPLY_ERR_POINT;
    }
    if (!m_ply->mdec) {
        return 0;
    }
    if (!m_ply->mdec->decoder) {
        return 0;
    }

    if (true == file_dec_is_stop()) {
        return 0;
    }
    /* music_ply_save_bp(m_ply);	 */
    log_info("dec stop ");
    _music_dec_server_close(m_ply->mdec);

    return 0;
}

int music_ply_file_open(MUSIC_PLAYER *m_ply, FILE_OPR_SEL_DEV *dev_sel, FILE_OPR_SEL_FILE *file_sel)
{
    int ret = 0;
    if ((!m_ply) || (!m_ply->fopr)) {
        return MPLY_ERR_POINT;
    }

    music_ply_stop(m_ply);

    clock_add_set(FINDF_CLK);
    file_opr_api_set_sel_status(m_ply->fopr, 1);
    if (dev_sel) {
        ret = file_opr_api_sel_dev(m_ply->fopr, dev_sel);
        if (ret) {
            log_info("fopr dev err:0x%x \n", ret);
            file_opr_api_set_sel_status(m_ply->fopr, 0);
            clock_remove_set(FINDF_CLK);
            return ret;
        }
    }
    if (file_sel) {
        ret = file_opr_api_sel_file(m_ply->fopr, file_sel);
        if (ret) {
            log_info("fopr file err:0x%x \n", ret - FOPR_ERR_POINT);
            if (ret == FOPR_ERR_FSEL) {
                ret = MPLY_ERR_BP;
            }
            file_opr_api_set_sel_status(m_ply->fopr, 0);
            clock_remove_set(FINDF_CLK);
            return ret;
        }
    }
    file_opr_api_set_sel_status(m_ply->fopr, 0);
    clock_remove_set(FINDF_CLK);
    return ret;
}

int music_ply_bp_file_open(MUSIC_PLAYER *m_ply, FILE_OPR_SEL_DEV *dev_sel, FILE_OPR_SEL_FILE *file_sel, MOPR_BP *mbp)
{
    int ret = 0;
    if ((!m_ply) || (!m_ply->fopr) || (!file_sel)) {
        return MPLY_ERR_POINT;
    }
    if (!mbp) {
        log_error("bp hdl err \n");
        return MPLY_ERR_POINT;
    }

    music_ply_stop(m_ply);

    clock_add_set(FINDF_CLK);
    file_opr_api_set_sel_status(m_ply->fopr, 1);
    if (dev_sel) {
        ret = file_opr_api_sel_dev(m_ply->fopr, dev_sel);
        if (ret) {
            log_info("fopr dev err:0x%x \n", ret);
            file_opr_api_set_sel_status(m_ply->fopr, 0);
            clock_remove_set(FINDF_CLK);
            return ret;
        }
    }

    u16 vm_id;
    if (!strcmp(m_ply->fopr->dev->logo, "sd0")) {
        vm_id = CFG_SD0_BREAKPOINT0;
    } else if (!strcmp(m_ply->fopr->dev->logo, "sd1")) {
        vm_id = CFG_SD1_BREAKPOINT0;
    } else if (!strcmp(m_ply->fopr->dev->logo, "udisk")) {
        vm_id = CFG_USB_BREAKPOINT0;
    } else {
        log_error("bp dev err \n");
        goto __err_bp;
    }
    int bp_len = mbp->bp.data_len;
    memset(mbp, 0, sizeof(MOPR_BP));

    /* syscfg_read(vm_id, mbp, sizeof(MOPR_BP) + bp_len); */
    if (0 == vm_api_read_mult(vm_id, vm_id + 10, mbp, sizeof(MOPR_BP) + bp_len)) {
        printf("vm api read err :\n");
        /* put_buf(mbp, sizeof(MOPR_BP) + bp_len); */

        mbp->bp.data_len = bp_len;
        goto __err_bp;
    } else {
        printf("vm read %s ok!!\n", m_ply->fopr->dev->logo);
    }

    mbp->bp.data_len = bp_len;
    if ((!mbp->fsize) || (!mbp->sclust)) {
        log_error("bp vm err \n");
        goto __err_bp;
    }
    file_sel->file_sel = FSEL_BY_SCLUST;
    file_sel->sel_param = mbp->sclust;

    extern void pb_point_debug_save(void *buf, u32 size);
    pb_point_debug_save(mbp, sizeof(MOPR_BP) + bp_len);

    ret = file_opr_api_sel_file(m_ply->fopr, file_sel);
    if (ret) {
        log_info("fopr file err:0x%x \n", ret - FOPR_ERR_POINT);
        if (ret == FOPR_ERR_FSEL) {
            ret = MPLY_ERR_BP;
        }
        file_opr_api_set_sel_status(m_ply->fopr, 0);
        clock_remove_set(FINDF_CLK);
        return ret;
    }
    file_opr_api_set_sel_status(m_ply->fopr, 0);
    clock_remove_set(FINDF_CLK);

    struct vfs_attr attr = {0};
    fget_attrs(m_ply->fopr->file, &attr);
    log_info("file attr:0x%x, fsize:%d, fsclust:%d \n", attr.attr, attr.fsize, attr.sclust);

    if (attr.fsize != mbp->fsize) {
        log_error("bp file size:%d err \n", attr.fsize);
        return MPLY_ERR_BP;
    }

    return ret;

__err_bp:
    ret = file_opr_api_scan_init(m_ply->fopr, file_sel);
    file_opr_api_set_sel_status(m_ply->fopr, 0);
    clock_remove_set(FINDF_CLK);
    if (ret) {
        return ret;
    }
    return MPLY_ERR_BP;
}

void music_ply_set_recplay_status(MUSIC_PLAYER *m_ply, u8 status)
{
    if (m_ply) {
        file_opr_api_set_recplay_status(m_ply->fopr, status);
    }
}

u8 music_ply_get_recplay_status(MUSIC_PLAYER *m_ply)
{
    if (m_ply) {
        return file_opr_api_get_recplay_status(m_ply->fopr);
    }

    return 0;
}

MUSIC_PLAYER *music_ply_create(void *priv, void (*handler)(void *, int argc, int *argv))
{
    MUSIC_PLAYER *m_ply = zalloc(sizeof(MUSIC_PLAYER));
    if (!m_ply) {
        log_error("malloc err \n");
        return NULL;
    }

    m_ply->fopr = file_opr_api_create();
    if (!m_ply->fopr) {
        log_info("fopr create err \n");
        goto __err;
    }

    m_ply->mdec = zalloc(sizeof(MOPR_DEC));
    if (!m_ply->mdec) {
        log_info("mdec malloc err \n");
        goto __err;
    }
    m_ply->mdec->event_handler = handler;
    m_ply->mdec->event_priv = priv;

    return m_ply;

__err:
    if (m_ply) {
        music_ply_release(m_ply);
    }
    return NULL;
}

void music_ply_release(MUSIC_PLAYER *m_ply)
{
    music_ply_save_bp(m_ply);

    music_ply_stop(m_ply);

    if (m_ply) {
        if (m_ply->mdec) {
            _music_dec_server_close(m_ply->mdec);
            free(m_ply->mdec);
            m_ply->mdec = NULL;
        }
        if (m_ply->fopr) {
            file_opr_api_release(m_ply->fopr);
            m_ply->fopr = NULL;
        }
        free(m_ply);
    }
}

#endif

