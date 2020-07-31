
#include "app_config.h"
#include "system/includes.h"
#include "system/os/os_cpu.h"
#include "file_operate/file_operate.h"
#include "file_operate/file_bs_deal.h"
#include "asm/sdmmc.h"
#include "update/update.h"
#include "update/update_loader_download.h"
#include "system/event.h"

#if (defined(CONFIG_FATFS_ENBALE) && CONFIG_FATFS_ENBALE)



#if defined(CONFIG_SD_UPDATE_ENABLE) || defined(CONFIG_USB_UPDATE_ENABLE)
#define STRG_UPDATE_EN		1
#else
#define STRG_UPDATE_EN		0
#endif

#if (defined(TCFG_DEC_FILE_NAME_ENABLE) && TCFG_DEC_FILE_NAME_ENABLE)
#define FILE_LFN_EN			1
#define FILE_LDN_EN			1
#endif

#if (defined(TCFG_DEV_PREPROCESSOR_ENABLE) && TCFG_DEV_PREPROCESSOR_ENABLE)
#define DEV_STR_HEAD		TCFG_DEV_PREPROCESSOR_HEAD
#else
#define DEV_STR_HEAD		""
#endif

#define FILE_SCAN_ENTER()	sys_key_event_disable()
#define FILE_SCAN_EXIT()	sys_key_event_enable()

#define LOG_TAG_CONST       APP_FILE_OPERATE
#define LOG_TAG             "[APP_FILE_OPERATE]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

struct file_storage_dev {
    u8 online;
#if STRG_UPDATE_EN
    int up_type;
    union {
        UPDATA_SD sd;
    } u;
#endif
};

#if TCFG_SD0_ENABLE
static struct file_storage_dev file_strg0 = {
    .up_type = SD0_UPDATA,
    .u.sd.control_type = SD_CONTROLLER_0,
#if (TCFG_SD0_PORTS=='A')
    .u.sd.control_io = SD0_IO_A,
#elif (TCFG_SD0_PORTS=='B')
    .u.sd.control_io = SD0_IO_B,
#elif (TCFG_SD0_PORTS=='C')
    .u.sd.control_io = SD0_IO_C,
#elif (TCFG_SD0_PORTS=='D')
    .u.sd.control_io = SD0_IO_D,
#elif (TCFG_SD0_PORTS=='E')
    .u.sd.control_io = SD0_IO_E,
#elif (TCFG_SD0_PORTS=='F')
    .u.sd.control_io = SD0_IO_F,
#endif
    .u.sd.power = 1,
};
static u32 sd0_time_stemp = 0;
#endif

#if TCFG_SD1_ENABLE
static struct file_storage_dev file_strg1 = {
    .up_type = SD1_UPDATA,
    .u.sd.control_type = SD_CONTROLLER_1,
#if (TCFG_SD1_PORTS=='A')
    .u.sd.control_io = SD1_IO_A,
#else
    .u.sd.control_io = SD1_IO_B,
#endif
    .u.sd.power = 1,
};
static u32 sd1_time_stemp = 0;
#endif

#if TCFG_UDISK_ENABLE
static struct file_storage_dev file_strg_udisk = {
    .up_type = USB_UPDATA,
};
static u32 udisk_time_stemp = 0;
#endif

#if SDFILE_STORAGE && (defined(TCFG_CODE_FLASH_ENABLE) && TCFG_CODE_FLASH_ENABLE)
static struct file_storage_dev file_flash = {
    .online = 1,
};
static u32 flash_time_stemp = 0;
#endif

#if TCFG_NORFLASH_DEV_ENABLE
static struct file_storage_dev file_strg_norflash = {
};
static u32 norflash_time_stemp = 0;
#endif

#if TCFG_NOR_FS_ENABLE
static struct file_storage_dev file_nor_fs = {};
static u32 nor_fs_time_stemp = 0;
#endif 

