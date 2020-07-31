#include "music/music_player_api.h"
#include <stdlib.h>
#include "system/app_core.h"
#include "system/includes.h"
#include "server/server_core.h"
#include "app_config.h"
#include "app_action.h"
#include "tone_player.h"
#include "asm/charge.h"
#include "app_charge.h"
#include "app_main.h"
#include "app_online_cfg.h"
#include "app_power_manage.h"
#include "gSensor/gSensor_manage.h"
#include "ui_manage.h"
#include "vm.h"
#include "app_chargestore.h"
#include "key_event_deal.h"
#include "music/music.h"
#include "asm/pwm_led.h"
#include "user_cfg.h"
#include "usb/otg.h"
#include "usb/host/usb_host.h"
#include "device/sdmmc.h"
#include "ui/ui_api.h"
#include "fm_emitter/fm_emitter_manage.h"
#include "spi/nor_fs.h"


#define LOG_TAG_CONST       APP_MUSIC
#define LOG_TAG             "[APP_MUSIC]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#if TCFG_APP_MUSIC_EN

#define FILT_RECORD_FILE_EN			0///录音文件夹过滤功能使能, 默认关闭
#define RECORD_FOLDER_PATH		"/JL_REC"


#define UDISK_MOUNT_PAUSE_DEC		1

#define DEC_SWITCH_DEV_USE_BP		1

#define DEV_CYCLE_ALL		0
#define DEV_CYCLE_ONE		1
#define DEV_CYCLE_MAX		2

#define MPLY_TASK_NAME		"music_ply"


struct music_opr {
    MUSIC_PLAYER *mplay;
    int err_cnt;
    u32 msg_tmr;
    u8 next_mode;
    volatile u8 release;
    volatile u8 stop;
    volatile u8 force_stop;
    MOPR_BP *bp;
};

#if (defined(TCFG_DEC_APE_ENABLE) && (TCFG_DEC_APE_ENABLE))
#define MUSIC_BP_DATA_LEN   (2036 + 4)
#elif (defined(TCFG_DEC_FLAC_ENABLE) && (TCFG_DEC_FLAC_ENABLE))
#define MUSIC_BP_DATA_LEN	688
#elif (defined(TCFG_DEC_M4A_ENABLE) && (TCFG_DEC_M4A_ENABLE))
#define MUSIC_BP_DATA_LEN	536
#endif

#ifndef MUSIC_BP_DATA_LEN
#define MUSIC_BP_DATA_LEN	32
#endif

#if (FILT_RECORD_FILE_EN)
static const u8 MUSIC_SCAN_PARAM[] = "-t"
#if (defined(TCFG_DEC_MP3_ENABLE) && (TCFG_DEC_MP3_ENABLE))
                                     "MP1MP2MP3"
#endif
#if (defined(TCFG_DEC_WMA_ENABLE) && (TCFG_DEC_WMA_ENABLE))
                                     "WMA"
#endif
#if (defined(TCFG_DEC_WAV_ENABLE) && (TCFG_DEC_WAV_ENABLE)) || (defined(TCFG_DEC_DTS_ENABLE) && (TCFG_DEC_DTS_ENABLE))
                                     "WAVDTS"
#endif
#if (defined(TCFG_DEC_FLAC_ENABLE) && (TCFG_DEC_FLAC_ENABLE))
                                     "FLA"
#endif
#if (defined(TCFG_DEC_APE_ENABLE) && (TCFG_DEC_APE_ENABLE))
                                     "APE"
#endif
#if (defined(TCFG_DEC_M4A_ENABLE) && (TCFG_DEC_M4A_ENABLE))
                                     "M4AMP4AAC"
#endif
#if (defined(TCFG_DEC_AMR_ENABLE) && (TCFG_DEC_AMR_ENABLE))
                                     "AMR"
#endif
#if (defined(TCFG_DEC_DECRYPT_ENABLE) && (TCFG_DEC_DECRYPT_ENABLE))
                                     "SMP"
#endif
                                     " -sn -r"
                                     " -m"
                                     "JL_REC"
                                     ;
#else
static const u8 MUSIC_SCAN_PARAM[] = "-t"
#if (defined(TCFG_DEC_MP3_ENABLE) && (TCFG_DEC_MP3_ENABLE))
                                     "MP1MP2MP3"
#endif
#if (defined(TCFG_DEC_WMA_ENABLE) && (TCFG_DEC_WMA_ENABLE))
                                     "WMA"
#endif
#if (defined(TCFG_DEC_WAV_ENABLE) && (TCFG_DEC_WAV_ENABLE)) || (defined(TCFG_DEC_DTS_ENABLE) && (TCFG_DEC_DTS_ENABLE))
                                     "WAVDTS"
#endif
#if (defined(TCFG_DEC_FLAC_ENABLE) && (TCFG_DEC_FLAC_ENABLE))
                                     "FLA"
#endif
#if (defined(TCFG_DEC_APE_ENABLE) && (TCFG_DEC_APE_ENABLE))
                                     "APE"
#endif
#if (defined(TCFG_DEC_M4A_ENABLE) && (TCFG_DEC_M4A_ENABLE))
                                     "M4AMP4AAC"
#endif
#if (defined(TCFG_DEC_AMR_ENABLE) && (TCFG_DEC_AMR_ENABLE))
                                     "AMR"
