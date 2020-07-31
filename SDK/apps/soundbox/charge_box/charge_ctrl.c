#include "system/includes.h"
#include "app_config.h"
#include "app_action.h"
#include "charge_box/chargeIc_manage.h"
#include "charge_box/charge_ctrl.h"
#include "charge_box/charge_box.h"
#include "charge_box/charge_det.h"
#include "charge_box/wireless.h"
#include "key_event_deal.h"
#include "device/chargebox.h"
#include "le_rcsp_adv_module.h"
#include "charge_box/charge_box_ui.h"
#include "asm/pwm_led.h"

#if(defined TCFG_CHARGE_BOX_ENABLE) && ( TCFG_CHARGE_BOX_ENABLE)

#define LOG_TAG_CONST       APP_CHGBOX
#define LOG_TAG             "[CHG_CTRL]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"



#ifndef TCFG_CHARGGBOX_AUTO_SHUT_DOWN_TIME
#define TCFG_CHARGGBOX_AUTO_SHUT_DOWN_TIME   8
#endif

void chargebox_set_newstatus(u8 newstatus);

SYS_INFO sys_info;
EAR_INFO ear_info;

static int auto_shutdown_timer = 0;
static void sys_auto_shutdown_deal(void *priv)
{
    sys_info.life_cnt++;

    if (sys_info.life_cnt > TCFG_CHARGGBOX_AUTO_SHUT_DOWN_TIME) {
        log_info("charegebox enter soft poweroff\n");
        power_set_soft_poweroff();
    }
}

void sys_auto_shutdown_reset(void)
{
    sys_info.life_cnt = 0;
}

void sys_auto_shutdown_enable(void)
{
    if (!auto_shutdown_timer) {
        sys_info.life_cnt = 0;
        auto_shutdown_timer = sys_timer_add(NULL, sys_auto_shutdown_deal, 1000);
    }
}

void sys_auto_shutdown_disable(void)
{
    if (auto_shutdown_timer) {
        sys_timer_del(auto_shutdown_timer);
        auto_shutdown_timer = 0;
        sys_info.life_cnt = 0;
    }
}

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
static u8 check_time;

static void charge_app_lid_close_deal(void)
{
    if (sys_info.lid_cnt & BIT(7)) {
        if (sys_info.lid_cnt & (~BIT(7))) {
            sys_info.lid_cnt--;
            app_chargebox_send_mag(CHGBOX_MSG_SEND_CLOSE_LID);
        } else {
            sys_info.lid_cnt = 0;
            check_time = 0;
            sys_info.charge = 1;//发完盒盖命令再开升压
            //先关闭IO,在打开开关输出5V
            chargeIc_boost_ctrl(1);
            chargebox_api_close_port(EAR_L);
            chargeIc_vol_ctrl(1);
            chargebox_api_close_port(EAR_R);
            chargeIc_vor_ctrl(1);
        }
    }
}

static void charge_app_shut_down_deal(void)
{
    if (sys_info.shut_cnt & BIT(7)) {
        if (sys_info.shut_cnt & (~BIT(7))) {
            sys_info.shut_cnt--;
            app_chargebox_send_mag(CHGBOX_MSG_SEND_SHUTDOWN);
        } else {
            //关机命令后,电源线断电
            sys_info.shut_cnt = 0;
            chargebox_api_close_port(EAR_L);
            chargebox_api_close_port(EAR_R);
        }
    }
}

static void charge_app_send_power(void)
{
    if (sys_info.shut_cnt || sys_info.lid_cnt || sys_info.earfull) {
        return;
    }
    check_time++;
    if (check_time > 50) { //10s
        check_time = 0;
        app_chargebox_send_mag(CHGBOX_MSG_SEND_POWER_CLOSE);
    }
}

//检测如果ldo无法升压的情况
#define LDO_NOT_SUCC_TIMES  20
static u8 ldo_not_succ_cnt = 0;
static void charge_ldo_succ_det_500ms(void)
{
    if (sys_info.status[USB_DET] == STATUS_OFFLINE) {
        if (sys_info.status[LDO_DET] == STATUS_OFFLINE) { //无法升压
            if (ldo_not_succ_cnt < LDO_NOT_SUCC_TIMES) {
                ldo_not_succ_cnt++;
                if (ldo_not_succ_cnt == LDO_NOT_SUCC_TIMES) {
                    log_info("auto shutdown by ldo not succ\n");
                    sys_auto_shutdown_enable();
                }
            }
        } else {
            ldo_not_succ_cnt = 0;
        }
    }
}