REGISTER_STORAGE_DEVICES(stroage_dev) = {
#if SDFILE_STORAGE && (defined(TCFG_CODE_FLASH_ENABLE) && TCFG_CODE_FLASH_ENABLE)
    {SDFILE_DEV, NULL, SDFILE_MOUNT_PATH, SDFILE_RES_ROOT_PATH, "sdfile", &file_flash, &flash_time_stemp},
#endif
#if TCFG_SD0_ENABLE
    {"sd0", DEV_STR_HEAD"sd0", "storage/sd0", "storage/sd0/C/", "fat", &file_strg0, &sd0_time_stemp},
#endif
#if TCFG_SD1_ENABLE
    {"sd1", DEV_STR_HEAD"sd1", "storage/sd1", "storage/sd1/C/", "fat", &file_strg1, &sd1_time_stemp},
#endif
#if TCFG_UDISK_ENABLE
    {"udisk", DEV_STR_HEAD"udisk", "storage/udisk", "storage/udisk/C/", "fat", &file_strg_udisk, &udisk_time_stemp},
#endif

#if TCFG_NORFLASH_DEV_ENABLE
    {"norflash", DEV_STR_HEAD"norflash", "storage/norflash", "storage/norflash/C/", "fat", &file_strg_norflash, &norflash_time_stemp},
    {"nor_tone", DEV_STR_HEAD"norflash", "storage/nor_tone", "storage/nor_tone/C/", "nor_sdfile", &file_strg_norflash, &norflash_time_stemp},
#endif

#if TCFG_NOR_FS_ENABLE
    {"nor_fs", DEV_STR_HEAD"nor_fs", "storage/nor_fs", "storage/nor_fs/C/", "nor_fs", &file_nor_fs, &nor_fs_time_stemp},
#endif 

};

extern const struct storage_dev storage_device_begin[];
extern const struct storage_dev storage_device_end[];
#define list_for_each_device(p) \
    for (p = storage_device_begin; p < storage_device_end; p++)



static volatile u8 ff_scan_flag = 0;
int ff_scan_notify(void)
{
    if (ff_scan_flag) {
        printf("--------------------------------------ff scan cancle done!!!\n");
        return -1;
    }
    return 0;
}

void ff_scan_notify_cancle(void)
{
    ff_scan_flag = 1;
    printf("++++++++++++++++++++++++++++++++++++++++++ff scan cancle!!!\n");
}

#if (defined(TCFG_DEC_FILE_NAME_ENABLE) && TCFG_DEC_FILE_NAME_ENABLE)
#if FILE_LFN_EN
static u8 lfn_buf[512];
#endif
#if FILE_LDN_EN
static u8 ldn_buf[512];
#endif
FS_DISP_INFO fil_disp_info;
#endif

FS_DISP_INFO *file_opr_get_disp_info(void)
{
#if (defined(TCFG_DEC_FILE_NAME_ENABLE) && TCFG_DEC_FILE_NAME_ENABLE)
    return &fil_disp_info;
#else
    return NULL;
#endif
}

#if STRG_UPDATE_EN
extern void update_mode_api(UPDATA_TYPE up_type, ...);
extern const char updata_file_name[];
static u8 file_update_path[sizeof("storage/udisk/C/") + 8 + 1 + 3 + 1];
static struct storage_dev *file_update_cur_dev = NULL;
static void file_update_callback(void *priv, u8 type, u8 cmd)
{
    file_update_cur_dev = priv;
    log_info(" update cmd:%d \n", cmd);
    if (cmd == UPDATE_LOADER_OK) {
        update_mode_api(type);
    }
}
void *sdmmc_get_update_parm(void)
{
    if (!file_update_cur_dev) {
        return NULL;
    }
    struct file_storage_dev *info = (struct file_storage_dev *)file_update_cur_dev->priv;
    return (void *)&info->u.sd;
}
#else
void *sdmmc_get_update_parm(void)
{
    return NULL;
}
#endif

static int _file_storage_cb_add(struct storage_dev *p)
{
    struct file_storage_dev *info = (struct file_storage_dev *)p->priv;
    info->online = 1;
    return 0;
}
static int _file_storage_cb_check(struct storage_dev *p)
{
    struct file_storage_dev *info = (struct file_storage_dev *)p->priv;
    if (!info->online) {
        return -1;
    }
    return 0;
}