#endif
#if (defined(TCFG_DEC_DECRYPT_ENABLE) && (TCFG_DEC_DECRYPT_ENABLE))
                                     "SMP"
#endif
                                     " -sn -r"
                                     ;
#endif//FILT_RECORD_FILE_EN
static u8 music_cycle_mde = FCYCLE_ALL;
static u8 music_dev_cycle_mde = DEV_CYCLE_ALL;
static struct music_opr *__this = NULL;


extern void sys_event_timeout_set(int timeout);
extern int sys_event_timeout_get(void);

extern void tone_event_to_user(u8 event);

extern bool file_dec_is_stop(void);
extern bool file_dec_is_play(void);
extern bool file_dec_is_pause(void);
extern int file_dec_pp(void);
extern int file_dec_FF(int step);
extern int file_dec_FR(int step);

extern int file_dec_get_total_time(void);
extern int file_dec_get_cur_time(void);

extern int dev_preprocessor_open(void);
extern void dev_preprocessor_close(void);



extern void ff_scan_notify_cancle(void);

static u8 music_last_onoff = (u8) - 1;
/* static u8 music_first_in = 1; */
static u8 music_bt_back_flag = 0;
static u8 music_poweron_first_play = 1;
extern u8 bt_back_flag;

#if UDISK_MOUNT_PAUSE_DEC
static u8 usb_mount_music_pause_flag = 0;
#endif




int music_play_msg_post(int argc, ...)
{
    if (!strcmp(os_current_task(), MPLY_TASK_NAME)) {
        return 1;
    }

    int ret = 0;
    int argv[8];
    va_list argptr;

    va_start(argptr, argc);
    for (int i = 0; i < argc; i++) {
        argv[i] = va_arg(argptr, int);
    }
    va_end(argptr);
    ret = __os_taskq_post(MPLY_TASK_NAME, Q_MSG, argc, argv);

    if (ret) {
        printf("%s, ret = %x\n", __FUNCTION__, ret);
        return -1;
    }

    return 0;
}


static int _music_play_start(MUSIC_PLAYER *m_ply, struct audio_dec_breakpoint *bp)
{
    /* printf("_music_play_start in\n"); */
    int ret = 	music_ply_start(m_ply, bp);
    printf(" _music_play_start out %d, cur dev = %s\n", ret, m_ply->fopr->dev->logo);
    if (!ret) {
        __this->err_cnt = 0;
#if TCFG_UI_ENABLE
        u16 file_num = music_play_get_file_number();
        ui_set_tmp_menu(MENU_FILENUM, 1000, file_num, NULL);
#endif
    }
    return ret;
}

// music play event
static void _music_play_dec_event_callback(void *priv, int argc, int *argv)
{
    os_taskq_post_msg(MPLY_TASK_NAME, 3, MPLY_MSG_DEC_EVENT, argv[0], argv[1]);
}

static int music_play_by_dev_bp(u8 dev_sel, int sel_param)
{
    printf("music_play_by_dev_bp start\n");

    music_ply_set_recplay_status(__this->mplay, 0);

    int ret = 0;
    if (!__this->mplay) {
        __this->err_cnt = 0;
        __this->mplay = music_ply_create(NULL, _music_play_dec_event_callback);
        if (!__this->mplay) {
            return MPLY_ERR_MALLOC;
        }
        __this->mplay->mbp = __this->bp;
    }
    FILE_OPR_SEL_DEV sel_dev = {dev_sel, sel_param};
    FILE_OPR_SEL_FILE sel_file = {0, music_cycle_mde, 0, MUSIC_SCAN_PARAM, NULL, 0};
    ret = music_ply_bp_file_open(__this->mplay, &sel_dev, &sel_file, __this->bp);
    if (ret) {
        log_error("play err \n");
        return ret;
    }
    ret = _music_play_start(__this->mplay, &__this->bp->bp);

    return ret;
}

static int music_play_base_by_dev(u8 dev_sel, int sel_param, u8 fil_sel)
{
    music_ply_set_recplay_status(__this->mplay, 0);

    int ret = 0;
    if (!__this->mplay) {
        __this->err_cnt = 0;
        __this->mplay = music_ply_create(NULL, _music_play_dec_event_callback);
        if (!__this->mplay) {
            return MPLY_ERR_MALLOC;
        }
        __this->mplay->mbp = __this->bp;
    }
    FILE_OPR_SEL_DEV sel_dev = {dev_sel, sel_param};
    FILE_OPR_SEL_FILE sel_file = {fil_sel, music_cycle_mde, 0, MUSIC_SCAN_PARAM, NULL, 0};
    ret = music_ply_file_open(__this->mplay, &sel_dev, &sel_file);
    if (ret) {
        log_error("play err \n");
        return ret;
    }
    ret = _music_play_start(__this->mplay, NULL);

    return ret;
}

static int music_play_by_dev(u8 dev_sel, int sel_param)
{
    return music_play_base_by_dev(dev_sel, sel_param, FSEL_FIRST_FILE);
}

