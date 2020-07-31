#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "string.h"
#include "JL_rcsp_api.h"
#include "JL_rcsp_protocol.h"
#include "JL_rcsp_packet.h"
#include "spp_user.h"
#include "btstack/avctp_user.h"
#include "system/timer.h"
#include "app_core.h"
#include "user_cfg.h"
#include "asm/pwm_led.h"
#include "ui_manage.h"
#include "key_event_deal.h"
#include "syscfg_id.h"
#include "classic/tws_api.h"
#include "event.h"
#include "bt_tws.h"

#define  TULING_AI_EN         0
#define  DEEPBRAN_AI_EN       1

#if (RCSP_ADV_EN)
#include "rcsp_adv_user_update.h"
#include "le_rcsp_adv_module.h"
#include "rcsp_adv_bluetooth.h"
#include "adv_setting_common.h"

#define JL_RCSP_LOW_VERSION  0x01
#define JL_RCSP_HIGH_VERSION 0x01

#define RCSP_DEBUG_EN
#ifdef RCSP_DEBUG_EN
#define rcsp_putchar(x)                putchar(x)
#define rcsp_printf                    printf
#define rcsp_printf_buf(x,len)         put_buf(x,len)
#else
#define rcsp_putchar(...)
#define rcsp_printf(...)
#define rcsp_printf_buf(...)
#endif

struct JL_AI_VAR jl_ai_var = {
    .rcsp_run_flag = 0,
};

#define __this (&jl_ai_var)

#define RCSP_USE_BLE      0
#define RCSP_USE_SPP      1
#define RCSP_CHANNEL_SEL  RCSP_USE_BLE

#pragma pack(1)

struct _SYS_info {
    u8 bat_lev;
    u8 sys_vol;
    u8 max_vol;
    u8 reserve;
};

struct _EDR_info {
    u8 addr_buf[6];
    u8 profile;
    u8 state;
};

struct _DEV_info {
    u8 status;
    u32 usb_handle;
    u32 sd0_handle;
    u32 sd1_handle;
    u32 flash_handle;
};

struct _EQ_INFO {
    u8 mode;
    s8 gain_val[10];
};

struct _MUSIC_STATUS_info {
    u8 status;
    u32 cur_time;
    u32 total_time;
    u8 cur_dev;
};

struct _dev_version {
    u16 _sw_ver2: 4; //software l version
    u16 _sw_ver1: 4; //software m version
    u16 _sw_ver0: 4; //software h version
    u16 _hw_ver:  4; //hardware version
};


#pragma pack()

/* #pragma pack(1) */
/* #pragma pack() */

#if RCSP_UPDATE_EN
static volatile u8 JL_bt_chl = 0;
u8 JL_get_cur_bt_channel_sel(void);
void JL_ble_disconnect(void);
#endif

void JL_rcsp_event_to_user(u32 type, u8 event, u8 *msg, u8 size)
{
    struct sys_event e;
    e.type = SYS_DEVICE_EVENT;

    if (size > sizeof(e.u.rcsp.args)) {
        rcsp_printf("rcsp event size overflow:%x %x\n", size, sizeof(e.u.rcsp.args));
    }

    e.arg  = (void *)type;
    e.u.rcsp.event = event;

    if (size) {
        memcpy(e.u.rcsp.args, msg, size);
    }

    e.u.rcsp.size = size;

    sys_event_notify(&e);
}

int JL_rcsp_event_handler(struct rcsp_event *rcsp)
{
    int ret = 0;

    switch (rcsp->event) {
    case MSG_JL_ADV_SETTING_SYNC:
        update_adv_setting((u8) - 1);
        break;
    case MSG_JL_ADV_SETTING_UPDATE:
        update_info_from_adv_vm_info();
        break;
    default:
#if RCSP_UPDATE_EN
        JL_rcsp_msg_deal(NULL, rcsp->event, rcsp->args);
#endif
        break;
    }

    return ret;
}

u8 adv_info_notify(u8 *buf, u16 len)
{
    return JL_CMD_send(JL_OPCODE_ADV_DEVICE_NOTIFY, buf, len, JL_NOT_NEED_RESPOND);
}

static u8 add_one_attr(u8 *buf, u16 max_len, u8 offset, u8 type, u8 *data, u8 size)
{
    if (offset + size + 2 > max_len) {
        rcsp_printf("\n\nadd attr err!\n\n");
        return 0;
    }

    buf[offset] = size + 1;
    buf[offset + 1] = type ;
    memcpy(&buf[offset + 2], data, size);
    return size + 2;
}

