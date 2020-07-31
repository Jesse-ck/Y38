
#include "system/app_core.h"
#include "system/includes.h"
#include "server/server_core.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_action.h"
#include "tone_player.h"
#include "asm/charge.h"
#include "asm/usb.h"
#include "app_charge.h"
#include "app_main.h"
#include "app_online_cfg.h"
#include "app_power_manage.h"
#include "ui_manage.h"
#include "app_chargestore.h"
#include "key_event_deal.h"
#include "os/os_api.h"
#include "usb/usb_config.h"
#include "usb/device/usb_stack.h"
#include "usb/device/hid.h"
#include "usb/device/msd.h"
#include "uac_stream.h"
#include "pc/pc.h"
#include "user_cfg.h"
#include "device/sdmmc.h"
#include "pc/usb_msd.h"
#include "ui/ui_api.h"
#include "audio_reverb.h"
#include "clock_cfg.h"


#if TCFG_APP_PC_EN

#define LOG_TAG_CONST        APP_PC
#define LOG_TAG             "[APP_PC]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


struct pc_opr {
    u8 volume;
    u8 onoff : 1;
};

static struct pc_opr pc_hdl = {0};
#define __this 	(&pc_hdl)


extern struct dac_platform_data dac_data;
extern void charge_event_to_user(u8 event);
extern void tone_event_to_user(u8 event);
extern int usb_audio_and_tone_play(u8 start);
extern void usbstack_init();
extern void usbstack_exit();
extern void usb_start();
extern void usb_stop();
extern void usb_pause();
extern void bt_tws_sync_volume();

static int pc_volume_pp(void);

#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
/* static u8 msg_notify_enable = 1; */
extern u8 msg_notify_enable; */

extern int m_sdio_supend(u8 num);
extern void m_sdio_resume(u8 num);
extern void usb_otg_resume(usb_dev usb_id);
extern void usb_otg_suspend(usb_dev usb_id, u8 uninstall);

/* int sd_notify_enable() */
/* { */
    /* return msg_notify_enable; */
/* } */

extern int sd_notify_enable();

static void pc_dm_multiplex_init()
{
    msg_notify_enable  = 0;
    m_sdio_resume(2);
    m_sdio_supend(2);
    gpio_direction_output(TCFG_USB_SD_MULTIPLEX_IO, 0);
    os_time_dly(200);
    gpio_set_direction(TCFG_USB_SD_MULTIPLEX_IO, 1);
    gpio_set_pull_up(TCFG_USB_SD_MULTIPLEX_IO, 0);
    gpio_set_pull_down(TCFG_USB_SD_MULTIPLEX_IO, 0);
    gpio_set_die(TCFG_USB_SD_MULTIPLEX_IO, 0);
}


static void pc_dm_multiplex_exit()
{
    m_sdio_resume(2);
    usb_otg_resume(0);
    msg_notify_enable  = 1;
}

#endif

static void pc_start()
{
    if (__this->onoff) {
        log_info("PC is start ");
        return ;
    }
    __this->onoff = 1;
    log_info("App Start - PC");
    usb_start();
}

static void pc_stop()
{
    if (!__this->onoff) {
        log_info("PC is stop ");
        return ;
    }
    __this->onoff = 0;

    log_info("App Stop - PC");
    usb_stop();
}

static void pc_hold()
{
    if (!__this->onoff) {
        log_info("PC is hold");
        return ;
    }
    __this->onoff = 0;

    log_info("App Hold- PC");
    usb_pause();
}


// tone play event
static int _tone_event_opr(struct device_event *dev)
{
    int err = 0;
    int ret = false;

    switch (dev->event) {
    case AUDIO_PLAY_EVENT_END:
        log_info(">>>%s\n", __func__);
        break;
    }
    return ret;
}
/* extern u8 pc_key_table[KEY_NUM_MAX][KEY_EVENT_MAX]; */
extern u8 pc_key_event_get(struct key_event *key);
static int _key_event_opr(struct sys_event *event)
{
    int ret = true;
    int err = 0;
    struct key_event *key = &event->u.key;
    u8 key_event = pc_key_event_get(key);//pc_key_table[key->value][key->event];
    /* log_info("key value:%d, event:%d \n", key->value, key->event); */
    log_info("key_event:%d \n", key_event);

    switch (key_event) {
    case  KEY_MUSIC_PP:
        log_info("KEY_MUSIC_PP\n");
        hid_key_handler(0, USB_AUDIO_PP);
        break;
    case  KEY_MUSIC_PREV:
        log_info("KEY_MUSIC_PREV\n");
        hid_key_handler(0, USB_AUDIO_PREFILE);
        break;
    case  KEY_MUSIC_NEXT:
        log_info("KEY_MUSIC_NEXT\n");
        hid_key_handler(0, USB_AUDIO_NEXTFILE);
        break;
    case  KEY_VOL_UP:
        log_info("pc KEY_VOL_UP\n");
        hid_key_handler(0, USB_AUDIO_VOLUP);
        printf(">>>pc vol+: %d", app_audio_get_volume(APP_AUDIO_CURRENT_STATE));

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        bt_tws_sync_volume();
#endif
        break;
    case  KEY_VOL_DOWN:
        log_info("pc KEY_VOL_DOWN\n");
        hid_key_handler(0, USB_AUDIO_VOLDOWN);
        printf(">>>pc vol-: %d", app_audio_get_volume(APP_AUDIO_CURRENT_STATE));

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        bt_tws_sync_volume();
#endif
        break;
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
    case KEY_REVERB_OPEN:
    case KEY_REVERB_DEEPVAL_UP:
    case KEY_REVERB_DEEPVAL_DOWN:
        return true;
        break;
#endif
    default:
        /* app_earphone_key_event_handler(key); */
        ret = false;
        break;
    }

    return ret;
}


