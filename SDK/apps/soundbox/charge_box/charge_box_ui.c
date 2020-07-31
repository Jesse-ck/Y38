#include "typedef.h"
#include "asm/pwm_led.h"
#include "system/includes.h"
#include "charge_box/charge_ctrl.h"
#include "charge_box/chargeIc_manage.h"
#include "charge_box/charge_box_ui.h"

#if(defined TCFG_CHARGE_BOX_ENABLE) && ( TCFG_CHARGE_BOX_ENABLE)

#define LOG_TAG_CONST       APP_CHGBOX
#define LOG_TAG             "[CHGBOXUI]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"


typedef struct _chgbox_ui_var_ {
    int ui_timer;
} _chgbox_ui_var;

static _chgbox_ui_var chgbox_ui_var;
#define __this  (&chgbox_ui_var)

void chgbox_ui_update_timeout(void *priv)
{
    u8 ledmode = (u8)priv;
    __this->ui_timer = 0;
    chgbox_led_set_mode(ledmode);
}

u16 chgbox_ui_timeout_add(int priv, void (*func)(void *priv), u32 msec)
{
    if (__this->ui_timer) {
        sys_timer_del(__this->ui_timer);
        __this->ui_timer = 0;
    }
    if (func != NULL) {
        __this->ui_timer = sys_timeout_add((void *)priv, func, msec);
    }
    return __this->ui_timer;
}
void chgbox_ui_update_local_power(void)
{
    //配对状态,不显示舱电量UI
    if (sys_info.pair_status) {
        return;
    }
    if (sys_info.status[USB_DET] == STATUS_ONLINE) {
        chgbox_ui_timeout_add(0, NULL, 0);
        if (sys_info.localfull) {
            chgbox_led_set_mode(CHGBOX_LED_GREEN_ON);//充满后绿灯常亮
        } else {
            chgbox_led_set_mode(CHGBOX_LED_RED_SLOW_FLASH);//充电中灯慢闪
        }
    } else {
        if (sys_info.lowpower_flag) {
            chgbox_led_set_mode(CHGBOX_LED_GREEN_FAST_FLASH); //快闪4秒
            chgbox_ui_timeout_add(CHGBOX_LED_GREEN_OFF, chgbox_ui_update_timeout, 4000);
        } else {
            chgbox_led_set_mode(CHGBOX_LED_GREEN_ON);
            chgbox_ui_timeout_add(CHGBOX_LED_GREEN_OFF, chgbox_ui_update_timeout, 8000);
        }
    }
}

void chgbox_ui_updata_default_status(u8 status)
{
    switch (status) {
    case CHGBOX_UI_ALL_OFF:
        chgbox_ui_timeout_add(0, NULL, 0);
        chgbox_led_set_mode(CHGBOX_LED_ALL_OFF);
        break;
    case CHGBOX_UI_ALL_ON:
        chgbox_ui_timeout_add(0, NULL, 0);
        chgbox_led_set_mode(CHGBOX_LED_ALL_ON);
        break;
    case CHGBOX_UI_POWER:
        chgbox_ui_update_local_power();
        break;
    }
}

void chgbox_ui_updata_charge_status(u8 status)
{
    switch (status) {
    case CHGBOX_UI_USB_IN:
    case CHGBOX_UI_KEY_CLICK:
    case CHGBOX_UI_LOCAL_FULL:
        chgbox_ui_update_local_power();
        break;
    case CHGBOX_UI_USB_OUT:
        chgbox_ui_timeout_add(0, NULL, 0);
        chgbox_led_set_mode(CHGBOX_LED_ALL_OFF);
        break;
    case CHGBOX_UI_CLOSE_LID:
        if (sys_info.status[USB_DET] == STATUS_ONLINE) {
            chgbox_ui_update_local_power();
        } else {
            chgbox_ui_timeout_add(0, NULL, 0);
            chgbox_led_set_mode(CHGBOX_LED_ALL_OFF);
        }
        break;
    case CHGBOX_UI_EAR_FULL:
        break;
    default:
        chgbox_ui_updata_default_status(status);
        break;
    }
}