static int music_play_by_file(u8 file_sel, int sel_param)
{
    if (music_ply_get_recplay_status(__this->mplay) == 0) {
        if ((music_dev_cycle_mde == DEV_CYCLE_ALL) && (file_opr_available_dev_total() > 1)
            && (music_cycle_mde == FCYCLE_ALL)
            && (__this->mplay && __this->mplay->fopr && __this->mplay->fopr->fsn)
           ) {
            switch (file_sel) {
            case FSEL_AUTO_FILE:
            case FSEL_NEXT_FILE:
                if (__this->mplay->fopr->fsn->file_counter >= __this->mplay->fopr->totalfile) {
                    music_ply_clear_bp(__this->mplay);
                    return music_play_by_dev(DEV_SEL_NEXT, 0);
                }
                break;
            case FSEL_PREV_FILE:
                if (__this->mplay->fopr->fsn->file_counter <= 1) {
                    music_ply_clear_bp(__this->mplay);
                    return music_play_base_by_dev(DEV_SEL_PREV, 0, FSEL_LAST_FILE);
                }
                break;
            }
        }
    }

    int ret = 0;
    FILE_OPR_SEL_FILE sel_file = {0};
    sel_file.file_sel = file_sel;
    sel_file.sel_param = sel_param;
    ret = music_ply_file_open(__this->mplay, NULL, &sel_file);
    if (ret) {
        return ret;
    }
    ret = _music_play_start(__this->mplay, NULL);

    return ret;
}


void music_play_usb_host_mount_before(void)
{
#if UDISK_MOUNT_PAUSE_DEC
    if (true == app_cur_task_check(APP_NAME_MUSIC)) {
        if (true == file_dec_is_play()) {
            log_info("dec pause \n");
            file_dec_pp();
            usb_mount_music_pause_flag = 1;
        }
    }
#endif
}

void music_play_usb_host_mount_after(void)
{
#if UDISK_MOUNT_PAUSE_DEC
    if (usb_mount_music_pause_flag && (file_opr_available_dev_total())) {
        if (true == file_dec_is_pause()) {
            log_info("dec play \n");
            file_dec_pp();
        }
    }
    usb_mount_music_pause_flag = 0;
#endif
}

u8 music_play_get_rpt_mode(void)
{
    return music_cycle_mde;
}

int music_play_get_status(void)
{
    if (true == file_dec_is_play()) {
        return true;
    }
    return false;
}

char *music_play_get_cur_dev(void)
{
    if (__this && __this->mplay && __this->mplay->fopr && __this->mplay->fopr->dev) {
        if (__this->mplay->fopr->dev->dev_name) {
            return __this->mplay->fopr->dev->dev_name;
        }
        return __this->mplay->fopr->dev->logo;
    }
    return NULL;
}

int music_play_get_cur_time(void)
{
    return file_dec_get_cur_time();
}

int music_play_get_total_time(void)
{
    return file_dec_get_total_time();
}

int music_play_get_file_number(void)
{
    if (__this && __this->mplay && __this->mplay->fopr && __this->mplay->fopr->fsn) {
        return __this->mplay->fopr->fsn->file_counter;
    }
    return 0;
}

int music_play_get_file_total_file(void)
{
    if (__this && __this->mplay && __this->mplay->fopr && __this->mplay->fopr->fsn) {
        return __this->mplay->fopr->totalfile;
    }
    return 0;
}

int music_play_get_dir_number_index(void)
{
    if (__this && __this->mplay && __this->mplay->fopr && __this->mplay->fopr->fsn) {
        return __this->mplay->fopr->fsn->musicdir_counter;
    }
    return 0;

}

int music_play_get_dir_total_number(void)
{
    if (__this && __this->mplay && __this->mplay->fopr && __this->mplay->fopr->fsn) {
        return __this->mplay->fopr->fsn->dir_totalnumber;
    }
    return 0;

}

FS_DISP_INFO *music_play_get_file_info(void)
{
    return file_opr_get_disp_info();
}


int music_play_file_next(void)
{
    int ret = music_play_msg_post(1, MPLY_MSG_FILE_NEXT);
    if (ret != 1) {
        return ret;
    }

    int err;
    __this->next_mode = FSEL_NEXT_FILE;
    err = music_play_by_file(FSEL_NEXT_FILE, 0);
    return err;
}

int music_play_file_prev(void)
{
    int ret = music_play_msg_post(1, MPLY_MSG_FILE_PREV);
    if (ret != 1) {
        return ret;
    }

    int err;
    __this->next_mode = FSEL_PREV_FILE;
    err = music_play_by_file(FSEL_PREV_FILE, 0);
    return err;
}

int music_play_file_pp(void)
{
    int ret = music_play_msg_post(1, MPLY_MSG_PP);
    if (ret != 1) {
        return ret;
    }

    file_dec_pp();
    music_last_onoff = !file_dec_is_pause();
#if TCFG_UI_ENABLE
    ui_menu_reflash(true);
#endif

    return 0;
}

///如果dev_logo == 0， 使用当前设备指定文件序号播放
int music_play_file_by_dev_filenum(char *dev_logo, u32 filenum)
{
    printf("dev_logo 1 = %s\n", dev_logo);
    int ret = music_play_msg_post(3, MPLY_MSG_FILE_BY_DEV_FILENUM, (int)dev_logo, filenum);
    if (ret != 1) {
        return ret;
    }


    music_ply_set_recplay_status(__this->mplay, 0);

    FILE_OPR_SEL_FILE sel_file = {FSEL_BY_NUMBER, music_cycle_mde, filenum, MUSIC_SCAN_PARAM, NULL, 0};
    if (dev_logo) {
        printf("dev_logo = %s\n", dev_logo);
        FILE_OPR_SEL_DEV sel_dev = {DEV_SEL_SPEC, (int)dev_logo};
        ret = music_ply_file_open(__this->mplay, &sel_dev, &sel_file);
    } else {
        ret = music_ply_file_open(__this->mplay, NULL, &sel_file);
    }
    if (ret) {
        log_error("play err \n");
        return ret;
    }
    ret = _music_play_start(__this->mplay, NULL);

    return ret;
}


