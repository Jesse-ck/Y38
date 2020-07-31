#ifndef _FM_SEVER__H_
#define _FM_SEVER__H_

#include "system/app_core.h"
#include "system/includes.h"
#include "server/server_core.h"
#include "media/includes.h"
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
#include "asm/pwm_led.h"
#include "user_cfg.h"
#include "fm/fm.h"
#include "fm/fm_manage.h"
#include "fm_rw.h"
#include "ui/ui_api.h"
#include "audio_reverb.h"
#include "clock_cfg.h"
#include "includes.h"

#define LOG_TAG_CONST       APP_FM
#define LOG_TAG             "[APP_FM]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"



#define  FM_MSG_EXIT       (0xff)
#define  FM_MSG_USER       (0xfe)

#define  FM_MUSIC_PP       (0x01)
#define  FM_SCAN_ALL_UP    (0x02)
#define  FM_SCAN_ALL_DOWN  (0x03)
#define  FM_PREV_STATION   (0x04)
#define  FM_NEXT_STATION   (0x05)
#define  FM_PREV_FREQ      (0x06)
#define  FM_NEXT_FREQ      (0x07)
#define  FM_VOLUME_UP      (0x08)
#define  FM_VOLUME_DOWN    (0x09)
#define  FM_SCAN_UP        (0x0a)
#define  FM_SCAN_DOWN      (0x0b)//半自动搜台


void fm_server_create();
void fm_sever_kill();
int  fm_server_msg_post(int msg);







#endif