void chgbox_ui_updata_comm_status(u8 status)
{
    switch (status) {
    case CHGBOX_UI_USB_IN:
    case CHGBOX_UI_LOCAL_FULL:
    case CHGBOX_UI_KEY_CLICK:
    case CHGBOX_UI_OPEN_LID:
        chgbox_ui_update_local_power();
        break;
    case CHGBOX_UI_USB_OUT:
        if (!sys_info.pair_status) {
            chgbox_ui_timeout_add(0, NULL, 0);
            chgbox_led_set_mode(CHGBOX_LED_RED_OFF);
        }
        break;
    case CHGBOX_UI_EAR_L_IN:
    case CHGBOX_UI_EAR_R_IN:
    case CHGBOX_UI_EAR_L_OUT:
    case CHGBOX_UI_EAR_R_OUT:
        if (!sys_info.pair_status) {
            if (sys_info.status[USB_DET] == STATUS_ONLINE) {
                if (sys_info.localfull) {
                    chgbox_led_set_mode(CHGBOX_LED_RED_OFF);
                    chgbox_ui_timeout_add(CHGBOX_LED_RED_ON, chgbox_ui_update_timeout, 500);
                }
            } else {
                chgbox_led_set_mode(CHGBOX_LED_GREEN_ON);
                chgbox_ui_timeout_add(CHGBOX_LED_GREEN_OFF, chgbox_ui_update_timeout, 500);
            }
        }
        break;
    case CHGBOX_UI_KEY_LONG:
        chgbox_ui_timeout_add(0, NULL, 0);
        chgbox_led_set_mode(CHGBOX_LED_BLUE_ON);
        break;
    case CHGBOX_UI_PAIR_START:
        chgbox_ui_timeout_add(0, NULL, 0);
        chgbox_led_set_mode(CHGBOX_LED_BLUE_FAST_FLASH);
        break;
    case CHGBOX_UI_PAIR_SUCC:
        if (sys_info.status[USB_DET] == STATUS_OFFLINE) {
            chgbox_ui_timeout_add(CHGBOX_LED_BLUE_OFF, chgbox_ui_update_timeout, 500);
        } else {
            if (!sys_info.localfull) {
                chgbox_ui_timeout_add(CHGBOX_LED_RED_SLOW_FLASH, chgbox_ui_update_timeout, 500);
            } else {
                chgbox_ui_timeout_add(0, NULL, 0);
                chgbox_led_set_mode(CHGBOX_LED_BLUE_ON);
            }
        }
        break;
    case CHGBOX_UI_PAIR_STOP:
        if (sys_info.status[USB_DET] == STATUS_ONLINE) {
            chgbox_ui_update_local_power();
        } else {
            chgbox_ui_timeout_add(0, NULL, 0);
            chgbox_led_set_mode(CHGBOX_LED_BLUE_OFF);
        }
        break;
    default:
        chgbox_ui_updata_default_status(status);
        break;
    }
}

void chgbox_ui_updata_lowpower_status(u8 status)
{
    switch (status) {
    case CHGBOX_UI_LOCAL_FULL:
    case CHGBOX_UI_LOWPOWER:
    case CHGBOX_UI_KEY_CLICK:
    case CHGBOX_UI_OPEN_LID:
    case CHGBOX_UI_CLOSE_LID:
    case CHGBOX_UI_USB_OUT:
    case CHGBOX_UI_USB_IN:
        chgbox_ui_update_local_power();
        break;
    default:
        chgbox_ui_updata_default_status(status);
        break;
    }
}

void chgbox_ui_update_status(u8 mode, u8 status)
{
    switch (mode) {
    case UI_MODE_CHARGE:
        chgbox_ui_updata_charge_status(status);
        break;
    case UI_MODE_COMM:
        chgbox_ui_updata_comm_status(status);
        break;
    case UI_MODE_LOWPOWER:
        chgbox_ui_updata_lowpower_status(status);
        break;
    }
}