static u8 add_one_attr_ex(u8 *buf, u16 max_len, u8 offset, u8 type, u8 *data, u8 size, u8 att_size)
{
    if (offset + size + 2 > max_len) {
        rcsp_printf("\n\nadd attr err!\n\n");
        return 0;
    }

    buf[offset] = att_size + 1;
    buf[offset + 1] = type ;
    memcpy(&buf[offset + 2], data, size);
    return size + 2;
}

static u8 add_one_attr_continue(u8 *buf, u16 max_len, u8 offset, u8 type, u8 *data, u8 size)
{
    if ((offset + size) > max_len) {
        rcsp_printf("\n\nadd attr err 2 !\n\n");
        return 0;
    }

    memcpy(&buf[offset], data, size);
    return size;
}

struct t_s_info _s_info = {
    .key_setting = {
        0x01, 0x01, 0x05, \
        0x02, 0x01, 0x05, \
        0x01, 0x02, 0x08, \
        0x02, 0x02, 0x08
    },
    .led_status = {
        0x01, 0x06, \
        0x02, 0x05, \
        0x03, 0x04
    },
    .mic_mode = 1,
    .work_mode = 1,
};

extern const char *bt_get_local_name();
extern void bt_adv_get_bat(u8 *buf);
static u32 JL_opcode_get_adv_info(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len)
{
    u8 buf[256];
    u8 offset = 0;

    u32 ret = 0;
    u32 mask = READ_BIG_U32(data);
    rcsp_printf("FEATURE MASK : %x\n", mask);
    /* #define ATTR_TYPE_BAT_VALUE  	(0) */
    /* #define ATTR_TYPE_EDR_NAME   	(1) */
    //get version
    if (mask & BIT(ATTR_TYPE_BAT_VALUE)) {
        rcsp_printf("ATTR_TYPE_BAT_VALUE\n");
        u8 bat[3];
        bt_adv_get_bat(bat);
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_BAT_VALUE, bat, 3);
    }
    if (mask & BIT(ATTR_TYPE_EDR_NAME)) {
        rcsp_printf("ATTR_TYPE_EDR_NAME\n");
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_EDR_NAME, (void *)_s_info.edr_name, strlen(_s_info.edr_name));
    }

    if (mask & BIT(ATTR_TYPE_KEY_SETTING)) {
        rcsp_printf("ATTR_TYPE_KEY_SETTING\n");
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_KEY_SETTING, (void *)_s_info.key_setting, sizeof(_s_info.key_setting));
    }

    if (mask & BIT(ATTR_TYPE_LED_SETTING)) {
        rcsp_printf("ATTR_TYPE_LED_SETTING\n");
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_LED_SETTING, (void *)_s_info.led_status, sizeof(_s_info.led_status));
    }


    if (mask & BIT(ATTR_TYPE_MIC_SETTING)) {
        rcsp_printf("ATTR_TYPE_MIC_SETTING\n");
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_MIC_SETTING, (void *)&_s_info.mic_mode, 1);
    }

    if (mask & BIT(ATTR_TYPE_WORK_MODE)) {
        rcsp_printf("ATTR_TYPE_WORK_MODE\n");
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_WORK_MODE, (void *)&_s_info.work_mode, 1);
    }

    if (mask & BIT(ATTR_TYPE_PRODUCT_MESSAGE)) {
        rcsp_printf("ATTR_TYPE_PRODUCT_MESSAGE\n");
        u8 tversion[6];
        tversion[0] = 0x05;
        tversion[1] = 0xD6;
        tversion[2] = 0x00;
        tversion[3] = 0x02;
        tversion[4] = 0x00;
        tversion[5] = 0x0F;
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_PRODUCT_MESSAGE, (void *)tversion, 6);

    }
    rcsp_printf_buf(buf, offset);

    ret = JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, buf, offset);

    return ret;
}

// ----------------------------reboot----------------------//
/* extern void tws_sync_modify_bt_name_reset(void); */
/* void modify_bt_name_and_reset(u32 msec) */
/* { */
/* 	sys_timer_add(NULL, cpu_reset, msec); */
/* } */
// ----------------------------end----------------------//