int music_play_file_by_dev_sclust(char *dev_logo, u32 sclust)
{
    int ret = music_play_msg_post(3, MPLY_MSG_FILE_BY_DEV_SCLUST, (int)dev_logo, sclust);
    if (ret != 1) {
        return ret;
    }

    music_ply_set_recplay_status(__this->mplay, 0);

    if (dev_logo) {
        printf("dev_logo = %s\n", dev_logo);
        FILE_OPR_SEL_FILE sel_file = {FSEL_BY_SCLUST, music_cycle_mde, sclust, MUSIC_SCAN_PARAM, NULL, 0};
        FILE_OPR_SEL_DEV sel_dev = {DEV_SEL_SPEC, (int)dev_logo};
        ret = music_ply_file_open(__this->mplay, &sel_dev, &sel_file);
    } else {
        FILE_OPR_SEL_FILE sel_file = {FSEL_BY_SCLUST, music_cycle_mde, sclust, 0, NULL, 0};
        ret = music_ply_file_open(__this->mplay, NULL, &sel_file);
    }
    if (ret) {
        log_error("play err \n");
        return ret;
    }


    ret = _music_play_start(__this->mplay, NULL);

    return ret;

}


int music_play_file_by_dev_path(char *dev_logo, char *path)
{
    int ret = music_play_msg_post(3, MPLY_MSG_FILE_BY_DEV_PATH, (int)dev_logo, path);
    if (ret != 1) {
        return ret;
    }

    music_ply_set_recplay_status(__this->mplay, 0);

    if (dev_logo) {
        printf("dev_logo = %s\n", dev_logo);
        FILE_OPR_SEL_FILE sel_file = {FSEL_BY_PATH, music_cycle_mde, (int)path, MUSIC_SCAN_PARAM, NULL, 0};
        FILE_OPR_SEL_DEV sel_dev = {DEV_SEL_SPEC, (int)dev_logo};
        ret = music_ply_file_open(__this->mplay, &sel_dev, &sel_file);
    } else {
        FILE_OPR_SEL_FILE sel_file = {FSEL_BY_PATH, music_cycle_mde, (int)path, 0, NULL, 0};
        ret = music_ply_file_open(__this->mplay, NULL, &sel_file);
    }
    if (ret) {
        log_error("play err \n");
        return ret;
    }


    ret = _music_play_start(__this->mplay, NULL);

    return ret;

}

int music_flash_play_file_by_dev_filenum(u32 filenum)
{
#if defined(TCFG_NOR_FS_ENABLE) && TCFG_NOR_FS_ENABLE
    int ret = music_play_msg_post(3, MPLY_MSG_FLASH_FILE_BY_DEV_FILENUM, filenum);
    if (ret != 1) {
        return ret;
    }
    music_ply_set_recplay_status(__this->mplay, 0);


    char *dev_logo = "nor_fs";
    FILE_OPR_SEL_DEV sel_dev = {DEV_SEL_SPEC, (int)dev_logo};
    ret = music_ply_file_open(__this->mplay, &sel_dev, NULL);

    ret = music_flash_file_set_index(FSEL_CURR_FILE , filenum);
    if (ret){
        log_e("file error");
        return ret;
    }
    ret = record_file_play();

    return ret;
#endif 
    return 0;
}


int recfile_insizeflash_play_by_filenum(u32 filenum)
{
#if defined(FLASH_INSIDE_REC_ENABLE) && FLASH_INSIDE_REC_ENABLE
    int ret = recfile_insizeflash_set_index(FSEL_CURR_FILE , filenum);
    if (ret){
        log_e("file error");
        return ret;
    }
    ret = record_file_play();

    return ret;
#endif 
    return 0;
}


///direct = 1:下一个文件夹
int music_play_file_by_change_folder(u8 direct)
{
    int ret = music_play_msg_post(2, MPLY_MSG_FILE_BY_NEXT_CHG_FOLDER, direct);
    if (ret != 1) {
        return ret;
    }


    if (music_ply_get_recplay_status(__this->mplay)) {
        printf("playing rec folder!! not support\n");
        return 0;
    }

    int err;
    if (direct) {
        /* __this->next_mode = FSEL_NEXT_FILE; */
        err = music_play_by_file(FSEL_NEXT_FOLDER_FILE, 0);
    } else {
        /* __this->next_mode = FSEL_PREV_FILE; */
        err = music_play_by_file(FSEL_PREV_FOLDER_FILE, 0);
    }
    return err;
}

