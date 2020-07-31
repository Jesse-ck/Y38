#include "app_config.h"
#include "key_event_deal.h"
#include "system/includes.h"
#include "tone_player.h"
#include "app_action.h"
#include "tone_player.h"
#include "media/includes.h"
#include "system/sys_time.h"
#include "ui/ui_api.h"
#include "alarm.h"
#include "audio_reverb.h"
#include "clock_cfg.h"


#if TCFG_APP_RTC_EN

#define LOG_TAG_CONST       APP_RTC
#define LOG_TAG             "[APP_RTC]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"



#define RTC_SET_MODE  0x55
#define ALM_SET_MODE  0xAA

#define RTC_POS_DEFAULT      RTC_POS_YEAR
#define RTC_ALM_POS_DEFAULT  ALM_POS_HOUR
#define RTC_MODE_DEFAULT     RTC_SET_MODE

#define MAX_YEAR          2099
#define MIN_YEAR          2000

enum {
    RTC_POS_NULL = 0,
    RTC_POS_YEAR,
    RTC_POS_MONTH,
    RTC_POS_DAY,
    RTC_POS_HOUR,
    RTC_POS_MIN,
    /* RTC_POS_SEC, */
    RTC_POS_MAX,
    ALM_POS_HOUR,
    ALM_POS_MIN,
    ALM_POS_ENABLE,
    ALM_POS_MAX,
};

struct rtc_opr {
    void *dev_handle;
    u8  rtc_set_mode;
    u8  rtc_pos;
    u8  alm_enable;
    u8  alm_num;
    struct sys_time set_time;
};

static struct rtc_opr *__this = NULL;



const char *alm_string[] =  {" AL ", " ON ", " OFF"};
const char *alm_select[] =  {"AL-1", "AL-2", "AL-3", "AL-4", "AL-5"};



static void ui_set_rtc_timeout(u8 menu)
{
    if (!__this) {
        return ;
    }
    __this->rtc_set_mode =  RTC_SET_MODE;
    __this->rtc_pos = RTC_POS_NULL;
}

struct ui_rtc_display *__attribute__((weak)) rtc_ui_get_display_buf()
{
    return NULL;
}



static void set_rtc_sw()
{
    if ((!__this) || (!__this->dev_handle)) {
        return;
    }


    struct ui_rtc_display *rtc = rtc_ui_get_display_buf();
    if (!rtc) {
        return;
    }
    switch (__this->rtc_set_mode) {
    case RTC_SET_MODE:
        __this->rtc_set_mode = ALM_SET_MODE;
        __this->rtc_pos = RTC_POS_NULL;
        __this->alm_num = 0;
        rtc->rtc_menu = UI_RTC_ACTION_STRING_SET;
        rtc->str = alm_select[__this->alm_num];
        ui_set_tmp_menu(MENU_RTC_SET, 10 * 1000, 0, ui_set_rtc_timeout);
        break;

    case ALM_SET_MODE:
        __this->alm_num++;
        __this->rtc_pos = RTC_POS_NULL;
        if (__this->alm_num >= sizeof(alm_select) / sizeof(alm_select[0])) {
            __this->rtc_set_mode = RTC_SET_MODE;
            __this->alm_num = 0;
            ui_menu_reflash(true);
            break;
        }
        rtc->rtc_menu = UI_RTC_ACTION_STRING_SET;
        rtc->str = alm_select[__this->alm_num];
        ui_set_tmp_menu(MENU_RTC_SET, 10 * 1000, 0, ui_set_rtc_timeout);
        break;

    }
}

