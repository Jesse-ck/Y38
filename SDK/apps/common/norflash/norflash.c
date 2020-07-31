#include "norflash.h"
#include "app_config.h"

#if defined(TCFG_NORFLASH_DEV_ENABLE) && TCFG_NORFLASH_DEV_ENABLE

#undef LOG_TAG_CONST
#define LOG_TAG     "[FLASH]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#include "debug.h"

static int spi_err;
static int spi_num;
static u32 spi_cs_io;
#define spi_cs_h()                  gpio_write(spi_cs_io, 1)
#define spi_cs_l()                  gpio_write(spi_cs_io, 0)
#define spi_read_byte()             spi_recv_byte(spi_num, (int *)&spi_err)
#define spi_write_byte(x)           spi_send_byte(spi_num, x)
#define spi_dma_read(x, y)          spi_dma_recv(spi_num, x, y)
#define spi_dma_write(x, y)         spi_dma_send(spi_num, x, y)

static u32 flash_id = 0;
static u32 flash_capacity = 0;
static u32 flash_base_addr = 0;
static u32 flash_user_capacity = 0;

#define FLASH_CACHE_ENABLE  0

#if FLASH_CACHE_ENABLE
static u32 flash_cache_addr;
static u8 *flash_cache_buf = NULL; //缓存4K的数据，与flash里的数据一样。
static int _check_0xff(u8 *buf, u32 len)
{
    for (u32 i = 0; i < len; i ++) {
        if ((*(buf + i)) != 0xff) {
            return 1;
        }
    }
    return 0;
}
#endif


static u32 _pow(u32 num, int n)
{
    u32 powint = 1;
    int i;
    for (i = 1; i <= n; i++) {
        powint *= num;
    }
    return powint;
}

static int _norflash_wait_ok()
{
    u32 timeout = 8 * 1000 * 1000 / 100;
    while (timeout--) {
        spi_cs_l();
        spi_write_byte(WINBOND_READ_SR1);
        u8 reg_1 = spi_read_byte();
        spi_cs_h();
        if (!(reg_1 & BIT(0))) {
            break;
        }
        delay(100);
    }
    if (timeout == 0) {
        log_error("norflash_wait_ok timeout!\r\n");
        return 1;
    }
    return 0;
}

static void _norflash_send_write_enable()
{
    spi_cs_l();
    spi_write_byte(WINBOND_WRITE_ENABLE);
    spi_cs_h();
}

void set_norflash_user_capacity(u32 capacity)
{
    flash_user_capacity = capacity;
}

void set_norflash_base_addr(u32 addr)
{
    flash_base_addr = addr;
}

static void _norflash_send_addr(u32 addr)
{
    addr += flash_base_addr;
    spi_write_byte(addr >> 16);
    spi_write_byte(addr >> 8);
    spi_write_byte(addr);
}

static u32 _norflash_read_id()
{
    u8 id[3];
    spi_cs_l();
    spi_write_byte(WINBOND_JEDEC_ID);
    for (u8 i = 0; i < sizeof(id); i++) {
        id[i] = spi_read_byte();
    }
    spi_cs_h();
    return id[0] << 16 | id[1] << 8 | id[2];
}

int _norflash_init(struct norflash_dev_platform_data *pdata)
{
    log_info("norflash_init !\n");
    flash_id = 0;
    flash_capacity = 0;
    flash_base_addr = 0;
    flash_user_capacity = 0;
    spi_num = pdata->spi_hw_num;
    spi_cs_io = pdata->spi_cs_port;
    return 0;
}