#define UP_TIMES_DEFAULT    50
#define DOWN_TIMES_DEFAULT  50

CHG_SOFT_PWM_LED chgbox_led[CHG_LED_MAX];


//设置灯的状态status
void chgbox_set_led_stu(u8 led_type, u8 on_off)
{
    //只开关一个灯,其他关掉
    u8 i;
    for (i = 0; i < CHG_LED_MAX; i++) {
        chgbox_led[i].busy = 1;

        if (led_type == i) {
            if (on_off) {
                chgbox_led[i].mode = GHGBOX_LED_MODE_ON; //常亮
                //要根据当前占空比，把cnt计算好，避免亮度突变
                chgbox_led[i].step_cnt = chgbox_led[i].up_times * chgbox_led[i].cur_duty / chgbox_led[i].max_duty;
                chgbox_led[i].up_times = UP_TIMES_DEFAULT;
            } else {
                chgbox_led[i].mode = GHGBOX_LED_MODE_OFF; //常暗
                //要根据当前占空比，把cnt计算好
                chgbox_led[i].step_cnt = chgbox_led[i].down_times - chgbox_led[i].down_times * chgbox_led[i].cur_duty / chgbox_led[i].max_duty;
                chgbox_led[i].down_times = DOWN_TIMES_DEFAULT;
            }
        } else {
            chgbox_led[i].mode = GHGBOX_LED_MODE_OFF; //其他灯常暗
            chgbox_led[i].step_cnt = chgbox_led[i].down_times - chgbox_led[i].down_times * chgbox_led[i].cur_duty / chgbox_led[i].max_duty;
            chgbox_led[i].down_times = 20;
        }
        chgbox_led[i].bre_times = 0;
        chgbox_led[i].busy = 0;
    }
}
enum {
    LED_FLASH_FAST,
    LED_FLASH_SLOW,

};
//设置呼吸灯
void chgbox_set_led_bre(u8 led_type, u8 mode)
{
    //只开关一个灯,其他关掉
    u8 i;
    for (i = 0; i < CHG_LED_MAX; i++) {
        chgbox_led[i].busy = 1;

        if (led_type == i) {
            chgbox_led[i].mode = GHGBOX_LED_MODE_BRE; //呼吸
            chgbox_led[i].bre_times = 0xffff;         //循环
            if (mode == LED_FLASH_FAST) {
                chgbox_led[i].up_times = 50;
                chgbox_led[i].light_times = 50;
                chgbox_led[i].down_times = 50;
                chgbox_led[i].dark_times = 10;
            } else if (mode == LED_FLASH_SLOW) {
                chgbox_led[i].up_times = 300;
                chgbox_led[i].light_times = 200;
                chgbox_led[i].down_times = 300;
                chgbox_led[i].dark_times = 100;
            }
            //要根据当前占空比，把cnt计算好,
            chgbox_led[i].step_cnt = chgbox_led[i].up_times * chgbox_led[i].cur_duty / chgbox_led[i].max_duty;
        } else {
            chgbox_led[i].mode = GHGBOX_LED_MODE_OFF; //常暗
            chgbox_led[i].down_times = 20;
            chgbox_led[i].step_cnt = chgbox_led[i].down_times - chgbox_led[i].down_times * chgbox_led[i].cur_duty / chgbox_led[i].max_duty;
            chgbox_led[i].bre_times = 0;
        }
        chgbox_led[i].busy = 0;
    }
}

