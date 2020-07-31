#include "gpio.h"
#include "system/event.h"
#include "app_config.h"
#include "charge_box/charge_det.h"
#include "charge_box/charge_box.h"
#include "charge_box/charge_ctrl.h"
#include "system/timer.h"

#if(defined TCFG_CHARGE_BOX_ENABLE) && ( TCFG_CHARGE_BOX_ENABLE)

struct _io_det_hdl {
    const struct io_det_platform_data *data;
    u16 det_cnt;
};

struct _io_det_hdl hall_det = {
    .data = NULL,
    .det_cnt = 0,
};

static void hall_detect_cb(void *priv)
{
    u8 io_level = gpio_read(hall_det.data->port);
    if (sys_info.status[LID_DET] == STATUS_ONLINE) {
        if (((hall_det.data->level == LOW_LEVEL) && io_level) || ((hall_det.data->level == HIGHT_LEVEL) && (!io_level))) {
            hall_det.det_cnt++;
            sys_info.life_cnt = 0;
            if (hall_det.det_cnt >= hall_det.data->offline_time) {
                hall_det.det_cnt = 0;
                sys_info.status[LID_DET] = STATUS_OFFLINE;
                app_chargebox_event_to_user(CHGBOX_EVENT_CLOSE_LID);
            }
        } else {
            hall_det.det_cnt = 0;//去抖
        }
    } else {
        if (((hall_det.data->level == LOW_LEVEL) && (!io_level)) || ((hall_det.data->level == HIGHT_LEVEL) && io_level)) {
            hall_det.det_cnt++;
            sys_info.life_cnt = 0;
            if (hall_det.det_cnt >= hall_det.data->online_time) {
                hall_det.det_cnt = 0;
                sys_info.status[LID_DET] = STATUS_ONLINE;
                app_chargebox_event_to_user(CHGBOX_EVENT_OPEN_LID);
            }
        } else {
            hall_det.det_cnt = 0;//去抖
        }
    }
}
void hall_det_init(const struct io_det_platform_data *data)
{
    u8 io_level;
    hall_det.data = data;
    sys_info.status[LID_DET] = STATUS_OFFLINE;
    gpio_set_direction(hall_det.data->port, 1);
    gpio_set_die(hall_det.data->port, 1);
    gpio_set_pull_down(hall_det.data->port, 0);
    gpio_set_pull_up(hall_det.data->port, 0);
    sys_s_hi_timer_add(NULL, hall_detect_cb, 10);  //10ms
    delay(100);
    io_level = gpio_read(hall_det.data->port);
    if (((hall_det.data->level == LOW_LEVEL) && (!io_level)) || ((hall_det.data->level == HIGHT_LEVEL) && io_level)) {
        sys_info.status[LID_DET] = STATUS_ONLINE;
    }
}


#endif