int file_opr_dev_add(void *logo)
{
    const struct storage_dev *p;
    list_for_each_device(p) {
        r_printf(">>>[test]:p->logo = %s, logo = %s\n", p->logo , logo);
        if (!strcmp(p->logo, logo)) {
            void *fmnt = mount(p->dev_name, p->storage_path, p->fs_type, 3, NULL);
            if (!fmnt) {
                return STORAGE_DEV_FAT_MOUNT_FAIL;
            } else {
                return storage_dev_add_ex(p->logo, _file_storage_cb_add);
            }
        }
    }
    return STORAGE_DEV_NO_FIND;
}
int file_opr_dev_del(void *logo)
{
    const struct storage_dev *p;
    list_for_each_device(p) {
        if (!strcmp(p->logo, logo)) {
            unmount(p->storage_path);
        }
    }
    int ret = storage_dev_del(logo);
    return ret;
}
struct storage_dev *file_opr_dev_check(void *logo)
{
    return storage_dev_check_ex(logo, NULL);
}

struct storage_dev *file_opr_dev_get_last_active(void)
{
    return storage_dev_last_active_ex(NULL);
}


int file_opr_dev_total(void)
{
    return storage_dev_total_ex(NULL);
}


struct storage_dev *file_opr_available_dev_check(void *logo)
{
    return storage_dev_check_ex(logo, _file_storage_cb_check);
}

struct storage_dev *file_opr_available_dev_get_last_active(void)
{
    return storage_dev_last_active_ex(_file_storage_cb_check);
}


int file_opr_available_dev_total(void)
{
    return storage_dev_total_ex(_file_storage_cb_check);
}

int file_opr_available_dev_offline(struct storage_dev *p)
{
    if (!p) {
        return 0;
    }

    struct file_storage_dev *info = (struct file_storage_dev *)p->priv;
    info->online = 0;
    return 0;
}

int file_opr_available_dev_online(struct storage_dev *p)
{
    if (!p) {
        return 0;
    }

    struct file_storage_dev *info = (struct file_storage_dev *)p->priv;
    info->online = 1;
    return 0;
}

int file_opr_api_sel_dev(FILE_OPERATE *fopr, FILE_OPR_SEL_DEV *sel)
{
    if ((!fopr) || (!sel)) {
        return FOPR_ERR_POINT;
    }

    struct storage_dev *dev = NULL;
    struct storage_dev *cur = (struct storage_dev *)sel->sel_param;
    if (!cur) {
        cur = fopr->dev;
    }

    switch (sel->dev_sel) {
    case DEV_SEL_CUR:
        dev = cur;//(struct storage_dev *)sel->sel_param;
        break;
    case DEV_SEL_NEXT:
        dev = storage_dev_next_ex(cur, _file_storage_cb_check);
        break;
    case DEV_SEL_PREV:
        dev = storage_dev_prev_ex(cur, _file_storage_cb_check);
        break;
    case DEV_SEL_FIRST:
        dev = storage_dev_frist_ex(_file_storage_cb_check);
        break;
    case DEV_SEL_LAST:
        dev = storage_dev_last_ex(_file_storage_cb_check);
        break;
    case DEV_SEL_SPEC:
        dev = storage_dev_check_ex((void *)sel->sel_param, _file_storage_cb_check);
        break;
    case DEV_SEL_LAST_ACTIVE:
        printf("================================================================DEV_SEL_LAST_ACTIVE\n");
        dev = storage_dev_last_active_ex(_file_storage_cb_check);
        break;
    }
    if (!dev) {
        log_error("dev err \n");
        return FOPR_ERR_DEV_NO_FIND;
    }

    if (fopr->dev != dev) {
        if (fopr->file) {
            fclose(fopr->file);
            fopr->file = NULL;
        }
        if (fopr->fsn) {
            fscan_release(fopr->fsn);
            fopr->fsn = NULL;
        }
        if (fopr->fsn_path) {
            free(fopr->fsn_path);
            fopr->fsn_path = NULL;
        }

#if TCFG_USB_DM_MULTIPLEX_WITH_SD_DAT0
        extern int dev_sd_change_usb();
        extern int dev_usb_change_sd();
        if (fopr->dev) {
            printf("<<<<<<<<<<<<<<<<<<<<<<<<%s \n", fopr->dev->logo);
        }
        if (dev) {
            printf(">>>>>>>>>>>>>>>>>>>>>>%s \n", dev->logo);
        }
#if 1
        if (!strcmp(dev->logo, "sd0")) {
            dev_usb_change_sd();
        } else if (!strcmp(dev->logo, "sd1")) {
            dev_usb_change_sd();
        } else if (!strcmp(dev->logo, "udisk")) {
            dev_sd_change_usb();
        }
#endif
#endif

        /* if (fopr->dev) { */
        /* unmount(fopr->dev->storage_path); */
        /* fopr->dev = NULL; */
        /* } */

        fopr->dev = dev;
        /* void *fmnt = mount(dev->dev_name, dev->storage_path, dev->fs_type, 3, NULL); */
        /* if (!fmnt) { */
        /* log_error("mount err \n"); */
        /* return FOPR_ERR_DEV_MOUNT; */
        /* } */
    }

    return FOPR_OK;
}

