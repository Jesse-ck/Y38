#include "common/app_common.h"
#include "app_action.h"
#include "app_main.h"
#include "key_event_deal.h"
#include "music/music.h"
#include "pc/pc.h"
#include "record/record.h"
#include "linein/linein.h"
#include "fm/fm.h"
#include "btstack/avctp_user.h"
#include "app_power_manage.h"
#include "app_chargestore.h"
#include "usb/otg.h"
#include "usb/host/usb_host.h"
#include <stdlib.h>
#include "bt_tws.h"
#include "app_api/app_debug_api.h"
#include "music/music_player_api.h"
#include "audio_config.h"
#include "common/power_off.h"
#include "common/user_msg.h"
#include "audio_config.h"
#include "audio_enc.h"
#include "ui/ui_api.h"
#include "fm_emitter/fm_emitter_manage.h"
#include "common/fm_emitter_led7_ui.h"
#include "audio_reverb.h"
#if TCFG_CHARGE_ENABLE
#include "app_charge.h"
#endif
#include "charge_box/charge_ctrl.h"
#include "device/chargebox.h"


#include "app_online_cfg.h"


#define LOG_TAG_CONST       APP_ACTION
#define LOG_TAG             "[APP_ACTION]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


extern int app_sys_bt_event_opr(struct sys_event *event);
extern u32 timer_get_ms(void);
extern int alarm_sys_event_handler(struct sys_event *event);
extern void bt_tws_sync_volume();

#if (TCFG_SPI_LCD_ENABLE)
extern int key_is_ui_takeover();
extern int ui_key_msg_post(int msg);
#endif

extern s32 sd_io_suspend(u8 sd_io);
extern s32 sd_io_resume(u8 sd_io);
extern int music_play_stop(void);
static usb_dev g_usb_id = (usb_dev) - 1;
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
#define USB_ACTIVE 0x1
#define SD_ACTIVE  0x2

static u8 sd_first_online = 0;
static u8 mult_curr_dev = 0;

#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0

u8 msg_notify_enable = 1;

int sd_notify_enable()
{
    return msg_notify_enable;
}

static u8 flag_tp1 = 0;
static u32 flag_tp2 = 0;

#endif



static int resume = 0;
void m_sdio_resume(u8 num)
{
    if (resume) {
        resume--;
        if (!resume) {
            sd_io_resume(num);
        }
    }
}

int m_sdio_supend(u8 num)
{
__try_t:
    if (sd_io_suspend(num)) {
        os_time_dly(1);
        goto __try_t;
    }
    resume++;
    return 0;
}
#endif
int dev_sd_change_usb()
{
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    if (mult_curr_dev != SD_ACTIVE) {
        return 0;
    }
    printf("sd_change_usb start \n");

#if 1
    m_sdio_resume(2);
__try_1:
    if (m_sdio_supend(2)) {
        os_time_dly(1);
        goto __try_1;
    }
#endif
    gpio_set_direction(TCFG_USB_SD_MULTIPLEX_IO, 1);
    gpio_set_pull_up(TCFG_USB_SD_MULTIPLEX_IO, 0);
    gpio_set_pull_down(TCFG_USB_SD_MULTIPLEX_IO, 0);
    gpio_set_die(TCFG_USB_SD_MULTIPLEX_IO, 0);
    printf("sd_change_usb finsh \n");
    usb_otg_io_resume(g_usb_id);
    /* usb_host_resume(usb_id); */
    if (usb_host_remount(g_usb_id, MOUNT_RETRY, MOUNT_RESET, MOUNT_TIMEOUT, 0)) {
        log_error("udisk remount fail\n");
    } else {
        if (file_opr_dev_check("udisk")) {
            file_opr_dev_del((void *)"udisk");
            file_opr_dev_add("udisk");
        }
    }
    mult_curr_dev = USB_ACTIVE;
#endif
    return 0;
}

int dev_usb_change_sd()
{
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    if (mult_curr_dev != USB_ACTIVE) {
        return 0;
    }

    if (g_usb_id != (usb_dev) - 1 && usb_otg_online(g_usb_id) == HOST_MODE) {
        printf("dev_usb_change_sd start \n");
        usb_otg_io_suspend(g_usb_id);
#if 1
        m_sdio_resume(2);
#endif
        mult_curr_dev = SD_ACTIVE;
        os_time_dly(3);
        printf("dev_usb_change_sd finsh \n");
        return 0;
    }
    return -1;
#endif
    return true;
}


