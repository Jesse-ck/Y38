#include "app_config.h"
#if RCSP_FILE_OPT

#include "rcsp_bs_api.h"
#include "file_operate/file_bs_deal.h"
#include "rcsp_bluetooth.h"
#include "rcsp_msg.h"
#include "rcsp_bluetooth.h"
#include "string.h"
#include "JL_rcsp_api.h"
#include "JL_rcsp_protocol.h"

#define MAX_DEEPTH 		9		/* 0~9 deepth of system */
enum {
    BS_UDISK = 0,
    BS_SD0,
    BS_SD1,
    BS_FLASH,
    BS_DEV_MAX,
};

static const char *dev_logo[] = {
    [BS_UDISK] = "udisk",
    [BS_SD0] = "sd0",
    [BS_SD1] = "sd1",
    [BS_FLASH] = "flash",
};


#pragma pack(1)
struct JL_FILE_PATH {
    u8 path_type;///
    u8 read_file_num;
    u16 start_num;
    u32 dev_handle;
    u16 path_len;
    u32 path_clust[MAX_DEEPTH];
};

struct JL_FILE_DATA {
    u8 type : 1;
    u8 format : 1;
    u8 device : 2;
    u8 reserve : 4;

    u32 clust;
    u16 file_num;
    u8 name_len;
    u8 file_data[0];
};
#pragma pack()

const char dec_file_ext[][3] = {
#if (defined(TCFG_DEC_MP3_ENABLE) && (TCFG_DEC_MP3_ENABLE))
    {"MP1"},
    {"MP2"},
    {"MP3"},
#endif

#if (defined(TCFG_DEC_WMA_ENABLE) && (TCFG_DEC_WMA_ENABLE))
    {"WMA"},
#endif

#if (defined(TCFG_DEC_WAV_ENABLE) && (TCFG_DEC_WAV_ENABLE)) || (defined(TCFG_DEC_DTS_ENABLE) && (TCFG_DEC_DTS_ENABLE))
    {"WAV"},
#endif

#if (defined(TCFG_DEC_FLAC_ENABLE) && (TCFG_DEC_FLAC_ENABLE))
    {"FLA"},
#endif

#if (defined(TCFG_DEC_APE_ENABLE) && (TCFG_DEC_APE_ENABLE))
    {"APE"},
#endif

#if (defined(TCFG_DEC_DECRYPT_ENABLE) && (TCFG_DEC_DECRYPT_ENABLE))
    {"SMP"},
#endif

#if (defined(TCFG_DEC_AMR_ENABLE) && (TCFG_DEC_AMR_ENABLE))
    {"AMR"},
#endif

#if (defined(TCFG_DEC_M4A_ENABLE) && (TCFG_DEC_M4A_ENABLE))
    {"M4A"},
    {"MP4"},
    {"AAC"},
#endif
    {'\0'},
};

char *rcsp_bs_file_ext(void)
{
    return (char *)dec_file_ext;
}

u16 rcsp_bs_file_ext_size(void)
{
    return strlen((const char *)dec_file_ext);
}


bool rcsp_bs_start_check(u8 *data, u16 len)
{
    return true;
}



static bool get_path_dir_info(FILE_BS_DEAL *fil_bs, u8 *path_buf, u16 len, u8 type, void *ptr)
{
    s16 ret = 0;
    u16 i, j = 0;
    u32 deep_clust = 0;
    FS_DIR_INFO dir_info;

    printf("path len:%d\n", len);
    put_buf(path_buf, len);

    //open root

    ret = file_bs_entern_dir(fil_bs, NULL);
    if (ret == 0) {
        return true;
    }

    if (len == 4 && type == BS_FLODER) {
        printf("root deep\n");
        if (ptr) {
            *((u32 *)ptr) = ret;
        }
        return true;
    } else if (len == 4 && type == BS_FILE) {
        printf("play file path\n");
        memcpy((u8 *)&deep_clust, path_buf, 4);
        deep_clust = app_ntohl(deep_clust);
        if (ptr) {
            *((u32 *)ptr) = deep_clust;
        }
        return true;
    }


    //deep check
    u8 deep = len / 4;
    if (deep >  MAX_DEEPTH) {
        printf("deep err : %d\n", deep);
        return false;
    }
    printf("get deep%d data\n", deep - 1);

    for (i = 1; i < deep; i++) {
        memcpy((u8 *)&deep_clust, path_buf + 4 * i, 4);
        deep_clust = app_ntohl(deep_clust);
        printf("deep_clust:%x\n", deep_clust);
        for (j = 1; j < ret + 1; j++) {
            /* file_browse_get_dir(obj, (void *)&dir_info, j, 1); */
            file_bs_get_dir_info(fil_bs, &dir_info, j, 1);
            if (dir_info.sclust == deep_clust) {
                break;
            }
        }

        if (i < deep - 1) {
            ret = file_bs_entern_dir(fil_bs, &dir_info);
            if (ret == 0) {
                return true;
            }
        }
    }

    if (type == BS_FILE) {
        if (ptr) {
            *((u32 *)ptr) = dir_info.sclust;    //file return clust
        }
        return true;
    } else {
        ret = file_bs_entern_dir(fil_bs, &dir_info);
        if (ret == 0) {
            return true;
        }

        if (ptr) {
            *((u32 *)ptr) = ret;    //file return clust
        }

        return true;
    }
    return true;
}