int file_opr_api_scan_init(FILE_OPERATE *fopr, FILE_OPR_SEL_FILE *sel)
{
    if ((!fopr) || (!fopr->dev) || (!sel)) {
        return FOPR_ERR_POINT;
    }
    if (sel->scan_type) {
        if (fopr->file) {
            fclose(fopr->file);
            fopr->file = NULL;
        }
        if (fopr->fsn) {
            fscan_release(fopr->fsn);
            fopr->fsn = NULL;
        }

        int root_len = strlen(fopr->dev->root_path);
        char *fsn_path = zalloc(root_len + sel->path_len + 1);
        if (fsn_path == NULL) {
            log_error("malloc err \n");
            return FOPR_ERR_MALLOC;
        }

        char *path = sel->path;
        if (path && (*path == '/')) {
            path++;
            if (sel->path_len) {
                sel->path_len -= 1;
            }
        }
        memcpy(fsn_path, fopr->dev->root_path, root_len);
        memcpy(&fsn_path[root_len], path, sel->path_len);

        if (fopr->fsn_path) {
            if (0 == strcmp(fsn_path, fopr->fsn_path)) {
                free(fsn_path);
            } else {
                free(fopr->fsn_path);
                fopr->fsn_path = fsn_path;
            }
        } else {
            fopr->fsn_path = fsn_path;
        }

        ff_scan_flag = 0;
        FILE_SCAN_ENTER();
        fopr->fsn = fscan(fopr->fsn_path, sel->scan_type, 9);
        FILE_SCAN_EXIT();
        if (!fopr->fsn) {
            log_error("fscan err \n");
            return FOPR_ERR_FSCAN;
        }

        fopr->fsn->cycle_mode = sel->cycle_mode;
        fopr->totalfile = fopr->fsn->file_number;

        if (!fopr->totalfile) {
            log_error("total err \n");
#if STRG_UPDATE_EN
            char *updata_file = updata_file_name;
            if (*updata_file == '/') {
                updata_file ++;
            }
            file_update_path[0] = 0;
            strcpy(file_update_path, fopr->dev->root_path);
            strcat(file_update_path, updata_file);
            log_info("update file: %s ", file_update_path);
            file_update_cur_dev = fopr->dev;
            struct file_storage_dev *info = (struct file_storage_dev *)file_update_cur_dev->priv;
            storage_update_loader_download_init(info->up_type, file_update_path, file_update_callback, fopr->dev, 0);
#endif
            return FOPR_ERR_NO_FILE;
        }

#if (defined(TCFG_DEC_FILE_NAME_ENABLE) && TCFG_DEC_FILE_NAME_ENABLE)
#if FILE_LFN_EN
        fset_lfn_buf(fopr->fsn, lfn_buf);
#endif
#if FILE_LDN_EN
        fset_ldn_buf(fopr->fsn, ldn_buf);
#endif
#endif

        switch (sel->file_sel) {
        case FSEL_NEXT_FILE:
        case FSEL_PREV_FILE:
        case FSEL_AUTO_FILE:
            sel->file_sel = FSEL_FIRST_FILE;
            break;
        }
    }
    return 0;
}