static int sd_online_mount_before(usb_dev usb_id)
{
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    sd_first_online = 1;
    if (usb_id != (usb_dev) - 1 && usb_otg_online(usb_id) == HOST_MODE) {
        printf("suspend dm\n");
        music_play_stop();
        usb_otg_io_suspend(usb_id);
#if 1
        m_sdio_resume(2);
#endif
    }
#endif
    return 0;
}

static int sd_online_mount_fail(usb_dev usb_id)
{

    int ret = 0;
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    struct storage_dev *udisk_stor;
    if (usb_id != (usb_dev) - 1 && usb_otg_online(usb_id) == HOST_MODE) {
        m_sdio_supend(2);
        gpio_set_direction(TCFG_USB_SD_MULTIPLEX_IO, 1);
        gpio_set_pull_up(TCFG_USB_SD_MULTIPLEX_IO, 0);
        gpio_set_pull_down(TCFG_USB_SD_MULTIPLEX_IO, 0);
        gpio_set_die(TCFG_USB_SD_MULTIPLEX_IO, 0);
        usb_otg_io_resume(usb_id);
        /* usb_host_resume(usb_id); */
        if (usb_host_remount(usb_id, MOUNT_RETRY, MOUNT_RESET, MOUNT_TIMEOUT, 0)) {
            log_error("udisk remount fail\n");
        } else {
            if (file_opr_dev_check("udisk")) {
                file_opr_dev_del((void *)"udisk");
                file_opr_dev_add("udisk");
            }
            mult_curr_dev = USB_ACTIVE;
        }
    }
#endif
    return ret;
}

static int sd_online_mount_after()
{
    int ret = 0;
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    mult_curr_dev = SD_ACTIVE;
#endif
    return ret;
}
static int sd_offline_before(void *logo, usb_dev usb_id)
{
    int ret = 0 ;

#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    if (!sd_first_online) { //没有收到过上线消息不处理下线
        return 0;
    }

    struct storage_dev *udisk_stor;
    if (usb_id != (usb_dev) - 1 && usb_otg_online(usb_id) == HOST_MODE && mult_curr_dev != USB_ACTIVE) {
        printf("resume dm\n");

        music_play_stop();

        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s %d \n", __FUNCTION__, __LINE__);

        if (file_opr_dev_check(logo)) {
            file_opr_dev_del((void *)logo);
        }
#if 1

        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s %d \n", __FUNCTION__, __LINE__);

        m_sdio_resume(2);
__try_2:
        if (m_sdio_supend(2)) {
            os_time_dly(1);
            goto __try_2;
        }

        printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> %s %d \n", __FUNCTION__, __LINE__);
        /* sd_clk_resume(2); */
#endif

        gpio_set_direction(TCFG_USB_SD_MULTIPLEX_IO, 1);
        gpio_set_pull_up(TCFG_USB_SD_MULTIPLEX_IO, 0);
        gpio_set_pull_down(TCFG_USB_SD_MULTIPLEX_IO, 0);
        gpio_set_die(TCFG_USB_SD_MULTIPLEX_IO, 0);

        usb_otg_io_resume(usb_id);
        /* usb_host_resume(usb_id); */
        if (usb_host_remount(usb_id, MOUNT_RETRY, MOUNT_RESET, MOUNT_TIMEOUT, 0)) {
            log_error(">>>>>>>>>>> udisk remount fail\n");
        } else {
            if (file_opr_dev_check("udisk")) {
                file_opr_dev_del((void *)"udisk");
                file_opr_dev_add("udisk");
                mult_curr_dev = USB_ACTIVE;
            }
        }
    } else {
        m_sdio_resume(2);
        file_opr_dev_del((void *)logo);
        mult_curr_dev = 0;
    }
#endif
    return ret;
}

static int usb_mount_before(usb_dev usb_id)
{
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    music_play_stop();
    os_time_dly(3);
#if 1
    m_sdio_resume(2);
__try_1:
    if (m_sdio_supend(2)) {
        os_time_dly(1);
        goto __try_1;
    }
    /* sd_clk_resume(2); */
#endif

    gpio_set_direction(TCFG_USB_SD_MULTIPLEX_IO, 1);
    gpio_set_pull_up(TCFG_USB_SD_MULTIPLEX_IO, 0);
    gpio_set_pull_down(TCFG_USB_SD_MULTIPLEX_IO, 0);
    gpio_set_die(TCFG_USB_SD_MULTIPLEX_IO, 0);
#endif
    return 0;
}