static int charge_chargebox_event_handler(struct chargebox_event *e)
{
    switch (e->event) {
#if(WIRELESS_ENABLE)
    case CHGBOX_EVENT_WL_100MS:
        wireless_100ms_run_app();
        break;
    case CHGBOX_EVENT_WIRELESS_ONLINE:
        log_info("WL_ONLINE_1\n");
        wireless_api_open();
        break;
    case CHGBOX_EVENT_WIRELESS_OFFLINE:
        log_info("WL_OFF_1\n");
        wireless_api_close();
        break;
#endif
    case CHGBOX_EVENT_200MS:
        charge_app_lid_close_deal();
        if (chgbox_adv_addr_scan()) {
            charge_app_send_power();
        }
        charge_app_shut_down_deal();
        chgbox_handshake_repeatedly();
        break;
    case CHGBOX_EVENT_500MS:
        charge_ldo_succ_det_500ms();
        break;
    case CHGBOX_EVENT_USB_IN:
        log_info("USB_IN_1\n");
        if (sys_info.status[WIRELESS_DET] == STATUS_OFFLINE) { //判断当前无线充是否在线，在线不响应
            chgbox_handshake_run_app();
            chgbox_handshake_set_repeat(2);//多握手几次
        }
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_USB_IN);
        sys_auto_shutdown_disable();
        if (sys_info.status[LID_DET] == STATUS_ONLINE) {
            //开盖插入USB
            chargebox_set_newstatus(CHG_STATUS_COMM);     //设置新模式
        }
        break;
    case CHGBOX_EVENT_USB_OUT:
        log_info("USB_OUT_1\n");
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_USB_OUT);
        if (sys_info.earfull) {
            sys_auto_shutdown_enable();
        }
        if (sys_info.status[LID_DET] == STATUS_ONLINE) {
            //开盖拔出USB
            chargebox_set_newstatus(CHG_STATUS_COMM);     //设置新模式
        }
        break;
    case CHGBOX_EVENT_OPEN_LID:
        log_info("OPEN_LID_1\n");
#if RCSP_ADV_EN
        bt_ble_rcsp_adv_enable();
#endif
        chargebox_set_newstatus(CHG_STATUS_COMM);     //设置新模式
        break;
    case CHGBOX_EVENT_CLOSE_LID:
        log_info("CLOSE_LID_1\n");
        //开盖超时进来的,可以不做任何操作
#if RCSP_ADV_EN
        bt_ble_rcsp_adv_disable();
#endif
        break;
    case CHGBOX_EVENT_EAR_L_ONLINE:
        log_info("EAR_L_IN_1\n");
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_EAR_L_IN);
        break;
    case CHGBOX_EVENT_EAR_L_OFFLINE:
        log_info("EAR_L_OUT_1\n");
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_EAR_L_OUT);
        break;
    case CHGBOX_EVENT_EAR_R_ONLINE:
        log_info("EAR_R_IN_1\n");
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_EAR_R_IN);
        break;
    case CHGBOX_EVENT_EAR_R_OFFLINE:
        log_info("EAR_R_OUT_1\n");
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_EAR_R_OUT);
        break;
    case CHGBOX_EVENT_ENTER_LOWPOWER:
    case CHGBOX_EVENT_NEED_SHUTDOWN:
        //关闭升压,进入低电模式
        chargebox_set_newstatus(CHG_STATUS_LOWPOWER);     //设置新模式
        break;
    case CHGBOX_EVENT_EAR_FULL:
        //耳机充满后发送shutdown指令
        log_info("EAR_FULL_1\n");
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_EAR_FULL);
        //充满电时,先关闭输出再打开IO
        chargeIc_vol_ctrl(0);
        chargebox_api_open_port(EAR_L);
        chargeIc_vor_ctrl(0);
        chargebox_api_open_port(EAR_R);
        chargeIc_boost_ctrl(0);
        sys_info.shut_cnt = BIT(7) | TCFG_SEND_SHUT_DOWN_MAX;
        sys_info.lid_cnt = 0;
        sys_info.charge = 0;//此时充电结束
        if (sys_info.status[USB_DET] == STATUS_OFFLINE) {
            sys_auto_shutdown_enable();
        } else {
            if (sys_info.localfull) {
                sys_auto_shutdown_enable();
            }
        }
        break;
    case CHGBOX_EVENT_LOCAL_FULL:
        log_info("LOCAL_FULL_1\n");
        chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_LOCAL_FULL);
        if (sys_info.earfull) {
            sys_auto_shutdown_enable();
        }
        break;
    }
    return 0;
}