int file_opr_api_sel_file(FILE_OPERATE *fopr, FILE_OPR_SEL_FILE *sel)
{
    if ((!fopr) || (!fopr->dev) || (!sel)) {
        return FOPR_ERR_POINT;
    }
    int ret = file_opr_api_scan_init(fopr, sel);
    if (ret) {
        return ret;
    }
    if (!fopr->fsn) {
        log_error("fscan err \n");
        return FOPR_ERR_FSCAN;
    }
    if (fopr->file) {
        fclose(fopr->file);
        fopr->file = NULL;
    }
    if (!fopr->fsn->file_counter) {
        switch (sel->file_sel) {
        case FSEL_NEXT_FILE:
        case FSEL_PREV_FILE:
        case FSEL_AUTO_FILE:
            sel->file_sel = FSEL_FIRST_FILE;
            break;
        }
    }


    ff_scan_flag = 0;
    FILE_SCAN_ENTER();
    fopr->file = fselect(fopr->fsn, sel->file_sel, sel->sel_param);
    FILE_SCAN_EXIT();


    if (!fopr->file) {
        log_error("file sel err \n");
        return FOPR_ERR_FSEL;
    }

#if (defined(TCFG_DEC_FILE_NAME_ENABLE) && TCFG_DEC_FILE_NAME_ENABLE)
    memset(&fil_disp_info, 0, sizeof(FS_DISP_INFO));
    fget_disp_info(fopr->file, &fil_disp_info);
    file_comm_change_display_name(fil_disp_info.tpath, &fil_disp_info.file_name, &fil_disp_info.dir_name);
    fil_disp_info.update_flag = 1;
#endif

#if 1
    log_info("filecnt:%d \n", fopr->fsn->file_counter);

    u8 name[128];
    memset(name, 0, sizeof(name));
    fget_name(fopr->file, name, sizeof(name));
    log_info("file name: %s \n", name);

    struct ffolder folder = {0};
    fget_folder(fopr->fsn, &folder);
    log_info("folder start:%d, in:%d \n", folder.fileStart, folder.fileTotal);
#endif

    return FOPR_OK;
}

int file_opr_api_set_cycle_mode(FILE_OPERATE *fopr, u8 cycle_mode)
{
    if ((!fopr) || (!fopr->fsn)) {
        return FOPR_ERR_POINT;
    }
    fopr->fsn->cycle_mode = cycle_mode;
    return FOPR_OK;
}

FILE_OPERATE *file_opr_api_create(void)
{
    FILE_OPERATE *fopr = zalloc(sizeof(FILE_OPERATE));
    if (!fopr) {
        return NULL;
    }

    return fopr;
}

void file_opr_api_release(FILE_OPERATE *fopr)
{
    if (fopr) {
        if (fopr->file) {
            fclose(fopr->file);
            fopr->file = NULL;
        }
        if (fopr->fsn) {
            fscan_release(fopr->fsn);
            fopr->fsn = NULL;
        }
        if (fopr->fsn_path) {
            free(fopr->fsn_path);
            fopr->fsn_path = NULL;
        }
        /* if (fopr->dev) { */
        /* unmount(fopr->dev->storage_path); */
        /* fopr->dev = NULL; */
        /* } */
        free(fopr);
    }
}


struct evt2dev {
    u32 evt;
    const char *logo;
};

static const struct evt2dev evt2dev_map[] = {
    {DRIVER_EVENT_FROM_SD0, 		"sd0"},
    {DRIVER_EVENT_FROM_SD1, 		"sd1"},
    {DRIVER_EVENT_FROM_SD2, 		"sd2"},
    {DEVICE_EVENT_FROM_USB_HOST, 	"udisk"},
};


const char *evt2dev_map_logo(u32 evt)
{
    for (int i = 0; i < (sizeof(evt2dev_map) / sizeof(evt2dev_map[0])); i++) {
        if (evt2dev_map[i].evt == evt) {
            return evt2dev_map[i].logo;
        }
    }

    return NULL;
}


void file_opr_api_set_recplay_status(FILE_OPERATE *fopr, u8 status)
{
    if (fopr) {
        if (status) {
            fopr->is_recfolder = 1;
        } else {
            fopr->is_recfolder = 0;
        }
    }
}

u8 file_opr_api_get_recplay_status(FILE_OPERATE *fopr)
{
    if (fopr) {
        return fopr->is_recfolder;
    }

    return 0;
}

void file_opr_api_set_sel_status(FILE_OPERATE *fopr, u8 status)
{
    if (fopr) {
        if (status) {
            fopr->sel_flag = 1;
        } else {
            fopr->sel_flag = 0;
        }
    }
}

u8 file_opr_api_get_sel_status(FILE_OPERATE *fopr)
{
    if (fopr) {
        return fopr->sel_flag;
    }

    return 0;
}

#endif//(defined(CONFIG_FATFS_ENBALE) && CONFIG_FATFS_ENBALE)