static int usb_mount_fail(usb_dev usb_id)
{
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0

#if 1
    m_sdio_resume(2);
#endif
    /* gpio_set_die(TCFG_USB_SD_MULTIPLEX_IO, 1); */
#endif
    return 0;

}

static int usb_online_mount_after(usb_dev usb_id)
{
    int ret = 0;
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
    mult_curr_dev = USB_ACTIVE;
#endif
    return ret;
}

static int usb_mount_offline(usb_dev usb_id)
{
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0

    printf(">>>>>>>>>>>>> %s %d \n", __FUNCTION__, __LINE__);
    usb_otg_io_suspend(usb_id);

#if 1
    m_sdio_resume(2);
#endif
    /* gpio_set_die(TCFG_USB_SD_MULTIPLEX_IO, 1); */
    if (mult_curr_dev == USB_ACTIVE) {
        mult_curr_dev = SD_ACTIVE;
    } else {
        mult_curr_dev = 0;
    }

    printf(">>>>>>>>>>>>> %s %d \n", __FUNCTION__, __LINE__);
#endif
    return 0;

}


void app_event_prepare_handler(struct sys_event *event)
{
    const char *logo = NULL;
    int ret = 0;
    switch ((u32)event->arg) {
    case DRIVER_EVENT_FROM_SD0:
    case DRIVER_EVENT_FROM_SD1:
    case DRIVER_EVENT_FROM_SD2:
#if (defined(CONFIG_FATFS_ENBALE) && CONFIG_FATFS_ENBALE)
        logo = evt2dev_map_logo((u32)event->arg);
        if (logo == NULL) {
            break;
        }

        if (event->u.dev.event == DEVICE_EVENT_IN) {
            printf("sd_online >>>>>>>>>>>>>>>>>> %s\n", logo);
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
            if (!sd_notify_enable()) {
                flag_tp1 = 1;
                flag_tp2 = (u32)event->arg;
                return ;
            }
#endif
            sd_online_mount_before(g_usb_id);
            ret = file_opr_dev_add((void *)logo);
            if (ret) {
                log_error("file_opr_dev_add fail %x\n", ret - STORAGE_DEV_ALREADY);
                sd_online_mount_fail(g_usb_id);

            } else {
                sd_online_mount_after();
            }
        } else if (event->u.dev.event == DEVICE_EVENT_OUT) {
            printf("sd_offline <<<<<<<<<<<<<<<<<< %s\n", logo);

#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
            if (!sd_notify_enable()) {
                flag_tp1 = 2;
                flag_tp2 = (u32)event->arg;
                return ;
            }
#endif

#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
            sd_offline_before((void *)logo, g_usb_id);
#else
            ret = file_opr_dev_del((void *)logo);
            if (ret) {
                log_error("file_opr_dev_del fail %x\n", ret - STORAGE_DEV_ALREADY);
            }
#endif
        }
#endif//CONFIG_FATFS_ENBALE
        break;
    case DEVICE_EVENT_FROM_USB_HOST:
#if TCFG_UDISK_ENABLE
        if (event->u.dev.event == DEVICE_EVENT_IN) {
            if (!strncmp((char *)event->u.dev.value, "udisk", 5)) {
                ///if usb host mount ok, need check udisk fat mount, if mount err , do func music_play_usb_host_mount_after()
                printf("udisk mount\n");
#if (defined(CONFIG_FATFS_ENBALE) && CONFIG_FATFS_ENBALE)
                ret = file_opr_dev_add("udisk");
#endif//CONFIG_FATFS_ENBALE
                if (ret) {
                    log_error("file_opr_dev_add fail 11111 %x\n", ret - STORAGE_DEV_ALREADY);
                    usb_mount_fail(g_usb_id);
                    music_play_usb_host_mount_after();
                } else {
                    usb_online_mount_after(g_usb_id);
                    printf("usb_mount ok\n");
                }
            } else {
                usb_online_mount_after(g_usb_id);
            }

#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
            msg_notify_enable = 1;
            if (flag_tp1) {
                struct sys_event e = {0};
                e.type = SYS_DEVICE_EVENT;
                e.arg = (void *)flag_tp2;
                if (flag_tp1 == 1) {
                    e.u.dev.event = DEVICE_EVENT_IN;
                } else if (flag_tp1 == 2) {
                    e.u.dev.event = DEVICE_EVENT_OUT;
                }
                flag_tp1 = 0;
                sys_event_notify(&e);
            }
#endif

        } else if (event->u.dev.event == DEVICE_EVENT_OUT) {
            if (file_opr_dev_check("udisk")) {
                printf("udisk unmount\n");
                ret = file_opr_dev_del("udisk");
                if (ret) {
                    log_error("file_opr_dev_del fail %x\n",  ret - STORAGE_DEV_ALREADY);
                }
            }
            usb_mount_offline(g_usb_id);
            g_usb_id = (usb_dev) - 1;
        }
#endif
        break;
    default:
        break;
    }
}


