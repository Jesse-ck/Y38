#include "app_config.h"
#if (RCSP_ADV_EN)
#include "syscfg_id.h"
#include "le_rcsp_adv_module.h"

#include "adv_bt_name_setting.h"
#include "adv_setting_common.h"
#include "rcsp_adv_tws_sync.h"
#include "rcsp_adv_opt.h"

extern int get_bt_tws_connect_status();

void set_bt_name_setting(u8 *bt_name_setting)
{
    memcpy(_s_info.edr_name, bt_name_setting, 32);
}

void get_bt_name_setting(u8 *bt_name_setting)
{
    memcpy(bt_name_setting, _s_info.edr_name, 32);
}

// 1、写入VM
static void update_bt_name_vm_value(u8 *bt_name_setting)
{
    syscfg_write(CFG_BT_NAME, bt_name_setting, 32);
}
// 2、同步对端
static void bt_name_sync(u8 *bt_name_setting)
{
#if TCFG_USER_TWS_ENABLE
    if (get_bt_tws_connect_status()) {
        update_adv_setting(ATTR_TYPE_EDR_NAME);
    }
#endif
}

void deal_bt_name_setting(u8 *bt_name_setting, u8 write_vm, u8 tws_sync)
{
    u8 bt_name[32] = {0};
    if (!bt_name_setting) {
        get_bt_name_setting(bt_name);
    } else {
        memcpy(bt_name, bt_name_setting, 32);
    }
    if (write_vm) {
        update_bt_name_vm_value(bt_name);
    }
    if (tws_sync) {
        bt_name_sync(bt_name);
    }
}

#endif
