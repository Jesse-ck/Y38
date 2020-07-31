#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_action.h"
#include "tone_player.h"
#include "asm/charge.h"
#include "app_charge.h"
#include "app_main.h"
#include "ui_manage.h"
#include "vm.h"
#include "app_chargestore.h"
#include "user_cfg.h"
#include "ui/ui_api.h"

#define LOG_TAG_CONST       APP_IDLE
#define LOG_TAG             "[APP_IDLE]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

static int timer_printf_1sec = 0;
static u8 is_idle_flag = 0;

extern void set_poweron_charge_idle(u8 flag);
extern u8 get_poweron_charge_idle();

#define NOT_POWEROFF_IN_IDLE    0

#if NOT_POWEROFF_IN_IDLE
struct timer_hdl {
    u32 ticks;
    int index;
    int prd;
    u32 fine_cnt;
    void *power_ctrl;
    struct list_head head;
};

static struct timer_hdl hdl;

#define __this  (&hdl)

static const u32 timer_div[] = {
    /*0000*/    1,
    /*0001*/    4,
    /*0010*/    16,
    /*0011*/    64,
    /*0100*/    2,
    /*0101*/    8,
    /*0110*/    32,
    /*0111*/    128,
    /*1000*/    256,
    /*1001*/    4 * 256,
    /*1010*/    16 * 256,
    /*1011*/    64 * 256,
    /*1100*/    2 * 256,
    /*1101*/    8 * 256,
    /*1110*/    32 * 256,
    /*1111*/    128 * 256,
};

#define APP_TIMER_CLK           clk_get("timer")
#define MAX_TIME_CNT            0x7fff
#define MIN_TIME_CNT            0x100

#define TIMER_UNIT_MS           2
#define MAX_TIMER_PERIOD_MS     (1000/TIMER_UNIT_MS)

void idle_key_scan();
extern void adc_scan(void *priv);
___interrupt
static void idle_timer2_isr()
{
    static u32 cnt = 0;
    JL_TIMER2->CON |= BIT(14);

    ++cnt;
    if ((cnt % 5) == 0) {
        idle_key_scan();
    }

    if (cnt == 500) {
        cnt = 0;
    }
    /* adc_scan(NULL); */
}

static int idle_timer2_init()
{
    u32 prd_cnt;
    u8 index;

    for (index = 0; index < (sizeof(timer_div) / sizeof(timer_div[0])); index++) {
        prd_cnt = TIMER_UNIT_MS * (APP_TIMER_CLK / 1000) / timer_div[index];
        if (prd_cnt > MIN_TIME_CNT && prd_cnt < MAX_TIME_CNT) {
            break;
        }
    }
    __this->index   = index;
    __this->prd     = prd_cnt;

    JL_TIMER2->CNT = 0;
    JL_TIMER2->PRD = prd_cnt; //2ms
    request_irq(IRQ_TIME2_IDX, 3, idle_timer2_isr, 0);
    JL_TIMER2->CON = (index << 4) | BIT(0) | BIT(3);

    log_info("PRD : 0x%x / %d", JL_TIMER2->PRD, clk_get("timer"));

    return 0;
}

static void idle_timer2_uninit()
{
    JL_TIMER2->CON &= ~(BIT(1) | BIT(0));
}

static void idle_timer1_open()
{
    JL_TIMER1->CON |= BIT(0);
}

static void idle_timer1_close()
{
    JL_TIMER1->CON &= ~(BIT(1) | BIT(0));
}


extern u8 get_power_on_status(void);
void idle_key_on_deal();
void idle_key_scan()
{
    static u16 cnt = 0;
    if (get_power_on_status()) {
        log_info("+");
        cnt++;
        if (cnt == 70) { //按键开机
            /* cpu_reset(); */
            /* idle_timer1_open(); */
            app_var.goto_poweroff_flag = 0;
            is_idle_flag = 0;
#if (TCFG_SD0_ENABLE || TCFG_SD1_ENABLE)
            extern void sd_detect_timer_add();
            sd_detect_timer_add();
#endif

#if (TCFG_PC_ENABLE || TCFG_UDISK_ENABLE)
            extern void usb_detect_timer_add();
            usb_detect_timer_add();
#endif

#if TCFG_LINEIN_ENABLE
            extern void linein_detect_timer_add();
            linein_detect_timer_add();
#endif
            if (timer_printf_1sec) {
                sys_timer_del(timer_printf_1sec);
            }
            idle_timer2_uninit();
            idle_key_on_deal();
        }
    } else if (cnt) {
        log_info("-");
        cnt = 0;
    }
}

static u8 is_idle_query(void)
{
    return is_idle_flag;
}