#if (!TCFG_APP_MUSIC_EN)
#define music_play_usb_host_mount_before(...)
#define music_play_usb_host_mount_after(...)
#endif//(!TCFG_APP_MUSIC_EN)




int app_common_otg_devcie_event(struct sys_event *event)
{
    int ret = false;
    const char *usb_msg = (const char *)event->u.dev.value;
    g_usb_id = usb_msg[2] - '0';
    if (usb_msg[0] == 'h') {
#if TCFG_UDISK_ENABLE
        int err = 0;
        if (event->u.dev.event == DEVICE_EVENT_ONLINE) {
            printf("usb_mount >>>>>>>>>>>>>>>>>>>>>>>>>>\n");
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
            msg_notify_enable = 0;
#endif

            music_play_usb_host_mount_before();
            usb_mount_before(g_usb_id);
            if (usb_host_mount(g_usb_id, MOUNT_RETRY, MOUNT_RESET, MOUNT_TIMEOUT)) {
                log_error("usb_probe fail\n");
                usb_mount_fail(g_usb_id);
                music_play_usb_host_mount_after();
#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
                msg_notify_enable = 1;
#endif
            }
        } else if (event->u.dev.event == DEVICE_EVENT_OFFLINE) {
            log_info("usb_unmount<<<<<<<<<<<<<<<<<<<<<<<<<\n");
            usb_host_unmount(g_usb_id);
        }
        ///主机处理返回 false, 不响应PC模式切换
        return false;
#endif//TCFG_UDISK_ENABLE
    } else {
#if TCFG_APP_PC_EN

#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
        usb_mount_before(g_usb_id);
#endif
        ret = pc_sys_event_handler(event);
        //如果是PC事件，返回true， 切换到PC
        return ret;
#endif//TCFG_APP_PC_EN
    }
    return ret;
}