static void set_rtc_pos()
{
    T_ALARM alarm;
    if ((!__this) || (!__this->dev_handle)) {
        return;
    }

    struct ui_rtc_display *rtc = rtc_ui_get_display_buf();

    if (!rtc) {
        return;
    }

    switch (__this->rtc_set_mode) {
    case RTC_SET_MODE:

        if (__this->rtc_pos == RTC_POS_NULL) {
            __this->rtc_pos = RTC_POS_DEFAULT;
            dev_ioctl(__this->dev_handle, IOCTL_GET_SYS_TIME, (u32)&__this->set_time);
        } else {
            __this->rtc_pos++;
            if (__this->rtc_pos == RTC_POS_MAX) {
                __this->rtc_pos = RTC_POS_NULL;
                rtc_update_time_api(&__this->set_time);
                ui_menu_reflash(true);
                break;
            }
        }

        rtc->rtc_menu = UI_RTC_ACTION_YEAR_SET + (__this->rtc_pos - RTC_POS_YEAR);
        rtc->time.Year = __this->set_time.year;
        rtc->time.Month = __this->set_time.month;
        rtc->time.Day = __this->set_time.day;
        rtc->time.Hour = __this->set_time.hour;
        rtc->time.Min = __this->set_time.min;
        rtc->time.Sec = __this->set_time.sec;

        ui_set_tmp_menu(MENU_RTC_SET, 10 * 1000, 0, ui_set_rtc_timeout);
        break;

    case ALM_SET_MODE:
        if (__this->rtc_pos == RTC_POS_NULL) {
            __this->rtc_pos = RTC_ALM_POS_DEFAULT;
            if (alarm_get_info(&alarm, __this->alm_num) != 0) {
                log_error("alarm_get_info \n");
            }

            __this->set_time.hour = alarm.time.bHour;
            __this->set_time.min = alarm.time.bMin;
            __this->alm_enable = alarm.sw;
        } else {
            __this->rtc_pos++;
            if (__this->rtc_pos == ALM_POS_MAX) {
                __this->rtc_pos = RTC_POS_NULL;
                alarm.time.bHour = __this->set_time.hour;
                alarm.time.bMin  = __this->set_time.min;
                alarm.time.bSec  = 0;
                alarm.sw = __this->alm_enable;
                alarm.index = __this->alm_num;
                alarm.mode  = 0;
                alarm_add(&alarm, __this->alm_num);
                __this->alm_num++;
                if (__this->alm_num >= sizeof(alm_select) / sizeof(alm_select[0])) {
                    __this->rtc_set_mode = RTC_SET_MODE;
                    __this->alm_num = 0;
                    ui_menu_reflash(true);
                } else {
                    rtc->rtc_menu = UI_RTC_ACTION_STRING_SET;
                    rtc->str = alm_select[__this->alm_num];
                    ui_set_tmp_menu(MENU_RTC_SET, 10 * 1000, 0, ui_set_rtc_timeout);
                }

                break;
            }
        }

        if (ALM_POS_ENABLE == __this->rtc_pos) {
            rtc->rtc_menu = UI_RTC_ACTION_STRING_SET;
            if (__this->alm_enable) {
                rtc->str = " ON ";
            } else {
                rtc->str = " OFF";
            }
        } else {
            rtc->rtc_menu = UI_RTC_ACTION_HOUR_SET + (__this->rtc_pos - ALM_POS_HOUR);
            rtc->time.Year = __this->set_time.year;
            rtc->time.Month = __this->set_time.month;
            rtc->time.Day = __this->set_time.day;
            rtc->time.Hour = __this->set_time.hour;
            rtc->time.Min = __this->set_time.min;
            rtc->time.Sec = __this->set_time.sec;
        }

        ui_set_tmp_menu(MENU_RTC_SET, 10 * 1000, 0, ui_set_rtc_timeout);
        break;

    }
}

static void set_rtc_up()
{

    if ((!__this) || (!__this->dev_handle)) {
        return;
    }

    struct ui_rtc_display *rtc = rtc_ui_get_display_buf();

    if (!rtc) {
        return;
    }

    if (__this->rtc_pos == RTC_POS_NULL) {
        return ;
    }


    switch (__this->rtc_set_mode) {
    case RTC_SET_MODE:
        switch (__this->rtc_pos) {
        case RTC_POS_YEAR:
            __this->set_time.year++;
            if (__this->set_time.year > MAX_YEAR) {
                __this->set_time.year = MIN_YEAR;
            }
            break;
        case RTC_POS_MONTH:
            if (++__this->set_time.month > 12) {
                __this->set_time.month = 1;
            }
            break;
        case RTC_POS_DAY:
            if (++__this->set_time.day > month_for_day(__this->set_time.month, __this->set_time.year)) {
                __this->set_time.day = 1;
            }
            break;
        case RTC_POS_HOUR:
            if (++__this->set_time.hour >= 24) {
                __this->set_time.hour = 0;
            }
            break;

        case RTC_POS_MIN:
            if (++__this->set_time.min >= 60) {
                __this->set_time.min = 0;
            }

            break;
            /* case RTC_POS_SEC: */

            /* break; */
        }


        rtc->rtc_menu = UI_RTC_ACTION_YEAR_SET + (__this->rtc_pos - RTC_POS_YEAR);
        rtc->time.Year = __this->set_time.year;
        rtc->time.Month = __this->set_time.month;
        rtc->time.Day = __this->set_time.day;
        rtc->time.Hour = __this->set_time.hour;
        rtc->time.Min = __this->set_time.min;
        rtc->time.Sec = __this->set_time.sec;
        ui_set_tmp_menu(MENU_RTC_SET, 10 * 1000, 0, ui_set_rtc_timeout);
        break;
    case ALM_SET_MODE:

        switch (__this->rtc_pos) {
        case ALM_POS_HOUR:
            if (++__this->set_time.hour >= 24) {
                __this->set_time.hour = 0;
            }
            break;

        case ALM_POS_MIN:
            if (++__this->set_time.min >= 60) {
                __this->set_time.min = 0;
            }
            break;
        case ALM_POS_ENABLE:
            __this->alm_enable = !__this->alm_enable;
            break;
        }

        if (ALM_POS_ENABLE == __this->rtc_pos) {
            rtc->rtc_menu = UI_RTC_ACTION_STRING_SET;
            if (__this->alm_enable) {
                rtc->str = " ON ";
            } else {
                rtc->str = " OFF";
            }
        } else {
            rtc->rtc_menu = UI_RTC_ACTION_HOUR_SET + (__this->rtc_pos - ALM_POS_HOUR);
            rtc->time.Year = __this->set_time.year;
            rtc->time.Month = __this->set_time.month;
            rtc->time.Day = __this->set_time.day;
            rtc->time.Hour = __this->set_time.hour;
            rtc->time.Min = __this->set_time.min;
            rtc->time.Sec = __this->set_time.sec;
        }

        ui_set_tmp_menu(MENU_RTC_SET, 10 * 1000, 0, ui_set_rtc_timeout);
        break;

    default:
        break;
    }
}

