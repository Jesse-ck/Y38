#include "app_config.h"
#include "system/includes.h"
#include "system/os/os_api.h"

#ifndef OS_SR_ALLOC
#define OS_SR_ALLOC()
#endif

/* #define DEV_PRE_DEBUG */
#ifdef DEV_PRE_DEBUG
#define dev_pre_deg             log_i
#define dev_pre_deg_puts        log_i
#define dev_pre_deg_buf         put_buf
#else
#define dev_pre_deg(...)
#define dev_pre_deg_puts(...)
#define dev_pre_deg_buf(...)
#endif

#define WBUF_LIMIT_LEN		(TCFG_DEV_PREPROCESSOR_SECTORS*3/4)

#if (defined(TCFG_DEV_PREPROCESSOR_ENABLE) && TCFG_DEV_PREPROCESSOR_ENABLE)

#define DEV_INFO_SD			0
#define DEV_INFO_USB		1
#define DEV_INFO_MAX		2

struct dev_wbuf {
    u8  data[512];
    u32 sector;
    u32 dev_idx : 4;
};

struct dev_info {
    struct device *device;
    struct device_operations *pos;
};

struct dev_preprocessor {
    volatile u32 init_ok : 1;
        volatile u32 wait_idle : 1;
        u32 update : 1;
        u32 close : 1;
        u32 err : 1;
        u32 dev_idx : 4;
        u32 win_idx : 4;
        u32 sector;
        char *write_buf;
        OS_SEM sem_task_run;
        OS_SEM sem_prepro_ctl;
        OS_MUTEX mutex;
        struct dev_info dev[DEV_INFO_MAX];

        u8 wbuf_r[TCFG_DEV_PREPROCESSOR_NUM];
        u8 wbuf_w[TCFG_DEV_PREPROCESSOR_NUM];
        u8 wbuf_len[TCFG_DEV_PREPROCESSOR_NUM];
        struct dev_wbuf wbuf[TCFG_DEV_PREPROCESSOR_NUM][TCFG_DEV_PREPROCESSOR_SECTORS];
    };
    static struct dev_preprocessor *dev_pre_hdl = NULL;

    static int dev_wbuf_sector_check(u8 dev_idx, u8 win_idx, u32 sector)
{
    int i, rcnt;
    rcnt = dev_pre_hdl->wbuf_r[win_idx];
    for (i = 0; i < TCFG_DEV_PREPROCESSOR_SECTORS; i++) {
        if (rcnt >= TCFG_DEV_PREPROCESSOR_SECTORS) {
            rcnt = 0;
        }
        if ((dev_pre_hdl->wbuf[win_idx][rcnt].dev_idx == dev_idx)
            && (dev_pre_hdl->wbuf[win_idx][rcnt].sector == sector)
           ) {
            return rcnt;
        }
        rcnt++;
    }
    return TCFG_DEV_PREPROCESSOR_SECTORS;
}

static int dev_wbuf_check_continue(u8 win_idx, u8 cnt)
{
    int i;
    u8 dev_idx = dev_pre_hdl->wbuf[win_idx][cnt].dev_idx;
    u32 sector = dev_pre_hdl->wbuf[win_idx][cnt].sector;
    if (sector == (u32) - 1) {
        return 0;
    }
    cnt++;
    sector++;
    for (i = 0; i < TCFG_DEV_PREPROCESSOR_SECTORS - 1; i++) {
        if (cnt >= TCFG_DEV_PREPROCESSOR_SECTORS) {
            cnt = 0;
        }
        /* printf("s:%d, d:%d, i:%d, cnt:%d, win:%d ", dev_pre_hdl->wbuf[win_idx][cnt].dev_idx, dev_pre_hdl->wbuf[win_idx][cnt].sector, i, cnt, win_idx); */
        if ((dev_pre_hdl->wbuf[win_idx][cnt].dev_idx != dev_idx)
            || (dev_pre_hdl->wbuf[win_idx][cnt].sector != sector)
           ) {
            return i + 1;
        }
        cnt++;
        sector++;
    }
    return i;
}

