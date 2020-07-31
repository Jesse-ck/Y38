#include "system/includes.h"
#include "usb/device/msd.h"
#include "usb/scsi.h"
#include "usb_config.h"
#include "app_config.h"

#if TCFG_PC_ENABLE

#if TCFG_PC_UPDATE
#define WRITE_FLASH                     0xFB
#define READ_FLASH                      0xFD
#define OTHER_CMD                       0xFC
typedef enum {
    UPGRADE_NULL = 0,
    UPGRADE_USB_HARD_KEY,
    UPGRADE_USB_SOFT_KEY,
    UPGRADE_UART_KEY,
} UPGRADE_STATE;
void nvram_set_boot_state(u32 state);
static void go_mask_usb_updata()
{
    nvram_set_boot_state(UPGRADE_USB_SOFT_KEY);

    /* chip_reset(); */
    /* JL_CLOCK->PWR_CON |= BIT(4);//复位到uboot升级 */
    cpu_reset();
    while (1);
}

u32 private_scsi_cmd(struct usb_scsi_cbw *cbw)
{
    switch (cbw->operationCode) {
//////////////////////Boot Loader Custom CMD
    case WRITE_FLASH:
    case READ_FLASH:
    case OTHER_CMD:
        log_d("goto mask pc mode\n");
        go_mask_usb_updata();
        break;

    default:
        return FALSE;
    }

    return TRUE;
}
#else
u32 private_scsi_cmd(struct usb_scsi_cbw *cbw)
{
    return FALSE;
}
#endif //PC_UPDATE_ENABLE

#endif