/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
#define KEY_PAIR_CNT    10
#define COMM_LIFE_MAX   (60*2)//一分钟超时
extern u8 key_table[KEY_AD_NUM_MAX][KEY_EVENT_MAX];
static u8 goto_pair_cnt, auto_exit_cnt;

static void comm_pair_connecting(void)
{
    if (sys_info.pair_status == 2) {
        if (sys_info.pair_succ) {
            sys_info.pair_status = 0;
            chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_PAIR_SUCC);
        } else {
            app_chargebox_send_mag(CHGBOX_MSG_SEND_PAIR);
        }
    }
}

static void comm_app_auto_exit(void)
{
    if (auto_exit_cnt++ > COMM_LIFE_MAX) {
#if RCSP_ADV_EN
        bt_ble_rcsp_adv_disable();
#endif
        if (sys_info.lowpower_flag) {
            chargebox_set_newstatus(CHG_STATUS_LOWPOWER);     //设置新模式
        } else {
            chargebox_set_newstatus(CHG_STATUS_CHARGE);     //设置新模式
        }
    }
}

static int comm_chargebox_event_handler(struct chargebox_event *e)
{
    switch (e->event) {
#if(WIRELESS_ENABLE)
    case CHGBOX_EVENT_WL_100MS:
        wireless_100ms_run_app();
        break;
    case CHGBOX_EVENT_WIRELESS_ONLINE:
        log_info("WL_ONLINE_2\n");
        wireless_api_open();
        break;
    case CHGBOX_EVENT_WIRELESS_OFFLINE:
        log_info("WL_OFF_2\n");
        wireless_api_close();
        break;
#endif
    case CHGBOX_EVENT_200MS:
        if (chgbox_adv_addr_scan()) {
            app_chargebox_send_mag(CHGBOX_MSG_SEND_POWER_OPEN);
        }
        chgbox_handshake_repeatedly();
        break;
    case CHGBOX_EVENT_500MS:
        comm_pair_connecting();
        comm_app_auto_exit();
        break;
    case CHGBOX_EVENT_USB_IN:
        log_info("USB_IN_2\n");
        if (sys_info.status[WIRELESS_DET] == STATUS_OFFLINE) {
            chgbox_handshake_run_app();
            chgbox_handshake_set_repeat(2);//多握手几次
        }
        chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_USB_IN);
        app_chargebox_send_mag(CHGBOX_MSG_SEND_POWER_OPEN);
        break;
    case CHGBOX_EVENT_USB_OUT:
        log_info("USB_OUT_2\n");
        chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_USB_OUT);
        app_chargebox_send_mag(CHGBOX_MSG_SEND_POWER_OPEN);
        break;
    case CHGBOX_EVENT_CLOSE_LID:
        log_info("CLOSE_LID_2\n");
#if RCSP_ADV_EN
        bt_ble_rcsp_adv_disable();
#endif
        if (sys_info.lowpower_flag) {
            chargebox_set_newstatus(CHG_STATUS_LOWPOWER);     //设置新模式
        } else {
            chargebox_set_newstatus(CHG_STATUS_CHARGE);     //设置新模式
        }
        break;
    case CHGBOX_EVENT_NEED_SHUTDOWN:
        chargebox_set_newstatus(CHG_STATUS_LOWPOWER);     //设置新模式
        break;
    case CHGBOX_EVENT_EAR_L_ONLINE:
        log_info("EAR_L_IN_2\n");
        chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_EAR_L_IN);
        break;
    case CHGBOX_EVENT_EAR_L_OFFLINE:
        log_info("EAR_L_OUT_2\n");
#if RCSP_ADV_EN
        bt_ble_rcsp_adv_disable();
#endif
        chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_EAR_L_OUT);
        break;
    case CHGBOX_EVENT_EAR_R_ONLINE:
        log_info("EAR_R_IN_2\n");
        chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_EAR_R_IN);
        break;
    case CHGBOX_EVENT_EAR_R_OFFLINE:
        log_info("EAR_R_OUT_2\n");
#if RCSP_ADV_EN
        bt_ble_rcsp_adv_disable();
