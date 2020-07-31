#include "app_config.h"
#if (RCSP_ADV_EN)
#include "syscfg_id.h"
#include "le_rcsp_adv_module.h"
#include "rcsp_adv_bluetooth.h"

#include "rcsp_adv_opt.h"
#include "adv_setting_common.h"
#include "rcsp_adv_tws_sync.h"
#include "adv_time_stamp_setting.h"
#include "adv_bt_name_setting.h"
#include "adv_key_setting.h"
#include "adv_led_setting.h"
#include "adv_mic_setting.h"
#include "adv_work_setting.h"

static u8 adv_setting_event_flag = 0xff;
static void set_adv_setting_event_flag(u8 flag)
{
    adv_setting_event_flag = flag;
}

static u8 get_adv_setting_event_flag(void)
{
    return adv_setting_event_flag;
}

static u8 deal_adv_setting_string_item(u8 *des, u8 *src, u8 src_len, u8 type)
{
    des[0] = type;
    memcpy(des + 1, src, src_len);
    return src_len + sizeof(type);
}

void update_info_from_adv_vm_info(void)
{
    if (get_adv_setting_event_flag() & BIT(ATTR_TYPE_TIME_STAMP)) {
        deal_time_stamp_setting(0, 1, 0);
    }

    if (get_adv_setting_event_flag() & BIT(ATTR_TYPE_EDR_NAME)) {
        deal_bt_name_setting(NULL, 1, 0);
    }

    if (get_adv_setting_event_flag() & BIT(ATTR_TYPE_KEY_SETTING)) {
        deal_key_setting(NULL, 1, 0);
    }

    if (get_adv_setting_event_flag() & BIT(ATTR_TYPE_LED_SETTING)) {
        deal_led_setting(NULL, 1, 0);
    }

    if (get_adv_setting_event_flag() & BIT(ATTR_TYPE_MIC_SETTING)) {
        deal_mic_setting(0, 1, 0);
    }

    if (get_adv_setting_event_flag() & BIT(ATTR_TYPE_WORK_MODE)) {
        deal_work_setting(0, 1, 0);
    }

    set_adv_setting_event_flag(0);
}

static u8 adv_read_data_from_vm(u8 syscfg_id, u8 *buf, u8 buf_len)
{
    return (buf_len == syscfg_read(syscfg_id, buf, buf_len));
}

void adv_setting_init(void)
{
    u32 time_stamp = 0;
    if (adv_read_data_from_vm(CFG_RCSP_ADV_TIME_STAMP, (u8 *)&time_stamp, sizeof(time_stamp))) {
        set_adv_time_stamp(time_stamp);
        //deal_time_stamp_setting(0, 0, 0);
    }

    u8 bt_name_info[32] = {0};
    if (adv_read_data_from_vm(CFG_BT_NAME, bt_name_info, sizeof(bt_name_info))) {
        set_bt_name_setting(bt_name_info);
        //deal_bt_name_setting(NULL, 0, 0);
    }

    u8 key_setting_info[4] = {0};
    if (adv_read_data_from_vm(CFG_RCSP_ADV_KEY_SETTING, key_setting_info, sizeof(key_setting_info))) {
        set_key_setting(key_setting_info);
        deal_key_setting(NULL, 0, 0);
    }

    u8 led_setting_info[3] = {0};
    if (adv_read_data_from_vm(CFG_RCSP_ADV_LED_SETTING, led_setting_info, sizeof(led_setting_info))) {
        set_led_setting(led_setting_info);
        deal_led_setting(NULL, 0, 0);
    }

    u8 mic_setting_info = 0;
    if (adv_read_data_from_vm(CFG_RCSP_ADV_MIC_SETTING, &mic_setting_info, sizeof(mic_setting_info))) {
        set_mic_setting(mic_setting_info);
        deal_mic_setting(0, 0, 0);
    }

    u8 work_setting_info = 0;
    if (adv_read_data_from_vm(CFG_RCSP_ADV_WORK_SETTING, &work_setting_info, sizeof(work_setting_info))) {
        set_work_setting(work_setting_info);
        deal_work_setting(0, 0, 0);
    }
}