static u8 adv_setting_result = 0;
static u8 adv_set_deal_one_attr(u8 *buf, u8 size, u8 offset)
{
    u8 rlen = buf[offset];
    if ((offset + rlen + 1) > (size - offset)) {
        rcsp_printf("\n\ndeal attr end!\n\n");
        return rlen;
    }
    u8 type = buf[offset + 1];
    u8 *pbuf = &buf[offset + 2];
    u8 dlen = rlen - 1;
    u8 bt_name[32];
    //adv_setting_result = 0;

    switch (type) {
    case ATTR_TYPE_EDR_NAME:
        memcpy(bt_name, pbuf, dlen);
        bt_name[dlen] = '\0';
        memcpy(_s_info.edr_name, bt_name, 32);
        rcsp_printf("ATTR_TYPE_EDR_NAME %s\n", bt_name);
        rcsp_printf_buf(pbuf, dlen);
        deal_bt_name_setting(NULL, 1, 1);
        break;
    case ATTR_TYPE_KEY_SETTING:
        rcsp_printf("ATTR_TYPE_KEY_SETTING\n");
        rcsp_printf_buf(pbuf, dlen);
        while (dlen >= 3) {
            if (pbuf[0] == 0x01) {
                if (pbuf[1] == 0x01) {
                    _s_info.key_setting[2] = pbuf[2];
                } else if (pbuf[1] == 0x02) {
                    _s_info.key_setting[8] = pbuf[2];
                }
            } else if (pbuf[0] == 0x02) {
                if (pbuf[1] == 0x01) {
                    _s_info.key_setting[5] = pbuf[2];
                } else if (pbuf[1] == 0x02) {
                    _s_info.key_setting[11] = pbuf[2];
                }
            }
            dlen -= 3;
            pbuf += 3;
        }
        deal_key_setting(NULL, 1, 1);
        break;
    case ATTR_TYPE_LED_SETTING:
        rcsp_printf("ATTR_TYPE_LED_SETTING\n");
        rcsp_printf_buf(pbuf, dlen);
        while (dlen >= 2) {
            if (pbuf[0] == 0 || pbuf[0] > 3) {
                break;
            } else {
                _s_info.led_status[2 * (pbuf[0] - 1) + 1] = pbuf[1];
            }
            dlen -= 2;
            pbuf += 2;
        }
        deal_led_setting(NULL, 1, 1);
        break;
    case ATTR_TYPE_MIC_SETTING:
        rcsp_printf("ATTR_TYPE_MIC_SETTING\n");
        rcsp_printf_buf(pbuf, dlen);
        if (2 == _s_info.work_mode) {
            adv_setting_result = -1;
        } else {
            adv_setting_result = 0;
        }
        deal_mic_setting(pbuf[0], 1, 1);
        break;
    case ATTR_TYPE_WORK_MODE:
        rcsp_printf("ATTR_TYPE_WORK_MODE\n");
        rcsp_printf_buf(pbuf, dlen);
        deal_work_setting(pbuf[0], 1, 1);
        break;
    case ATTR_TYPE_TIME_STAMP:
        rcsp_printf("ATTR_TYPE_TIME_STAMP\n");
        rcsp_printf_buf(pbuf, dlen);
        //adv_time_stamp = (((pbuf[0] << 8) | pbuf[1]) << 8 | pbuf[2]) << 8 | pbuf[3];
        syscfg_read(CFG_BT_NAME, _s_info.edr_name, sizeof(_s_info.edr_name));
        //deal_time_stamp_setting();
        //set_adv_time_stamp((((pbuf[0] << 8) | pbuf[1]) << 8 | pbuf[2]) << 8 | pbuf[3]);
        deal_time_stamp_setting((((pbuf[0] << 8) | pbuf[1]) << 8 | pbuf[2]) << 8 | pbuf[3], 1, 1);
        break;
    default:
        rcsp_printf("ATTR UNKNOW\n");
        rcsp_printf_buf(pbuf, dlen);
        break;
    }

    return rlen + 1;
}

static u32 JL_opcode_set_adv_info(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len)
{
    rcsp_printf("JL_opcode_set_adv_info:\n");
    rcsp_printf_buf(data, len);
    u8 offset = 0;
    while (offset < len) {
        offset += adv_set_deal_one_attr(data, len, offset);
    }
    u8 ret = 0;
    if (adv_setting_result) {
        JL_CMD_response_send(OpCode, JL_PRO_STATUS_FAIL, OpCode_SN, &ret, 1);
    } else {
        JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, &ret, 1);
    }
    return 0;
}