#endif
        chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_EAR_R_OUT);
        break;
    }
    return 0;
}


/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
static void lowpower_shut_down_deal(void)
{
    if (sys_info.shut_cnt & BIT(7)) {
        if (sys_info.shut_cnt & (~BIT(7))) {
            sys_info.shut_cnt--;
            app_chargebox_send_mag(CHGBOX_MSG_SEND_SHUTDOWN);
        } else {
            //关机命令后,电源线断电
            sys_info.charge = 0;
            sys_info.shut_cnt = 0;
            chargebox_api_close_port(EAR_L);
            chargebox_api_close_port(EAR_R);
            if (!sys_info.init_ok) {
                //低于2.8V,直接关机
                power_set_soft_poweroff();
            }
        }
    }
}

static void lowpower_lid_close_deal(void)
{
    if (sys_info.lid_cnt & BIT(7)) {
        if (sys_info.lid_cnt & (~BIT(7))) {
            sys_info.lid_cnt--;
            app_chargebox_send_mag(CHGBOX_MSG_SEND_CLOSE_LID);
        } else {
            sys_info.lid_cnt = 0;
            sys_info.shut_cnt = BIT(7) | TCFG_SEND_SHUT_DOWN_MAX;
        }
    }
}

static int lowpower_chargebox_event_handler(struct chargebox_event *e)
{
    switch (e->event) {
#if(WIRELESS_ENABLE)
    case CHGBOX_EVENT_WL_100MS:
        wireless_100ms_run_app();
        break;
    case CHGBOX_EVENT_WIRELESS_ONLINE:
        log_info("WL_ONLINE_3\n");
        wireless_api_open();
        break;
    case CHGBOX_EVENT_WIRELESS_OFFLINE:
        log_info("WL_OFF_3\n");
        wireless_api_close();
        break;
#endif
    case CHGBOX_EVENT_200MS:
        lowpower_lid_close_deal();
        lowpower_shut_down_deal();
        chgbox_handshake_repeatedly();
        break;
    case CHGBOX_EVENT_500MS:
        break;
    case CHGBOX_EVENT_USB_IN:
        log_info("USB_IN_3\n");
        if (sys_info.status[WIRELESS_DET] == STATUS_OFFLINE) {
            chgbox_handshake_run_app();
            chgbox_handshake_set_repeat(2);//多握手几次
        }
        sys_auto_shutdown_disable();
        chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_USB_IN);
        break;
    case CHGBOX_EVENT_USB_OUT:
        log_info("USB_OUT_3\n");
        sys_auto_shutdown_enable();
        chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_USB_OUT);
        break;
    case CHGBOX_EVENT_OPEN_LID:
        log_info("OPEN_LID_3\n");
        chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_OPEN_LID);
#if RCSP_ADV_EN
        bt_ble_rcsp_adv_enable();
#endif
        chargebox_set_newstatus(CHG_STATUS_COMM);     //设置新模式
        break;
    case CHGBOX_EVENT_CLOSE_LID:
        log_info("CLOSE_LID_3\n");
        chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_CLOSE_LID);
#if RCSP_ADV_EN
        bt_ble_rcsp_adv_disable();
#endif
        break;
    case CHGBOX_EVENT_NEED_SHUTDOWN:
        if ((!sys_info.lid_cnt) && (sys_info.shut_cnt)) {
            power_set_soft_poweroff();
        }
        break;
    case CHGBOX_EVENT_EXIT_LOWPOWER:
        log_info("exit lower\n");
        if (sys_info.status[LID_DET] == STATUS_ONLINE) {
            chargebox_set_newstatus(CHG_STATUS_COMM);     //设置新模式
        } else {
            chargebox_set_newstatus(CHG_STATUS_CHARGE);     //设置新模式
        }
        break;
    }
    return 0;
}


//充电仓事件管理
int charge_ctrl_event_handler(struct sys_event *event)
{
    if (sys_info.chgbox_status == CHG_STATUS_CHARGE) {
        charge_chargebox_event_handler(&event->u.chargebox);
    } else if (sys_info.chgbox_status == CHG_STATUS_COMM) {
        comm_chargebox_event_handler(&event->u.chargebox);
    } else if (sys_info.chgbox_status == CHG_STATUS_LOWPOWER) {
        lowpower_chargebox_event_handler(&event->u.chargebox);
    }
    return 0;
}