static int dev_preprocessor_read(u8 dev_idx, void *buf, u32 sector)
{
    int ret = 0;
    int cnt, i, win_idx;

    dev_pre_deg("rsec:%d ", sector);

    void *pdevice = dev_pre_hdl->dev[dev_idx].device;
    if (!pdevice || !dev_pre_hdl->init_ok) {
        return 0;
    }

    OS_SR_ALLOC();
    os_mutex_pend(&dev_pre_hdl->mutex, 0);

    while (dev_pre_hdl->init_ok) {
        OS_ENTER_CRITICAL();
        dev_pre_hdl->sector = sector;
        dev_pre_hdl->dev_idx = dev_idx;
        dev_pre_hdl->write_buf = NULL;
        dev_pre_hdl->err = 0;
        dev_pre_hdl->update = 1;
        os_sem_set(&dev_pre_hdl->sem_prepro_ctl, 0);

        win_idx = dev_pre_hdl->win_idx;
        for (i = 0; i < TCFG_DEV_PREPROCESSOR_NUM; i++) {
            cnt = dev_wbuf_sector_check(dev_idx, win_idx, sector);
            if (cnt < TCFG_DEV_PREPROCESSOR_SECTORS) {
                memcpy(buf, dev_pre_hdl->wbuf[win_idx][cnt].data, 512);
                dev_pre_hdl->win_idx = win_idx;
                break;
            }
            win_idx ++;
            if (win_idx >= TCFG_DEV_PREPROCESSOR_NUM) {
                win_idx = 0;
            }
        }
        os_sem_set(&dev_pre_hdl->sem_task_run, 0);
        os_sem_post(&dev_pre_hdl->sem_task_run);
        OS_EXIT_CRITICAL();

        if (cnt < TCFG_DEV_PREPROCESSOR_SECTORS) {
            ret = 1;
            break;
        }

        os_sem_pend(&dev_pre_hdl->sem_prepro_ctl, 0);

        if (dev_pre_hdl->err) {
            break;
        }
    }

    os_mutex_post(&dev_pre_hdl->mutex);
    return ret;
}

static int dev_preprocessor_write(u8 dev_idx, void *buf, u32 sector)
{
    int ret = 0;
    dev_pre_deg("wsec:%d ", sector);

    void *pdevice = dev_pre_hdl->dev[dev_idx].device;
    if (!pdevice || !dev_pre_hdl->init_ok) {
        return 0;
    }

    OS_SR_ALLOC();
    os_mutex_pend(&dev_pre_hdl->mutex, 0);

    ret = dev_pre_hdl->dev[dev_idx].pos->write(pdevice, buf, 1, sector);

    if (ret == 1) { // ok
        OS_ENTER_CRITICAL();
        dev_pre_hdl->sector = sector;
        dev_pre_hdl->dev_idx = dev_idx;
        dev_pre_hdl->write_buf = buf;
        dev_pre_hdl->err = 0;
        dev_pre_hdl->update = 1;
        os_sem_set(&dev_pre_hdl->sem_prepro_ctl, 0);

        os_sem_set(&dev_pre_hdl->sem_task_run, 0);
        os_sem_post(&dev_pre_hdl->sem_task_run);
        OS_EXIT_CRITICAL();

        os_sem_pend(&dev_pre_hdl->sem_prepro_ctl, 0);

        if (dev_pre_hdl->err) {
            ret = 0;
        }
        if (!dev_pre_hdl->init_ok) {
            ret = 0;
        }
    }

    os_mutex_post(&dev_pre_hdl->mutex);
    return ret;
}

static void dev_preprocessor_colse(u8 dev_idx)
{
    dev_pre_deg("close");

    void *pdevice = dev_pre_hdl->dev[dev_idx].device;
    if (!pdevice) {
        return ;
    }

    OS_SR_ALLOC();
    os_mutex_pend(&dev_pre_hdl->mutex, 0);

    OS_ENTER_CRITICAL();
    dev_pre_hdl->dev_idx = dev_idx;
    dev_pre_hdl->close = 1;
    os_sem_set(&dev_pre_hdl->sem_prepro_ctl, 0);

    os_sem_set(&dev_pre_hdl->sem_task_run, 0);
    os_sem_post(&dev_pre_hdl->sem_task_run);
    OS_EXIT_CRITICAL();

    os_sem_pend(&dev_pre_hdl->sem_prepro_ctl, 0);

    /* dev_pre_hdl->dev[dev_idx].pos->close(pdevice); */
    os_mutex_post(&dev_pre_hdl->mutex);
}