void chgbox_set_led_all_off(void)
{
    u8 i;
    for (i = 0; i < CHG_LED_MAX; i++) {
        chgbox_led[i].busy = 1;

        chgbox_led[i].mode = GHGBOX_LED_MODE_OFF; //常暗
        chgbox_led[i].step_cnt = chgbox_led[i].down_times - chgbox_led[i].down_times * chgbox_led[i].cur_duty / chgbox_led[i].max_duty;
        chgbox_led[i].down_times = DOWN_TIMES_DEFAULT;
        chgbox_led[i].bre_times = 0;

        chgbox_led[i].busy = 0;
    }
}
//不同的亮暗或闪烁可以自己调配
void chgbox_led_set_mode(u8 mode)
{
    u8 i;
    log_info("CHG_LED_mode:%d\n", mode);
    switch (mode) {
    case CHGBOX_LED_RED_ON://红灯
        chgbox_set_led_stu(CHG_LED_RED, 1);
        break;
    case CHGBOX_LED_RED_OFF://红灯
        chgbox_set_led_stu(CHG_LED_RED, 0);
        break;
    case CHGBOX_LED_GREEN_ON:
        chgbox_set_led_stu(CHG_LED_GREEN, 1);
        break;
    case CHGBOX_LED_GREEN_OFF:
        chgbox_set_led_stu(CHG_LED_GREEN, 0);
        break;
    case CHGBOX_LED_BLUE_ON:
        chgbox_set_led_stu(CHG_LED_BLUE, 1);
        break;
    case CHGBOX_LED_BLUE_OFF:
        chgbox_set_led_stu(CHG_LED_BLUE, 0);
        break;
    case CHGBOX_LED_RED_SLOW_FLASH:
        chgbox_set_led_bre(CHG_LED_RED, LED_FLASH_SLOW);
        break;
    case CHGBOX_LED_RED_FLAST_FLASH:
        chgbox_set_led_bre(CHG_LED_RED, LED_FLASH_FAST);
        break;
    case CHGBOX_LED_GREEN_FAST_FLASH:
        chgbox_set_led_bre(CHG_LED_GREEN, LED_FLASH_FAST);
        break;
    case CHGBOX_LED_BLUE_FAST_FLASH:
        chgbox_set_led_bre(CHG_LED_BLUE, LED_FLASH_FAST);
        break;
    case CHGBOX_LED_ALL_OFF:
        chgbox_set_led_all_off();
        break;
    case CHGBOX_LED_ALL_ON:
        break;
    }
}


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
//硬件、软pwm驱动部分
void chg_red_led_init()
{
    gpio_set_die(CHG_RED_LED_IO, 1);
    gpio_set_pull_down(CHG_RED_LED_IO, 0);
    gpio_set_pull_up(CHG_RED_LED_IO, 0);
    gpio_direction_output(CHG_RED_LED_IO, 0);
    //初始化为暗态
    gpio_direction_output(CHG_RED_LED_IO, 0);
}

SEC(.chargebox_code)//频繁调用的放ram里
void chg_set_red_led(u8 on_off)
{
    u8 io_num;
    io_num = CHG_RED_LED_IO % 16;
    if (on_off) {
#if(CHG_RED_LED_IO <= IO_PORTA_15)
        JL_PORTA->OUT |= BIT(io_num);
#elif(CHG_RED_LED_IO <= IO_PORTB_15)
        JL_PORTB->OUT |= BIT(io_num);
#elif(CHG_RED_LED_IO <= IO_PORTC_15)
        JL_PORTC->OUT |= BIT(io_num);
#elif(CHG_RED_LED_IO <= IO_PORTD_7)
        JL_PORTD->OUT |= BIT(io_num);
#endif
    } else {
#if(CHG_RED_LED_IO <= IO_PORTA_15)
        JL_PORTA->OUT &= ~BIT(io_num);
#elif(CHG_RED_LED_IO <= IO_PORTB_15)
        JL_PORTB->OUT &= ~BIT(io_num);
#elif(CHG_RED_LED_IO <= IO_PORTC_15)
        JL_PORTC->OUT &= ~BIT(io_num);
#elif(CHG_RED_LED_IO <= IO_PORTD_7)
        JL_PORTD->OUT &= ~BIT(io_num);
#endif
    }
}