extern const int support_dual_bank_update_en;
static u32 JL_opcode_get_target_info(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len)
{
    u8 buf[256];
    u8 offset = 0;

    __this->phone_platform = data[4];
    if (__this->phone_platform == ANDROID) {
        rcsp_printf("phone_platform == ANDROID\n");
    } else if (__this->phone_platform == APPLE_IOS) {
        rcsp_printf("phone_platform == APPLE_IOS\n");
    } else {
        rcsp_printf("phone_platform ERR\n");
    }

    u32 mask = READ_BIG_U32(data);
    rcsp_printf("FEATURE MASK : %x\n", mask);

    u32 ret = 0;

    //get version
    if (mask & BIT(ATTR_TYPE_PROTOCOL_VERSION)) {
        rcsp_printf("ATTR_TYPE_PROTOCOL_VERSION\n");
        u8 ver = get_rcsp_version();
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_PROTOCOL_VERSION, &ver, 1);
    }

    //get powerup sys info
    if (mask & BIT(ATTR_TYPE_SYS_INFO)) {
        rcsp_printf("ATTR_TYPE_SYS_INFO\n");

        struct _SYS_info sys_info;
        //extern u16 get_battery_level(void);
        sys_info.bat_lev = 0; //get_battery_level() / 10;
        sys_info.sys_vol = 0; //sound.vol.sys_vol_l;
        sys_info.max_vol = 0; //MAX_SYS_VOL_L;

        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_SYS_INFO, \
                               (u8 *)&sys_info, sizeof(sys_info));
    }

    //get EDR info
    if (mask & BIT(ATTR_TYPE_EDR_ADDR)) {
        rcsp_printf("ATTR_TYPE_EDR_ADDR\n");
        struct _EDR_info edr_info;
        /* extern void hook_get_mac_addr(u8 * btaddr); */
        /* hook_get_mac_addr(edr_info.addr_buf); */

        extern const u8 *bt_get_mac_addr();
        u8 taddr_buf[6];
        memcpy(taddr_buf, bt_get_mac_addr(), 6);
        edr_info.addr_buf[0] =  taddr_buf[5];
        edr_info.addr_buf[1] =  taddr_buf[4];
        edr_info.addr_buf[2] =  taddr_buf[3];
        edr_info.addr_buf[3] =  taddr_buf[2];
        edr_info.addr_buf[4] =  taddr_buf[1];
        edr_info.addr_buf[5] =  taddr_buf[0];
        /* extern u8 get_edr_suppor_profile(void); */
        /* edr_info.profile = get_edr_suppor_profile(); */
        edr_info.profile = 0x0E;
#if (RCSP_CHANNEL_SEL == RCSP_USE_BLE)
        edr_info.profile &= ~BIT(7);
#else
        edr_info.profile |= BIT(7);
#endif
        extern u8 get_bt_connect_status(void);
        if (get_bt_connect_status() ==  BT_STATUS_WAITINT_CONN) {
            edr_info.state = 0;
        } else {
            edr_info.state = 1;
        }
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_EDR_ADDR, (u8 *)&edr_info, sizeof(struct _EDR_info));
    }

    //get platform info
    if (mask & BIT(ATTR_TYPE_PLATFORM)) {
        rcsp_printf("ATTR_TYPE_PLATFORM\n");

        u8 lic_val = 0;
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_PLATFORM, &lic_val, 1);
    }

    //get function info
    if (mask & BIT(ATTR_TYPE_FUNCTION_INFO)) {
        rcsp_printf("ATTR_TYPE_FUNCTION_INFO\n");

        u8 function_info = 0x16;

        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_FUNCTION_INFO, &function_info, 1);
    }

    //get dev info
    if (mask & BIT(ATTR_TYPE_DEV_VERSION)) {
        rcsp_printf("ATTR_TYPE_DEV_VERSION\n");

        u8 tmp_ver[2];
        tmp_ver[0] = JL_RCSP_LOW_VERSION;
        tmp_ver[1] = JL_RCSP_HIGH_VERSION;
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_DEV_VERSION, (u8 *)tmp_ver, 2);
    }

    if (mask & BIT(ATTR_TYPE_UBOOT_VERSION)) {
        rcsp_printf("ATTR_TYPE_UBOOT_VERSION\n");
        u8 *uboot_ver_flag = (u8 *)(0x1C000 - 0x8);
        u8 uboot_version[2] = {uboot_ver_flag[0], uboot_ver_flag[1]};
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_UBOOT_VERSION, uboot_version, sizeof(uboot_version));
    }

    if (mask & BIT(ATTR_TYPE_DOUBLE_PARITION)) {
        rcsp_printf("ATTR_TYPE_DOUBLE_PARITION:%x\n", support_dual_bank_update_en);
        u8 double_partition_value;
        u8 ota_loader_need_download_flag;
        if (support_dual_bank_update_en) {
            double_partition_value = 0x1;
            ota_loader_need_download_flag = 0x00;
        } else {
            double_partition_value = 0x0;
            ota_loader_need_download_flag = 0x01;
        }
        u8 update_param[2] = {
            double_partition_value,
            ota_loader_need_download_flag,
        };
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_DOUBLE_PARITION, (u8 *)update_param, sizeof(update_param));
    }

    if (mask & BIT(ATTR_TYPE_UPDATE_STATUS)) {
        rcsp_printf("ATTR_TYPE_UPDATE_STATUS\n");
        u8 update_status_value = 0x0;
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_UPDATE_STATUS, (u8 *)&update_status_value, sizeof(update_status_value));
    }

    if (mask & BIT(ATTR_TYPE_DEV_VID_PID)) {
        rcsp_printf("ATTR_TYPE_DEV_VID_PID\n");

        u8 temp_dev_vid_pid = 0;
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_DEV_VID_PID, (u8 *)temp_dev_vid_pid, sizeof(temp_dev_vid_pid));
    }

    if (mask & BIT(ATTR_TYPE_SDK_TYPE)) {
        rcsp_printf("ATTR_TYPE_SDK_TYPE\n");
        u8 sdk_type = 2;
        offset += add_one_attr(buf, sizeof(buf), offset, ATTR_TYPE_SDK_TYPE, &sdk_type, 1);
    }

    // get AuthKey
    if (mask & ATTR_TYPE_DEV_AUTHKEY) {
        rcsp_printf("ATTR_TYPE_DEV_AUTHKEY\n");
    }

    if (mask & ATTR_TYPE_DEV_PROCODE) {
        rcsp_printf("ATTR_TYPE_DEV_PROCODE\n");
    }
    if (mask & ATTR_TYPE_DEV_MAX_MTU) {
        rcsp_printf(" ATTR_TYPE_DEV_MTU_SIZE\n");
        /* u16 JL_packet_get_rx_mtu(void); */
        /* u16 JL_packet_get_rx_max_mtu(void); */
        /* u16 JL_packet_get_tx_max_mtu(void); */
        u16 rx_max_mtu = JL_packet_get_rx_max_mtu();
        u16 tx_max_mtu = JL_packet_get_tx_max_mtu();
        u8 t_buf[4];
        t_buf[0] = (tx_max_mtu >> 8) & 0xFF;
        t_buf[1] = tx_max_mtu & 0xFF;
        t_buf[2] = (rx_max_mtu >> 8) & 0xFF;
        t_buf[3] = rx_max_mtu & 0xFF;
        offset += add_one_attr(buf, sizeof(buf), offset,  ATTR_TYPE_DEV_MAX_MTU, t_buf, 4);
    }

    rcsp_printf_buf(buf, offset);

    ret = JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, buf, offset);

    return ret;
}