static void dev_preprocessor_task(void *p)
{
    int i, cnt, ret;
    int pend_dly = -1;
    u32 cur_sec;
    u8 cur_dev;
    u8 cur_win;
    u8 is_read = 0;

    while (dev_pre_hdl) {
        if (pend_dly) {
            os_sem_pend(&dev_pre_hdl->sem_task_run, pend_dly);
        }
        if (!dev_pre_hdl->init_ok) {
            dev_pre_hdl->wait_idle = 0;
            break;
        }
        pend_dly++;
        OS_ENTER_CRITICAL();
        if (dev_pre_hdl->close) {
            dev_pre_hdl->close = 0;
            int x, y;
            for (x = 0; x < TCFG_DEV_PREPROCESSOR_NUM; x++) {
                for (y = 0; y < TCFG_DEV_PREPROCESSOR_SECTORS; y++) {
                    if (dev_pre_hdl->wbuf[x][y].dev_idx == dev_pre_hdl->dev_idx) {
                        dev_pre_hdl->wbuf[x][y].sector = (u32) - 1;
                    }
                }
            }
            cur_dev = dev_pre_hdl->dev_idx;
            dev_pre_hdl->dev[dev_pre_hdl->dev_idx].device = NULL;
            os_sem_set(&dev_pre_hdl->sem_prepro_ctl, 0);
            os_sem_post(&dev_pre_hdl->sem_prepro_ctl);
            OS_EXIT_CRITICAL();
            continue;
        }
        if (dev_pre_hdl->update) {
            // 有更新，看是否在wbuf中
            cur_dev = dev_pre_hdl->dev_idx;
            cur_sec = dev_pre_hdl->sector;
            cur_win = dev_pre_hdl->win_idx;
            if (dev_pre_hdl->write_buf) {
                // is write;
                is_read = 0;
                for (i = 0; i < TCFG_DEV_PREPROCESSOR_NUM; i++) {
                    cnt = dev_wbuf_sector_check(cur_dev, cur_win, cur_sec);
                    if (cnt < TCFG_DEV_PREPROCESSOR_SECTORS) {
                        // 在缓存中，更新
                        memcpy(dev_pre_hdl->wbuf[cur_win][cnt].data, dev_pre_hdl->write_buf, 512);
                    }
                    cur_win ++;
                    if (cur_win >= TCFG_DEV_PREPROCESSOR_NUM) {
                        cur_win = 0;
                    }
                }
                if (cnt < TCFG_DEV_PREPROCESSOR_SECTORS) {
                } else {
                    cnt = dev_wbuf_sector_check(cur_dev, cur_win, cur_sec - 1);
                    if (cnt >= TCFG_DEV_PREPROCESSOR_SECTORS) {
                        cur_win = dev_pre_hdl->win_idx + 1;
                        if (cur_win >= TCFG_DEV_PREPROCESSOR_NUM) {
                            cur_win = 0;
                        }
                    }
                    cnt = dev_pre_hdl->wbuf_r[cur_win] + TCFG_DEV_PREPROCESSOR_SECTORS * 2 / 3;
                    if (cnt >= TCFG_DEV_PREPROCESSOR_SECTORS) {
                        cnt -= TCFG_DEV_PREPROCESSOR_SECTORS;
                    }
                    memcpy(dev_pre_hdl->wbuf[cur_win][cnt].data, dev_pre_hdl->write_buf, 512);
                    dev_pre_hdl->wbuf[cur_win][cnt].sector = cur_sec;
                    dev_pre_hdl->wbuf[cur_win][cnt].dev_idx = cur_dev;
                }
                dev_pre_hdl->update = 0;
                os_sem_set(&dev_pre_hdl->sem_prepro_ctl, 0);
                os_sem_post(&dev_pre_hdl->sem_prepro_ctl);
            } else {
                is_read = 1;
                for (i = 0; i < TCFG_DEV_PREPROCESSOR_NUM; i++) {
                    cnt = dev_wbuf_sector_check(cur_dev, cur_win, cur_sec);
                    if (cnt < TCFG_DEV_PREPROCESSOR_SECTORS) {
                        break;
                    }
                    cur_win ++;
                    if (cur_win >= TCFG_DEV_PREPROCESSOR_NUM) {
                        cur_win = 0;
                    }
                }
                if (cnt < TCFG_DEV_PREPROCESSOR_SECTORS) {
                    // 在缓存中，更新
                    dev_pre_deg("sec already, curwin:%d ", cur_win);
                    dev_pre_hdl->wbuf_r[cur_win] = cnt;
                    dev_pre_hdl->wbuf_len[cur_win] = dev_wbuf_check_continue(cur_win, cnt);
                    dev_pre_hdl->wbuf_w[cur_win] = dev_pre_hdl->wbuf_len[cur_win] + cnt;
                    if (dev_pre_hdl->wbuf_w[cur_win] >= TCFG_DEV_PREPROCESSOR_SECTORS) {
                        dev_pre_hdl->wbuf_w[cur_win] -= TCFG_DEV_PREPROCESSOR_SECTORS;
                    }
                    dev_pre_hdl->update = 0;
                    os_sem_set(&dev_pre_hdl->sem_prepro_ctl, 0);
                    os_sem_post(&dev_pre_hdl->sem_prepro_ctl);
                    /* printf("r:%d, w:%d, l:%d ", dev_pre_hdl->wbuf_r[cur_win], dev_pre_hdl->wbuf_w[cur_win], dev_pre_hdl->wbuf_len[cur_win]); */
                    cur_sec += dev_pre_hdl->wbuf_len[cur_win];
                } else {
                    // 不在缓存中，从后面接着写
                    cnt = dev_wbuf_sector_check(cur_dev, cur_win, cur_sec - 1);
                    if (cnt >= TCFG_DEV_PREPROCESSOR_SECTORS) {
                        cur_win = dev_pre_hdl->win_idx + 1;
                        if (cur_win >= TCFG_DEV_PREPROCESSOR_NUM) {
                            cur_win = 0;
                        }
                    }
                    dev_pre_deg("sec no yet, curwin:%d ", cur_win);
                    dev_pre_hdl->wbuf_r[cur_win] = dev_pre_hdl->wbuf_w[cur_win];
                    dev_pre_hdl->wbuf_len[cur_win] = 0;
                }
            }
        }
        if (!is_read) {
            OS_EXIT_CRITICAL();
            continue;
        }
        if (!dev_pre_hdl->dev[cur_dev].device) {
            goto __dev_err;
        }

        for (i = 0; i < TCFG_DEV_PREPROCESSOR_NUM; i++) {
            if (dev_pre_hdl->wbuf_len[cur_win] < WBUF_LIMIT_LEN) {
                // 数据太少，接着写wbuf
                dev_pre_hdl->wbuf[cur_win][dev_pre_hdl->wbuf_w[cur_win]].sector = (u32) - 1;
                pend_dly = 0;
                break;
            }
            cur_win ++;
            if (cur_win >= TCFG_DEV_PREPROCESSOR_NUM) {
                cur_win = 0;
            }
        }
        OS_EXIT_CRITICAL();

        if (pend_dly == 0) {
            void *pdevice = dev_pre_hdl->dev[cur_dev].device;
            if (pdevice) {
                /* printf("pre sec ============:%d ", cur_sec); */
                ret = dev_pre_hdl->dev[cur_dev].pos->read(pdevice,
                        dev_pre_hdl->wbuf[cur_win][dev_pre_hdl->wbuf_w[cur_win]].data, 1, cur_sec);

                /* if(cur_sec == 0) */
                /* { */
                /* put_buf(dev_pre_hdl->wbuf[cur_win][dev_pre_hdl->wbuf_w[cur_win]].data, 512);		 */
                /* } */

            } else {
                ret = 0;
            }
            OS_ENTER_CRITICAL();
            if (ret == 1) { // ok
                dev_pre_hdl->wbuf[cur_win][dev_pre_hdl->wbuf_w[cur_win]].sector = cur_sec;
                dev_pre_hdl->wbuf[cur_win][dev_pre_hdl->wbuf_w[cur_win]].dev_idx = cur_dev;
                dev_pre_hdl->wbuf_len[cur_win] ++;
                dev_pre_hdl->wbuf_w[cur_win] ++;
                if (dev_pre_hdl->wbuf_w[cur_win] >= TCFG_DEV_PREPROCESSOR_SECTORS) {
                    dev_pre_hdl->wbuf_w[cur_win] = 0;
                }
                cur_sec++;
            } else { // err
__dev_err:
                if (dev_pre_hdl->update) {
                    if ((cur_sec == dev_pre_hdl->sector) && (cur_dev == dev_pre_hdl->dev_idx)) {
                        dev_pre_hdl->update = 0;
                        dev_pre_hdl->err = 1;
                        os_sem_set(&dev_pre_hdl->sem_prepro_ctl, 0);
                        os_sem_post(&dev_pre_hdl->sem_prepro_ctl);
                    }
                }
                pend_dly = 100;
            }
            OS_EXIT_CRITICAL();
        }
    }
    while (1) {
        os_time_dly(100);
    }
}






