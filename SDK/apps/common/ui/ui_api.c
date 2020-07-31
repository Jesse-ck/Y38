#include "includes.h"
#include "ui/ui_api.h"
#include "clock_cfg.h"

#if TCFG_UI_ENABLE
#define UI_DEBUG_ENABLE
#ifdef UI_DEBUG_ENABLE
#define ui_debug(fmt, ...) 	printf("[UI] "fmt, ##__VA_ARGS__)
#define ui_log(fmt, ...) 	printf("[UI] "fmt, ##__VA_ARGS__)
#define ui_error(fmt, ...) 	printf("[UI error] "fmt, ##__VA_ARGS__)
#else
#define ui_debug(...)
#define ui_log(...)
#define ui_error(...)
#endif

#define UI_NO_ARG (-1)
#define UI_TASK_NAME "ui"

struct ui_display_env {
    u8 init;
    u8 main_menu;
    u8 this_menu;
    u16 tmp_menu_ret_cnt;
    u16 auto_reflash_time;
    u16 time_count;
    void *display_buf;
    LCD_API *ui_api;
    void (*timeout_cb)(u8);
    UI_DIS_VAR *ui_dis_var;
};

static struct ui_display_env *__ui_display = NULL;


static void ui_user(void *hd, u8 menu, u32 arg)
{
    int ret = false;
    if (!hd) {
        return;
    }
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    if (ui_dis_var->ui && ui_dis_var->ui->ui_user) {
        ret =  ui_dis_var->ui->ui_user(hd, ui_dis_var->u.head, menu, arg);
    }

    if (ret != true) {
        ui_common(hd, ui_dis_var->u.head, menu, arg);    //公共显示
    }
}



static int ui_main_open(void *hd, u8 ui_main)
{
    int ret = false;
    UI_DIS_VAR *ui_dis_var = (UI_DIS_VAR *)hd;
    const struct ui_dis_api *ui;
    if (!hd) {
        return ret;
    }
    list_for_each_ui_main(ui) {
        if (ui->ui == ui_main) {
            if (ui_dis_var->ui) {
                ui_dis_var->ui->uninit(hd, ui_dis_var->u.head);
            }
            ui_dis_var->ui = ui;
            ui_dis_var->cur_main = ui_main;
            ui_dis_var->u.head = ui->init();
            ui_dis_var->cur_menu = 0;
            ui->ui_main((void *)ui_dis_var, ui_dis_var->u.head);
            ret = true;
            break;
        }
    }

    if (ui_main == UI_MENU_MAIN_NULL) {
        if (ui_dis_var->ui) {
            ui_dis_var->ui->uninit(hd, ui_dis_var->u.head);
            ui_dis_var->ui = NULL;
        }
        ret = true;
    }

    return ret;
}



//=================================================================================//
// @brief: 非主界面显示
// @input:
// 	1)tmp_menu: 要显示的非主界面
// 	2)ret_time: 持续时间ms, 返回主界面
//  3)arg: 显示参数
//  4)子菜单被打断或者时间到了
//=================================================================================//
void ui_set_tmp_menu(u8 app_menu, u16 ret_time, s32 arg, void (*timeout_cb)(u8 menu))
{

#if (TCFG_LED_LCD_ENABLE)
    /* printf("ui_set_tmp_menu %d %d %d\n", app_menu, ret_time, arg); */
    int msg[4];
    int count = 0;
    int err;
_wait:
    if (!__ui_display->init) {
        if (count > 10) {
            return;
        }
        count++;
        os_time_dly(1);
        goto _wait;

    }

    count = 0;
__try:
        if (__ui_display->init) {
            msg[0] = app_menu;
            msg[1] = arg;
            msg[2] = (int)((ret_time + 99) / 100);
            msg[3] = (int)timeout_cb;
            err =  os_taskq_post_type(UI_TASK_NAME, UI_MSG_OTHER, 4, msg);
            if (err) {
                if (count > 10) {
                    return;
                }
                count++;
                os_time_dly(1);
                goto __try;
            }
        }
#endif
}



void ui_menu_reflash(u8 break_in)//break_in 是否打断显示,例如显示设置过程中需要刷新新界面。是是否打断设置界面显示
{

#if (TCFG_LED_LCD_ENABLE)
    int msg[1];
    if (__ui_display->init) {
        msg[0] = (!!break_in);
        os_taskq_post_type(UI_TASK_NAME, UI_MSG_REFLASH, 1, msg);
    }
#endif
}