u32 file_browse_open_file_path(FILE_BS_DEAL *fil_bs, u8 *path_buf, u16 len)
{
    u32 file_clust = 0;
    get_path_dir_info(fil_bs, path_buf, len, BS_FILE, (void *)&file_clust);
    return file_clust;
}

u32 file_browse_open_dir_path(FILE_BS_DEAL *fil_bs, u8 *path_buf, u16 len)
{
    u32 file_cnt = 0;
    get_path_dir_info(fil_bs, path_buf, len, BS_FLODER, (void *)&file_cnt);
    return file_cnt;
}

void file_printf_dir(FS_DIR_INFO *dir_inf, u8 cnt)
{
    u8 i;
    LONG_FILE_NAME *l_name_pt;

    for (i = 0; i < cnt; i++) {
        printf("file type %d nt:%d clust %x \n", dir_inf[i].dir_type, dir_inf[i].fn_type, dir_inf[i].sclust);
        l_name_pt = &dir_inf[i].lfn_buf;
        if (dir_inf[i].fn_type == BS_FNAME_TYPE_SHORT) {
            file_comm_display_83name((void *)&l_name_pt->lfn[32], (void *)l_name_pt->lfn);
            strcpy(l_name_pt->lfn, &l_name_pt->lfn[32]);
            l_name_pt->lfn_cnt = strlen(l_name_pt->lfn);
            printf("%s\n", l_name_pt->lfn);
        } else {
            if (l_name_pt->lfn_cnt > 510) {
                l_name_pt->lfn_cnt = 510;
                puts("***get long name err!!!");
            }
            l_name_pt->lfn_cnt = file_comm_long_name_fix((void *)l_name_pt->lfn, l_name_pt->lfn_cnt);
            l_name_pt->lfn[l_name_pt->lfn_cnt] = 0;
            l_name_pt->lfn[l_name_pt->lfn_cnt + 1] = 0;

            printf("file name len : %d \n", l_name_pt->lfn_cnt);
            put_buf((u8 *)l_name_pt->lfn, l_name_pt->lfn_cnt);
        }
    }
}
static u16 add_one_iterm_to_sendbuf(u8 *dest, u16 max_buf_len, u16 offset, FS_DIR_INFO *p_dir_info, u16 file_cnt, u8 dev_type)
{
    struct JL_FILE_DATA file_data;
    memset((u8 *)&file_data, 0, sizeof(struct JL_FILE_DATA));

    u16 buf_offset = p_dir_info->lfn_buf.lfn_cnt + sizeof(struct JL_FILE_DATA);

    file_data.type = p_dir_info->dir_type;

    if (p_dir_info->fn_type == BS_FNAME_TYPE_SHORT) {
        file_data.format = BS_ANSI;
    } else {
        file_data.format = BS_UNICODE;
    }

    file_data.device = dev_type;
    file_data.clust = app_htonl(p_dir_info->sclust);
    file_data.file_num = app_htons(file_cnt);
    file_data.name_len = p_dir_info->lfn_buf.lfn_cnt;

    memcpy(dest + offset, (u8 *)&file_data, sizeof(struct JL_FILE_DATA));
    memcpy(dest + offset + sizeof(struct JL_FILE_DATA), (u8 *)p_dir_info->lfn_buf.lfn, p_dir_info->lfn_buf.lfn_cnt);

    /* printf("add send data:"); */
    /* printf_buf(dest+offset,p_dir_info->lfn_buf.lfn_cnt + sizeof(struct JL_FILE_DATA)); */

    return buf_offset;
}


struct JL_FILE_PATH g_path_info;
#define FILE_BROWSE_BUF_LEN    256//JL_MTU_DEFAULT