//phone send cmd to firmware need respond
static void JL_rcsp_cmd_resp(void *priv, u8 OpCode, u8 OpCode_SN, u8 *data, u16 len)
{
    rcsp_printf("JL_ble_cmd_resp\n");
    switch (OpCode) {
    case JL_OPCODE_GET_TARGET_FEATURE:
        rcsp_printf("JL_OPCODE_GET_TARGET_INFO\n");
        JL_opcode_get_target_info(priv, OpCode, OpCode_SN, data, len);
        break;
    case JL_OPCODE_SWITCH_DEVICE:
        __this->device_type = data[0];
        rcsp_printf("device_type:%x\n", __this->device_type);
        JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, NULL, 0);
#if RCSP_UPDATE_EN
        if (get_jl_update_flag()) {
            if (RCSP_BLE == JL_get_cur_bt_channel_sel()) {
                rcsp_printf("BLE_ CON START DISCON\n");
                JL_rcsp_event_to_user(DEVICE_EVENT_FROM_RCSP, MSG_JL_DEV_DISCONNECT, NULL, 0);
            } else {
                rcsp_printf("WAIT_FOR_SPP_DISCON\n");
            }
        }
#endif
        break;

    case JL_OPCODE_SET_ADV:
        rcsp_printf(" JL_OPCODE_SET_ADV\n");
        JL_opcode_set_adv_info(priv, OpCode, OpCode_SN, data, len);
        break;
    case JL_OPCODE_GET_ADV:
        rcsp_printf(" JL_OPCODE_GET_ADV\n");
        JL_opcode_get_adv_info(priv, OpCode, OpCode_SN, data, len);
        break;
    case JL_OPCODE_ADV_NOTIFY_SETTING:
        rcsp_printf(" JL_OPCODE_ADV_NOTIFY_SETTING\n");
        bt_ble_adv_ioctl(BT_ADV_SET_NOTIFY_EN, *((u8 *)data), 1);
        JL_CMD_response_send(OpCode, JL_PRO_STATUS_SUCCESS, OpCode_SN, NULL, 0);
        break;
    case JL_OPCODE_ADV_DEVICE_REQUEST:
        rcsp_printf("JL_OPCODE_ADV_DEVICE_REQUEST\n");
        break;
    case JL_OPCODE_SET_DEVICE_REBOOT:
        rcsp_printf("JL_OPCODE_SET_DEVICE_REBOOT\n");
        extern void ble_module_enable(u8 en);
        ble_module_enable(0);
#if TCFG_USER_TWS_ENABLE
        if (get_bt_tws_connect_status()) {
            //tws_sync_modify_bt_name_reset();
            modify_bt_name_and_reset(500);
        } else {
            cpu_reset();
        }
#else
        cpu_reset();
#endif
        break;
    default:
#if RCSP_UPDATE_EN
        if ((OpCode >= JL_OPCODE_GET_DEVICE_UPDATE_FILE_INFO_OFFSET) && \
            (OpCode <= JL_OPCODE_SET_DEVICE_REBOOT)) {
            JL_rcsp_update_cmd_resp(priv, OpCode, OpCode_SN, data, len);
        } else
#endif
        {
            JL_CMD_response_send(OpCode, JL_ERR_NONE, OpCode_SN, data, len);
        }
        break;
    }