extern void reverb_eq_cal_coef(u8 filtN, u32 gainN, u8 sw);
extern u8 app_common_key_event_get(struct key_event *key);
static int app_common_key_event_handler(struct sys_event *event)
{
    int ret = false;
    struct key_event *key = &event->u.key;
    u8 key_event = app_common_key_event_get(key);
    log_info("common_key_event:%d %d %d\n", key_event, key->value, key->event);

#if (TCFG_SPI_LCD_ENABLE)
    if (key_event == KEY_NULL) {
        return false;
    }

    if (key_is_ui_takeover()) {
        ui_key_msg_post(key_event);
        return false;
    }
#endif

    switch (key_event) {
    case  KEY_POWEROFF:
    case  KEY_POWEROFF_HOLD:
        power_off_deal(event, key_event - KEY_POWEROFF);
        break;

    case KEY_IR_NUM_0:
    case KEY_IR_NUM_1:
    case KEY_IR_NUM_2:
    case KEY_IR_NUM_3:
    case KEY_IR_NUM_4:
    case KEY_IR_NUM_5:
    case KEY_IR_NUM_6:
    case KEY_IR_NUM_7:
    case KEY_IR_NUM_8:
    case KEY_IR_NUM_9:
#if TCFG_UI_ENABLE
#if TCFG_APP_FM_EMITTER_EN
        fm_emitter_fre_set_by_number(key_event - KEY_IR_NUM_0);
#endif
#endif
        break;

#if TCFG_UI_ENABLE
#if TCFG_APP_FM_EMITTER_EN
    case KEY_FM_EMITTER_MENU:
        fm_emitter_enter_ui_menu();
        break;
    case KEY_FM_EMITTER_NEXT_FREQ:
        fm_emitter_enter_ui_next_fre();
        break;
    case KEY_FM_EMITTER_PERV_FREQ:
        fm_emitter_enter_ui_prev_fre();
        break;
#endif
#endif
    case KEY_CHANGE_MODE:
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        if (!key->init) {
            break;
        }
#endif
#if TWFG_APP_POWERON_IGNORE_DEV
        if ((timer_get_ms() - app_var.start_time) > TWFG_APP_POWERON_IGNORE_DEV)
#endif//TWFG_APP_POWERON_IGNORE_DEV
        {
            printf("KEY_CHANGE_MODE\n");
            app_task_next();
        }
        break;

    case KEY_VOL_UP:
        log_info("COMMON KEY_VOL_UP\n");
#if (TCFG_SPI_LCD_ENABLE)
        /* if(key_is_ui_takeover()){ */
        /* ui_key_msg_post(KEY_UP); */
        /* return TRUE; */
        /* } */
#endif


#if TCFG_APP_FM_EMITTER_EN
#if TCFG_UI_ENABLE
        if (ui_get_app_menu(GRT_CUR_MENU) == MENU_FM_SET_FRE) {
            fm_emitter_enter_ui_next_fre();
            break;
        }
#endif  // TCFG_UI_ENABLE
#endif  // TCFG_APP_FM_EMITTER_EN
        app_audio_volume_up(1);
        printf("common vol+: %d", app_audio_get_volume(APP_AUDIO_CURRENT_STATE));

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        bt_tws_sync_volume();
#endif

#if TCFG_UI_ENABLE
        ui_set_tmp_menu(MENU_MAIN_VOL, 1000, app_audio_get_volume(APP_AUDIO_CURRENT_STATE), NULL);
#endif //TCFG_UI_ENABLE
        break;

    case KEY_VOL_DOWN:

        log_info("COMMON KEY_VOL_DOWN\n");
#if (TCFG_SPI_LCD_ENABLE)
        /* if(key_is_ui_takeover()){ */
        /* ui_key_msg_post(KEY_DOWN); */
        /* return TRUE; */
        /* } */
#endif


#if TCFG_APP_FM_EMITTER_EN
#if TCFG_UI_ENABLE
        if (ui_get_app_menu(GRT_CUR_MENU) == MENU_FM_SET_FRE) {
            fm_emitter_enter_ui_prev_fre();
            break;
        }
#endif  // TCFG_UI_ENABLE
#endif  // TCFG_APP_FM_EMITTER_EN
        app_audio_volume_down(1);
        printf("common vol-: %d", app_audio_get_volume(APP_AUDIO_CURRENT_STATE));

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        bt_tws_sync_volume();
#endif

#if TCFG_UI_ENABLE
        ui_set_tmp_menu(MENU_MAIN_VOL, 1000, app_audio_get_volume(APP_AUDIO_CURRENT_STATE), NULL);
#endif //TCFG_UI_ENABLE
        break;

    case  KEY_EQ_MODE:
#if(TCFG_EQ_ENABLE == 1)
        eq_mode_sw();
#endif
        break;
#if(defined(TCFG_MIXERCH_REC_EN) && (TCFG_MIXERCH_REC_EN))
    case KEY_ENC_START:
        printf("KEY_ENC_START\n");
        ///转发消息启动/停止mixer录音
        app_task_msg_post(USER_MSG_SYS_MIXER_RECORD_SWITCH, 0, 0);
        break;
#endif//(TCFG_MIXERCH_REC_EN) && (TCFG_MIXERCH_REC_EN))
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
    case KEY_BT_EMITTER_SW:
        printf("KEY_BT_EMITTER_SW\n");
        {
            extern u8 bt_emitter_stu_sw(void);

            if (bt_emitter_stu_sw()) {
                printf("bt emitter start \n");
            } else {
                printf("bt emitter stop \n");
            }
        }
        break;
#endif
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
#if (defined(TCFG_REVERB_PITCH_EN) && (TCFG_REVERB_PITCH_EN))
    case KEY_SWITCH_PITCH_MODE:
        switch_pitch_mode();//变声
        break;
#endif
    case KEY_REVERB_OPEN:

        if (!key->init) {
            break;
        }

        printf("KEY_REVERB_ OPEN\n");
        if (reverb_if_working()) {
            stop_reverb_mic2dac();
        } else {
            start_reverb_mic2dac(NULL);
        }
        break;
    case KEY_REVERB_DEEPVAL_UP:
        printf("kkKEY_REVERB_DEEPVAL_UP \n");
        set_reverb_deepval_up(512);//混响升读，爆音调试
        break;
    case KEY_REVERB_DEEPVAL_DOWN:
        printf("KEY_REVERB_DEEPVAL_DOWN \n");
        set_reverb_deepval_down(512);//混响升读，爆音调试
        break;
#if 1
    case KEY_REVERB_GAIN0_UP:
        log_info("KEY_REVERB_GAIN0_UP filt  0\n");
        {
            static u8 sw = 0;
            sw = !sw;
            reverb_eq_cal_coef(0, 0, sw);//高低音喊mic开关
        }
        break;
    case KEY_REVERB_GAIN1_UP:
        log_info("KEY_REVERB_GAIN0_UP filt  1\n");
        {
            static u32 gain_step = 1000;
            gain_step += 500;
            if (gain_step > 6000) {
                gain_step = 0;
            }
            reverb_eq_cal_coef(1, gain_step, 0);//高低音
        }
        break;
    case KEY_REVERB_GAIN2_UP:
        log_info("KEY_REVERB_GAIN0_UP filt  2\n");
        {
            static u32 gain_step = 1000;
            gain_step += 500;
            if (gain_step > 6000) {
                gain_step = 0;
            }
            reverb_eq_cal_coef(2, gain_step, 0);//高低音
        }
        break;
#endif

#endif


#if(defined TCFG_CHARGE_BOX_ENABLE) && ( TCFG_CHARGE_BOX_ENABLE)
    case  KEY_BOX_POWER_CLICK:
    case  KEY_BOX_POWER_LONG:
    case  KEY_BOX_POWER_HOLD:
    case  KEY_BOX_POWER_UP:
        charge_key_event_handler(key_event);
        break;
#endif

    default:
#if (TCFG_SPI_LCD_ENABLE)
        ui_key_msg_post(key_event);
#endif
        break;

    }
    return ret;
}