void chg_green_led_init()
{
    gpio_set_die(CHG_GREEN_LED_IO, 1);
    gpio_set_pull_down(CHG_GREEN_LED_IO, 0);
    gpio_set_pull_up(CHG_GREEN_LED_IO, 0);
    gpio_direction_output(CHG_GREEN_LED_IO, 0);
    //初始化为暗态
    gpio_direction_output(CHG_GREEN_LED_IO, 0);
}

SEC(.chargebox_code)//频繁调用的放ram里
void chg_set_green_led(u8 on_off)
{
    u8 io_num;
    io_num = CHG_GREEN_LED_IO % 16;
    if (on_off) {
#if(CHG_GREEN_LED_IO <= IO_PORTA_15)
        JL_PORTA->OUT |= BIT(io_num);
#elif(CHG_GREEN_LED_IO <= IO_PORTB_15)
        JL_PORTB->OUT |= BIT(io_num);
#elif(CHG_GREEN_LED_IO <= IO_PORTC_15)
        JL_PORTC->OUT |= BIT(io_num);
#elif(CHG_GREEN_LED_IO <= IO_PORTD_7)
        JL_PORTD->OUT |= BIT(io_num);
#endif
    } else {
#if(CHG_GREEN_LED_IO <= IO_PORTA_15)
        JL_PORTA->OUT &= ~BIT(io_num);
#elif(CHG_GREEN_LED_IO <= IO_PORTB_15)
        JL_PORTB->OUT &= ~BIT(io_num);
#elif(CHG_GREEN_LED_IO <= IO_PORTC_15)
        JL_PORTC->OUT &= ~BIT(io_num);
#elif(CHG_GREEN_LED_IO <= IO_PORTD_7)
        JL_PORTD->OUT &= ~BIT(io_num);
#endif
    }
}


void chg_blue_led_init()
{
    gpio_set_die(CHG_BLUE_LED_IO, 1);
    gpio_set_pull_down(CHG_BLUE_LED_IO, 0);
    gpio_set_pull_up(CHG_BLUE_LED_IO, 0);
    gpio_direction_output(CHG_BLUE_LED_IO, 0);
    //初始化为暗态
    gpio_direction_output(CHG_BLUE_LED_IO, 0);
}

SEC(.chargebox_code)//频繁调用的放ram里
void chg_set_blue_led(u8 on_off)
{
    u8 io_num;
    io_num = CHG_BLUE_LED_IO % 16;
    if (on_off) {
#if(CHG_BLUE_LED_IO <= IO_PORTA_15)
        JL_PORTA->OUT |= BIT(io_num);
#elif(CHG_BLUE_LED_IO <= IO_PORTB_15)
        JL_PORTB->OUT |= BIT(io_num);
#elif(CHG_BLUE_LED_IO <= IO_PORTC_15)
        JL_PORTC->OUT |= BIT(io_num);
#elif(CHG_BLUE_LED_IO <= IO_PORTD_7)
        JL_PORTD->OUT |= BIT(io_num);
#endif
    } else {
#if(CHG_BLUE_LED_IO <= IO_PORTA_15)
        JL_PORTA->OUT &= ~BIT(io_num);
#elif(CHG_BLUE_LED_IO <= IO_PORTB_15)
        JL_PORTB->OUT &= ~BIT(io_num);
#elif(CHG_BLUE_LED_IO <= IO_PORTC_15)
        JL_PORTC->OUT &= ~BIT(io_num);
#elif(CHG_BLUE_LED_IO <= IO_PORTD_7)
        JL_PORTD->OUT &= ~BIT(io_num);
#endif
    }
}



#define MC_TIMER_UNIT_US           30  //多少us起一次中断
#define SOFT_MC_PWM_MAX            128  //pwm周期(MC_TIMER_UNIT_US*SOFT_MC_PWM_MAX us)

