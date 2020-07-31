#ifndef _CHARGE_BOX_UI_H_
#define _CHARGE_BOX_UI_H_

#include "typedef.h"

typedef enum {
    CHGBOX_UI_NULL = 0,

    CHGBOX_UI_ALL_OFF,
    CHGBOX_UI_ALL_ON,

    CHGBOX_UI_POWER,
    CHGBOX_UI_EAR_FULL,
    CHGBOX_UI_LOCAL_FULL,
    CHGBOX_UI_LOWPOWER,

    CHGBOX_UI_EAR_L_IN,
    CHGBOX_UI_EAR_R_IN,
    CHGBOX_UI_EAR_L_OUT,
    CHGBOX_UI_EAR_R_OUT,

    CHGBOX_UI_KEY_CLICK,
    CHGBOX_UI_KEY_LONG,
    CHGBOX_UI_PAIR_START,
    CHGBOX_UI_PAIR_SUCC,
    CHGBOX_UI_PAIR_STOP,

    CHGBOX_UI_OPEN_LID,
    CHGBOX_UI_CLOSE_LID,

    CHGBOX_UI_USB_IN,
    CHGBOX_UI_USB_OUT,
} UI_STATUS;

enum {
    UI_MODE_CHARGE,
    UI_MODE_COMM,
    UI_MODE_LOWPOWER,
};
//UI
int  chgbox_ui_manage_init(void);
void chgbox_ui_update_status(u8 mode, u8 status);



//定义n个灯
enum {
    CHG_LED_RED,
    CHG_LED_GREEN,
    CHG_LED_BLUE,
    CHG_LED_MAX,
};

///IO设置
#define CHG_RED_LED_IO      IO_PORTC_02

#define CHG_GREEN_LED_IO    IO_PORTC_03

#define CHG_BLUE_LED_IO     IO_PORTC_04
//常亮、常暗、呼吸
enum {
    GHGBOX_LED_MODE_ON,
    GHGBOX_LED_MODE_OFF,
    GHGBOX_LED_MODE_BRE,
};

//点灯模式
enum {
    CHGBOX_LED_RED_OFF,
    CHGBOX_LED_RED_ON,
    CHGBOX_LED_RED_SLOW_FLASH,
    CHGBOX_LED_RED_FLAST_FLASH,

    CHGBOX_LED_GREEN_OFF,
    CHGBOX_LED_GREEN_ON,
    CHGBOX_LED_GREEN_FAST_FLASH,

    CHGBOX_LED_BLUE_ON,
    CHGBOX_LED_BLUE_OFF,
    CHGBOX_LED_BLUE_FAST_FLASH,

    CHGBOX_LED_ALL_OFF,
    CHGBOX_LED_ALL_ON,
};

typedef struct _CHG_SOFT_PWM_LED {
    //初始化，亮暗接口接口
    void (*led_init)(void);
    void (*led_on_off)(u8 on_off);

    u16 bre_times;    //呼吸次数,0xffff为循环

    u16 up_times;     //渐亮次数
    u16 light_times;  //亮次数
    u16 down_times;   //渐暗次数
    u16 dark_times;   //暗次数

    u16 step_cnt;
    u8  step;         //步骤

    u8  p_cnt;        //占空比计数
    u8  cur_duty;     //当前占空比
    u8  max_duty;     //最大占空比，控制最大亮度

    u8  busy;         //忙标志，更换参数时作保护用
    u8  mode;         //亮灯模式
} CHG_SOFT_PWM_LED;

//led驱动
void chgbox_led_init(void);
void chgbox_led_set_mode(u8 mode);

#endif    //_APP_CHARGEBOX_H_