static void app_common_device_event_handler(struct sys_event *event)
{
    int ret = 0;
    const char *logo = NULL;
    const char *app_name = NULL;
    u8 alarm_flag = 0;
    switch ((u32)event->arg) {
#if TCFG_CHARGE_ENABLE
    case DEVICE_EVENT_FROM_CHARGE:
        app_charge_event_handler(&event->u.dev);
        break;
#endif//TCFG_CHARGE_ENABLE

#if TCFG_ONLINE_ENABLE
    case DEVICE_EVENT_FROM_CI_UART:
        ci_data_rx_handler(CI_UART);
        break;

#if TCFG_USER_TWS_ENABLE
    case DEVICE_EVENT_FROM_CI_TWS:
        ci_data_rx_handler(CI_TWS);
        break;
#endif//TCFG_USER_TWS_ENABLE
#endif//TCFG_ONLINE_ENABLE

    case DEVICE_EVENT_FROM_POWER:
        app_power_event_handler(&event->u.dev);
        break;

#if TCFG_CHARGESTORE_ENABLE || TCFG_TEST_BOX_ENABLE
    case DEVICE_EVENT_CHARGE_STORE:
        app_chargestore_event_handler(&event->u.chargestore);
        break;
#endif//TCFG_CHARGESTORE_ENABLE || TCFG_TEST_BOX_ENABLE

#if(defined TCFG_CHARGE_BOX_ENABLE) && ( TCFG_CHARGE_BOX_ENABLE)
    case DEVICE_EVENT_FROM_CHARGEBOX:
        charge_ctrl_event_handler(event);
        break;
#endif


#if (DUEROS_DMA_EN)
    case SYS_BT_AI_EVENT_TYPE_STATUS:
        bt_ai_event_handler(&event->u.bt);
        break;
#endif//DUEROS_DMA_EN

    case DRIVER_EVENT_FROM_SD0:
    case DRIVER_EVENT_FROM_SD1:
    case DRIVER_EVENT_FROM_SD2:
    case DEVICE_EVENT_FROM_USB_HOST:
#if (defined(CONFIG_FATFS_ENBALE) && CONFIG_FATFS_ENBALE)
        logo = evt2dev_map_logo((u32)event->arg);
        if (logo == NULL) {
            break;
        }
        if (event->u.dev.event == DEVICE_EVENT_IN) {
            if (file_opr_available_dev_check((void *)logo)) {
                app_name = APP_NAME_MUSIC;
            }
        } else if (event->u.dev.event == DEVICE_EVENT_OUT) {
        }
#endif//CONFIG_FATFS_ENBALE
        break;
    case DEVICE_EVENT_FROM_OTG:
        ret = app_common_otg_devcie_event(event);
        if (ret == true) {
            app_name = APP_NAME_PC;
        }
        break;

#if TCFG_APP_LINEIN_EN
    case DEVICE_EVENT_FROM_LINEIN:
        ret = linein_sys_event_handler(event);
        if (ret == true) {
            app_name = APP_NAME_LINEIN;
        }
        break;
#endif//TCFG_APP_LINEIN_EN

#if TCFG_APP_RTC_EN
    case DEVICE_EVENT_FROM_ALM:
        ret = alarm_sys_event_handler(event);
        if (ret == true) {
            alarm_flag = 1;
            app_name = APP_NAME_RTC;
        }
        break;
#endif//TCFG_APP_RTC_EN

    /* case DEVICE_EVENT_FROM_ENDLESS_LOOP_DEBUG: */
    /* endless_loop_debug(); */
    /* break; */

    default:
        printf("unknow SYS_DEVICE_EVENT!!\n");
        break;
    }

    if (app_name) {
        if ((true != app_cur_task_check(APP_NAME_PC)) || alarm_flag) {

            //PC 不响应因为设备上线引发的模式切换
#if TWFG_APP_POWERON_IGNORE_DEV
            if ((timer_get_ms() - app_var.start_time) > TWFG_APP_POWERON_IGNORE_DEV)
#endif//TWFG_APP_POWERON_IGNORE_DEV
            {
#if (TCFG_CHARGE_ENABLE && (!TCFG_CHARGE_POWERON_ENABLE))
                extern u8 get_charge_online_flag(void);
                if (get_charge_online_flag()) {

                } else
#endif
                {
                    app_task_switch(app_name, ACTION_APP_MAIN, (void *)logo);
                }
            }
        }
    }
}