// pc play event
static int _pc_event_opr(struct device_event *dev)
{
    int ret = true;
    int err = 0;

    switch (dev->event) {
    case AUDIO_PLAY_EVENT_END:
        log_info("AUDIO_PLAY_EVENT_END\n");
        break;
    case AUDIO_PLAY_EVENT_ERR:
        break;
    default:
        ret = false;
        break;
    }

    return ret;
}


static int pc_event_handler(struct application *app, struct sys_event *event)
{
    switch (event->type) {
    case SYS_KEY_EVENT:
        return _key_event_opr(event);

    case SYS_DEVICE_EVENT:
        if ((u32)event->arg == DEVICE_EVENT_FROM_TONE) {
            return  _tone_event_opr(&event->u.dev);
        } else {
            if (pc_sys_event_handler(event) == 2) {
                pc_stop();
                app_task_next();
            }
        }
        return false;

    default:
        return false;
    }
    return false;
}

static int pc_app_check(void)
{
    log_info("pc_app_check\n");
    if (usb_otg_online(0) == SLAVE_MODE) {
        return true;
    }
    return false;
}


static void pc_app_init(void)
{

#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    pc_dm_multiplex_init();
#endif

#if TCFG_UI_ENABLE
    ui_set_main_menu(UI_PC_MENU_MAIN);
    ui_set_tmp_menu(MENU_PC, 1000, 0, NULL);
#endif
    ui_update_status(STATUS_PC_MODE);
    /* 按键消息使能 */
    sys_key_event_enable();

    __this->volume =  app_audio_get_volume(APP_AUDIO_STATE_MUSIC);

    pc_start();
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    usb_otg_resume(0);
#endif
}

static void pc_app_uninit(void)
{
    if (pc_app_check() == false) {
        pc_stop();
    } else {
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
        usb_otg_suspend(0, 0);
#endif
        pc_hold();
    }

    tone_play_stop();
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    pc_dm_multiplex_exit();
#endif

}

static int pc_state_machine(struct application *app, enum app_state state, struct intent *it)
{
    int ret;
    switch (state) {
    case APP_STA_CREATE:
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
        reverb_pause();
#endif

        break;
    case APP_STA_START:
        if (!it) {
            break;
        }
        switch (it->action) {
        case ACTION_APP_MAIN:
            log_info("ACTION_APP_MAIN\n");
            pc_app_init();
            break;
        }
        break;
    case APP_STA_PAUSE:
        log_info("APP_STA_PAUSE\n");
        /* pc_stop(); */
        pc_hold();
        break;
    case APP_STA_RESUME:
        log_info("APP_STA_RESUME\n");
        break;
    case APP_STA_STOP:
        log_info("APP_STA_STOP\n");
        break;
    case APP_STA_DESTROY:
        log_info("APP_STA_DESTROY\n");
        pc_app_uninit();
        app_audio_set_volume(APP_AUDIO_STATE_MUSIC, __this->volume, 1);
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        bt_tws_sync_volume();
#endif

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
        reverb_resume();
#endif
        break;
    }

    return 0;
}


static int pc_user_msg_deal(int msg, int argc, int *argv)
{
    switch (msg) {
    case USER_MSG_SYS_MIXER_RECORD_SWITCH:
        ///pc模式不响应混合录音
        return 1;
    default:
        break;
    }
    return 0;
}

static const struct application_reg app_pc_reg = {
    .tone_name = TONE_PC,
    .tone_play_check = NULL,
    .tone_prepare = NULL,
    .enter_check = pc_app_check,
    .exit_check = NULL,
    .user_msg = pc_user_msg_deal,
#if (defined(TCFG_TONE2TWS_ENABLE) && (TCFG_TONE2TWS_ENABLE))
    .tone_tws_cmd = SYNC_CMD_MODE_PC,
#endif
};

static const struct application_operation app_pc_ops = {
    .state_machine  = pc_state_machine,
    .event_handler 	= pc_event_handler,
};

REGISTER_APPLICATION(app_app_pc) = {
    .name 	= APP_NAME_PC,
    .action	= ACTION_APP_MAIN,
    .ops 	= &app_pc_ops,
    .state  = APP_STA_DESTROY,
    .private_data = (void *) &app_pc_reg,
};


#endif /* TCFG_APP_PC_EN */

