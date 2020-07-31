#include "dev_manager.h"
#include <stdlib.h>
#include "system/app_core.h"
#include "system/includes.h"
#include "app_config.h"

#if (defined(TCFG_SLIDE_KEY_ENABLE) && (TCFG_SLIDE_KEY_ENABLE == ENABLE))
#include "slidekey.h"
#endif

#define DEV_MANAGER_TASK_NAME	"dev_mg"

extern void sd_mult_io_detect(void *arg);
static void dev_manager_task(void *p)
{
    int res = 0;
    int msg[8] = {0};
    devices_init();

#if (defined(TCFG_IO_MULTIPLEX_WITH_SD) && (TCFG_IO_MULTIPLEX_WITH_SD == ENABLE))
    sys_timer_add(0, sd_mult_io_detect, 20);
#endif

#if (defined(TCFG_SLIDE_KEY_ENABLE) && (TCFG_SLIDE_KEY_ENABLE == ENABLE))
    sys_timer_add(0, slidekey_scan, 50);
#endif

#if TCFG_NORFLASH_DEV_ENABLE
    extern int file_opr_dev_add(void *logo);
    int ret = file_opr_dev_add((void *)"norflash");//this is demo
    ret = file_opr_dev_add((void *)"nor_tone");//this is demo
    if (ret) {
        r_printf(">>>[test]:error add nor_tone!!!!!!!!!!!!\n");
    } else {
        extern int set_tone_startaddr(int offset);
        set_tone_startaddr(0);//用于设置外挂flash放提示音的偏移地址。
    }

#if TCFG_NOR_FS_ENABLE
    extern u32 nor_fs_init(void);
    extern int nor_fs_set_rec_capacity(int capacity);// musct before nor_fs_init();
    nor_fs_set_rec_capacity(4 * 1024 * 1024);
    nor_fs_init();

    res = file_opr_dev_add((void *)"nor_fs");
    if (res) {
        r_printf(">>>[test]:error!!!!!!!!!!!!!\n");
    }
#endif
#endif

    while (1) {
        res = os_task_pend("taskq", msg, ARRAY_SIZE(msg));

        switch (res) {
        case OS_TASKQ:
            switch (msg[0]) {
            case Q_EVENT:
                break;
            case Q_MSG:
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }
}

void dev_manager_init(void)
{
    int err = task_create(dev_manager_task, NULL, DEV_MANAGER_TASK_NAME);
    if (err != OS_NO_ERR) {
        ASSERT(0, "task_create fail!!! %x\n", err);
    }
}