void rcsp_bs_task_deal(void *p)
{
    struct JL_FILE_PATH *path_info = (struct JL_FILE_PATH *)p;//&g_path_info;//
    u32 play_file_clust = 0;
    u8 path_data[FILE_BROWSE_BUF_LEN];
    FS_DIR_INFO dir_info;
    u16 i;
    u8 reason = 0;

    memset(path_data, 0, FILE_BROWSE_BUF_LEN);
    memset((u8 *)&dir_info, 0, sizeof(FS_DIR_INFO));
    memcpy(path_data, (u8 *)path_info->path_clust, path_info->path_len);

    FS_DIR_INFO *dir_buf = zalloc(sizeof(FS_DIR_INFO) * path_info->read_file_num);
    ASSERT(dir_buf);
    FILE_BS_DEAL *fil_bs = zalloc(sizeof(FILE_BS_DEAL));
    ASSERT(fil_bs);

    printf("dev_logo[path_info->dev_handle] = %d, %s\n", path_info->dev_handle, dev_logo[path_info->dev_handle]);
    fil_bs->dev = storage_dev_check((void *)dev_logo[path_info->dev_handle]);
    //设别有可能不在线
    ASSERT(fil_bs->dev);
    /* void *fmnt = mount(fil_bs->dev->dev_name, fil_bs->dev->storage_path, fil_bs->dev->fs_type, 3, NULL); */
    /* ASSERT(fmnt); */

    file_bs_open_handle(fil_bs, (u8 *)rcsp_bs_file_ext());

    if (path_info->path_type) {
        play_file_clust = file_browse_open_file_path(fil_bs, path_data, path_info->path_len);
        reason = 2;
        printf("play_file_clust = %x\n", play_file_clust);
        goto _EXIT;
    } else {
        u32 dir_file_cnt = file_browse_open_dir_path(fil_bs, path_data, path_info->path_len);
        printf("read file : %d\n", dir_file_cnt);

        if (path_info->start_num  + path_info->read_file_num >= dir_file_cnt) {
            printf("file range err\n");
            reason = 1;
            path_info->read_file_num =  dir_file_cnt - path_info->start_num + 1;
        }
        printf("start num:%d read file num:%d\n", path_info->start_num, path_info->read_file_num);

        u16 offset = 0;
        u32 ret = 0;
        memset(path_data, 0, FILE_BROWSE_BUF_LEN);
        for (i = path_info->start_num ; i < (path_info->start_num + path_info->read_file_num); i++) {
            ret = file_bs_get_dir_info(fil_bs, &dir_info, i, 1);
            if (!ret) {
                break;
            }

            file_printf_dir(&dir_info, 1);
            if ((offset + dir_info.lfn_buf.lfn_cnt + sizeof(struct JL_FILE_DATA)) > FILE_BROWSE_BUF_LEN) {
                //send dir info
                put_buf(path_data, offset);
                ret = JL_DATA_send(JL_OPCODE_DATA, JL_OPCODE_FILE_BROWSE_REQUEST_START, path_data, offset, JL_NOT_NEED_RESPOND);
                if (ret) {
                    printf("send data err: %d\n", ret);
                    goto _EXIT;
                }
                //reset send buf
                offset = 0;
                memset(path_data, 0, FILE_BROWSE_BUF_LEN);
                offset += add_one_iterm_to_sendbuf(path_data, FILE_BROWSE_BUF_LEN, offset, &dir_info, i, path_info->dev_handle);
                printf("reset: %d\n", offset);

            } else {
                offset += add_one_iterm_to_sendbuf(path_data, FILE_BROWSE_BUF_LEN, offset, &dir_info, i, path_info->dev_handle);
                printf("continue : %d\n", offset);
            }
        }

        //send last package
        if (offset) {
            printf("end : %d\n", offset);
            put_buf(path_data, offset);
            ret = JL_DATA_send(JL_OPCODE_DATA, JL_OPCODE_FILE_BROWSE_REQUEST_START, path_data, offset, JL_NOT_NEED_RESPOND);
            if (ret) {
                printf("send data err: %d\n", ret);
                goto _EXIT;
            }
        }
    }
_EXIT:


    file_bs_close_handle(fil_bs);

    /* unmount(fil_bs->dev->storage_path); */

    free(dir_buf);
    free(fil_bs);

    if (path_info) {
        free(path_info);
    }

    rcsp_msg_post(RCSP_MSG_BS_END, 3, reason, (int)dev_logo[path_info->dev_handle], (int)play_file_clust);

    while (1) {
        os_time_dly(10);
    }
}


bool rcsp_bs_start(u8 *data, u16 len)
{
    struct JL_FILE_PATH *path_info = (struct JL_FILE_PATH *)zalloc(sizeof(struct JL_FILE_PATH));//&g_path_info;//
    ASSERT(path_info);
    memset((u8 *)path_info, 0, sizeof(struct JL_FILE_PATH));

    if (len > sizeof(struct JL_FILE_PATH)) {
        return false;
    }

    memcpy((u8 *)path_info, data, sizeof(struct JL_FILE_PATH));
    path_info->start_num = app_ntohs(path_info->start_num);
    path_info->dev_handle = app_ntohl(path_info->dev_handle);
    path_info->path_len = app_ntohs(path_info->path_len);

    if (path_info->dev_handle >= BS_DEV_MAX) {
        printf(">>>>>>>>>>>>>>>>>>>>\n");
        return false;
    }

    if (task_create(rcsp_bs_task_deal, (void *)path_info, "file_bs")) {
        free(path_info);
        return false;
    }

    return true;
}

void rcsp_bs_stop(void)
{
    task_kill("file_bs");
}

#endif



