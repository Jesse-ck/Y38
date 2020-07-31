#include "app_config.h"
#include "system/includes.h"
#include "charge_box/charge_box.h"
#include "user_cfg.h"
#include "device/vm.h"
#include "app_action.h"
#include "app_main.h"
#include "charge_box/chargeIc_manage.h"
#include "charge_box/charge_ctrl.h"
#include "device/chargebox.h"
#include "asm/power/p33.h"

#if(defined TCFG_CHARGE_BOX_ENABLE) && ( TCFG_CHARGE_BOX_ENABLE)

#define LOG_TAG_CONST       APP_CHGBOX
#define LOG_TAG             "[APP_CBOX]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"


#define CHGBOX_THR_NAME   "chgbox_n"

//是否绑定的LDO_IN脚
#define BINDING_WITH_LDO_IN  1

//耳机满电检测次数，满电电压
#define CHG_FULL_DET_CNT       160 //50ms*16 =  8s
#define CHG_FULL_DET_LEVEL     100 //电压值


///电池参数，电池不同，参数不同
#define POWER_TOP_LVL   4200
#define POWER_BOOT_LVL  3100
#define POWER_LVL_MAX   11
const u16  voltage_table[2][POWER_LVL_MAX] = {
    {            0,  10,  20,  30,  40,  50,  60,   70,   80,   90,            100},
    {POWER_BOOT_LVL, 3600, 3660, 3720, 3780, 3840, 3900, 3950, 4000, 4050, POWER_TOP_LVL},
};

u8 get_box_power_lvl()
{
    u16 max, min, power;
    u8 i;

    power = chargeIc_get_vbat();

    if (power <= POWER_BOOT_LVL) {
        return 0;
    }
    if (power >= POWER_TOP_LVL) {
        return 100;
    }

    for (i = 0; i < POWER_LVL_MAX; i++) {
        if (power < voltage_table[1][i]) {
            break;
        }
    }
    min = voltage_table[1][i - 1];
    max = voltage_table[1][i];
    return ((u8)(((power - min) * 10 / (max - min)) + voltage_table[0][i - 1]));
}

void enter_hook(void)
{
    //进入发送数据前,先关闭升压输出
    if (sys_info.charge) {
        chargeIc_vol_ctrl(0);
        chargeIc_vor_ctrl(0);
    }
    chargebox_api_open_port(EAR_L);
    chargebox_api_open_port(EAR_R);
}

void exit_hook(void)
{
    //退出发送数据后,是否需要打开升压输出
    if (sys_info.charge) {
        chargebox_api_close_port(EAR_L);
        chargeIc_vol_ctrl(1);
        chargebox_api_close_port(EAR_R);
        chargeIc_vor_ctrl(1);
    }
}

void app_chargebox_send_mag(int msg)
{
    //有数据需要发送,自动关机计时器复位
    sys_info.life_cnt = 0;
    os_taskq_post_msg(CHGBOX_THR_NAME, 1, msg);
}

void app_chargebox_event_to_user(u8 event)
{
    struct sys_event e;
    e.type = SYS_DEVICE_EVENT;
    e.arg  = (void *)DEVICE_EVENT_FROM_CHARGEBOX;
    e.u.dev.event = event;
    sys_event_notify(&e);
}

void app_chargebox_api_check_online(bool ret_l, bool ret_r)
{
    if (ret_l == TRUE) {
        if (ear_info.online[EAR_L] == 0) {
            //发事件,耳机入舱
            app_chargebox_event_to_user(CHGBOX_EVENT_EAR_L_ONLINE);
        }
        ear_info.online[EAR_L] = TCFG_EAR_OFFLINE_MAX;
    } else {
        if (ear_info.online[EAR_L]) {
            ear_info.online[EAR_L]--;
            if (ear_info.online[EAR_L] == 0) {
                ear_info.power[EAR_L] = 0xff;
                //发事件,耳机离舱
                app_chargebox_event_to_user(CHGBOX_EVENT_EAR_L_OFFLINE);
            }
        }
    }
    if (ret_r == TRUE) {
        if (ear_info.online[EAR_R] == 0) {
            //发事件,耳机入舱
            app_chargebox_event_to_user(CHGBOX_EVENT_EAR_R_ONLINE);
        }
        ear_info.online[EAR_R] = TCFG_EAR_OFFLINE_MAX;
    } else {
        if (ear_info.online[EAR_R]) {
            ear_info.online[EAR_R]--;
            if (ear_info.online[EAR_R] == 0) {
                ear_info.power[EAR_R] = 0xff;
                //发事件,耳机离舱
                app_chargebox_event_to_user(CHGBOX_EVENT_EAR_R_OFFLINE);
            }
        }
    }
}

u8 app_chargebox_api_send_shutdown(void)
{
    u8 ret0, ret1;
    enter_hook();
    ret0 = chargebox_send_shut_down(EAR_L);
    ret1 = chargebox_send_shut_down(EAR_R);
    exit_hook();
    if ((ret0 == TRUE) && (ret1 == TRUE)) {
        return TRUE;
    }
    return FALSE;
}

