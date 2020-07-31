#include "app_config.h"
#include "system/includes.h"
#include "system/os/os_cpu.h"
#include "storage_dev/storage_dev.h"

#if (TCFG_UDISK_ENABLE || TCFG_SD0_ENABLE || TCFG_SD1_ENABLE || SDFILE_STORAGE)

#define LOG_TAG_CONST       APP_STORAGE_DEV
#define LOG_TAG             "[APP_STORAGE_DEV]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#ifndef OS_SR_ALLOC
#define OS_SR_ALLOC()
#endif


struct storage_devinfo {
    u32 *dev;
    u16 ptr;
    u16 max;
} strg_dev;
#define __this 	(&strg_dev)


extern const struct storage_dev storage_device_begin[];
extern const struct storage_dev storage_device_end[];

#define list_for_each_device(p) \
    for (p = storage_device_begin; p < storage_device_end; p++)

static u16 storage_dev_find_ex(void *logo, storage_callback cb)
{
    u16 i;
    for (i = 0; i < __this->ptr; i++) {
        struct storage_dev *p = (struct storage_dev *)__this->dev[i];
        if (!strcmp(p->logo, logo)) {
            if (cb && cb(p)) {
                continue;
            }
            return i;
        }
    }

    return i;
}

static u16 storage_dev_find(void *logo)
{
    return storage_dev_find_ex(logo, NULL);
}


int storage_dev_add_ex(void *logo, storage_callback cb)
{
    if (!__this->max) {
        return STORAGE_DEV_EMPTY;
    } else if (__this->ptr >= __this->max) {
        return STORAGE_DEV_FULL;
    }
    OS_SR_ALLOC();
    OS_ENTER_CRITICAL();
    u16 idx = storage_dev_find(logo);
    if (idx < __this->ptr) {
        if (cb && cb((struct storage_dev *)__this->dev[idx])) {
        } else {
            OS_EXIT_CRITICAL();
            return STORAGE_DEV_OK;//STORAGE_DEV_ALREADY;
        }
    }
    const struct storage_dev *p;
    list_for_each_device(p) {
        if (!strcmp(p->logo, logo)) {
            if (cb && cb((struct storage_dev *)p)) {
                continue;
            }
            *(p->counter) = 0;
            __this->dev[__this->ptr++] = (u32)p;
            OS_EXIT_CRITICAL();
            return STORAGE_DEV_OK;
        }
    }
    OS_EXIT_CRITICAL();
    return STORAGE_DEV_IS_NOT_STORAGE;
}

int storage_dev_del_ex(void *logo, storage_callback cb)
{
    if ((!__this->max) || (!__this->ptr)) {
        return STORAGE_DEV_EMPTY;
    }
    OS_SR_ALLOC();
    OS_ENTER_CRITICAL();
    u16 idx = storage_dev_find(logo);
    if (idx >= __this->ptr) {
        OS_EXIT_CRITICAL();
        return STORAGE_DEV_NO_FIND;
    }
    if (cb && cb((struct storage_dev *)__this->dev[idx])) {
        OS_EXIT_CRITICAL();
        log_info("del err callback \n");
        return STORAGE_DEV_CALLBACK;
    }
    __this->ptr--;
    for (; idx < __this->ptr; idx++) {
        __this->dev[idx] = __this->dev[idx + 1];
    }
    OS_EXIT_CRITICAL();
    return STORAGE_DEV_OK;
}

int storage_dev_total_ex(storage_callback cb)
{
    u16 i, cnt = 0;

    OS_SR_ALLOC();
    OS_ENTER_CRITICAL();
    for (i = 0; i < __this->ptr; i++) {
        if (cb && cb((struct storage_dev *)__this->dev[i])) {
            continue;
        }
        cnt++;
    }
    OS_EXIT_CRITICAL();

    return cnt;
}

struct storage_dev *storage_dev_check_ex(void *logo, storage_callback cb)
{
    if ((!__this->max) || (!__this->ptr)) {
        return NULL;
    }
    OS_SR_ALLOC();
    OS_ENTER_CRITICAL();
    u16 idx = storage_dev_find_ex(logo, cb);
    if (idx >= __this->ptr) {
        OS_EXIT_CRITICAL();
        return NULL;
    }
    struct storage_dev *p = (struct storage_dev *)__this->dev[idx];
    OS_EXIT_CRITICAL();

    return p;
}

struct storage_dev *storage_dev_frist_ex(storage_callback cb)
{
    if ((!__this->max) || (!__this->ptr)) {
        return NULL;
    }
    OS_SR_ALLOC();
    OS_ENTER_CRITICAL();
    struct storage_dev *p = NULL;
    u16 i;
    for (i = 0; i < __this->ptr; i++) {
        p = (struct storage_dev *)__this->dev[i];
        if (cb && cb(p)) {
            continue;
        }
        OS_EXIT_CRITICAL();
        return p;
    }

    OS_EXIT_CRITICAL();

    return NULL;
}

struct storage_dev *storage_dev_last_ex(storage_callback cb)
{
    if ((!__this->max) || (!__this->ptr)) {
        return NULL;
    }
    OS_SR_ALLOC();
    OS_ENTER_CRITICAL();
    struct storage_dev *p = NULL;
    u16 i;
    for (i = __this->ptr; i > 0; i--) {
        p = (struct storage_dev *)__this->dev[i - 1];
        if (cb && cb(p)) {
            continue;
        }
        OS_EXIT_CRITICAL();
        return p;
    }