REGISTER_LP_TARGET(idle_lp_target) = {
    .name = "not_idle",
    .is_idle = is_idle_query,
};

#endif

void idle_key_on_deal()
{
#if TWFG_APP_POWERON_IGNORE_DEV
    app_task_switch(APP_NAME_POWERON, ACTION_APP_MAIN, NULL);
    /* app_task_switch(APP_NAME_BT, ACTION_APP_MAIN, NULL); */
#else
    if (true == app_task_online_check(APP_NAME_LINEIN)) {
        app_task_switch(APP_NAME_LINEIN, ACTION_APP_MAIN, NULL);
    } else if (true == app_task_online_check(APP_NAME_MUSIC)) {
        app_task_switch(APP_NAME_MUSIC, ACTION_APP_MAIN, NULL);
    } else if (true == app_task_online_check(APP_NAME_PC)) {
        app_task_switch(APP_NAME_PC, ACTION_APP_MAIN, NULL);
    } else {
        app_task_switch(APP_NAME_BT, ACTION_APP_MAIN, NULL);
    }
#endif
}

#if (TCFG_CHARGE_ENABLE && TCFG_CHARGE_POWERON_ENABLE)
#define KEY_SCAN_IDLE_STATUS        250
#define KEY_SCAN_ACTIVE_STATUS      10

static u16 charge_key_scan_timer = 0;
extern u8 get_power_on_status(void);
extern int app_task_online_check(char *name);
extern int app_task_switch(const char *name, int action, void *param);

void charge_key_scan(void *priv)
{
    static u16 cnt = 0;
    static u32 scan_freq = KEY_SCAN_IDLE_STATUS;
    if (get_power_on_status()) {
        log_info("+");
        cnt++;
        if (scan_freq != KEY_SCAN_ACTIVE_STATUS) {
            scan_freq = KEY_SCAN_ACTIVE_STATUS;
            sys_timer_modify(charge_key_scan_timer, KEY_SCAN_ACTIVE_STATUS);
        }
        if (cnt == 70) { //按键开机
            set_poweron_charge_idle(0);
            idle_key_on_deal();
        }
    } else if (cnt) {
        log_info("-");
        cnt = 0;
        scan_freq = KEY_SCAN_IDLE_STATUS;
        sys_timer_modify(charge_key_scan_timer, KEY_SCAN_IDLE_STATUS);
    }
}
#endif // TCFG_CHARGE_POWERON_ENABLE

static int app_idle_tone_event_handler(struct device_event *dev)
{
    int ret = false;

    switch (dev->event) {
    case AUDIO_PLAY_EVENT_END:
        if (app_var.goto_poweroff_flag) {
            log_info("audio_play_event_end,enter soft poweroff");
            //ui_update_status(STATUS_POWEROFF);
            extern void timer(void *p);
            timer_printf_1sec = sys_timer_add(NULL, timer, 1 * 1000);
            while (get_ui_busy_status()) {
                /* log_info("ui_status:%d\n", get_ui_busy_status()); */
            }
#if (TCFG_CHARGE_ENABLE && TCFG_CHARGE_POWERON_ENABLE)
            if (get_charge_online_flag()) {
                cpu_reset();
            } else {
                power_set_soft_poweroff();
            }
#else
#if NOT_POWEROFF_IN_IDLE
            /* idle_timer1_close(); */
            is_idle_flag = 1;
            idle_timer2_init();
#else
            power_set_soft_poweroff();
#endif
#endif
        }
        break;
    }

    return ret;
}
#if (TWFG_APP_POWERON_IGNORE_DEV == 0)
static u16 ignore_dev_timeout = 0;
static void poweron_task_switch_to_bt(void *priv) //超时还没有设备挂载，则切到蓝牙模式
{
    app_task_switch(APP_NAME_BT, ACTION_APP_MAIN, NULL);
}
#endif

extern int app_common_otg_devcie_event(struct sys_event *event);
static int idle_event_handler(struct application *app, struct sys_event *event)
{
    switch (event->type) {
    case SYS_KEY_EVENT:
        /* return 0; */
        return true;
    case SYS_BT_EVENT:
        /* return 0; */
        return true;
    case SYS_DEVICE_EVENT:
        if ((u32)event->arg == DEVICE_EVENT_FROM_CHARGE) {
#if TCFG_CHARGE_ENABLE
            return app_charge_event_handler(&event->u.dev);
#endif
        }
        if ((u32)event->arg == DEVICE_EVENT_FROM_TONE) {
            return app_idle_tone_event_handler(&event->u.dev);
        }
#if TCFG_CHARGESTORE_ENABLE || TCFG_TEST_BOX_ENABLE
        if ((u32)event->arg == DEVICE_EVENT_CHARGE_STORE) {
            app_chargestore_event_handler(&event->u.chargestore);
        }
#endif

#if NOT_POWEROFF_IN_IDLE
#if (TCFG_PC_ENABLE || TCFG_UDISK_ENABLE)
        if ((u32)event->arg == DEVICE_EVENT_FROM_OTG) {
            app_common_otg_devcie_event(event);
        }
#endif
#endif

        if (app_var.goto_poweroff_flag) { //关机时候不执行通用消息处理
            return true;
        } else {
            return 0;
        }
    /* return true; */
    default:
        /* return false; */
        return true;
    }
}