u8 app_chargebox_api_send_close_cid(void)
{
    u8 online_cnt = 0;;
    u8 ret0, ret1;
    if (ear_info.online[EAR_L]) {
        online_cnt += 1;
    }
    if (ear_info.online[EAR_R]) {
        online_cnt += 1;
    }
    enter_hook();
    ret0 = chargebox_send_close_cid(EAR_L, online_cnt);
    ret1 = chargebox_send_close_cid(EAR_R, online_cnt);
    exit_hook();
    app_chargebox_api_check_online(ret0, ret1);
    if ((ret0 == TRUE) && (ret1 == TRUE)) {
        log_debug("send close CID ok\n");
        return TRUE;
    } else {
        log_error("L:%d,R:%d\n", ret0, ret1);
    }
    return FALSE;
}

//存放左、右、公共地址
static u8 adv_addr_tmp_buf[3][6];
void get_lr_adr_cb(u8 lr, u8 *inbuf)
{
    if (lr) {
        memcpy(&adv_addr_tmp_buf[1][0], inbuf, 6);
    } else {
        memcpy(&adv_addr_tmp_buf[0][0], inbuf, 6);
    }
}

void exchange_addr_succ_cb(void)
{
    u8 i;
    for (i = 0; i < 6; i++) {
        adv_addr_tmp_buf[2][i] = adv_addr_tmp_buf[0][i] + adv_addr_tmp_buf[1][i];
    }
    sys_info.chg_addr_ok = 1;
}

u8 *get_chargebox_adv_addr(void)
{
    if (sys_info.chg_addr_ok) {
        return &adv_addr_tmp_buf[2][0];
    } else {
        return NULL;
    }
}

u8 chgbox_addr_read_from_vm(void)
{
    if (6 == syscfg_read(CFG_CHGBOX_ADDR, &adv_addr_tmp_buf[2][0], 6)) {
        sys_info.chg_addr_ok = 1;
        log_info("Read adv addr OK:");
        put_buf(&adv_addr_tmp_buf[2][0], 6);
        return TRUE;
    } else {
        sys_info.chg_addr_ok = 0;
        log_error("Read adv addr error\n");
        return FALSE;
    }
}

u8 chgbox_addr_save_to_vm(void)
{
    if (6 == syscfg_write(CFG_CHGBOX_ADDR, &adv_addr_tmp_buf[2][0], 6)) {
        log_info("Write adv addr OK!\n");
        return TRUE;
    } else {
        log_error("Write adv addr error!\n");
        return FALSE;
    }
}

u8 chgbox_adv_addr_scan(void)
{
    static u8 caa_cnt = 0;
    if (!sys_info.chg_addr_ok) {
        ///左右耳在线,且lid_cnt已清
        if (ear_info.online[EAR_L] && ear_info.online[EAR_R] && !sys_info.lid_cnt) {
            if (caa_cnt == 0) { //交换地址
                app_chargebox_send_mag(CHGBOX_MSG_SEND_PAIR);
            }
            caa_cnt++;
            if (caa_cnt == 3) {
                caa_cnt = 0;
            }
            return 0;//拿地址的过程中不发其他命令
        }
    }
    return 1;
}


u8 app_chargebox_api_exchange_addr(void)
{
    u8 ret = FALSE;
    if (ear_info.online[EAR_L] && ear_info.online[EAR_R]) {
        enter_hook();
        ret = chargebox_exchange_addr(get_lr_adr_cb, exchange_addr_succ_cb);
        exit_hook();
    }

    if (ret) {
        //交换地址成功后记录地址
        chgbox_addr_save_to_vm();
    }
    return ret;
}




void app_chargebox_api_send_power(u8 flag)
{
    u8 power;
    u8 ret0, ret1, is_charge = 0;
    power = 98;//get_power_lvl();
    if (sys_info.status[USB_DET] == STATUS_ONLINE) {
        is_charge = 1;
    }
    enter_hook();
    if (flag == 0) {
        ret0 = chargebox_send_power_open(EAR_L, power, is_charge, ear_info.power[EAR_R]);
        ret1 = chargebox_send_power_open(EAR_R, power, is_charge, ear_info.power[EAR_L]);
    } else {
        ret0 = chargebox_send_power_close(EAR_L, power, is_charge, ear_info.power[EAR_R]);
        ret1 = chargebox_send_power_close(EAR_R, power, is_charge, ear_info.power[EAR_L]);
    }
    exit_hook();
    app_chargebox_api_check_online(ret0, ret1);
    if (ret0 == TRUE) {
        ear_info.power[EAR_L] = chargebox_get_power(EAR_L);
        log_info("EARL:%d_%d \n", ear_info.power[EAR_L]&BIT(7) ? 1 : 0, ear_info.power[EAR_L] & (~BIT(7)));
    } else {
        /* log_error("Can't got L\n"); */
    }
    if (ret1 == TRUE) {
        ear_info.power[EAR_R] = chargebox_get_power(EAR_R);
        log_info("EARR:%d_%d\n", ear_info.power[EAR_R]&BIT(7) ? 1 : 0, ear_info.power[EAR_R] & (~BIT(7)));
    } else {
        /* log_error("Can't got R\n"); */
    }
}

