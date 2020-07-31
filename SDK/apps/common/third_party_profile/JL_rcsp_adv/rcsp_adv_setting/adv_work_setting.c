#include "app_config.h"
#if (RCSP_ADV_EN)
#include "syscfg_id.h"
#include "le_rcsp_adv_module.h"

#include "adv_work_setting.h"
#include "adv_setting_common.h"
#include "rcsp_adv_tws_sync.h"
#include "rcsp_adv_opt.h"

extern int get_bt_tws_connect_status();
extern void bt_set_low_latency_mode(int enable);
void set_work_setting(u8 work_setting_info)
{
    _s_info.work_mode =	work_setting_info;
}

u8 get_work_setting(void)
{
    return _s_info.work_mode;
}

static void adv_work_setting_vm_value(u8 work_setting_info)
{
    syscfg_write(CFG_RCSP_ADV_WORK_SETTING, &work_setting_info, 1);
}

static void adv_work_setting_sync(u8 work_setting_info)
{
#if TCFG_USER_TWS_ENABLE
    if (get_bt_tws_connect_status()) {
        update_adv_setting(ATTR_TYPE_WORK_MODE);
    }
#endif
}

static void update_work_setting_state(void)
{
    if (1 == _s_info.work_mode) {
        bt_set_low_latency_mode(0);
    } else if (2 == _s_info.work_mode) {
        bt_set_low_latency_mode(1);
    }
}

void deal_work_setting(u8 work_setting_info, u8 write_vm, u8 tws_sync)
{
    if (!work_setting_info) {
        work_setting_info = get_work_setting();
    }
    if (write_vm) {
        adv_work_setting_vm_value(work_setting_info);
    }
    if (tws_sync) {
        adv_work_setting_sync(work_setting_info);
    }
    update_work_setting_state();
}

#endif