#if (TCFG_SD0_ENABLE || TCFG_SD1_ENABLE)
#include "asm/sdmmc.h"

static bool sd_pre_online(const struct dev_node *node)
{
    return sd_dev_ops.online(node);
}
static int sd_pre_f_open(const char *name, struct device **device, void *arg)
{
    int ret = sd_dev_ops.open(name + sizeof(TCFG_DEV_PREPROCESSOR_HEAD) - 1, device, arg);
    if (ret == 0) {
        if (dev_pre_hdl && dev_pre_hdl->init_ok) {
            dev_pre_hdl->dev[DEV_INFO_SD].device = *device;
        }
    }
    return ret;
}

static int sd_pre_f_read(struct device *device, void *buf, u32 len, u32 offset)
{
    if (dev_pre_hdl && dev_pre_hdl->init_ok) {
        int i, addr;
        for (i = 0, addr = offset; i < len; i++, addr++) {
            int ret = dev_preprocessor_read(DEV_INFO_SD, buf, addr);
            if (!ret) {
                return 0;
            }
        }
        return len;
    }
    return sd_dev_ops.read(device, buf, len, offset);
}

static int sd_pre_f_write(struct device *device, void *buf, u32 len, u32 offset)
{
    if (dev_pre_hdl && dev_pre_hdl->init_ok) {
        int i, addr;
        for (i = 0, addr = offset; i < len; i++, addr++) {
            int ret = dev_preprocessor_write(DEV_INFO_SD, buf, addr);
            if (!ret) {
                return 0;
            }
        }
        return len;
    }
    return sd_dev_ops.write(device, buf, len, offset);
}