int _norflash_open(void)
{
    log_info("norflash open\n");
    gpio_set_die(spi_cs_io, 1);
    gpio_set_direction(spi_cs_io, 0);
    gpio_write(spi_cs_io, 1);
    spi_open(spi_num);
    flash_id = _norflash_read_id();
    log_info("norflash_read_id: 0x%x\n", flash_id);
    if ((flash_id == 0) || (flash_id == 0xffffff)) {
        log_error("read norflash id error !\n");
        return 1;
    }
    flash_capacity = 64 * _pow(2, (flash_id & 0xff) - 0x10) * 1024;
    log_info("norflash_capacity: 0x%x \n", flash_capacity);

#if TCFG_NOR_FS_ENABLE
    extern int offset_addr;
    offset_addr = flash_capacity / 2;
    r_printf(">>>[test]:offset_addr = %d\n", offset_addr);
#endif

#if FLASH_CACHE_ENABLE
    flash_cache_buf = (u8 *)malloc(4096);
    ASSERT(flash_cache_buf, "flash_cache_buf is not ok\n");
    flash_cache_addr = 4096;//先给一个大于4096的数
    _norflash_read(0, flash_cache_buf, 4096);
    flash_cache_addr = 0;
#endif

    log_info("norflash open success !\n");
    return 0;
}

int _norflash_close(void)
{
    log_info("norflash close\n");
    spi_close(spi_num);
    gpio_set_die(spi_cs_io, 0);
    gpio_set_direction(spi_cs_io, 1);
    gpio_set_pull_up(spi_cs_io, 0);
    gpio_set_pull_down(spi_cs_io, 0);

#if FLASH_CACHE_ENABLE
    free(flash_cache_buf);
    flash_cache_buf = NULL;
#endif
    return 0;
}

int _norflash_read(u32 addr, u8 *buf, u32 len)
{
#if FLASH_CACHE_ENABLE
    u32 r_len = 4096 - (addr % 4096);
    if ((addr >= flash_cache_addr) && (addr < (flash_cache_addr + 4096))) {
        if (len <= r_len) {
            memcpy(buf, flash_cache_buf + (addr - flash_cache_addr), len);
            return 0;
        } else {
            memcpy(buf, flash_cache_buf + (addr - flash_cache_addr), r_len);
            addr += r_len;
            buf += r_len;
            len -= r_len;
        }
    }
#endif
    spi_cs_l();
    spi_write_byte(WINBOND_READ_DATA);
    _norflash_send_addr(addr);
    spi_dma_read(buf, len);
    spi_cs_h();
    return 0;
}

static int _norflash_write_pages(u32 addr, u8 *buf, u32 len)
{
    int reg;
    u32 first_page_len = 256 - (addr % 256);
    first_page_len = len > first_page_len ? first_page_len : len;
    _norflash_send_write_enable();
    spi_cs_l();
    spi_write_byte(WINBOND_PAGE_PROGRAM);
    _norflash_send_addr(addr) ;
    spi_dma_write(buf, first_page_len);
    spi_cs_h();
    reg = _norflash_wait_ok();
    if (reg) {
        return 1;
    }
    addr += first_page_len;
    buf += first_page_len;
    len -= first_page_len;
    while (len) {
        u32 cnt = len > 256 ? 256 : len;
        _norflash_send_write_enable();
        spi_cs_l();
        spi_write_byte(WINBOND_PAGE_PROGRAM);
        _norflash_send_addr(addr) ;
        spi_dma_write(buf, cnt);
        spi_cs_h();
        reg = _norflash_wait_ok();
        if (reg) {
            return 1;
        }
        addr += cnt;
        buf += cnt;
        len -= cnt;
    }
    return 0;
}

