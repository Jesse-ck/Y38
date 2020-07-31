#ifndef __FS_H__
#define __FS_H__



#include "generic/typedef.h"
#include "generic/list.h"
#include "generic/ioctl.h"
#include "generic/atomic.h"
#include "system/task.h"
#include "system/malloc.h"
#include "system/sys_time.h"
#include "stdarg.h"
#include "fs_file_name.h"

#define SEEK_SET	0	/* Seek from beginning of file.  */
#define SEEK_CUR	1	/* Seek from current position.  */
#define SEEK_END	2	/* Seek from end of file.  */

#include "sdfile.h"

#define F_ATTR_RO       0x01
#define F_ATTR_ARC      0x02
#define F_ATTR_DIR      0x04
#define F_ATTR_VOL      0x08

#if (VFS_ENABLE == 1)

#ifndef FSELECT_MODE
#define FSELECT_MODE
#define    FSEL_FIRST_FILE     			 0
#define    FSEL_LAST_FILE      			 1
#define    FSEL_NEXT_FILE      			 2
#define    FSEL_PREV_FILE      			 3
#define    FSEL_CURR_FILE      			 4
#define    FSEL_BY_NUMBER      			 5
#define    FSEL_BY_SCLUST      			 6
#define    FSEL_AUTO_FILE      			 7
#define    FSEL_NEXT_FOLDER_FILE       	 8
#define    FSEL_PREV_FOLDER_FILE         9
#define    FSEL_BY_PATH                 10
#endif

#ifndef FCYCLE_MODE
#define FCYCLE_MODE
#define    FCYCLE_LIST			0
#define    FCYCLE_ALL			1
#define    FCYCLE_ONE			2
#define    FCYCLE_FOLDER		3
#define    FCYCLE_RANDOM		4
#define    FCYCLE_MAX			5
#endif

enum {
    FS_IOCTL_GET_FILE_NUM,
    FS_IOCTL_FILE_CHECK,
    FS_IOCTL_GET_ERR_CODE,
    FS_IOCTL_FREE_CACHE,
    FS_IOCTL_SET_NAME_FILTER,
    FS_IOCTL_GET_FOLDER_INFO,
    FS_IOCTL_SET_LFN_BUF,	// 512
    FS_IOCTL_SET_LDN_BUF,	// 512

    FS_IOCTL_SET_EXT_TYPE,
    FS_IOCTL_OPEN_DIR,
    FS_IOCTL_EXIT_DIR,
    FS_IOCTL_GET_DIR_INFO,

    FS_IOCTL_GETFILE_BYNAME_INDIR,

    FS_IOCTL_GET_DISP_INFO,

    FS_IOCTL_MK_DIR,
    FS_IOCTL_GET_ENCFOLDER_INFO,
};


struct vfs_devinfo;
struct vfscan;
struct vfs_operations;



struct vfs_devinfo {
    void *fd;
    u32 sector_size;
    void *private_data;
};

#define VFS_PART_DIR_MAX 16


struct vfs_partition {
    struct vfs_partition *next;
    u32 offset;
    u32 clust_size;
    u32 total_size;
    u8 fs_attr;
    char dir[VFS_PART_DIR_MAX];
    void *private_data;
};

struct fiter {
    u32 index;
};

struct ffolder {
    u16 fileStart;
    u16 fileTotal;
};



struct imount {
    int fd;
    const char *path;
    struct vfs_operations *ops;
    struct vfs_devinfo dev;
    struct vfs_partition part;
    struct list_head entry;
    atomic_t ref;
    OS_MUTEX mutex;
    u8 avaliable;
    u8 part_num;
};

struct vfs_attr {
    u8 attr;
    u32 fsize;
    u32 sclust;
    struct sys_time crt_time;
    struct sys_time wrt_time;
};

typedef struct {
    struct imount *mt;
    struct vfs_devinfo *dev;
    struct vfs_partition *part;
    void *private_data;
} FILE;


struct vfscan {
    u8 scan_file;
    u8 subpath;
    u8 scan_dir;
    u8 attr;
    u8 cycle_mode;
    char sort;
    char ftype[12 * 3 + 1];
    u16 file_number;
    u16 file_counter;

    u16  dir_totalnumber;          // 文件夹总数
    u16  musicdir_counter;          // 播放文件所在文件夹序号

    void *priv;
    struct vfs_devinfo *dev;
    struct vfs_partition *part;
    char  filt_dir[12];
};