static void ui_strick_loop()
{
    if (__ui_display->init) {
        if (__ui_display->auto_reflash_time && ((++__ui_display->time_count) >= __ui_display->auto_reflash_time)) {
            ui_menu_reflash(0);
            __ui_display->time_count = 0;
        } else {
            os_taskq_post_type(UI_TASK_NAME, UI_MSG_STRICK, 0, NULL);
        }
    }
}

static void ui_task(void *p)
{
    int msg[32];
    int ret;
    sys_timer_add(NULL, ui_strick_loop, 100); //500ms
    __ui_display->init = 1;
    while (1) {
        ret = os_taskq_pend(NULL, msg, ARRAY_SIZE(msg)); //500ms_reflash
        if (ret != OS_TASKQ) {
            continue;
        }
        switch (msg[0]) { //action
        case UI_MSG_EXIT:
            os_sem_post((OS_SEM *)msg[1]);
            os_time_dly(10000);
            break;
        case UI_MSG_STRICK:
            if (__ui_display->tmp_menu_ret_cnt) {
                __ui_display->tmp_menu_ret_cnt--;
                if (!__ui_display->tmp_menu_ret_cnt && __ui_display->main_menu) {
                    if (__ui_display->timeout_cb) {
                        __ui_display->timeout_cb(__ui_display->this_menu);
                        __ui_display->timeout_cb = NULL;
                    }

                    if (__ui_display->ui_dis_var && __ui_display->ui_dis_var->ui) {
                        __ui_display->ui_dis_var->ui->ui_main(__ui_display->ui_dis_var, __ui_display->ui_dis_var->u.head);
                    }
                    __ui_display->this_menu = MENU_MAIN;
                }
            }
            break;
        case UI_MSG_REFLASH:
            if (msg[1] && (__ui_display->this_menu != MENU_MAIN)) {

                if (__ui_display->timeout_cb) {
                    __ui_display->timeout_cb(__ui_display->this_menu);
                    __ui_display->timeout_cb = NULL;
                }
            } else if (__ui_display->this_menu != MENU_MAIN) {
                break;
            }

            if (__ui_display->ui_dis_var && __ui_display->ui_dis_var->ui) {
                __ui_display->ui_dis_var->ui->ui_main(__ui_display->ui_dis_var, __ui_display->ui_dis_var->u.head);
            }

            __ui_display->this_menu = MENU_MAIN;
            break;

        case UI_MSG_OTHER:
            if (msg[1] != __ui_display->this_menu) {
                if (__ui_display->timeout_cb) {
                    __ui_display->timeout_cb(__ui_display->this_menu);
                    __ui_display->timeout_cb = NULL;
                }
            }
            __ui_display->tmp_menu_ret_cnt = msg[3];
            __ui_display->timeout_cb = (void (*)(u8))msg[4];
            __ui_display->this_menu = msg[1];

            if (__ui_display->ui_dis_var) {
                ui_user(__ui_display->ui_dis_var, msg[1], msg[2]);
            }
            break;

        case UI_MSG_MENU_SW:

            __ui_display->auto_reflash_time = 0;

            if (__ui_display->timeout_cb) {
                __ui_display->timeout_cb(__ui_display->this_menu);
                __ui_display->timeout_cb = NULL;
            }


            if (ui_main_open(__ui_display->ui_dis_var, msg[1])) {
                __ui_display->main_menu = msg[1];
                __ui_display->this_menu = MENU_MAIN;
                __ui_display->tmp_menu_ret_cnt = 0;
                __ui_display->time_count        = 0;
            }

        default:
            break;
        }
    }
}


static void *ui_core_init()
{
    UI_DIS_VAR *ui_dis_var = NULL;
    if (!ui_dis_var) {
        ui_dis_var = malloc(sizeof(UI_DIS_VAR));
    }
    memset(ui_dis_var, 0x00, sizeof(UI_DIS_VAR));
    return ui_dis_var;
}