//  JL_ERR JL_CMD_response_send(u8 OpCode, u8 status, u8 sn, u8 *data, u16 len)
}

//phone send cmd to firmware not need respond
static void JL_rcsp_cmd_no_resp(void *priv, u8 OpCode, u8 *data, u16 len)
{
    rcsp_printf("JL_ble_cmd_no_resp\n");
}

//phone send data to firmware need respond
static void JL_rcsp_data_resp(void *priv, u8 OpCode_SN, u8 CMD_OpCode, u8 *data, u16 len)
{
    rcsp_printf("JL_ble_data_resp\n");
    switch (CMD_OpCode) {
    case 0:
        break;

    default:
        break;
    }

}
//phone send data to firmware not need respond
static void JL_rcsp_data_no_resp(void *priv, u8 CMD_OpCode, u8 *data, u16 len)
{
    rcsp_printf("JL_ble_data_no_resp\n");
    switch (CMD_OpCode) {
    case 0:
        break;

    default:
        break;
    }

}

//phone respone firmware cmd
static void JL_rcsp_cmd_recieve_resp(void *priv, u8 OpCode, u8 status, u8 *data, u16 len)
{
    rcsp_printf("rec resp:%x\n", OpCode);

    switch (OpCode) {
    default:
#if RCSP_UPDATE_EN
        if ((OpCode >= JL_OPCODE_GET_DEVICE_UPDATE_FILE_INFO_OFFSET) && \
            (OpCode <= JL_OPCODE_SET_DEVICE_REBOOT)) {
            JL_rcsp_update_cmd_receive_resp(priv, OpCode, status, data, len);
        }
#endif
        break;
    }
}
//phone respone firmware data
static void JL_rcsp_data_recieve_resp(void *priv, u8 status, u8 CMD_OpCode, u8 *data, u16 len)
{
    rcsp_printf("JL_ble_data_recieve_resp\n");
    switch (CMD_OpCode) {

    case 0:
        break;

    default:
        break;
    }

}
//wait resp timout
static u8 JL_rcsp_wait_resp_timeout(void *priv, u8 OpCode, u8 counter)
{
    rcsp_printf("JL_rcsp_wait_resp_timeout\n");

    return 0;
}


///**************************************************************************************///
///************     rcsp ble                                                   **********///
///**************************************************************************************///
static void JL_ble_status_callback(void *priv, ble_state_e status)
{
    rcsp_printf("JL_ble_status_callback==================== %d\n", status);
    __this->JL_ble_status = status;
    switch (status) {
    case BLE_ST_IDLE:
#if RCSP_UPDATE_EN
        if (get_jl_update_flag()) {
            JL_rcsp_event_to_user(DEVICE_EVENT_FROM_RCSP, MSG_JL_UPDATE_START, NULL, 0);
        }
#endif
        break;
    case BLE_ST_ADV:
        break;
    case BLE_ST_CONNECT:
        break;
    case BLE_ST_SEND_DISCONN:
        break;
    case BLE_ST_NOTIFY_IDICATE:
        break;
    default:
        break;
    }
}

