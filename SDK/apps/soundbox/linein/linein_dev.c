#include "app_config.h"
#include "system/event.h"
#include "system/init.h"
#include "system/timer.h"
#include "asm/power_interface.h"
#include "asm/adc_api.h"
#include "linein/linein.h"
#include "linein/linein_dev.h"
#include "gpio.h"

#if TCFG_LINEIN_ENABLE

#define LOG_TAG_CONST       APP_LINEIN
#define LOG_TAG             "[APP_LINEIN_DEV]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

struct linein_dev_opr {
    u8 cnt;
    u8 stu;
    u8 online;
    u8 active;
    u8 init;
    u8 step;
    int timer;
    struct linein_dev_data *dev;
};
static struct linein_dev_opr linein_dev_hdl = {0};
#define __this 	(&linein_dev_hdl)

// line online
u8 linein_is_online(void)
{
    return __this->online;
}
void linein_set_online(u8 online)
{
    __this->online = online;
}


#define LINEIN_STU_HOLD		0
#define LINEIN_STU_ON		1
#define LINEIN_STU_OFF		2

void linein_io_start(void)
{
    /* printf("<<<linein_io_start\n"); */
    struct linein_dev_data *linein_dev = (struct linein_dev_data *)__this->dev;
    if (__this->init) {
        return ;
    }
    __this->init = 1;
    if (linein_dev->down) {
        gpio_set_pull_down(linein_dev->port, 1);
    } else {
        gpio_set_pull_down(linein_dev->port, 0);
    }
    if (linein_dev->up) {
        gpio_set_pull_up(linein_dev->port, 1);
    } else {
        gpio_set_pull_up(linein_dev->port, 0);
    }
    if (linein_dev->ad_channel == (u8)NO_CONFIG_PORT) {
        gpio_set_die(linein_dev->port, 1);
    } else {
        gpio_set_die(linein_dev->port, 0);
    }
    gpio_set_hd(linein_dev->port, 0);
    gpio_set_hd0(linein_dev->port, 0);
    gpio_direction_input(linein_dev->port);
}

void linein_io_stop(void)
{
    /* printf("<<<linein_io_stop\n"); */
    struct linein_dev_data *linein_dev = (struct linein_dev_data *)__this->dev;
    if (!__this->init) {
        return ;
    }
    __this->init = 0;
    gpio_direction_input(linein_dev->port);
    gpio_set_pull_up(linein_dev->port, 0);
    gpio_set_pull_down(linein_dev->port, 0);
    gpio_set_hd(linein_dev->port, 0);
    gpio_set_hd0(linein_dev->port, 0);
    gpio_set_die(linein_dev->port, 0);
}

static int linein_check(void *arg, u8 cnt)
{
    struct linein_dev_data *linein_dev = (struct linein_dev_data *)arg;
    u8 cur_stu;

    if (linein_dev->ad_channel == (u8)NO_CONFIG_PORT) {
        cur_stu = gpio_read(linein_dev->port) ? false : true;
    } else {
        cur_stu = adc_get_value(linein_dev->ad_channel) > linein_dev->ad_vol ? false : true;
        /* printf("<%d> ", adc_get_value(linein_dev->ad_channel)); */
    }
    if (!linein_dev->up) {
        cur_stu	= !cur_stu;
    }
    /* putchar('A' + cur_stu); */

    if (cur_stu != __this->stu) {
        __this->stu = cur_stu;
        __this->cnt = 0;
        __this->active = 1;
    } else {
        __this->cnt ++;
    }

    if (__this->cnt < cnt) {
        return LINEIN_STU_HOLD;
    }
    __this->active = 0;

    if ((linein_is_online()) && (!__this->stu)) {
        linein_set_online(false);
        return LINEIN_STU_OFF;
    } else if ((!linein_is_online()) && (__this->stu)) {
        linein_set_online(true);
        return LINEIN_STU_ON;
    }
    return LINEIN_STU_HOLD;
}