int _norflash_write(u32 addr, void *buf, u32 len)
{
    int reg;
    u8 *w_buf = (u8 *)buf;
    u32 w_len = len;

#if FLASH_CACHE_ENABLE
    u32 align_addr = addr / 4096 * 4096;
    u32 align_len = 4096 - (addr - align_addr);
    align_len = w_len > align_len ? align_len : w_len;
    if (align_addr != flash_cache_addr) {
        _norflash_read(align_addr, flash_cache_buf, 4096);
        flash_cache_addr = align_addr;
    }
    if (_check_0xff(flash_cache_buf + (addr - align_addr), align_len)) {
        memcpy(flash_cache_buf + (addr - align_addr), w_buf, align_len);
        reg = _norflash_eraser(FLASH_SECTOR_ERASER, align_addr);
        if (reg) {
            return 1;
        }
        reg = _norflash_write_pages(align_addr, flash_cache_buf, 4096);
        if (reg) {
            return 1;
        }
    } else {
        memcpy(flash_cache_buf + (addr - align_addr), w_buf, align_len);
        reg = _norflash_write_pages(addr, w_buf, align_len);
        if (reg) {
            return 1;
        }
    }
    addr += align_len;
    w_buf += align_len;
    w_len -= align_len;
    while (w_len) {
        u32 cnt = w_len > 4096 ? 4096 : w_len;
        _norflash_read(addr, flash_cache_buf, 4096);
        flash_cache_addr = addr;
        if (_check_0xff(flash_cache_buf, cnt)) {
            memcpy(flash_cache_buf, w_buf, cnt);
            reg = _norflash_eraser(FLASH_SECTOR_ERASER, addr);
            if (reg) {
                return 1;
            }
            reg = _norflash_write_pages(addr, flash_cache_buf, 4096);
            if (reg) {
                return 1;
            }
        } else {
            memcpy(flash_cache_buf, w_buf, cnt);
            reg = _norflash_write_pages(addr, w_buf, cnt);
            if (reg) {
                return 1;
            }
        }
        addr += cnt;
        w_buf += cnt;
        w_len -= cnt;
    }
#else
    reg = _norflash_write_pages(addr, w_buf, w_len);
    if (reg) {
        return 1;
    }
#endif
    return 0;
}

int _norflash_eraser(u8 eraser, u32 addr)
{
    u8 eraser_cmd;
    switch (eraser) {
    case FLASH_PAGE_ERASER:
        eraser_cmd = WINBOND_PAGE_ERASE;
        addr = addr / 256 * 256;
        break;
    case FLASH_SECTOR_ERASER:
        eraser_cmd = WINBOND_SECTOR_ERASE;
        addr = addr / 4096 * 4096;
        break;
    case FLASH_BLOCK_ERASER:
        eraser_cmd = WINBOND_BLOCK_ERASE;
        addr = addr / 65536 * 65536;
        break;
    case FLASH_CHIP_ERASER:
        eraser_cmd = WINBOND_CHIP_ERASE;
        break;
    }
    _norflash_send_write_enable();
    spi_cs_l();
    spi_write_byte(eraser_cmd);
    if (eraser_cmd != WINBOND_CHIP_ERASE) {
        _norflash_send_addr(addr) ;
    }
    spi_cs_h();
    return _norflash_wait_ok();
}

int _norflash_ioctl(u32 cmd, u32 arg)
{
    int err = 0;
    switch (cmd) {
    case IOCTL_SET_BASE_ADDR:
        flash_base_addr = arg;
        break;
    case IOCTL_SET_CAPACITY:
        flash_user_capacity = arg;
        break;
    case IOCTL_GET_STATUS:
        *(u32 *)arg = 1;
        break;
    case IOCTL_GET_ID:
        *((u32 *)arg) = flash_id;
        break;
    case IOCTL_GET_CAPACITY:
        if (flash_capacity == 0) {
            *((u32 *)arg) = 0;
        } else if (flash_user_capacity) {
            *((u32 *)arg) = flash_user_capacity / 512;
        } else if (flash_capacity > flash_base_addr) {
            *((u32 *)arg) = (flash_capacity / 512) - (flash_base_addr / 512);
        } else {
            *((u32 *)arg) = 0;
        }
        break;
    case IOCTL_GET_BLOCK_SIZE:
        *((u32 *)arg) = 512;
        break;
    case IOCTL_ERASE_PAGE:
        err = _norflash_eraser(FLASH_PAGE_ERASER, arg);
        break;
    case IOCTL_ERASE_SECTOR:
        err = _norflash_eraser(FLASH_SECTOR_ERASER, arg);
        break;
    case IOCTL_ERASE_BLOCK:
        err = _norflash_eraser(FLASH_BLOCK_ERASER, arg);
        break;
    case IOCTL_ERASE_CHIP:
        err = _norflash_eraser(FLASH_CHIP_ERASER, 0);
        break;
    case IOCTL_FLUSH:
        break;
    case IOCTL_CMD_RESUME:
        break;
    case IOCTL_CMD_SUSPEND:
        break;
    default:
        err = -EINVAL;
        break;
    }
    return err;
}