static bool JL_ble_fw_ready(void *priv)
{
    return ((__this->JL_ble_status == BLE_ST_NOTIFY_IDICATE) ? true : false);
}

static s32 JL_ble_send(void *priv, void *data, u16 len)
{
    if ((__this->rcsp_ble != NULL) && (__this->JL_ble_status == BLE_ST_NOTIFY_IDICATE)) {
        int err = __this->rcsp_ble->send_data(NULL, (u8 *)data, len);
        /* rcsp_printf("send :%d\n", len); */
        if (len < 128) {
            /* rcsp_printf_buf(data, len); */
        } else {
            /* rcsp_printf_buf(data, 128); */
        }

        if (err == 0) {
            return 0;
        } else if (err == APP_BLE_BUFF_FULL) {
            return 1;
        }
    } else {
        rcsp_printf("send err -1 !!\n");
    }

    return -1;
}

static const JL_PRO_CB JL_pro_BLE_callback = {
    .priv              = NULL,
    .fw_ready          = JL_ble_fw_ready,
    .fw_send           = JL_ble_send,
    .CMD_resp          = JL_rcsp_cmd_resp,
    .CMD_no_resp       = JL_rcsp_cmd_no_resp,
    .DATA_resp         = JL_rcsp_data_resp,
    .DATA_no_resp      = JL_rcsp_data_no_resp,
    .CMD_recieve_resp  = JL_rcsp_cmd_recieve_resp,
    .DATA_recieve_resp = JL_rcsp_data_recieve_resp,
    .wait_resp_timeout = JL_rcsp_wait_resp_timeout,
};

static void rcsp_ble_callback_set(void (*resume)(void), void (*recieve)(void *, void *, u16), void (*status)(void *, ble_state_e))
{
    __this->rcsp_ble->regist_wakeup_send(NULL, resume);
    __this->rcsp_ble->regist_recieve_cbk(NULL, recieve);
    __this->rcsp_ble->regist_state_cbk(NULL, status);
}
#if RCSP_UPDATE_EN
u8 JL_get_cur_bt_channel_sel(void)
{
    return JL_bt_chl;
}

void JL_ble_disconnect(void)
{
    __this->rcsp_ble->disconnect(NULL);
}

u8 get_curr_device_type(void)
{
    return __this->device_type;
}

void set_curr_update_type(u8 type)
{
    __this->device_type = type;
}
#endif

///**************************************************************************************///
///************     rcsp spp                                                   **********///
///**************************************************************************************///
static void JL_spp_status_callback(u8 status)
{
    switch (status) {
    case 0:
        __this->JL_spp_status = 0;
        break;
    case 1:
        __this->JL_spp_status = 1;
        break;
    default:
        __this->JL_spp_status = 0;
        break;
    }
}

static bool JL_spp_fw_ready(void *priv)
{
    return (__this->JL_spp_status ? true : false);
}

static s32 JL_spp_send(void *priv, void *data, u16 len)
{
    if (len < 128) {
        rcsp_printf("send: \n");
        rcsp_printf_buf(data, (u32)len);
    }
    if ((__this->rcsp_spp != NULL) && (__this->JL_spp_status == 1)) {
        u32 err = __this->rcsp_spp->send_data(NULL, (u8 *)data, len);
        if (err == 0) {
            return 0;
        } else if (err == SPP_USER_ERR_SEND_BUFF_BUSY) {
            return 1;
        }
    } else {
        rcsp_printf("send err -1 !!\n");
    }

    return -1;
}

static const JL_PRO_CB JL_pro_SPP_callback = {
    .priv              = NULL,
    .fw_ready          = JL_spp_fw_ready,
    .fw_send           = JL_spp_send,
    .CMD_resp          = JL_rcsp_cmd_resp,
    .DATA_resp         = JL_rcsp_data_resp,
    .CMD_no_resp       = JL_rcsp_cmd_no_resp,
    .DATA_no_resp      = JL_rcsp_data_no_resp,
    .CMD_recieve_resp  = JL_rcsp_cmd_recieve_resp,
    .DATA_recieve_resp = JL_rcsp_data_recieve_resp,
    .wait_resp_timeout = JL_rcsp_wait_resp_timeout,
};

static void rcsp_spp_callback_set(void (*resume)(void), void (*recieve)(void *, void *, u16), void (*status)(u8))
{
    __this->rcsp_spp->regist_wakeup_send(NULL, resume);
    __this->rcsp_spp->regist_recieve_cbk(NULL, recieve);
    __this->rcsp_spp->regist_state_cbk(NULL, status);
}