int music_play_record_folder_file_switch(void)
{
    int ret = music_play_msg_post(2, MPLY_MSG_PLAY_RECORD_FOLDER_SWITCH);
    if (ret != 1) {
        return ret;
    }

    if (music_ply_get_recplay_status(__this->mplay)) {
        printf("is musicplay!!!!\n");
        ret = music_play_by_dev_bp(DEV_SEL_CUR, 0);
    } else {
        music_ply_save_bp(__this->mplay);
        music_ply_set_recplay_status(__this->mplay, 1);

        __this->next_mode = FSEL_NEXT_FILE;

        int err;
        FILE_OPR_SEL_DEV sel_dev = {DEV_SEL_CUR, 0};//选择当前设备
        FILE_OPR_SEL_FILE sel_file = {FSEL_FIRST_FILE, FCYCLE_FOLDER, 0, MUSIC_SCAN_PARAM, RECORD_FOLDER_PATH, strlen(RECORD_FOLDER_PATH)};
        ret = music_ply_file_open(__this->mplay, &sel_dev, &sel_file);
        if (ret) {
            printf("play err \n");
            return ret;
        }

        ret = _music_play_start(__this->mplay, NULL);
        if (ret) {
            printf("rec_play fail, ret = %d\n", ret);
        }
        printf("is recplay!!!!\n");
    }


    return ret;
}



int music_play_ff(int step)
{
    int ret = music_play_msg_post(2, MPLY_MSG_FF, step);
    if (ret != 1) {
        return ret;
    }

    file_dec_FF(step);
    return 0;
}

int music_play_fr(int step)
{
    int ret = music_play_msg_post(2, MPLY_MSG_FR, step);
    if (ret != 1) {
        return ret;
    }

    file_dec_FR(step);
    return 0;
}

// music play set cycle mode
int music_play_set_cycle_mode(u8 cyc)
{
    int ret = music_play_msg_post(2, MPLY_MSG_SET_FILE_RPT, cyc);
    if (ret != 1) {
        return ret;
    }

    if (music_ply_get_recplay_status(__this->mplay)) {
        printf("playing rec folder!! not support\n");
        return 0;
    }

    if (music_cycle_mde == cyc) {
        return 0;
    }
    music_cycle_mde = cyc;
    if (!__this->mplay) {
        return 0;
    }
    log_info("repeat:%d \n", music_cycle_mde);
    ret = file_opr_api_set_cycle_mode(__this->mplay->fopr, music_cycle_mde);
    if (!ret) {
        log_info("music_play_set_cycle_mode succ \n");
    }
    return 0;
}

int music_play_set_repeat_mode_auto(void)
{
    int ret = music_play_msg_post(1, MPLY_MSG_CHG_FILE_RPT);
    if (ret != 1) {
        return ret;
    }

    if (!__this->mplay) {
        return 0;
    }

    if (music_ply_get_recplay_status(__this->mplay)) {
        printf("playing rec folder!! not support\n");
        return 0;
    }

    music_cycle_mde ++;
    if (music_cycle_mde >= FCYCLE_MAX) {
        music_cycle_mde = FCYCLE_ALL;
    }
    log_info("repeat:%d \n", music_cycle_mde);
    ret = file_opr_api_set_cycle_mode(__this->mplay->fopr, music_cycle_mde);
    if (!ret) {
#if TCFG_UI_ENABLE
        ui_set_tmp_menu(MENU_MUSIC_REPEATMODE, 1000, music_cycle_mde, NULL);
#endif//TCFG_UI_ENABLE
        log_info("music_play_set_repeat_mode_auto succ \n");
    }
    return 0;
}

int music_play_change_dev_next(void)
{
    int ret = music_play_msg_post(1, MPLY_MSG_CHG_DEV);
    if (ret != 1) {
        return ret;
    }

    int err = 0;
    if (file_opr_available_dev_total() < 2) {
        log_info("dev total:%d \n", file_opr_available_dev_total());
        return 0;
    }

    music_ply_save_bp(__this->mplay);

#if DEC_SWITCH_DEV_USE_BP
    err = music_play_by_dev_bp(DEV_SEL_NEXT, 0);
#else
    err = music_play_by_dev(DEV_SEL_NEXT, 0);
#endif
    return err;
}

int music_play_change_dev_repeat_mode_auto(void)
{
    int ret = music_play_msg_post(1, MPLY_MSG_CHG_DEV);
    if (ret != 1) {
        return ret;
    }

    u8 dev_repeat = music_dev_cycle_mde + 1;
    if (dev_repeat >= DEV_CYCLE_MAX) {
        dev_repeat = DEV_CYCLE_ALL;
    }
    music_dev_cycle_mde = dev_repeat;
    log_info("dev repeat:%d \n", music_dev_cycle_mde);
    return 0;
}


int music_play_stop(void)
{
    int err = 0;

    if (!(__this && __this->mplay && __this->mplay->fopr)) {
        return 0;
    }
    /* printf(">>>>>>>>>>>>> %s %d \n",__FUNCTION__,__LINE__); */

    if (false == file_dec_is_play()) {
        /* printf(">>>>>>>>>>>>> %s %d \n",__FUNCTION__,__LINE__); */
        if (file_opr_api_get_sel_status(__this->mplay->fopr)) {
            ff_scan_notify_cancle();///尝试停止文件扫描
            __this->force_stop = 1;
            while (file_opr_api_get_sel_status(__this->mplay->fopr)) {
                os_time_dly(1);
            }
            printf("ff_scan_notify_cancle done \n");
        }

        /* printf(">>>>>>>>>>>>> %s %d \n",__FUNCTION__,__LINE__); */
        return 0;
    }
    __this->force_stop = 1;
    __this->stop = 0;

    music_ply_save_bp(__this->mplay);
    do {
        err = music_play_msg_post(1, MPLY_MSG_DEC_STOP);
        if (!err) {
            break;
        }
        printf("music_play_uninit msg err = %x\n", err);
        os_time_dly(5);
    } while (err);

    /* printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s %d \n",__FUNCTION__,__LINE__); */
    while (!__this->stop) {
        os_time_dly(1);
    }

    __this->force_stop = 0;
    /* printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s %d \n",__FUNCTION__,__LINE__); */

    return 0;
}