static int idle_state_machine(struct application *app, enum app_state state,
                              struct intent *it)
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
            printf("------------------------------------------------it->exdata = %d\n", it->exdata);
            if (app_var.goto_poweroff_flag) {
#if NOT_POWEROFF_IN_IDLE
#if (TCFG_SD0_ENABLE || TCFG_SD1_ENABLE)
                extern void sd_detect_timer_del();
                sd_detect_timer_del();
#endif

#if (TCFG_PC_ENABLE || TCFG_UDISK_ENABLE)
                extern void usb_detect_timer_del();
                usb_detect_timer_del();
#endif

#if TCFG_LINEIN_ENABLE
                extern void linein_detect_timer_del();
                linein_detect_timer_del();
#endif
#endif

#if TCFG_UI_ENABLE
                ui_set_main_menu(UI_MENU_MAIN_NULL);
#endif
                syscfg_write(CFG_MUSIC_VOL, &app_var.music_volume, 1);
                /* tone_play(TONE_POWER_OFF); */
                os_taskq_flush();
                STATUS *p_tone = get_tone_config();
                ret = tone_play_index(p_tone->power_off, 1);

                printf("power_off tone play ret:%d", ret);
                if (ret) {
                    if (app_var.goto_poweroff_flag) {
                        log_info("power_off tone play err,enter soft poweroff");
                        //ui_update_status(STATUS_POWEROFF);
                        //
#if TCFG_UI_ENABLE
#if (TCFG_LED_LCD_ENABLE)
                        u8 count = 0;
__try:
                            if (ui_get_app_menu(GET_MAIN_MENU) != UI_MENU_MAIN_NULL) {
                                os_time_dly(10);//增加延时防止没有关显示
                                if (count < 3) {
                                    goto __try;
                                }
                                count++;
                            }
#endif
#endif

                        while (get_ui_busy_status()) {
                            log_info("ui_status:%d\n", get_ui_busy_status());
                        }
#if NOT_POWEROFF_IN_IDLE
                        /* idle_timer1_close(); */
                        idle_timer2_init();
#else
                        power_set_soft_poweroff();
#endif
                    }
                }
            }
#if (TCFG_CHARGE_ENABLE && TCFG_CHARGE_POWERON_ENABLE)
            else if (get_poweron_charge_idle()) { //开机充电，检测按键
                charge_key_scan_timer = sys_timer_add(NULL, charge_key_scan, KEY_SCAN_IDLE_STATUS);
            }
#endif
#if (TWFG_APP_POWERON_IGNORE_DEV == 0)
            else {
                ignore_dev_timeout = sys_timeout_add(NULL, poweron_task_switch_to_bt, 2000);//2s内没有设备挂载，切到蓝牙模式
            }
#endif
            break;
        }
        break;
    case APP_STA_PAUSE:
        break;
    case APP_STA_RESUME:
        break;
    case APP_STA_STOP:
        break;
    case APP_STA_DESTROY:
#if (TWFG_APP_POWERON_IGNORE_DEV == 0)
        if (ignore_dev_timeout) {
            sys_timeout_del(ignore_dev_timeout);
        }
#endif
#if (TCFG_CHARGE_ENABLE && TCFG_CHARGE_POWERON_ENABLE)
        if (charge_key_scan_timer) {
            sys_timer_del(charge_key_scan_timer);
        }
#endif
        break;
    }

    return 0;
}


static int idle_app_check(void)
{
    return true;
}



static const struct application_reg app_idle_reg = {
    .tone_name = NULL,
    .enter_check = idle_app_check,
    .exit_check = NULL,
};


static const struct application_operation app_idle_ops = {
    .state_machine  = idle_state_machine,
    .event_handler 	= idle_event_handler,
};

REGISTER_APPLICATION(app_app_idle) = {
    .name 	= APP_NAME_IDLE,
    .action	= ACTION_APP_MAIN,
    .ops 	= &app_idle_ops,
    .state  = APP_STA_DESTROY,
    .private_data = (void *) &app_idle_reg,
};