enum {
    SOFT_LED_STEP_UP = 0,
    SOFT_LED_STEP_LIGHT,
    SOFT_LED_STEP_DOWN,
    SOFT_LED_STEP_DARK,
};

SEC(.chargebox_code)
void soft_pwm_led_ctrl(u8 i)
{
    switch (chgbox_led[i].mode) {
    case GHGBOX_LED_MODE_ON: //常亮
        if (chgbox_led[i].cur_duty < chgbox_led[i].max_duty) {
            chgbox_led[i].step_cnt++;
            if (chgbox_led[i].step_cnt >= chgbox_led[i].up_times) {
                chgbox_led[i].step_cnt = 0;
                chgbox_led[i].cur_duty  = chgbox_led[i].max_duty;
            } else {
                chgbox_led[i].cur_duty = chgbox_led[i].step_cnt * chgbox_led[i].max_duty / chgbox_led[i].up_times;
            }
        }
        break;
    case GHGBOX_LED_MODE_OFF://常暗
        if (chgbox_led[i].cur_duty > 0) {
            chgbox_led[i].step_cnt++;
            if (chgbox_led[i].step_cnt >= chgbox_led[i].up_times) {
                chgbox_led[i].step_cnt = 0;
                chgbox_led[i].cur_duty  = 0;
            } else {
                chgbox_led[i].cur_duty = (chgbox_led[i].down_times - chgbox_led[i].step_cnt) * chgbox_led[i].max_duty / chgbox_led[i].down_times;
            }
        }
        break;
    case GHGBOX_LED_MODE_BRE://呼吸灯模式
        if (chgbox_led[i].bre_times == 0) {
            break;
        }

        if (chgbox_led[i].step == SOFT_LED_STEP_UP) {
            chgbox_led[i].step_cnt++;
            if (chgbox_led[i].step_cnt >= chgbox_led[i].up_times) { //当前段结束
                chgbox_led[i].step_cnt = 0;
                chgbox_led[i].step++; //进入下一个步骤
            } else {
                chgbox_led[i].cur_duty = chgbox_led[i].step_cnt * chgbox_led[i].max_duty / chgbox_led[i].up_times;
            }
        } else if (chgbox_led[i].step == SOFT_LED_STEP_LIGHT) {
            chgbox_led[i].step_cnt++;
            chgbox_led[i].cur_duty = chgbox_led[i].max_duty;
            if (chgbox_led[i].step_cnt >= chgbox_led[i].light_times) {
                chgbox_led[i].step_cnt = 0;
                chgbox_led[i].step++;
            }
        } else if (chgbox_led[i].step == SOFT_LED_STEP_DOWN) {
            chgbox_led[i].step_cnt++;
            if (chgbox_led[i].step_cnt >= chgbox_led[i].down_times) {
                chgbox_led[i].step_cnt = 0;
                chgbox_led[i].step++;
            } else {
                chgbox_led[i].cur_duty = (chgbox_led[i].down_times - chgbox_led[i].step_cnt) * chgbox_led[i].max_duty / chgbox_led[i].down_times;
            }
        } else if (chgbox_led[i].step == SOFT_LED_STEP_DARK) {
            chgbox_led[i].step_cnt++;
            chgbox_led[i].cur_duty = 0;
            if (chgbox_led[i].step_cnt >= chgbox_led[i].dark_times) {
                chgbox_led[i].step_cnt = 0;
                chgbox_led[i].step = 0;    //重新开始下一次呼吸
                if (chgbox_led[i].bre_times != 0xffff) { //非循环
                    chgbox_led[i].bre_times--; //呼吸次数递减
                }
            }
        }
        break;
    }
}