//充电仓按键响应
int charge_key_event_handler(u8 event)
{
    if (sys_info.chgbox_status == CHG_STATUS_CHARGE) {
        switch (event) {
        case KEY_BOX_POWER_CLICK:
            log_info("KEY_POWER_CLICK_chg\n");
            if (sys_info.status[LID_DET] == STATUS_ONLINE) {
                //开盖拔出USB
                chargebox_set_newstatus(CHG_STATUS_COMM);     //设置新模式
            } else {
                chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_KEY_CLICK);
            }
            break;
        default:
            break;
        }
    } else if (sys_info.chgbox_status == CHG_STATUS_COMM) {
        switch (event) {
        case KEY_BOX_POWER_CLICK:
            log_info("KEY_POWER_CLICK_comm\n");
            chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_KEY_CLICK);
            break;
        case KEY_BOX_POWER_LONG:
            /* log_info("KEY_POWER_LONG\n"); */
            if (sys_info.pair_status == 0) {
                sys_info.pair_status = 1;
                goto_pair_cnt = 0;
                chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_KEY_LONG);
            }
            break;
        case KEY_BOX_POWER_HOLD:
            /* log_info("KEY_POWER_HOLD\n"); */
            if (sys_info.pair_status == 1) {
                goto_pair_cnt++;
                if (goto_pair_cnt >= KEY_PAIR_CNT) {
                    sys_info.pair_status = 2;
                    sys_info.pair_succ = 0;
                    chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_PAIR_START);
                }
            }
            break;
        case KEY_BOX_POWER_UP:
            log_info("KEY_POWER_UP\n");
            if (sys_info.pair_status != 2) {
                sys_info.pair_status = 0;
                chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_PAIR_STOP);
            }
            break;
        default:
            break;
        }
    } else if (sys_info.chgbox_status == CHG_STATUS_LOWPOWER) {
        switch (event) {
        case KEY_BOX_POWER_CLICK:
            log_info("KEY_POWER_CLICK_low\n");
            if (sys_info.status[USB_DET] == STATUS_OFFLINE) {
                chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_KEY_CLICK);
            }
            break;
        default:
            break;
        }
    }
    return 0;
}


///状态转换
void chargebox_set_newstatus(u8 newstatus)
{
    ///先退出当前状态
    log_info("chargebbox exit:%d\n", sys_info.chgbox_status);
    if (sys_info.chgbox_status  == CHG_STATUS_COMM) {
        sys_auto_shutdown_enable();
        if (sys_info.pair_status) {
            sys_info.pair_status = 0;
            chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_ALL_OFF);
        }
    } else if (sys_info.chgbox_status == CHG_STATUS_CHARGE) {
        sys_info.lid_cnt = 0;
        sys_info.shut_cnt = 0;
        sys_info.charge = 0;//不在充电状态
        //关闭充电输出时,需要把IO打开
        chargeIc_boost_ctrl(0);
        chargeIc_vol_ctrl(0);
        chargebox_api_open_port(EAR_L);
        chargeIc_vor_ctrl(0);
        chargebox_api_open_port(EAR_R);
        sys_auto_shutdown_enable();
    } else if (sys_info.chgbox_status == CHG_STATUS_LOWPOWER) {
        sys_info.shut_cnt = 0;
        sys_info.lid_cnt = 0;
    }

    ///设置新状态
    sys_info.chgbox_status = newstatus;
    if (newstatus  == CHG_STATUS_COMM) {
        sys_auto_shutdown_disable();
        sys_info.shut_cnt = 0;
        sys_info.pair_status = 0;
        sys_info.pair_succ = 0;
        sys_info.ear_l_full = 0;
        sys_info.ear_r_full = 0;
        sys_info.earfull = 0;
        goto_pair_cnt = 0;
        auto_exit_cnt = 0;
        sys_info.power_on = 0;
        chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_OPEN_LID);
    } else if (newstatus == CHG_STATUS_CHARGE) {
        check_time = 0;
        sys_info.shut_cnt = 0;
        sys_info.lid_cnt = BIT(7) | TCFG_SEND_CLOSE_LID_MAX;
        sys_info.charge = 0;//先关闭,等合盖命令发完才打开升压
        sys_info.ear_l_full = 0;
        sys_info.ear_r_full = 0;
        sys_info.earfull = 0;
        sys_auto_shutdown_disable();
        if (sys_info.power_on) {
            chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_POWER);
        } else {
            chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_CLOSE_LID);
        }
        sys_info.power_on = 0;
    } else if (newstatus == CHG_STATUS_LOWPOWER) {
        sys_info.shut_cnt = 0;
        sys_info.lid_cnt = 0;
        if (sys_info.status[USB_DET] == STATUS_ONLINE) {
            sys_auto_shutdown_disable();
            chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_USB_IN);
        } else {
            sys_auto_shutdown_enable();
            chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_LOWPOWER);
        }
        if (sys_info.power_on == 0) {
            if (!sys_info.earfull) {
                sys_info.lid_cnt = BIT(7) | TCFG_SEND_CLOSE_LID_MAX;
            }
        }
        sys_info.power_on = 0;
    }
    log_info("chargebbox newstatus:%d\n", newstatus);
}