    OS_EXIT_CRITICAL();

    return NULL;
}

struct storage_dev *storage_dev_last_active_ex(storage_callback cb)
{
    if ((!__this->max) || (!__this->ptr)) {
        return NULL;
    }
    OS_SR_ALLOC();
    OS_ENTER_CRITICAL();
    u16 i;
    u8 find = 0;
    struct storage_dev *p_last_active = NULL;
    struct storage_dev *p;

    for (i = 0; i < __this->ptr; i++) {
        p_last_active = (struct storage_dev *)__this->dev[i];
        if (cb && cb(p_last_active)) {
            continue;
        }
        find = 1;
        break;
    }
    if (find == 0) {
        return NULL;
    }

    for (i = 0; i < __this->ptr; i++) {
        struct storage_dev *p = (struct storage_dev *)__this->dev[i];

        if (cb && cb(p)) {
            continue;
        }
        /* printf("*(p_last_active->counter) = %d, *(p->counter) = %d\n ", *(p_last_active->counter), *(p->counter)); */
        if (*(p_last_active->counter) < * (p->counter)) {
            p_last_active = p;
        }
    }

    OS_EXIT_CRITICAL();
    return p_last_active;
}

extern u32 timer_get_ms(void);
static void storage_dev_active_mark_ex(void *logo, storage_callback cb)
{
    OS_SR_ALLOC();
    OS_ENTER_CRITICAL();
    u16 idx = storage_dev_find_ex(logo, cb);
    if (idx >= __this->ptr) {
        OS_EXIT_CRITICAL();
        return ;
    }
    struct storage_dev *p = (struct storage_dev *)__this->dev[idx];
    *(p->counter) = timer_get_ms();
    /* printf("%s ====================++++++++++++++++++++++++++++++++++counter %d\n" ,__FUNCTION__, *(p->counter)); */
    OS_EXIT_CRITICAL();
}

struct storage_dev *storage_dev_next_ex(struct storage_dev *cur, storage_callback cb)
{
    if ((!__this->max) || (!__this->ptr) || (!cur)) {
        return NULL;
    }
    OS_SR_ALLOC();
    OS_ENTER_CRITICAL();
    u16 idx = storage_dev_find_ex(cur->logo, cb);
    /* if (idx >= __this->ptr) { */
    /* OS_EXIT_CRITICAL(); */
    /* return STORAGE_DEV_NO_FIND;	 */
    /* } */
    idx++;
    if (idx >= __this->ptr) {
        idx = 0;
    }
    struct storage_dev *p = (struct storage_dev *)__this->dev[idx];
    OS_EXIT_CRITICAL();

    return p;
}

struct storage_dev *storage_dev_prev_ex(struct storage_dev *cur, storage_callback cb)
{
    if ((!__this->max) || (!__this->ptr) || (!cur)) {
        return NULL;
    }
    OS_SR_ALLOC();
    OS_ENTER_CRITICAL();
    u16 idx = storage_dev_find_ex(cur->logo, cb);
    /* if (idx >= __this->ptr) { */
    /* OS_EXIT_CRITICAL(); */
    /* return STORAGE_DEV_NO_FIND;	 */
    /* } */
    idx--;
    if (idx >= __this->ptr) {
        idx = __this->ptr - 1;
    }
    struct storage_dev *p = (struct storage_dev *)__this->dev[idx];
    OS_EXIT_CRITICAL();

    return p;
}

int storage_dev_add(void *logo)
{
    return storage_dev_add_ex(logo, NULL);
}

int storage_dev_del(void *logo)
{
    return storage_dev_del_ex(logo, NULL);
}

int storage_dev_total(void)
{
    return __this->ptr;
}

struct storage_dev *storage_dev_check(void *logo)
{
    return storage_dev_check_ex(logo, NULL);
}

struct storage_dev *storage_dev_frist(void)
{
    return storage_dev_frist_ex(NULL);
}

struct storage_dev *storage_dev_last(void)
{
    return storage_dev_last_ex(NULL);
}

struct storage_dev *storage_dev_last_active(void)
{
    return storage_dev_last_active_ex(NULL);
}

void storage_dev_active_mark(void *logo)
{
    storage_dev_active_mark_ex(logo, NULL);
}

struct storage_dev *storage_dev_next(struct storage_dev *cur)
{
    return storage_dev_next_ex(cur, NULL);
}

struct storage_dev *storage_dev_prev(struct storage_dev *cur)
{
    return storage_dev_prev_ex(cur, NULL);
}

int storage_dev_init(void)
{
    memset(__this, 0, sizeof(struct storage_devinfo));

    u16 max = 0;
    const struct storage_dev *p;
    list_for_each_device(p) {
        max++;
    }
    log_info("storage max:%d, \n", max);
    if (!max) {
        return -1;
    }

    __this->dev = malloc(max * sizeof(u32 *));
    if (__this->dev) {
        __this->max = max;
#if SDFILE_STORAGE && (defined(TCFG_CODE_FLASH_ENABLE) && TCFG_CODE_FLASH_ENABLE)
        storage_dev_add(SDFILE_DEV);
#endif
        return 0;
    }

    return -1;
}
late_initcall(storage_dev_init);

#endif

