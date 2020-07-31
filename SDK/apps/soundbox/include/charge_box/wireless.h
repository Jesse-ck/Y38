#ifndef _WIRELESS_H_
#define _WIRELESS_H_

#include "device/wireless_charge.h"

extern _wireless_hdl wl_hdl;
//api函数声明
u16 get_wireless_voltage(void);
u16 get_wireless_power(void);
void wireless_api_close(void);
void wireless_api_open(void);
void wireless_init_api(void);
void wireless_100ms_run_app(void);

//handshake
void chgbox_handshake_run_app(void);
void chgbox_handshake_init(void);
void chgbox_handshake_set_repeat(u8 times);
void chgbox_handshake_repeatedly(void);
#endif