///公共事件处理， 各自模式没有处理的事件， 会统一在这里处理
void app_default_event_handler(struct sys_event *event)
{
    int ret;
    switch (event->type) {
    case SYS_DEVICE_EVENT:
        /*默认公共设备事件处理*/
        app_common_device_event_handler(event);
        break;
    case SYS_BT_EVENT:
        /*默认公共BT事件处理*/
        app_sys_bt_event_opr(event);
        break;
    case SYS_KEY_EVENT:
        /*默认公共按键事件处理*/
        app_common_key_event_handler(event);
        break;
    default:
        printf("unknow event\n");
        break;
    }
}



void app_common_user_msg_deal(int msg, int argc, int *argv)
{
    switch (msg) {
#if TCFG_SLIDE_KEY_ENABLE
    case USER_MSG_SLIDE1:
        r_printf("slide1 post success  level = %d\n", argv[0]);
        break;
    case USER_MSG_SLIDE2:

        y_printf("slide2 post success  level = %d\n", argv[0]);
        break;

#endif
    case USER_MSG_TEST:
        printf("common get user msg %d, msg_val_count = %d, msg_val:\n", msg, argc);
        for (int i = 0; i < argc; i++) {
            printf("%d ", argv[i]);
        }
        break;
#if(defined(TCFG_MIXERCH_REC_EN) && (TCFG_MIXERCH_REC_EN))
    case USER_MSG_SYS_MIXER_RECORD_SWITCH:
        ///该消息，按键事件触发后都会统一发，不响应处理的模式在独自模式里进行过滤该消息

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
        //开混响暂不录音
        log_info("not support recorder \n");
        break;
#endif
        printf("USER_MSG_SYS_MIXER_RECORD_SWITCH\n");
        if (mixer_recorder_encoding()) {
            log_info("mixer_recorder_stop\n");
            mixer_recorder_stop();
        } else {
            log_info("mixer_recorder_start\n");
            mixer_recorder_start();
        }
        break;
    case USER_MSG_SYS_MIXER_RECORD_STOP:
        log_info("USER_MSG_SYS_MIXER_RECORD_STOP\n");
        mixer_recorder_stop();
        break;
#endif//(defined(TCFG_MIXERCH_REC_EN) && (TCFG_MIXERCH_REC_EN))

    default:
        break;

    }
}