static void _music_play_err_deal(int err);
int music_play_first_start(const char *logo)
{
    int ret = music_play_msg_post(2, MPLY_MSG_FIRST_START, (int)logo);
    if (ret != 1) {
        return ret;
    }

    int err;

    extern u8 file_dec_start_pause;
    if ((music_bt_back_flag == 2) && (music_last_onoff == 0)) {
        file_dec_start_pause = 1;
    }

    __this->next_mode = FSEL_NEXT_FILE;

    printf("music_play_first_start , logo = %s\n", logo);

    if (logo == NULL) {
        if (music_poweron_first_play == 1) {
            printf("first time play!!!!! _______fun = %s, _line = %d\n", __FUNCTION__, __LINE__);
            u8 last_dev;
            u8 last_dev_logo[10] = {0};
            syscfg_read(VM_MUSIC_LAST_DEV, &last_dev, 1);
            switch (last_dev) {
            case 0x01:
                strcpy(last_dev_logo, "sd0");
                break;
            case 0x02:
                strcpy(last_dev_logo, "sd1");
                break;
            case 0x03:
                strcpy(last_dev_logo, "udisk");
                break;
            default:
                break;
            }
            if (file_opr_available_dev_check(last_dev_logo)) {
                err = music_play_by_dev_bp(DEV_SEL_SPEC, (int)last_dev_logo);
            } else {
                err = music_play_by_dev_bp(DEV_SEL_LAST, 0);
            }
        } else {
            err = music_play_by_dev_bp(DEV_SEL_LAST_ACTIVE, 0);
        }
    } else {
        err = music_play_by_dev_bp(DEV_SEL_SPEC, (int)logo);
    }

    music_poweron_first_play = 0;
    return err;
}


static int muisc_play_decode_event(u8 event, u8 value)
{
    int ret = false;
    int err = 0;
    u16 file_num;

    switch (event) {
    case AUDIO_PLAY_EVENT_END:
        log_info("AUDIO_PLAY_EVENT_END\n");
        if (__this->mplay) {
            u8 read_err = value;
            log_info("read err:%d ", read_err);
            if (__this->force_stop) {
                __this->force_stop = 0;
                printf(">>>>>>>>>>>>> STOP  %s %d \n", __FUNCTION__, __LINE__);
                break;
            }
            if (read_err) {
                ///设备读错误应该是播放下一个设备
                if (file_opr_available_dev_total() >= 2) {
                    music_ply_save_bp(__this->mplay);
                    err = music_play_by_dev_bp(DEV_SEL_NEXT, 0);
                } else {
                    goto __change_mode;
                }
                break;
            }

            music_ply_save_bp(__this->mplay);
            __this->next_mode = FSEL_NEXT_FILE;
            err = music_play_by_file(FSEL_AUTO_FILE, 0);
        }
        break;
    case AUDIO_PLAY_EVENT_ERR:
        log_info("AUDIO_PLAY_EVENT_ERR\n");
        if (__this->mplay->fopr && (!file_opr_available_dev_check(__this->mplay->fopr->dev->logo))) {
            // dev offline
            if (!file_opr_available_dev_total()) {
                log_error("no dev \n");
                goto __change_mode;
            }
            music_ply_save_bp(__this->mplay);
#if DEC_SWITCH_DEV_USE_BP
            err = music_play_by_dev_bp(DEV_SEL_LAST, 0);
#else
            err = music_play_by_dev(DEV_SEL_LAST, 0);
#endif
        } else {
            err = music_play_by_file(FSEL_NEXT_FILE, 0);
        }
        break;
#if 0
    case MUSIC_PLAY_EVENT_CHANGE_MODE:
        log_info("MUSIC_PLAY_EVENT_CHANGE_MODE\n");
        goto __change_mode;
        break;
#endif
    }

    return err;

__change_mode:
    app_task_next();
    return 0;
}

