#ifndef _UI_API_H_
#define _UI_API_H_

#include "app_config.h"
#include "ui/ui_common.h"
#include "ui/led7/led7_driver.h"
#include "ui/lcd_seg/lcd_seg3x9_driver.h"
#include "ui/lcd_spi/lcd_drive.h"
#include "music/music_ui.h"

extern void ui_common(void *hd, void *private, u8 menu, u32 arg);//公共显示


#define GRT_CUR_MENU    (0)
#define GET_MAIN_MENU   (1)




enum ui_menu_main {
    UI_MENU_MAIN_NULL = 0,
    UI_RTC_MENU_MAIN,
    UI_MUSIC_MENU_MAIN,
    UI_AUX_MENU_MAIN,
    UI_BT_MENU_MAIN,
    UI_RECORD_MENU_MAIN,
    UI_FM_MENU_MAIN,
    UI_PC_MENU_MAIN,
};



enum {
    UI_MSG_NON = 0,
    UI_MSG_REFLASH,
    UI_MSG_OTHER,
    UI_MSG_STRICK,
    UI_MSG_MENU_SW,
    UI_MSG_KEY,
    UI_MSG_SHOW,
    UI_MSG_HIDE,
    UI_MSG_EXIT,
    UI_MSG_NULL,
};



enum {

    MENU_POWER_UP = 1,
    MENU_WAIT,
    MENU_BT,
    MENU_PC,
    MENU_PC_VOL_UP,
    MENU_PC_VOL_DOWN,
    MENU_AUX,
    MENU_ALM_UP,

    MENU_SHOW_STRING,
    MENU_MAIN_VOL,
    MENU_SET_EQ,
    MENU_SET_PLAY_MODE,

    MENU_PLAY_TIME,
    MENU_FILENUM,
    MENU_INPUT_NUMBER,
    MENU_MUSIC_PAUSE,
    MENU_MUSIC_REPEATMODE,

    MENU_FM_MAIN,
    MENU_FM_DISP_FRE,
    MENU_FM_SET_FRE,
    MENU_FM_STATION,
    MENU_IR_FM_SET_FRE,

    MENU_RTC_SET,
    MENU_RTC_PWD,
    MENU_ALM_SET,

    MENU_BT_SEARCH_DEVICE,
    MENU_BT_CONNECT_DEVICE,
    MENU_BT_DEVICE_ADD,
    MENU_BT_DEVICE_NAME,
    MENU_RECODE_MAIN,
    MENU_RECODE_ERR,
    MENU_POWER,
    MENU_LIST_DISPLAY,


    MENU_LED0,
    MENU_LED1,


    MENU_RECORD,

    MENU_SEC_REFRESH = 0x80,
    MENU_REFRESH,
    MENU_MAIN = 0xff,
};
//=================================================================================//
//                        			非主界面模式                    			   //
//=================================================================================//

//=================================================================================//
//                        			RTC模式数据结构                    			   //
//=================================================================================//
/*
显示内容, 每个模式下显示内容是固定的, 枚举是有穷的;
*/
enum rtc_menu_mode {
    UI_RTC_ACTION_SHOW_TIME,   //显示时间
    UI_RTC_ACTION_SHOW_DATE,   //显示日期
    UI_RTC_ACTION_YEAR_SET,    //年设置
    UI_RTC_ACTION_MONTH_SET,   //月设置
    UI_RTC_ACTION_DAY_SET,     //日设置
    UI_RTC_ACTION_HOUR_SET,    //时设置
    UI_RTC_ACTION_MINUTE_SET,  //分设置
    UI_RTC_ACTION_ALARM_UP,    //闹铃响
    UI_RTC_ACTION_STRING_SET,//设置字符
};

struct ui_rtc_time {
    u16 Year;
    u8 Month;
    u8 Day;
    u8 Hour;
    u8 Min;
    u8 Sec;
};