// type : 0 ~ 8
// mode : 0 - 从vm读出并更新全局变量数据 // 1 - 同步
void update_adv_setting(u8 type)
{
    u8 offset = 1;
    u8 adv_setting_to_sync[32 + 4 + 3 + 1 + 1 + 1] = {0};

    if (type & BIT(ATTR_TYPE_TIME_STAMP)) {
        u32 time_stamp = get_adv_time_stamp();
        offset += deal_adv_setting_string_item(adv_setting_to_sync + offset, (u8 *)&time_stamp, sizeof(time_stamp), ATTR_TYPE_TIME_STAMP);
    }

    if (type & BIT(ATTR_TYPE_EDR_NAME)) {
        u8 bt_name_info[32] = {0};
        get_bt_name_setting(bt_name_info);
        offset += deal_adv_setting_string_item(adv_setting_to_sync + offset, bt_name_info, sizeof(bt_name_info), ATTR_TYPE_EDR_NAME);
    }

    if (type & BIT(ATTR_TYPE_KEY_SETTING)) {
        u8 key_setting_info[4] = {0};
        get_key_setting(key_setting_info);
        offset += deal_adv_setting_string_item(adv_setting_to_sync + offset, key_setting_info, sizeof(key_setting_info), ATTR_TYPE_KEY_SETTING);
    }

    if (type & BIT(ATTR_TYPE_LED_SETTING)) {
        u8 led_setting_info[3] = {0};
        get_led_setting(led_setting_info);
        offset += deal_adv_setting_string_item(adv_setting_to_sync + offset, led_setting_info, sizeof(led_setting_info), ATTR_TYPE_LED_SETTING);
    }

    if (type & BIT(ATTR_TYPE_MIC_SETTING)) {
        u8 mic_setting_info = get_mic_setting();
        offset += deal_adv_setting_string_item(adv_setting_to_sync + offset, &mic_setting_info, sizeof(mic_setting_info), ATTR_TYPE_MIC_SETTING);
    }

    if (type & BIT(ATTR_TYPE_WORK_MODE)) {
        u8 work_setting_info = get_work_setting();
        offset += deal_adv_setting_string_item(adv_setting_to_sync + offset, &work_setting_info, sizeof(work_setting_info), ATTR_TYPE_WORK_MODE);
    }

    if (offset > 1) {
        adv_setting_to_sync[0] = offset;
        tws_api_send_data_to_sibling(adv_setting_to_sync, sizeof(adv_setting_to_sync), TWS_FUNC_ID_ADV_SETTING_SYNC);
    }
}

void deal_sibling_setting(u8 *buf)
{
    u8 type;
    u8 len = buf[0];
    u8 offset = 1;
    u8 *data;
    while (offset < len) {
        type = buf[offset++];
        data = buf + offset;
        switch (type) {
        case ATTR_TYPE_EDR_NAME:
            set_bt_name_setting(data);
            offset += 32;
        case ATTR_TYPE_KEY_SETTING :
            set_key_setting(data);
            offset += 4;
            break;
        case ATTR_TYPE_LED_SETTING :
            set_led_setting(data);
            offset += 3;
            break;
        case ATTR_TYPE_MIC_SETTING :
            set_mic_setting(*data);
            offset += 1;
            break;
        case ATTR_TYPE_WORK_MODE :
            set_work_setting(*data);
            offset += 1;
            break;
        case ATTR_TYPE_TIME_STAMP:
            set_adv_time_stamp(*(u32 *)data);
        default:
            return;
        }
        set_adv_setting_event_flag(get_adv_setting_event_flag() | BIT(type));
    }
    // 发送事件
    JL_rcsp_event_to_user(DEVICE_EVENT_FROM_RCSP, MSG_JL_ADV_SETTING_UPDATE, NULL, 0);
}

#endif
