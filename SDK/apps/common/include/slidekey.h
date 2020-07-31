#ifndef DEVICE_SLIDEKEY_H
#define DEVICE_SLIDEKEY_H

#include "typedef.h"
#include "asm/adc_api.h"

struct slidekey_port {
    u8 io;
    u32 ad_channel;
    int msg;
    u16 max_ad;
    u16 min_ad;
    u8 max_level;
};

struct slidekey_platform_data {
    u8 enable;
    u8 num;
    const struct slidekey_port *port;
};
extern void slidekey_scan(void *);
extern int slidekey_init(const struct slidekey_platform_data *slidekey_data);

#endif