int ui_init(const struct ui_devices_cfg *ui_cfg)
{

    clock_add(DEC_UI_CLK);
#if (TCFG_LED_LCD_ENABLE)
    int err = 0;
    __ui_display = (struct ui_display_env *)malloc(sizeof(struct ui_display_env));
    if (__ui_display == NULL) {
        return -ENODEV;
    }
    memset(__ui_display, 0x00, sizeof(struct ui_display_env));

    __ui_display->ui_dis_var = ui_core_init();

    if (!__ui_display->ui_dis_var) {
        return -ENODEV;
    }

#if TCFG_UI_LED1888_ENABLE
    if (ui_cfg->type == LED_7) {
        void *led_1888_init(const struct led7_platform_data * _data);
        __ui_display->ui_api = (LCD_API *)led_1888_init(ui_cfg->private_data);
    }
#endif

#if TCFG_UI_LED7_ENABLE
    void *led7_init(const struct led7_platform_data * _data);
    __ui_display->ui_api = (LCD_API *)led7_init(ui_cfg->private_data);
#endif

    if (__ui_display->ui_api == NULL) {
        return -ENODEV;
    }

    __ui_display->ui_dis_var->dis = __ui_display->ui_api;

    err = task_create(ui_task, NULL, UI_TASK_NAME);

#elif (TCFG_SPI_LCD_ENABLE)

    extern int ui_lcd_init(void *arg);
    ui_lcd_init(ui_cfg);

#endif
    return 0;
}



//进入app时设置一次, 设置主界面
void ui_set_main_menu(enum ui_menu_main menu)
{

#if (TCFG_LED_LCD_ENABLE)
    int msg[1];
    int err;
    int count = 0;
_wait:
    if (!__ui_display->init) {
        if (count > 10) {
            /* printf("!!!!!!!!! ui_set_main_menu err !!!\n"); */
            return;
        }
        count++;
        os_time_dly(1);
        goto _wait;
    }

    count = 0;

    if (__ui_display->init) {
        msg[0] = menu;
__try:
            err = os_taskq_post_type(UI_TASK_NAME, UI_MSG_MENU_SW, sizeof(msg) / sizeof(msg[0]), msg);
        if (err) {
            if (count > 5) {
                /* printf("~~~~~~~~~ ui_set_main_menu err !!!\n"); */
                return;
            }
            count++;
            os_time_dly(5);
            goto __try;
        }
    }
#endif

}




void ui_set_auto_reflash(u32 msec)//自动刷新主页
{

#if (TCFG_LED_LCD_ENABLE)
    if (__ui_display->init) {
        __ui_display->auto_reflash_time = (msec + 99) / 100;
    }
#endif

}


void ui_set_led(u8 app_menu, u8 on, u8 phase, u16 highlight, u16 period)
{

#if (TCFG_LED_LCD_ENABLE)
#if TCFG_UI_LED1888_ENABLE
    extern void LED1888_show_led0(u32 arg);
    extern void LED1888_show_led1(u32 arg);
    u32 arg = 0;
    arg = (!!on) << 31 | (!!phase) << 30 | highlight << 15 | (period - highlight);

    if (period >= highlight) {
        if (app_menu == MENU_LED0) {
            if (arg) {
                LED1888_show_led0(arg);
            } else {
                LED1888_show_led0(0);
            }
            return;
        } else if (app_menu == MENU_LED1) {
            if (arg) {
                LED1888_show_led1(arg);
            } else {
                LED1888_show_led1(0);
            }
            return;
        }

    }
#endif
#endif
    /* if(period>=highlight) */
    /* ui_set_menu_ioctl(app_menu, (!!on)<<31|(!!phase)<<30|highlight<<15|(period-highlight)); */
}


u8 ui_get_app_menu(u8 type)
{

#if (TCFG_LED_LCD_ENABLE)
    if (!__ui_display->ui_api) {
        return 0;
    }
    if (type) {
        return __ui_display->main_menu;
    } else {
        return __ui_display->this_menu;
    }
#endif
    return 0;
}

#if 0
void ___ui_run_test()
{
    static u8 i = 0;

    switch (i) {
    case 0:
        ui_set_menu_ioctl(MENU_BT_MAIN, 0);
        break;
    case 1:
        /* ui_set_menu_ioctl(MENU_AUX_MAIN,0); */
        break;
    case 2:
        ui_set_tmp_menu(MENU_FM_DISP_FRE, 2000, 1238, NULL);
        break;
    case 3:
        /* ui_set_menu_ioctl(MENU_FILENUM,454); */

        ui_set_menu_ioctl(MENU_AUX_MAIN, 0);
        break;

    default:
        i = 0;
        return;
        break;
    }
    i++;

}
#endif

#endif /* #if TCFG_UI_ENABLE */