static int sd_pre_f_ioctl(struct device *device, u32 cmd, u32 arg)
{
    return sd_dev_ops.ioctl(device, cmd, arg);
}

static int sd_pre_f_close(struct device *device)
{
    if (dev_pre_hdl) {
        dev_preprocessor_colse(DEV_INFO_SD);
    }
    return sd_dev_ops.close(device);
}


const struct device_operations sd_dev_pre_ops = {
    .init  = NULL,
    .online = sd_pre_online,
    .open  = sd_pre_f_open,
    .read  = sd_pre_f_read,
    .write = sd_pre_f_write,
    .ioctl = sd_pre_f_ioctl,
    .close = sd_pre_f_close,
};
#endif



#if TCFG_UDISK_ENABLE
#include "usb/otg.h"

extern const struct device_operations mass_storage_ops;

static bool usb_stor_pre_online(const struct dev_node *node)
{
    return mass_storage_ops.online(node);
}
static int usb_stor_pre_open(const char *name, struct device **device, void *arg)
{
    int ret = mass_storage_ops.open(name + sizeof(TCFG_DEV_PREPROCESSOR_HEAD) - 1, device, arg);
    if (ret == 0) {
        if (dev_pre_hdl && dev_pre_hdl->init_ok) {
            dev_pre_hdl->dev[DEV_INFO_USB].device = *device;
        }
    }
    return ret;
}

static int usb_stor_pre_read(struct device *device, void *pBuf, u32 num_lba, u32 lba)
{
    if (dev_pre_hdl && dev_pre_hdl->init_ok) {
        int i, addr;
        for (i = 0, addr = lba; i < num_lba; i++, addr++) {
            int ret = dev_preprocessor_read(DEV_INFO_USB, pBuf, addr);
            if (!ret) {
                return 0;
            }
        }
        return num_lba;
    }
    return mass_storage_ops.read(device, pBuf, num_lba, lba);
}

