#ifndef TM_GMA_HW_DRIVER_H
#define TM_GMA_HW_DRIVER_H
#include "typedef.h"
#include "tm_gma.h"

struct __gma_hw_api {
    void (*ENTER_CRITICAL)(void);
    void (*EXIT_CRITICAL)(void);
    void (*set_phone_sys_info)(_uint8 sys);
    _uint16(*get_fw_abilities)(void);
    _uint8(*get_enc_type)(void);
    _uint8 *(*get_bt_mac_addr)(void);
    void (*app_notify_state)(_uint8 state);
    void (*set_sys_vol)(_uint8 vol);
    _uint8(*a2dp_connected_state)(void);
    _uint8(*audio_state)(struct _gma_app_audio_ctl info);
    _uint8(*get_HFP_state)(struct _gma_app_HFP_ctl info);
    _uint8(*get_battery_value)(void);
    _uint8(*low_power_state)(void);
    _uint8(*battery_state)(void);
    _sint32(*set_fm_fre)(_uint8 *fre_str, _uint8 length);
    const _sint8 *(*get_fm_fre)(void);
    _uint8(*set_fm_fre_res)(void);
    const _sint8 *(*get_bt_name)(void);
    _uint8(*get_mic_state)(void);
    int (*start_speech)(void);
    int (*stop_speech)(void);
};

extern const struct __gma_hw_api gma_hw_api;

int gma_hw_stop_speech(void);
#endif