int music_play_device_event_deal(u32 evt_src, u8 evt)
{
    int err = 0;
    const char *logo = evt2dev_map_logo(evt_src);
    if (logo == NULL) {
        return 0;
    }

    if (0 != strcmp(os_current_task(), MPLY_TASK_NAME)) {
        if (__this->mplay
            && __this->mplay->fopr
            && __this->mplay->fopr->dev
            && (!strcmp(logo, __this->mplay->fopr->dev->logo))) {
            ff_scan_notify_cancle();//如果有设备事件， 停止当前文件扫描
        }
    }

    int ret = music_play_msg_post(3, MPLY_MSG_DEV_EVENT, evt_src, evt);
    if (ret != 1) {
        return ret;
    }

    if (evt == DEVICE_EVENT_IN) {
        printf("dev = %s, online>>>>>\n", logo);
        if (file_opr_available_dev_check((void *)logo)) {
            music_ply_save_bp(__this->mplay);
#if DEC_SWITCH_DEV_USE_BP
            err = music_play_by_dev_bp(DEV_SEL_SPEC, (int)logo);
#else
            err = music_play_by_dev(DEV_SEL_SPEC, (int)logo);
#endif//DEC_SWITCH_DEV_USE_BP
        } else {
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
            err = music_play_by_dev_bp(DEV_SEL_CUR, (int)0);
#endif
        }
    } else if (evt == DEVICE_EVENT_OUT) {
        printf("dev = %s, offline>>>>>\n", logo);
        if (__this->mplay
            && __this->mplay->fopr
            && __this->mplay->fopr->dev
            && (!strcmp(logo, __this->mplay->fopr->dev->logo))) {
            //当前播放的设备拔出
            if (file_opr_available_dev_total()) {
                music_ply_save_bp(__this->mplay);
#if DEC_SWITCH_DEV_USE_BP
                err = music_play_by_dev_bp(DEV_SEL_LAST, (int)logo);
#else
                err = music_play_by_dev(DEV_SEL_LAST, (int)logo);
#endif//DEC_SWITCH_DEV_USE_BP
            } else {
                printf("no dev online next app 111111\n");
                ///如果没有更多设备， 切换模式的时候保存断点, 此处不重复保存
                app_task_next();
                return 0;
            }
        }

        if (!file_opr_available_dev_total()) {
            printf("no dev online next app 222222\n");
            app_task_next();
            return 0;
        }
    }

    return err;
}

// total file
static int _music_play_get_file_total(void)
{
    if ((__this->mplay) && (__this->mplay->fopr)) {
        return __this->mplay->fopr->totalfile;
    }
    return 0;
}
// err opr
static void _music_play_err_deal(int err)
{
    int ret, cnt, dev_available;
    if (!err) {
        return ;
    }

    if (__this->release) {
        return ;
    }
    if (__this->force_stop) {
        printf("__this->force_stop TRUE%d\n", __LINE__);
        __this->force_stop = 0;
        return ;
    }


#if (FILT_RECORD_FILE_EN)
    if ((err == FOPR_ERR_NO_FILE) && (music_ply_get_recplay_status(__this->mplay) == 0)) {
        printf("FOPR_ERR_NO_FILE ++++++\n");
        err = music_play_record_folder_file_switch();
        if (!err) {
            return ;
        }
        if (__this->release) {
            return ;
        }
        if (__this->force_stop) {
            printf("__this->force_stop TRUE%d\n", __LINE__);
            __this->force_stop = 0;
            return ;
        }
    }
#endif

__err_opr:
    if ((__this->err_cnt >= _music_play_get_file_total()) || (__this->err_cnt > 20)) {

        if (music_ply_get_recplay_status(__this->mplay)) {
            printf("err overlimit __rec_play_err\n");
            goto __rec_play_err;
        }

        ret = -10;
        printf("err overlimit __err_end\n");
        goto __err_end;
    }
    __this->err_cnt++;
    ret = 0;

    /* log_info("music err:0x%x \n", err - FOPR_ERR_POINT);
    log_info("music err:0x%x \n", err - MPLY_ERR_POINT); */
    switch (err) {
    case FOPR_ERR_POINT:
    case FOPR_ERR_MALLOC:
        ret = -1;
        break;
    case FOPR_ERR_DEV_NO_FIND:
#if DEC_SWITCH_DEV_USE_BP
        ret = music_play_by_dev_bp(DEV_SEL_LAST, 0);
#else
        ret = music_play_by_dev(DEV_SEL_LAST, 0);
#endif
        if (ret && (ret != FOPR_ERR_DEV_NO_FIND)) {
            err = ret;
            goto __err_opr;
        }
        break;
    case FOPR_ERR_DEV_MOUNT:
    case FOPR_ERR_FSCAN:
    case FOPR_ERR_NO_FILE:
    case FOPR_ERR_FSEL:

        if ((err = FOPR_ERR_FSCAN) || (err == FOPR_ERR_NO_FILE)) {
            if (music_ply_get_recplay_status(__this->mplay)) {
                goto __rec_play_err;
            }
        }

        /* printf("cur %s dev mark offline\n ", __this->mplay->fopr->dev->logo); */

        file_opr_available_dev_offline(__this->mplay->fopr->dev);
        dev_available = file_opr_available_dev_total();
        /* printf("dev_available = %d\n", dev_available); */
        if (dev_available == 0) {
            ret = -3;
            break;
        }

        music_ply_save_bp(__this->mplay);
#if DEC_SWITCH_DEV_USE_BP
        ret = music_play_by_dev_bp(DEV_SEL_NEXT, 0);
#else
        ret = music_play_by_dev(DEV_SEL_NEXT, 0);
#endif
        if (ret) {
            err = ret;
            goto __err_opr;
        }
        break;
    case MPLY_ERR_SERVER_CMD:
        printf("MPLY_ERR_SERVER_CMD\n");
        ret = music_play_by_file(__this->next_mode, 0);
        if (ret) {
            err = ret;
            goto __err_opr;
        }
        break;
    case MPLY_ERR_BP:
        ret = music_play_by_file(FSEL_FIRST_FILE, 0);
        if (ret) {
            err = ret;
            goto __err_opr;
        }
        break;

    default:
        ret = -2;
        break;
    }

__err_end:
    if (ret) {
        if (file_opr_available_dev_total() >= 2) {
            __this->err_cnt = 0;
            music_ply_save_bp(__this->mplay);
#if DEC_SWITCH_DEV_USE_BP
            err = music_play_by_dev_bp(DEV_SEL_NEXT, 0);
#else
            err = music_play_by_dev(DEV_SEL_NEXT, 0);
#endif
            if (!err) {
                return;
            }

            goto __err_opr;
        }
        app_task_next();
    }
    return ;

__rec_play_err:
    printf("__rec_play_err \n");
    ret = music_play_by_dev_bp(DEV_SEL_CUR, 0);
    if (ret) {
        printf("__rec_play_err, ret ==========%d\n", ret);
        err = ret;
        goto __err_opr;
    }
    return;
}