struct ui_rtc_display {
    enum rtc_menu_mode rtc_menu; //用于选择是否闪烁/常亮;
    struct ui_rtc_time time;
    const char *str;
};


//=================================================================================//
//                        			UI 配置数据结构                    			   //
//=================================================================================//
struct ui_dis_api {
    u8  ui;
    void *(*init)();
    void (*ui_main)(void *hd, void *private);
    int (*ui_user)(void *hd, void *private, u8 menu, u32 arg);
    void (*uninit)(void *hd, void *private);
};



typedef struct _LCD_DISP_API {
    void (*clear)(void);
    void (*setXY)(u32 x, u32 y);
    void (*FlashChar)(u32);
    void (*Clear_FlashChar)(u32);
    void (*show_string)(u8 *);
    void (*show_char)(u8);
    void (*show_number)(u8);
    void (*show_icon)(u32);
    void (*flash_icon)(u32);
    void (*clear_icon)(u32);
    void (*show_pic)(u32);
    void (*hide_pic)(u32);
    void (*lock)(u32);
} LCD_API;




enum ui_devices_type {
    LED_7,
    LCD_SEG3X9,
    TFT_LCD,//彩屏
    DOT_LCD,//点阵屏
};

//板级配置数据结构
struct ui_devices_cfg {
    enum ui_devices_type type;
    void *private_data;
};

typedef struct _UI_DIS_VAR {
    u8 bt_connect_sta;//蓝牙连接状态
    u8 bt_a2dp_sta;//音乐播放状态
    u8 bt_eq_mode;
    u8 sys_vol;//系统音量
    u8 cur_main;//当前主页
    u8 cur_menu;//当前显示
    u16 fmtx_freq;
    union {
        MUSIC_DIS_VAR *music;
        void *head;
    } u;
    LCD_API *dis;
    const struct ui_dis_api *ui;//指向具体模式的处理函数
} UI_DIS_VAR;


//=================================================================================//
//                        			UI API                    			   		   //
//=================================================================================//
#if (TCFG_UI_ENABLE)
//common api
int ui_init(const struct ui_devices_cfg *ui_cfg);
void ui_menu_reflash(u8 break_in);
void ui_set_main_menu(enum ui_menu_main menu);
void ui_set_tmp_menu(u8 app_menu, u16 ret_time, s32 arg, void (*timeout_cb)(u8 menu));
u8   ui_get_app_menu(u8);
void ui_set_led(u8 app_menu, u8 on, u8 phase, u16 highlight, u16 period);
void ui_set_auto_reflash(u32 msec);//自动刷新主页
void *ui_get_main_menu_var();


#define ui_set_app_menu(...)
#define ui_menu_man_reflash(...)
#define ui_auto_auto_reflash_open(...)
#define ui_auto_auto_reflash_close(...)
// #define ui_set_menu_ioctl(...);

//rtc ui api
void rtc_ui_open(void);
void rtc_ui_close(void);
struct ui_rtc_display *rtc_ui_get_display_buf();


#else
//common api
#define ui_init(...)
#define ui_set_app_menu(...)
#define ui_menu_man_reflash(...)
#define ui_auto_auto_reflash_open(...)
#define ui_auto_auto_reflash_close(...)
#define ui_tmp_menu_display(...)
#define ui_menu_reflash(...)
//rtc ui api
#define rtc_ui_open(...)
#define rtc_ui_close(...)
// #define rtc_ui_get_display_buf(...)  (NULL)

#define ui_set_menu_ioctl(...)
#define ui_set_tmp_menu(...)
#define ui_set_main_menu(...)
#define ui_set_auto_reflash(...)
#endif /* #if TCFG_UI_ENABLE */



#define REGISTER_UI_MAIN(ui) \
        const struct ui_dis_api ui sec(.ui_main)

extern struct ui_dis_api  ui_main_begin[];
extern struct ui_dis_api  ui_main_end[];

#define list_for_each_ui_main(c) \
	for (c=ui_main_begin; c<ui_main_end; c++)



#endif