static int usb_stor_pre_write(struct device *device, void *pBuf,  u32 num_lba, u32 lba)
{
    if (dev_pre_hdl && dev_pre_hdl->init_ok) {
        int i, addr;
        for (i = 0, addr = lba; i < num_lba; i++, addr++) {
            int ret = dev_preprocessor_write(DEV_INFO_USB, pBuf, addr);
            if (!ret) {
                return 0;
            }
        }
        return num_lba;
    }
    return mass_storage_ops.write(device, pBuf, num_lba, lba);
}

static int usb_stor_pre_ioctrl(struct device *device, u32 cmd, u32 arg)
{
    return mass_storage_ops.ioctl(device, cmd, arg);
}
static int usb_stor_pre_close(struct device *device)
{
    if (dev_pre_hdl) {
        dev_preprocessor_colse(DEV_INFO_USB);
    }
    return mass_storage_ops.close(device);
}

const struct device_operations mass_storage_pre_ops = {
    .init = NULL,
    .online = usb_stor_pre_online,
    .open  = usb_stor_pre_open,
    .read  = usb_stor_pre_read,
    .write = usb_stor_pre_write,
    .ioctl = usb_stor_pre_ioctrl,
    .close = usb_stor_pre_close,
};
#endif



REGISTER_DEVICES(device_pre_table) = {
#if TCFG_SD0_ENABLE
    { TCFG_DEV_PREPROCESSOR_HEAD"sd0", 	&sd_dev_pre_ops, 	NULL},
#endif
#if TCFG_SD1_ENABLE
    { TCFG_DEV_PREPROCESSOR_HEAD"sd1", 	&sd_dev_pre_ops, 	NULL},
#endif
#if TCFG_UDISK_ENABLE
    { TCFG_DEV_PREPROCESSOR_HEAD"udisk",   &mass_storage_pre_ops, NULL},
#endif
};

int dev_preprocessor_open(void)
{
    int i, j;
    int err;
    if (dev_pre_hdl) {
        return 0;
    }
    dev_pre_hdl = zalloc(sizeof(struct dev_preprocessor));
    if (!dev_pre_hdl) {
        return OS_ERR_MEM_NO_MEM;
    }
    os_mutex_create(&dev_pre_hdl->mutex);
    os_sem_create(&dev_pre_hdl->sem_task_run, 0);
    os_sem_create(&dev_pre_hdl->sem_prepro_ctl, 0);
    for (i = 0; i < TCFG_DEV_PREPROCESSOR_NUM; i++) {
        for (j = 0; j < TCFG_DEV_PREPROCESSOR_SECTORS; j++) {
            dev_pre_hdl->wbuf[i][j].sector = (u32) - 1;
        }
    }

#if (TCFG_SD0_ENABLE || TCFG_SD1_ENABLE)
    dev_pre_hdl->dev[DEV_INFO_SD].pos = &sd_dev_ops;
#endif
#if (TCFG_UDISK_ENABLE)
    dev_pre_hdl->dev[DEV_INFO_USB].pos = &mass_storage_ops;
#endif

    err = task_create(dev_preprocessor_task, NULL, "dev_pre");
    if (err != OS_NO_ERR) {
        free(dev_pre_hdl);
        dev_pre_hdl = NULL;
        return err;
    }
    dev_pre_hdl->init_ok = 1;
    return 0;
}

void dev_preprocessor_close(void)
{
    if (!dev_pre_hdl) {
        return ;
    }
    void *ptr = dev_pre_hdl;
    OS_ENTER_CRITICAL();
    dev_pre_hdl->init_ok = 0;
    dev_pre_hdl->wait_idle = 1;
    os_sem_set(&dev_pre_hdl->sem_prepro_ctl, 0);
    os_sem_post(&dev_pre_hdl->sem_prepro_ctl);
    os_sem_set(&dev_pre_hdl->sem_task_run, 0);
    os_sem_post(&dev_pre_hdl->sem_task_run);
    OS_EXIT_CRITICAL();
    while (dev_pre_hdl->wait_idle) {
        os_time_dly(1);
    }
    task_kill("dev_pre");
    dev_pre_hdl = NULL;
    free(ptr);
}

int dev_preprocessor_init(void)
{
    dev_preprocessor_open();
    return 0;
}
late_initcall(dev_preprocessor_init);

#endif /*TCFG_DEV_PREPROCESSOR_ENABLE*/