extern void adc_enter_occupy_mode(u32 ch);
extern void adc_exit_occupy_mode();
extern u32 adc_occupy_run();
extern u32 adc_get_occupy_value();

int linein_sample_detect(void *arg)
{
    struct linein_dev_data *linein_dev = (struct linein_dev_data *)arg;
    linein_io_start();

    u8 cur_stu;
    if (linein_dev->ad_channel == (u8)NO_CONFIG_PORT) {
        cur_stu = gpio_read(linein_dev->port) ? false : true;
    } else {
        adc_enter_occupy_mode(linein_dev->ad_channel);
        if (adc_occupy_run()) {
            cur_stu = adc_get_occupy_value() > linein_dev->ad_vol ? false : true;
        } else {
            cur_stu = __this->stu;
        }
        adc_exit_occupy_mode();
        /* printf("\n<%d>\n", adc_get_voltage(linein_dev->ad_channel)); */
    }

    /* putchar('A'+cur_stu); */
    /* putchar(cur_stu); */
    linein_io_stop();

    if (cur_stu == __this->stu) {
        __this->cnt = 0;
    } else {
        __this->cnt++;
        if (__this->cnt == 3) {
            __this->cnt = 0;
            __this->stu = cur_stu;
            if (__this->stu == true) {
                linein_set_online(true);
                return LINEIN_STU_ON;
            } else {
                linein_set_online(false);
                return LINEIN_STU_OFF;
            }
        }
    }

    return -1;
}

extern s32 sd_io_suspend(u8 sd_io);
extern s32 sd_io_resume(u8 sd_io);
static void linein_detect(void *arg)
{
    int res;
    struct sys_event event = {0};
#if (defined(TCFG_LINEIN_MULTIPLEX_WITH_SD) && \
		(TCFG_LINEIN_MULTIPLEX_WITH_SD == ENABLE))
    if (sd_io_suspend(0) == 0) {
        res = linein_sample_detect(arg);
        sd_io_resume(0);
    } else {
        return;
    }
#else
    if (__this->step == 0) {
        __this->step = 1;
        linein_io_start();
        sys_timer_modify(__this->timer, 10);
        return ;
    }

    res = linein_check(arg, 3);
    if (!__this->active) {
        __this->step = 0;
        linein_io_stop();
        sys_timer_modify(__this->timer, 500);
    }
#endif

    event.arg  = (void *)DEVICE_EVENT_FROM_LINEIN;
    event.type = SYS_DEVICE_EVENT;

    if (res == LINEIN_STU_ON) {
        event.u.dev.event = DEVICE_EVENT_IN;
    } else if (res == LINEIN_STU_OFF) {
        event.u.dev.event = DEVICE_EVENT_OUT;
    } else {
        return ;
    }
    sys_event_notify(&event);
}

void linein_detect_timer_add()
{
    if (__this->timer == 0) {
        __this->timer = sys_timer_add(__this->dev, linein_detect, 25);
    }
}
void linein_detect_timer_del()
{
    if (__this->timer) {
        sys_timer_del(__this->timer);
        __this->timer = 0;
    }
}
static int linein_driver_init(const struct dev_node *node,  void *arg)
{
    struct linein_dev_data *linein_dev = (struct linein_dev_data *)arg;
    if (!linein_dev->enable) {
        linein_set_online(true);
        return 0;
    }
    if (linein_dev->port == (u8)NO_CONFIG_PORT) {
        linein_set_online(true);
        return 0;
    }
    linein_set_online(false);

    __this->dev = linein_dev;

    __this->timer = sys_timer_add(arg, linein_detect, 50);
    return 0;
}

const struct device_operations linein_dev_ops = {
    .init = linein_driver_init,
};

static u8 linein_dev_idle_query(void)
{
    return !__this->active;
}

REGISTER_LP_TARGET(linein_dev_lp_target) = {
    .name = "linein_dev",
    .is_idle = linein_dev_idle_query,
};

#endif