/*************************************************************************************
 *                                  挂钩 device_api
 ************************************************************************************/

static OS_MUTEX flash_mutex;
static struct device norflash_dev;

static int norflash_dev_init(const struct dev_node *node, void *arg)
{
    os_mutex_create(&flash_mutex);
    return _norflash_init((struct norflash_dev_platform_data *)arg);
}
static int norflash_dev_open(const char *name, struct device **device, void *arg)
{
    int reg;
    *device = &norflash_dev;
    if (atomic_read(&norflash_dev.ref)) {
        y_printf("\n >>>[test]:func = %s,line= %d\n",__FUNCTION__, __LINE__);
        return 0;
    }
    __asm__ volatile("csync");
    os_mutex_pend(&flash_mutex, 0);
    reg = _norflash_open();
    os_mutex_post(&flash_mutex);
    __asm__ volatile("csync");
    return reg;
}
static int norflash_dev_close(struct device *device)
{
    int reg;
    __asm__ volatile("csync");
    os_mutex_pend(&flash_mutex, 0);
    reg = _norflash_close();
    os_mutex_post(&flash_mutex);
    __asm__ volatile("csync");
    return reg;
}
static int norflash_dev_read(struct device *device, void *buf, u32 len, u32 offset)
{
    r_printf("flash read sector = %d, num = %d\n", offset, len);
    int reg;
    __asm__ volatile("csync");
    os_mutex_pend(&flash_mutex, 0);
    reg = _norflash_read(offset * 512, buf, len * 512);
    if (reg) {
        r_printf(">>>[r error]:\n");
        len = 0;
    }
    os_mutex_post(&flash_mutex);
    __asm__ volatile("csync");
    return len;
}
static int norflash_dev_write(struct device *device, void *buf, u32 len, u32 offset)
{
    r_printf("flash write sector = %d, num = %d\n", offset, len);
    int reg;
    __asm__ volatile("csync");
    os_mutex_pend(&flash_mutex, 0);
    reg = _norflash_write(offset * 512, buf, len * 512);
    if (reg) {
        r_printf(">>>[w error]:\n");
        len = 0;
    }
    os_mutex_post(&flash_mutex);
    __asm__ volatile("csync");
    return len;
}
static bool norflash_dev_online(const struct dev_node *node)
{
    return 1;
}
static int norflash_dev_ioctl(struct device *device, u32 cmd, u32 arg)
{
    int reg;
    __asm__ volatile("csync");
    os_mutex_pend(&flash_mutex, 0);
    reg = _norflash_ioctl(cmd, arg);
    os_mutex_post(&flash_mutex);
    __asm__ volatile("csync");
    return reg;
}

int hook_norflash_init(struct norflash_dev_platform_data *pdata)
{
    return _norflash_init(&pdata);
}

int hook_norflash_open(void)
{
    return  _norflash_open();
}

int hook_norflash_spirec_read(u8 *buf, u32 addr, u32 len)
{
    return _norflash_read( addr, buf,  len);
}

int hook_norflash_spirec_write(u8 *buf, u32 addr, u32 len)
{
    return _norflash_write_pages( addr,  buf, len);
    /* return _norflash_write( addr, buf,  len); */
}

void hook_norflash_spirec_eraser(u32 addr)
{
    _norflash_eraser(FLASH_SECTOR_ERASER , addr);
}


const struct device_operations norflash_dev_ops = {
    .init   = norflash_dev_init,
    .online = norflash_dev_online,
    .open   = norflash_dev_open,
    .read   = norflash_dev_read,
    .write  = norflash_dev_write,
    .ioctl  = norflash_dev_ioctl,
    .close  = norflash_dev_close,
};

#endif