static void set_rtc_down()
{

    if ((!__this) || (!__this->dev_handle)) {
        return;
    }
    struct ui_rtc_display *rtc = rtc_ui_get_display_buf();

    if (!rtc) {
        return;
    }

    if (__this->rtc_pos == RTC_POS_NULL) {
        return ;
    }

    switch (__this->rtc_set_mode) {
    case RTC_SET_MODE:
        switch (__this->rtc_pos) {
        case RTC_POS_YEAR:
            __this->set_time.year--;
            if (__this->set_time.year < MIN_YEAR) {
                __this->set_time.year = MAX_YEAR;
            }
            break;
        case RTC_POS_MONTH:

            if (__this->set_time.month == 1) {
                __this->set_time.month = 12;
            } else {
                __this->set_time.month--;
            }

            break;
        case RTC_POS_DAY:

            if (__this->set_time.day == 1) {
                __this->set_time.day = month_for_day(__this->set_time.month, __this->set_time.year);
            } else {
                __this->set_time.day --;
            }

            break;
        case RTC_POS_HOUR:
            if (__this->set_time.hour == 0) {
                __this->set_time.hour = 23;
            } else {
                __this->set_time.hour--;
            }
            break;
        case RTC_POS_MIN:
            if (__this->set_time.min == 0) {
                __this->set_time.min = 59;
            } else {
                __this->set_time.min--;
            }
            break;
        }

        rtc->rtc_menu = UI_RTC_ACTION_YEAR_SET + (__this->rtc_pos - RTC_POS_YEAR);
        rtc->time.Year = __this->set_time.year;
        rtc->time.Month = __this->set_time.month;
        rtc->time.Day = __this->set_time.day;
        rtc->time.Hour = __this->set_time.hour;
        rtc->time.Min = __this->set_time.min;
        rtc->time.Sec = __this->set_time.sec;
        ui_set_tmp_menu(MENU_RTC_SET, 10 * 1000, 0, ui_set_rtc_timeout);
        break;

    case ALM_SET_MODE:
        switch (__this->rtc_pos) {
        case ALM_POS_HOUR:
            if (__this->set_time.hour == 0) {
                __this->set_time.hour = 23;
            } else {
                __this->set_time.hour--;
            }
            break;

        case ALM_POS_MIN:
            if (__this->set_time.min == 0) {
                __this->set_time.min = 59;
            } else {
                __this->set_time.min--;
            }
            break;

        case ALM_POS_ENABLE:
            __this->alm_enable = !__this->alm_enable;
            break;
        }

        if (ALM_POS_ENABLE == __this->rtc_pos) {
            rtc->rtc_menu = UI_RTC_ACTION_STRING_SET;
            if (__this->alm_enable) {
                rtc->str = " ON ";
            } else {
                rtc->str = " OFF";
            }
        } else {
            rtc->rtc_menu = UI_RTC_ACTION_HOUR_SET + (__this->rtc_pos - ALM_POS_HOUR);
            rtc->time.Year = __this->set_time.year;
            rtc->time.Month = __this->set_time.month;
            rtc->time.Day = __this->set_time.day;
            rtc->time.Hour = __this->set_time.hour;
            rtc->time.Min = __this->set_time.min;
            rtc->time.Sec = __this->set_time.sec;
        }
        ui_set_tmp_menu(MENU_RTC_SET, 10 * 1000, 0, ui_set_rtc_timeout);

        break;

    default:

        break;
    }



}