static void music_player_task(void *p)
{
    int err = 0;
    int ret = 0;
    int msg[16];
    while (1) {
        /* ret = __os_taskq_pend(msg, ARRAY_SIZE(msg), 0); */
        ret = os_task_pend("taskq", msg, ARRAY_SIZE(msg));
        /* printf("music_player_task, ret = %x\n", ret); */
        if (ret != OS_TASKQ) {
            continue;
        }

        if (msg[0] != Q_MSG) {
            continue;
        }

        printf("mply msg = %x\n", msg[1]);

        switch (msg[1]) {
        case MPLY_MSG_DEC_RELEASE:
            if (__this &&  __this->mplay) {
                music_ply_release(__this->mplay);
                __this->mplay = NULL;
            }
            os_sem_post((OS_SEM *)msg[2]);
            while (1) {
                os_time_dly(10000);
            }
            break;
        case MPLY_MSG_DEC_STOP:
            music_ply_stop(__this->mplay);
            __this->stop = 1;
            break;
        case MPLY_MSG_FIRST_START:
            err = music_play_first_start((const char *)msg[2]);
            break;
        case MPLY_MSG_FILE_PREV:
            err = music_play_file_prev();
            break;
        case MPLY_MSG_FILE_NEXT:
            err = music_play_file_next();
            break;
        case MPLY_MSG_FF:
            err = music_play_ff(msg[2]);
            break;
        case MPLY_MSG_FR:
            err = music_play_fr(msg[2]);
            break;
        case MPLY_MSG_PP:
            err = music_play_file_pp();
            break;
        case MPLY_MSG_CHG_FILE_RPT:
            err = music_play_set_repeat_mode_auto();
            break;
        case MPLY_MSG_CHG_DEV_RPT:
            err = music_play_change_dev_repeat_mode_auto();
            break;
        case MPLY_MSG_CHG_DEV:
            err = music_play_change_dev_next();
            break;
        case MPLY_MSG_DEV_EVENT:
            err = music_play_device_event_deal((u32)msg[2], (u8)msg[3]);
            break;
        case MPLY_MSG_DEC_EVENT:
            err = muisc_play_decode_event((u8)msg[2], (u8)msg[3]);
            break;
        case MPLY_MSG_FILE_BY_DEV_FILENUM:
            err = music_play_file_by_dev_filenum((char *)msg[2], msg[3]);
            break;
        case MPLY_MSG_FILE_BY_DEV_SCLUST:
            err = music_play_file_by_dev_sclust((char *)msg[2], msg[3]);
            break;
        case MPLY_MSG_FILE_BY_DEV_PATH:
            err = music_play_file_by_dev_path((char *)msg[2], (char *) msg[3]);
            break;
        case MPLY_MSG_FLASH_FILE_BY_DEV_FILENUM:
            err = music_flash_play_file_by_dev_filenum(msg[2]);
            break;
        case MPLY_MSG_FILE_BY_NEXT_CHG_FOLDER:
            err = music_play_file_by_change_folder((u8)msg[2]);
            break;
        case MPLY_MSG_PLAY_RECORD_FOLDER_SWITCH:
            err = music_play_record_folder_file_switch();
            break;

        default:
            break;
        }
        _music_play_err_deal(err);
    }
}

int music_play_init(void)
{
    int err;
    if (!__this) {
        __this = zalloc(sizeof(struct music_opr) + sizeof(MOPR_BP) + MUSIC_BP_DATA_LEN);
        ASSERT(__this);
        __this->bp = (MOPR_BP *)((int)__this + sizeof(struct music_opr));
        __this->bp->bp.data_len = MUSIC_BP_DATA_LEN;

        err = task_create(music_player_task, NULL, MPLY_TASK_NAME);
        if (err != OS_NO_ERR) {
            printf("music_ply task creat fail %x\n", err);
            return -1;
        }
        printf("music_play_init succ\n");
    }
    return 0;
}

int music_play_uninit(void)
{
    int err;

    __this->release = 1;
    ff_scan_notify_cancle();///尝试停止文件扫描

    OS_SEM *sem = zalloc(sizeof(OS_SEM));
    os_sem_create(sem, 0);

    do {
        err = music_play_msg_post(2, MPLY_MSG_DEC_RELEASE, (int)sem);
        if (!err) {
            break;
        }
        printf("music_play_uninit msg err = %x\n", err);
        os_time_dly(5);
    } while (err);

    os_sem_pend(sem, 0);
    free(sem);

    err = task_kill(MPLY_TASK_NAME);

    if (__this) {
        free(__this);
        __this = NULL;
    }


    printf("music_play_uninit %x\n", err);

    return err;
}


#endif// TCFG_APP_MUSIC_EN