void app_chargebox_ear_full_det(void)
{
    /* log_debug("L:%d,F:%d,C:%d\n",ear_info.online[EAR_L],sys_info.ear_l_full,ear_info.full_cnt[EAR_L]); */
    if (ear_info.online[EAR_L]) { //在线
        if ((ear_info.power[EAR_L] & 0x7f) >= CHG_FULL_DET_LEVEL && sys_info.ear_l_full == 0) { //power的最高bit为标志位
            ear_info.full_cnt[EAR_L]++;
            if (ear_info.full_cnt[EAR_L] >= CHG_FULL_DET_CNT) { //50ms * 160 == 8s
                sys_info.ear_l_full = 1;       //充满标志置位
            }
        } else {
            ear_info.full_cnt[EAR_L] = 0;
        }
    } else {
        ear_info.full_cnt[EAR_L] = 0;  //计数清0
        sys_info.ear_l_full = 0;       //充满标志清0
    }

    if (ear_info.online[EAR_R]) { //在线
        if ((ear_info.power[EAR_R] & 0x7f) >= CHG_FULL_DET_LEVEL && sys_info.ear_r_full == 0) { //power的最高bit为标志位
            ear_info.full_cnt[EAR_R]++;
            if (ear_info.full_cnt[EAR_R] >= CHG_FULL_DET_CNT) {
                sys_info.ear_r_full = 1;       //充满标志置位
            }
        } else {
            ear_info.full_cnt[EAR_R] = 0;
        }
    } else {
        ear_info.full_cnt[EAR_R] = 0;  //计数清0
        sys_info.ear_r_full = 0;       //充满标志清0
    }

    if (sys_info.earfull == 0) {
        //同时在线两个在线都满了、单个在线电满了
        if ((sys_info.ear_r_full && sys_info.ear_l_full)
            || (sys_info.ear_l_full && ear_info.online[EAR_R] == 0)
            || (sys_info.ear_r_full && ear_info.online[EAR_L] == 0)) {
            sys_info.earfull  = 1;
            app_chargebox_event_to_user(CHGBOX_EVENT_EAR_FULL);
        }
    } else { //没满但在线
        if ((!sys_info.ear_l_full && ear_info.online[EAR_L])
            || (!sys_info.ear_r_full && ear_info.online[EAR_R])) {
            sys_info.earfull  = 0;
        }
    }
}


//该线程只负责给左右耳发送数据
void app_chargebox_task_handler(void *priv)
{
    int msg[32];
    log_info("data thread running! \n");
    while (1) {
        if (os_task_pend("taskq", msg, ARRAY_SIZE(msg)) != OS_TASKQ) {
            continue;
        }
        switch (msg[1]) {
        case CHGBOX_MSG_SEND_POWER_OPEN:
            app_chargebox_api_send_power(0);
            break;
        case CHGBOX_MSG_SEND_POWER_CLOSE:
            app_chargebox_api_send_power(1);
            break;
        case CHGBOX_MSG_SEND_CLOSE_LID:
            app_chargebox_api_send_close_cid();
            break;
        case CHGBOX_MSG_SEND_SHUTDOWN:
            app_chargebox_api_send_shutdown();
            break;
        case CHGBOX_MSG_SEND_PAIR:
            log_info("CHANGE ear ADDR\n");
            if (app_chargebox_api_exchange_addr() == TRUE) {
                sys_info.pair_succ = 1;
            } else {
                log_error("pair_fail\n");
            }
            break;
        default:
            log_info("default msg: %d\n", msg[1]);
            break;
        }
    }
}

CHARGEBOX_PLATFORM_DATA_BEGIN(chargebox_data)
.L_port = TCFG_CHARGEBOX_L_PORT,
 .R_port = TCFG_CHARGEBOX_R_PORT,
  CHARGEBOX_PLATFORM_DATA_END()

  void app_chargebox_timer_handle(void *priv)
{
    static u8 ms200_cnt = 0;
    static u8 ms500_cnt = 0;

    ms200_cnt++;
    if (ms200_cnt >= 2) {
        ms200_cnt = 0;
        app_chargebox_event_to_user(CHGBOX_EVENT_200MS);
    }
    ms500_cnt++;
    if (ms500_cnt >= 5) {
        ms500_cnt = 0;
        app_chargebox_event_to_user(CHGBOX_EVENT_500MS);
    }
}

int app_chargestore_init(void)
{
    chgbox_addr_read_from_vm();
    chargebox_api_init(&chargebox_data);
    task_create(app_chargebox_task_handler, NULL, CHGBOX_THR_NAME);
    sys_timer_add(NULL, app_chargebox_timer_handle, 100);//推事件处理

///若与LDOIN 邦定
#if BINDING_WITH_LDO_IN
    CHGBG_EN(0);
    CHARGE_EN(0);
#endif
    return 0;
}

__initcall(app_chargestore_init);

#endif