struct vfs_operations {
    const char *fs_type;
    int (*mount)(struct imount *, int);
    int (*unmount)(struct imount *);
    int (*format)(struct vfs_devinfo *, struct vfs_partition *);
    int (*fset_vol)(struct vfs_partition *, const char *name);
    int (*fget_free_space)(struct vfs_devinfo *, struct vfs_partition *, u32 *space);
    int (*fopen)(FILE *, const char *path, const char *mode);
    int (*fread)(FILE *, void *buf, u32 len);
    int (*fwrite)(FILE *, void *buf, u32 len);
    int (*fseek)(FILE *, int offset, int);
    int (*flen)(FILE *);
    int (*fpos)(FILE *);
    int (*fcopy)(FILE *, FILE *);
    int (*fget_name)(FILE *, u8 *name, int len);
    int (*frename)(FILE *, const char *path);
    int (*fclose)(FILE *);
    int (*fdelete)(FILE *);
    int (*fscan)(struct vfscan *, const char *path, u8 max_deepth);
    void (*fscan_release)(struct vfscan *);
    int (*fsel)(struct vfscan *, int sel_mode, FILE *, int);
    int (*fget_attr)(FILE *, int *attr);
    int (*fset_attr)(FILE *, int attr);
    int (*fget_attrs)(FILE *, struct vfs_attr *);
    int (*fmove)(FILE *file, const char *path_dst, FILE *, int clr_attr);
    int (*ioctl)(void *, int cmd, int arg);
};

#define REGISTER_VFS_OPERATIONS(ops) \
	static const struct vfs_operations ops sec(.vfs_operations)


static inline struct vfs_partition *vfs_partition_next(struct vfs_partition *p)
{
    struct vfs_partition *n = (struct vfs_partition *)zalloc(sizeof(*n));

    if (n) {
        p->next = n;
    }
    return n;
}


static inline void vfs_partition_free(struct vfs_partition *p)
{
    struct vfs_partition *n = p->next;

    while (n) {
        p = n->next;
        free(n);
        n = p;
    }
}


struct imount *mount(const char *dev_name, const char *path, const char *fs_type,
                     int cache_num, void *dev_arg);

int unmount(const char *path);

int f_format(const char *path, const char *fs_type, u32 clust_size);

int f_free_cache(const char *path);

FILE *fopen(const char *path, const char *mode);

int fread(FILE *file, void *buf, u32 len);

int fwrite(FILE *file, void *buf, u32 len);

int fseek(FILE *file, int offset, int orig);

int flen(FILE *file);

int fpos(FILE *file);

int fcopy(const char *format, ...);

int fget_name(FILE *file, u8 *name, int len);

int frename(FILE *file, const char *path);

int fclose(FILE *file);

int fdelete(FILE *file);
int fdelete_by_name(const char *fname);

int fget_free_space(const char *path, u32 *space);

/* arg:
 * -t  文件类型
 * -r  包含子目录
 * -d  扫描文件夹
 * -a  文件属性 r: 读， /: 非
 * -s  排序方式， t:按时间排序， n:按文件号排序
 */
struct vfscan *fscan(const char *path, const char *arg, u8 max_deepth);

void fscan_release(struct vfscan *fs);

FILE *fselect(struct vfscan *fs, int selt_mode, int arg);

int fdir_exist(const char *dir);

int fdir(FILE *file, const char *arg, char *name, int len, struct fiter *iter);

int fget_attr(FILE *file, int *attr);

int fset_attr(FILE *file, int attr);

int fget_attrs(FILE *file, struct vfs_attr *attr);

struct vfs_partition *fget_partition(const char *path);

int fset_vol(const char *path, const char *name);

int fmove(FILE *file, const char *path_dst, FILE **newFile, int clr_attr);

int fcheck(FILE *file);

int fget_err_code(const char *path);

int fset_name_filter(const char *path, void *name_filter);

int fget_folder(struct vfscan *fs, struct ffolder *arg);

int fset_lfn_buf(struct vfscan *fs, void *arg);
int fset_ldn_buf(struct vfscan *fs, void *arg);

int fset_ext_type(const char *path, void *ext_type);
int fopen_dir_info(const char *path, FILE **pp_file, void *dir_dj);
int fexit_dir_info(FILE *file);
int fget_dir_info(FILE *file, u32 start_num, u32 total_num, void *buf_info);

int fget_file_byname_indir(FILE *file, FILE **newFile, void *ext_name);

int fget_disp_info(FILE *file, void *arg);

int fmk_dir(const char *path, char *folder, u8 mode);

int fget_encfolder_info(const char *path, char *folder, char *ext, u32 *last_num, u32 *total_num);

int fname_to_path(char *result, const char *path, const char *fname, int len);

#endif  /* VFS_ENABLE */

#endif  /* __FS_H__ */