static void rtc_app_init()
{
    if (!__this) {
        __this = zalloc(sizeof(struct rtc_opr));
        ASSERT(__this, "%s %di \n", __func__, __LINE__);
        __this->dev_handle = dev_open("rtc", NULL);
        if (!__this->dev_handle) {
            ASSERT(0, "%s %d \n", __func__, __LINE__);
        }
    }
    __this->rtc_set_mode =  RTC_SET_MODE;
    __this->rtc_pos = RTC_POS_NULL;

    clock_idle(RTC_IDLE_CLOCK);

#if TCFG_UI_ENABLE
    ui_set_main_menu(UI_RTC_MENU_MAIN);
#endif
    sys_key_event_enable();

}


static void rtc_app_close()
{
    if (__this) {
        if (__this->dev_handle) {
            dev_close(__this->dev_handle);
            __this->dev_handle = NULL;
        }
        free(__this);
        __this = NULL;
    }
}

extern u8 rtc_key_event_get(struct key_event *key);
static int _key_event_opr(struct sys_event *event)
{
    int ret = true;
    int err = 0;
    struct key_event *key = &event->u.key;
    u8 key_event = rtc_key_event_get(key);

    log_info("key value:%d, event:%d \n", key->value, key->event);
    log_info("key_event:%d \n", key_event);


    if (__this && __this->dev_handle) {
        switch (key_event) {
        case KEY_RTC_UP:
            log_info("KEY_RTC_UP \n");
            /* set_reverb_deepval_up(128); */
            set_rtc_up();
            break;

        case KEY_RTC_DOWN:
            log_info("KEY_RTC_DOWN \n");
            /* set_reverb_deepval_down(128); */
            set_rtc_down();
            break;

        case KEY_RTC_SW:
            log_info("KEY_RTC_SW \n");
            set_rtc_sw();
            break;

        case KEY_RTC_SW_POS:
            log_info("KEY_RTC_SW_POS \n");
            set_rtc_pos();
            break;

        default :
            ret = false;
            break;
        }
    }
    return ret;
}

static int rtc_event_handler(struct application *app, struct sys_event *event)
{
    switch (event->type) {
    case SYS_KEY_EVENT:
        return _key_event_opr(event);
    case SYS_DEVICE_EVENT:
        return false;
    default:
        return false;
    }
    return false;
}




static int rtc_state_machine(struct application *app, enum app_state state, struct intent *it)
{
    int ret;
    switch (state) {
    case APP_STA_CREATE:
        break;
    case APP_STA_START:
        if (!it) {
            break;
        }
        switch (it->action) {
        case ACTION_APP_MAIN:
            log_info("ACTION_APP_MAIN\n");
            rtc_app_init();
            if (alarm_active_flag_get()) {
                alarm_ring_start();
            }
            break;
        }
        break;
    case APP_STA_PAUSE:
        log_info("APP_STA_PAUSE\n");
        break;
    case APP_STA_RESUME:
        log_info("APP_STA_RESUME\n");
        break;
    case APP_STA_STOP:
        log_info("APP_STA_STOP\n");
        break;
    case APP_STA_DESTROY:
        log_info("APP_STA_DESTROY\n");
        rtc_app_close();
        break;
    }
    return 0;
}


static int rtc_app_check(void)
{
#if TCFG_APP_RTC_EN
    return true;
#else
    return false;
#endif
}


static int rtc_user_msg_deal(int msg, int argc, int *argv)
{
    switch (msg) {
    case USER_MSG_SYS_MIXER_RECORD_SWITCH:
        ///rtc模式不响应混合录音
        return 1;
    default:
        break;
    }
    return 0;
}


static const struct application_reg app_rtc_reg = {
    .tone_name = TONE_RTC,
    .tone_play_check = NULL,
    .tone_prepare = NULL,
    .enter_check = rtc_app_check,
    .exit_check = NULL,
    .user_msg = rtc_user_msg_deal,
};

static const struct application_operation app_rtc_ops = {
    .state_machine  = rtc_state_machine,
    .event_handler 	= rtc_event_handler,
};

REGISTER_APPLICATION(app_app_rtc) = {
    .name 	= APP_NAME_RTC,
    .action	= ACTION_APP_MAIN,
    .ops 	= &app_rtc_ops,
    .state  = APP_STA_DESTROY,
    .private_data = (void *) &app_rtc_reg,
};

#endif