//上电时的状态初始化
void app_charge_box_ctrl_init(void)
{
    if (!sys_info.init_ok) {
        log_error("chargeIc not ok, need softoff!\n");
        for (u8 i = 0; i < 3; i++) {
            pwm_led_mode_set(PWM_LED1_ON);
            os_time_dly(20);
            pwm_led_mode_set(PWM_LED1_OFF);
            os_time_dly(10);
        }
        power_set_soft_poweroff();
        return;
    }

    sys_info.power_on = 1;

    ///状态判断
    if (sys_info.status[LID_DET] == STATUS_ONLINE) {
        sys_info.chgbox_status = CHG_STATUS_COMM;
        sys_auto_shutdown_disable();
        sys_info.shut_cnt = 0;
        sys_info.pair_status = 0;
        sys_info.pair_succ = 0;
        sys_info.ear_l_full = 0;
        sys_info.ear_r_full = 0;
        sys_info.earfull = 0;
        goto_pair_cnt = 0;
        auto_exit_cnt = 0;
        sys_info.power_on = 0;
        chgbox_ui_update_status(UI_MODE_COMM, CHGBOX_UI_OPEN_LID);
    } else {
        if (sys_info.lowpower_flag) {
            sys_info.chgbox_status = CHG_STATUS_LOWPOWER;
            sys_info.shut_cnt = 0;
            sys_info.lid_cnt = 0;
            if (sys_info.status[USB_DET] == STATUS_ONLINE) {
                sys_auto_shutdown_disable();
                chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_USB_IN);
            } else {
                sys_auto_shutdown_enable();
                chgbox_ui_update_status(UI_MODE_LOWPOWER, CHGBOX_UI_LOWPOWER);
            }
            if (sys_info.power_on == 0) {
                if (!(sys_info.earfull)) {
                    sys_info.lid_cnt = BIT(7) | TCFG_SEND_CLOSE_LID_MAX;
                }
            }
            sys_info.power_on = 0;
        } else {
            sys_info.chgbox_status = CHG_STATUS_CHARGE;
            check_time = 0;
            sys_info.shut_cnt = 0;
            sys_info.lid_cnt = BIT(7) | TCFG_SEND_CLOSE_LID_MAX;
            sys_info.charge = 0;//先关闭,等合盖命令发完才打开升压
            sys_info.ear_l_full = 0;
            sys_info.ear_r_full = 0;
            sys_info.earfull = 0;
            sys_auto_shutdown_disable();
            if (sys_info.power_on) {
                chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_POWER);
            } else {
                chgbox_ui_update_status(UI_MODE_CHARGE, CHGBOX_UI_CLOSE_LID);
            }
            sys_info.power_on = 0;
        }
    }

    if (sys_info.status[USB_DET] == STATUS_ONLINE) {
        app_chargebox_event_to_user(CHGBOX_EVENT_USB_IN);
    }
    log_info("chgbox_poweron_status:%d\n", sys_info.chgbox_status);
}

u8 get_tws_ear_status(void)
{
    return (ear_info.online[EAR_L] && ear_info.online[EAR_R]);
}

u8 get_chgbox_lid_status(void)
{
    return (sys_info.status[LID_DET] == STATUS_ONLINE);
}



void chgbox_led_init(void);
void  chgbox_init_app(void)
{
    chargeIc_init(&chargeIc_data);
    hall_det_init(&hall_det_data);
#if(WIRELESS_ENABLE)
    wireless_init_api();
#endif
    chgbox_handshake_init();
    chgbox_led_init();
    app_charge_box_ctrl_init();
}

#endif
