#ifndef __MUSIC_PLAYER_API_H__
#define __MUSIC_PLAYER_API_H__

#include "system/app_core.h"
#include "system/includes.h"
#include "server/server_core.h"
#include "media/audio_decoder.h"
#include "file_operate/file_operate.h"

enum {
    MPLY_MSG_FIRST_START = 0x0,
    MPLY_MSG_FILE_PREV,
    MPLY_MSG_FILE_NEXT,
    MPLY_MSG_FF,
    MPLY_MSG_FR,
    MPLY_MSG_PP,
    MPLY_MSG_SET_FILE_RPT,
    MPLY_MSG_CHG_FILE_RPT,
    MPLY_MSG_CHG_DEV_RPT,
    MPLY_MSG_CHG_DEV,
    MPLY_MSG_DEV_EVENT,
    MPLY_MSG_DEC_EVENT,
    MPLY_MSG_DEC_RELEASE,
    MPLY_MSG_DEC_STOP,
    MPLY_MSG_FILE_BY_DEV_FILENUM,
    MPLY_MSG_FILE_BY_DEV_SCLUST,
    MPLY_MSG_FILE_BY_DEV_PATH,
    MPLY_MSG_FLASH_FILE_BY_DEV_FILENUM,
    MPLY_MSG_FILE_BY_NEXT_CHG_FOLDER,
    MPLY_MSG_PLAY_RECORD_FOLDER_SWITCH,
};

void music_play_usb_host_mount_before(void);
void music_play_usb_host_mount_after(void);
u8 music_play_get_rpt_mode(void);
int music_play_get_status(void);
char *music_play_get_cur_dev(void);
int music_play_get_cur_time(void);
int music_play_get_total_time(void);
int music_play_get_file_number(void);
int music_play_get_file_total_file(void);
FS_DISP_INFO *music_play_get_file_info(void);
int music_play_file_next(void);
int music_play_file_prev(void);
int music_play_file_pp(void);
int music_play_ff(int step);
int music_play_fr(int step);
int music_play_set_cycle_mode(u8 cyc);
int music_play_set_repeat_mode_auto(void);
int music_play_change_dev_next(void);
int music_play_change_dev_repeat_mode_auto(void);
int music_play_first_start(const char *logo);
int music_play_device_event_deal(u32 evt_src, u8 evt);
int music_play_init(void);
int music_play_uninit(void);


int music_flash_play_file_by_dev_filenum(u32 filenum);
int music_play_file_by_dev_filenum(char *dev_logo, u32 filenum);
int music_play_file_by_dev_sclust(char *dev_logo, u32 sclust);
int music_play_file_by_dev_path(char *dev_logo, char *path);
int music_play_file_by_change_folder(u8 direct);
int music_play_record_folder_file_switch(void);

#endif//__MUSIC_PLAYER_API_H__