SEC(.chargebox_code)
___interrupt
void soft_pwm_led_isr(void)
{
    JL_MCPWM->TMR0_CON |= BIT(10); //清pending
    u8 i;
    for (i = 0; i < CHG_LED_MAX; i++) { //循环所有的灯
        if (!chgbox_led[i].busy) {
            if (chgbox_led[i].p_cnt < chgbox_led[i].cur_duty) { //占空比
                chgbox_led[i].led_on_off(1); //亮
            } else {
                chgbox_led[i].led_on_off(0);
            }
            chgbox_led[i].p_cnt++;
            if (chgbox_led[i].p_cnt >= SOFT_MC_PWM_MAX) { //完成一个PWM周期
                chgbox_led[i].p_cnt = 0;
                soft_pwm_led_ctrl(i);//占空比控制
            }
        }
    }
}

static const u32 timer_div_mc[] = {
    1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768
};
#define MC_MAX_TIME_CNT            0x7fff
#define MC_MIN_TIME_CNT            0x10
void mc_clk_init(void)
{
    u32 prd_cnt;
    u8 index;

    JL_MCPWM->TMR0_CON = BIT(10); //清pending,清其他bit
    JL_MCPWM->MCPWM_CON0 = 0;

    for (index = 0; index < (sizeof(timer_div_mc) / sizeof(timer_div_mc[0])); index++) {
        prd_cnt = MC_TIMER_UNIT_US * (clk_get("lsb") / 1000000) / timer_div_mc[index];
        if (prd_cnt > MC_MIN_TIME_CNT && prd_cnt < MC_MAX_TIME_CNT) {
            break;
        }
    }

    JL_MCPWM->TMR0_CNT = 0;
    JL_MCPWM->TMR0_PR = prd_cnt;
    JL_MCPWM->TMR0_CON |= index << 3; //分频系数

    request_irq(IRQ_MCTMRX_IDX, 3, soft_pwm_led_isr, 0);

    JL_MCPWM->TMR0_CON |= BIT(8);  //允许定时溢出中断
    JL_MCPWM->TMR0_CON |= BIT(0);  //递增模式

    JL_MCPWM->MCPWM_CON0 |= BIT(8); //只开mc timer 0
    /* log_info("prd_cnt:%d,index:%d,t0:%x,MCP:%x\n",prd_cnt,index,JL_MCPWM->TMR0_CON,JL_MCPWM->MCPWM_CON0); */
    /* log_info("lsb:%d\n",clk_get("lsb")); */
}

void chgbox_led_init(void)
{
    u8 i;
    for (i = 0; i < CHG_LED_MAX; i++) { //循环所有的灯
        memset(&chgbox_led[i], 0x0, sizeof(CHG_SOFT_PWM_LED));
        chgbox_led[i].up_times = UP_TIMES_DEFAULT;
        chgbox_led[i].light_times = 100;
        chgbox_led[i].down_times = DOWN_TIMES_DEFAULT;
        chgbox_led[i].dark_times = 10;
        chgbox_led[i].bre_times = 0;

        //可根据需要修改初始化，但要把初始化与亮灭注册进来
        if (i == CHG_LED_RED) {
            chgbox_led[i].led_on_off = chg_set_red_led;
            chgbox_led[i].led_init = chg_red_led_init;
            chgbox_led[i].max_duty = SOFT_MC_PWM_MAX;
            chgbox_led[i].mode = GHGBOX_LED_MODE_OFF;
        } else if (i == CHG_LED_GREEN) {
            chgbox_led[i].led_on_off = chg_set_green_led;
            chgbox_led[i].led_init = chg_green_led_init;
            chgbox_led[i].max_duty = SOFT_MC_PWM_MAX;
            chgbox_led[i].mode = GHGBOX_LED_MODE_OFF;
        } else if (i == CHG_LED_BLUE) {
            chgbox_led[i].led_on_off = chg_set_blue_led;
            chgbox_led[i].led_init = chg_blue_led_init;
            chgbox_led[i].max_duty = SOFT_MC_PWM_MAX;
            chgbox_led[i].mode = GHGBOX_LED_MODE_OFF;
        }
        chgbox_led[i].led_init();
    }
    mc_clk_init();
}
#endif
