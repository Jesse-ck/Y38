#include "charge_box/chargeIc_manage.h"
#include "device/device.h"
#include "app_config.h"
#include "app_main.h"
#include "user_cfg.h"


#if(defined TCFG_CHARGE_BOX_ENABLE) && ( TCFG_CHARGE_BOX_ENABLE)

#define LOG_TAG_CONST       APP_CHGBOX
#define LOG_TAG             "[CHG_IC]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


static const struct chargeIc_platform_data *platform_data;
CHARGE_IC_INTERFACE *chargeIc_hdl = NULL;
CHARGE_IC_INFO  __chargeIc_info = {.iic_delay = 30};
#define chargeIc_info (&__chargeIc_info)

#if TCFG_CHARGE_IC_USER_IIC_TYPE
#define iic_init(iic)                       hw_iic_init(iic)
#define iic_uninit(iic)                     hw_iic_uninit(iic)
#define iic_start(iic)                      hw_iic_start(iic)
#define iic_stop(iic)                       hw_iic_stop(iic)
#define iic_tx_byte(iic, byte)              hw_iic_tx_byte(iic, byte)
#define iic_rx_byte(iic, ack)               hw_iic_rx_byte(iic, ack)
#define iic_read_buf(iic, buf, len)         hw_iic_read_buf(iic, buf, len)
#define iic_write_buf(iic, buf, len)        hw_iic_write_buf(iic, buf, len)
#define iic_suspend(iic)                    hw_iic_suspend(iic)
#define iic_resume(iic)                     hw_iic_resume(iic)
#else
#define iic_init(iic)                       soft_iic_init(iic)
#define iic_uninit(iic)                     soft_iic_uninit(iic)
#define iic_start(iic)                      soft_iic_start(iic)
#define iic_stop(iic)                       soft_iic_stop(iic)
#define iic_tx_byte(iic, byte)              soft_iic_tx_byte(iic, byte)
#define iic_rx_byte(iic, ack)               soft_iic_rx_byte(iic, ack)
#define iic_read_buf(iic, buf, len)         soft_iic_read_buf(iic, buf, len)
#define iic_write_buf(iic, buf, len)        soft_iic_write_buf(iic, buf, len)
#define iic_suspend(iic)                    soft_iic_suspend(iic)
#define iic_resume(iic)                     soft_iic_resume(iic)
#endif

static void chargeIc_detect(void *priv)
{
    chargeIc_hdl->detect();
}

u8 chargeIc_command(u8 w_chip_id, u8 register_address, u8 function_command)
{
    u8 ret = 1;
    iic_start(chargeIc_info->iic_hdl);
    if (0 == iic_tx_byte(chargeIc_info->iic_hdl, w_chip_id)) {
        ret = 0;
        log_e("\n chargeIc iic wr err 0\n");
        goto __gcend;
    }

    delay(chargeIc_info->iic_delay);

    if (0 == iic_tx_byte(chargeIc_info->iic_hdl, register_address)) {
        ret = 0;
        log_e("\n chargeIc iic wr err 1\n");
        goto __gcend;
    }

    delay(chargeIc_info->iic_delay);

    if (0 == iic_tx_byte(chargeIc_info->iic_hdl, function_command)) {
        ret = 0;
        log_e("\n chargeIc iic wr err 2\n");
        goto __gcend;
    }

__gcend:
    iic_stop(chargeIc_info->iic_hdl);

    return ret;
}

u8 chargeIc_get_ndata(u8 r_chip_id, u8 register_address, u8 *buf, u8 data_len)
{
    u8 read_len = 0;

    iic_start(chargeIc_info->iic_hdl);
    if (0 == iic_tx_byte(chargeIc_info->iic_hdl, r_chip_id - 1)) {
        log_e("\n chargeIc iic rd err 0\n");
        read_len = 0;
        goto __gdend;
    }


    delay(chargeIc_info->iic_delay);
    if (0 == iic_tx_byte(chargeIc_info->iic_hdl, register_address)) {
        log_e("\n chargeIc iic rd err 1\n");
        read_len = 0;
        goto __gdend;
    }

    iic_start(chargeIc_info->iic_hdl);
    if (0 == iic_tx_byte(chargeIc_info->iic_hdl, r_chip_id)) {
        log_e("\n chargeIc iic rd err 2\n");
        read_len = 0;
        goto __gdend;
    }

    delay(chargeIc_info->iic_delay);

    for (; data_len > 1; data_len--) {
        *buf++ = iic_rx_byte(chargeIc_info->iic_hdl, 1);
        read_len ++;
    }

    *buf = iic_rx_byte(chargeIc_info->iic_hdl, 0);
    read_len ++;

__gdend:

    iic_stop(chargeIc_info->iic_hdl);
    delay(chargeIc_info->iic_delay);

    return read_len;
}

void chargeIc_boost_ctrl(u8 en)
{
    if (chargeIc_hdl) {
        chargeIc_hdl->boost_ctrl(en);
    }
}

void chargeIc_vol_ctrl(u8 en)
{
    if (chargeIc_hdl) {
        chargeIc_hdl->vol_ctrl(en);
    }
}

void chargeIc_vor_ctrl(u8 en)
{
    if (chargeIc_hdl) {
        chargeIc_hdl->vor_ctrl(en);
    }
}

u16 chargeIc_get_vbat(void)
{
    if (chargeIc_hdl) {
        return chargeIc_hdl->get_vbat();
    }
    return 0;
}

int chargeIc_init(const void *_data)
{
    int retval = 0;
    platform_data = (const struct chargeIc_platform_data *)_data;
    chargeIc_info->iic_hdl = platform_data->iic;

    if (chargeIc_info->iic_hdl) {
        retval = iic_init(chargeIc_info->iic_hdl);
        if (retval < 0) {
            log_e("\n  open iic for chargeIc err\n");
            return retval;
        } else {
            log_info("\n iic open succ\n");
        }
    }

    retval = -EINVAL;
    list_for_each_chargeIc(chargeIc_hdl) {
        if (!memcmp(chargeIc_hdl->logo, platform_data->chargeIc_name, strlen(platform_data->chargeIc_name))) {
            retval = 0;
            break;
        }
    }

    if (retval < 0) {
        log_e(">>>chargeIc_hdl logo err\n");
        return retval;
    }

    if (chargeIc_hdl->init()) {
        log_e(">>>>chargeIc_Int ERROR\n");
    } else {
        log_e(">>>>chargeIc_Int SUCC\n");
        chargeIc_info->init_flag  = 1;
        sys_timer_add(NULL, chargeIc_detect, 10); //10ms,iic通信的可以设置为50ms，注意ic->det 里各种计数时间
    }
    return 0;
}

#endif