static int rcsp_spp_data_send(void *priv, u8 *buf, u16 len)
{
    int err = 0;
    if (__this->rcsp_spp != NULL) {
        err = __this->rcsp_spp->send_data(NULL, (u8 *)buf, len);
    }

    return err;

}

static int rcsp_ble_data_send(void *priv, u8 *buf, u16 len)
{
    rcsp_printf("### rcsp_ble_data_send %d\n", len);
    int err = 0;
    if (__this->rcsp_ble != NULL) {
        err = __this->rcsp_ble->send_data(NULL, (u8 *)buf, len);
    }

    return err;

}

extern const u8 link_key_data[16];
void rcsp_dev_select(u8 type)
{
#if RCSP_UPDATE_EN
    set_jl_update_flag(0);
#endif
    if (type == RCSP_BLE) {
#if RCSP_UPDATE_EN
        JL_bt_chl = RCSP_BLE;
#endif
        rcsp_printf("------RCSP_BLE-----\n");
        rcsp_spp_callback_set(NULL, NULL, NULL);
        rcsp_ble_callback_set(JL_protocol_resume, JL_protocol_data_recieve, JL_ble_status_callback);
        JL_protocol_dev_switch(&JL_pro_BLE_callback);
        JL_rcsp_auth_init(rcsp_ble_data_send, (u8 *)link_key_data, NULL);
    } else {
#if RCSP_UPDATE_EN
        JL_bt_chl = RCSP_SPP;
#endif
        rcsp_printf("------RCSP_SPP-----\n");
        rcsp_spp_callback_set(JL_protocol_resume, JL_protocol_data_recieve, JL_spp_status_callback);
        rcsp_ble_callback_set(NULL, NULL, NULL);
        JL_protocol_dev_switch(&JL_pro_SPP_callback);
        JL_rcsp_auth_init(rcsp_spp_data_send, (u8 *)link_key_data, NULL);
    }
}


////////////////////// RCSP process ///////////////////////////////

static OS_SEM rcsp_sem;

void JL_rcsp_resume_do(void)
{
    os_sem_post(&rcsp_sem);
}

static u32 rcsp_protocol_tick = 0;
static void rcsp_process_timer()
{
    JL_set_cur_tick(rcsp_protocol_tick++);
    os_sem_post(&rcsp_sem);
}

static void rcsp_process_task(void *p)
{
    while (1) {
        os_sem_pend(&rcsp_sem, 0);
        JL_protocol_process();
    }
}

/////////////////////////////////////////////////////

void rcsp_init()
{
    memcpy(_s_info.edr_name, bt_get_local_name(), 32);

    if (__this->rcsp_run_flag) {
        return;
    }

    memset((u8 *)__this, 0, sizeof(struct JL_AI_VAR));

    /* __this->start_speech = start_speech; */
    /* __this->stop_speech = stop_speech; */
    //__this->rcsp_user = (struct __rcsp_user_var *)get_user_rcsp_opt();

    ble_get_server_operation_table(&__this->rcsp_ble);
    spp_get_operation_table(&__this->rcsp_spp);

    u32 size = rcsp_protocol_need_buf_size();
    rcsp_printf("rcsp need buf size:%x\n", size);
    u8 *ptr = zalloc(size);
    ASSERT(ptr, "no, memory for rcsp_init\n");
    JL_protocol_init(ptr, size);

    os_sem_create(&rcsp_sem, 0);
    sys_timer_add(NULL, rcsp_process_timer, 500);
    int err = task_create(rcsp_process_task, NULL, "rcsp_task");

    //default use ble , can switch spp anytime
    rcsp_dev_select(RCSP_BLE);

#if (defined MUTIl_CHARGING_BOX_EN) && (!MUTIl_CHARGING_BOX_EN)
    adv_setting_init();
#endif

    __this->rcsp_run_flag = 1;
}


void rcsp_exit(void)
{
#if ((AI_SOUNDBOX_EN==1) && (RCSP_ADV_EN==1))
    if (speech_status() == true) {
        rcsp_cancel_speech();
        speech_stop();
    }
    __set_a2dp_sound_detect_counter(50, 250); /*第一个参数是后台检测返回蓝牙的包数目，第二个参数是退出回到原来模式静音的包数目*/
    __this->wait_asr_end = 0;
#endif

    rcsp_printf("####  rcsp_exit_cb\n");
    task_kill("rcsp_task");
    return;
}


#endif
