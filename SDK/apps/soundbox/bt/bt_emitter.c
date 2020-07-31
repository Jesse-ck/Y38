#include "system/app_core.h"
#include "system/includes.h"

#include "app_config.h"
#include "app_action.h"

#include "btstack/btstack_task.h"
#include "user_cfg.h"
#include "vm.h"
#include "btcontroller_modules.h"
#include "btstack/avctp_user.h"
#include "app_main.h"
#include "media/includes.h"

#include "key_event_deal.h"
#include "rcsp_bluetooth.h"

#if TCFG_USER_EMITTER_ENABLE
extern int bt_wait_connect_and_phone_connect_switch(void *p);
extern void bt_search_device(void);
extern void emitter_media_source(u8 source, u8 en);
extern int a2dp_source_init(void *buf, u16 len, int deal_flag);
extern int hfp_ag_buf_init(void *buf, int size, int deal_flag);
extern void __set_emitter_enable_flag(u8 flag);

extern u8 connect_last_device_from_vm();
extern void audio_sbc_enc_init(void);
extern u8 app_common_key_var_2_event(u32 key_var);

extern void bredr_bulk_change(u8 mode);
extern void hci_cancel_inquiry();
extern u8 hci_standard_connect_check(void);
//提供按键切换发射器或者是音箱功能
void emitter_connect_switch()
{
    if (get_curr_channel_state() != 0) {
        user_send_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);
        while (hci_standard_connect_check() != 0) {
            //wait disconnect;
            os_time_dly(10);
        }
    } else {
        puts("start connect vm addr\n");
        user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR_MANUALLY, 0, NULL);
    }
}
void emitter_or_receiver_switch(u8 flag)
{
    u8 buffer_control_flag = 0;
    g_printf("===emitter_or_receiver_switch %d %x\n", flag, hci_standard_connect_check());
    /*如果上一次操作记录跟传进来的参数一致，则不操作*/
    if (bt_user_priv_var.emitter_or_receiver == flag) {
        return ;
    }
    while (hci_standard_connect_check() == 0x80) {
        //wait profile connect ok;
        if (get_curr_channel_state()) {
            break;
        }
        os_time_dly(10);
    }
    if (get_curr_channel_state() != 0) {
        user_send_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);
    } else {
        if (hci_standard_connect_check()) {
            user_send_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
            user_send_cmd_prepare(USER_CTRL_CONNECTION_CANCEL, 0, NULL);
        }
    }
    /* if there are some connected channel ,then disconnect*/
    while (hci_standard_connect_check() != 0) {
        //wait disconnect;
        os_time_dly(10);
    }

    buffer_control_flag = bt_user_priv_var.emitter_or_receiver;
    g_printf("===wait to switch to mode %d\n", flag);
    bt_user_priv_var.emitter_or_receiver = flag;
    if (flag == BT_EMITTER_EN) {
        if (buffer_control_flag) {
            bredr_bulk_change(0);
        }
        user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);
        __set_emitter_enable_flag(1);
        a2dp_source_init(NULL, 0, 1);
#if (USER_SUPPORT_PROFILE_HFP_AG==1)
        hfp_ag_buf_init(NULL, 0, 1);
#endif
        if (connect_last_device_from_vm()) {
            puts("start connect device vm addr\n");
        } else {
            bt_search_device();
        }
    } else if (flag == BT_RECEIVER_EN) {
        if (buffer_control_flag) {
            bredr_bulk_change(1);
        }
        __set_emitter_enable_flag(0);
        emitter_media_source(1, 0);
        hci_cancel_inquiry();
        a2dp_source_init(NULL, 0, 0);
#if (USER_SUPPORT_PROFILE_HFP_AG==1)
        hfp_ag_buf_init(NULL, 0, 0);
#endif
        if (connect_last_device_from_vm()) {
            puts("start connect vm addr phone \n");
        } else {
            user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
            user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
        }
    }
}

u8 emitter_search_result(char *name, u8 name_len, u8 *addr, u32 dev_class, char rssi);
void emitter_search_stop();

struct list_head inquiry_noname_list;

struct inquiry_noname_remote {
    struct list_head entry;
    u8 match;
    s8 rssi;
    u8 addr[6];
    u32 class;
};



#define  SEARCH_BD_ADDR_LIMITED 0
#define  SEARCH_BD_NAME_LIMITED 1
#define  SEARCH_CUSTOM_LIMITED  2
#define  SEARCH_NULL_LIMITED    3

#define SEARCH_LIMITED_MODE  SEARCH_BD_NAME_LIMITED

void bt_emitter_init()
{
    INIT_LIST_HEAD(&inquiry_noname_list);

    audio_sbc_enc_init();
}


void emitter_search_noname(u8 status, u8 *addr, u8 *name)
{
    struct  inquiry_noname_remote *remote, *n;
    u8 res = 0;
    if (status) {
        goto __find_next;
    }
    list_for_each_entry_safe(remote, n, &inquiry_noname_list, entry) {
        if (!memcmp(addr, remote->addr, 6)) {
            res = emitter_search_result(name, strlen(name), addr, remote->class, remote->rssi);
            if (res) {
                remote->match = 1;
                user_send_cmd_prepare(USER_CTRL_INQUIRY_CANCEL, 0, NULL);
                return;
            }
        }
    }

__find_next:

    remote = NULL;
    if (!list_empty(&inquiry_noname_list)) {
        remote =  list_first_entry(&inquiry_noname_list, struct inquiry_noname_remote, entry);
    }

    if (remote) {
        user_send_cmd_prepare(USER_CTRL_READ_REMOTE_NAME, 6, remote->addr);
    }
}


void emitter_search_stop()
{
    struct  inquiry_noname_remote *remote, *n;
    u8 wait_connect_flag = 1;
    if (!list_empty(&inquiry_noname_list)) {
        user_send_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
    }

    list_for_each_entry_safe(remote, n, &inquiry_noname_list, entry) {
        if (remote->match) {
            user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6, remote->addr);
            wait_connect_flag = 0;
        }
        list_del(&remote->entry);
        free(remote);
    }
    if (wait_connect_flag) {
        r_printf("wait conenct\n");
#if TCFG_SPI_LCD_ENABLE
        user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);
#else
        user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
#endif

    }
}


#if (SEARCH_LIMITED_MODE == SEARCH_BD_ADDR_LIMITED)
u8 bd_addr_filt[][6] = {
    {0x8E, 0xA7, 0xCA, 0x0A, 0x5E, 0xC8}, /*S10_H*/
    {0xA7, 0xDD, 0x05, 0xDD, 0x1F, 0x00}, /*ST-001*/
    {0xE9, 0x73, 0x13, 0xC0, 0x1F, 0x00}, /*HBS 730*/
    {0x38, 0x7C, 0x78, 0x1C, 0xFC, 0x02}, /*Bluetooth*/
};
u8 search_bd_addr_filt(u8 *addr)
{
    u8 i;
    puts("bd_addr:");
    put_buf(addr, 6);
    for (i = 0; i < (sizeof(bd_addr_filt) / sizeof(bd_addr_filt[0])); i++) {
        if (memcmp(addr, bd_addr_filt[i], 6) == 0) {
            printf("bd_addr match:%d\n", i);
            return TRUE;
        }
    }
    puts("bd_addr not match\n");
    return FALSE;
}
#endif


#if (SEARCH_LIMITED_MODE == SEARCH_BD_NAME_LIMITED)
#if (1)
u8 bd_name_filt[][32] = {
    "BeMine",
    "EDIFIER CSR8635",/*CSR*/
    "JL-BT-SDK",/*Realtek*/
    "I7-TWS",/*ZKLX*/
    "TWS-i7",/*ZKLX*/
    "I9",/*ZKLX*/
    "小米小钢炮蓝牙音箱",/*XiaoMi*/
    "小米蓝牙音箱",/*XiaoMi*/
    "XMFHZ02",/*XiaoMi*/
    "JBL GO 2",
    "i7mini",/*JL tws AC690x*/
    "S08U",
    "AI8006B_TWS00",
    "S046",/*BK*/
    "AirPods",
    "CSD-TWS-01",
    "AC692X_wh",
    "JBL GO 2",
    "JBL Flip 4",
    "BT Speaker",
    "CSC608",
    "QCY-QY19",
    "Newmine",
    "HT1+",
    "S-35",
    "T12-JL",
    "Redmi AirDots_R",
    "Redmi AirDots_L",
    "AC69_Bluetooth",
    "FlyPods 3",
    "MNS",
    "Jam Heavy Metal",
    "Bluedio",
    "HR-686",
    "BT MUSIC",
    "BW-USB-DONGLE",
    "S530",
    "XPDQ7",
    "MICGEEK Q9S",
    "S10_H",
    "S10",/*JL AC690x*/
    "S11",/*JL AC460x*/
    "HBS-730",
    "SPORT-S9",
    "Q5",
    "IAEB25",
    "T5-JL",
    "MS-808",
    "LG HBS-730",
    "NG-BT07"
};
#else
u8 bd_name_filt[20][30] = {
    "AC69_BT_SDK",
};
#endif

extern const char *bt_get_emitter_connect_name();
u8 search_bd_name_filt(char *data, u8 len, u32 dev_class, char rssi)
{
    char bd_name[64] = {0};
    u8 i;
    char *targe_name = NULL;
    char char_a = 0, char_b = 0;

    if ((len > (sizeof(bd_name))) || (len == 0)) {
        //printf("bd_name_len error:%d\n", len);
        return FALSE;
    }

    memcpy(bd_name, data, len);
    printf("name:%s,len:%d,class %x ,rssi %d\n", bd_name, len, dev_class, rssi);
#if 0
    printf("tar name:%s,len:%d\n", bt_get_emitter_connect_name(), strlen(bt_get_emitter_connect_name()));
    targe_name = (char *)bt_get_emitter_connect_name();
#if 1
//不区分大小写
    for (i = 0; i < len; i++) {
        char_a = bd_name[i];
        char_b = targe_name[i];
        if ('A' <= char_a && char_a <= 'Z') {
            char_a += 32;    //转换成小写
        }
        if ('A' <= char_b && char_b <= 'Z') {
            char_b += 32;    //转换成小写
        }
        //printf("{%d-%d}",char_a,char_b);
        if (char_a != char_b) {
            return FALSE;
        }
    }
    puts("\n*****find dev ok******\n");
    return TRUE;
#else
//区分大小写
    if (memcmp(data, bt_get_emitter_connect_name(), len) == 0) {
        puts("\n*****find dev ok******\n");
        return TRUE;
    }
    return FALSE;
#endif

#else
    for (i = 0; i < (sizeof(bd_name_filt) / sizeof(bd_name_filt[0])); i++) {
        if (memcmp(data, bd_name_filt[i], len) == 0) {
            puts("\n*****find dev ok******\n");
            return TRUE;
        }
    }

    return FALSE;
#endif

}


/*
 *inquiry result
 *蓝牙设备搜索结果，可以做名字/地址过滤，也可以保存搜到的所有设备
 *在选择一个进行连接，获取其他你想要的操作。
 *返回TRUE，表示搜到指定的想要的设备，搜索结束，直接连接当前设备
 *返回FALSE，则继续搜索，直到搜索完成或者超时
 */
u8 emitter_search_result(char *name, u8 name_len, u8 *addr, u32 dev_class, char rssi)
{
    if (name == NULL) {
        struct inquiry_noname_remote *remote = malloc(sizeof(struct inquiry_noname_remote));
        remote->match  = 0;
        remote->class = dev_class;
        remote->rssi = rssi;
        memcpy(remote->addr, addr, 6);
        list_add_tail(&remote->entry, &inquiry_noname_list);
        user_send_cmd_prepare(USER_CTRL_READ_REMOTE_NAME, 6, addr);
    }
#if (RCSP_BTMATE_EN)
    rcsp_msg_post(RCSP_MSG_BT_SCAN, 5, dev_class, addr, rssi, name, name_len);
#endif

#if TCFG_SPI_LCD_ENABLE
    void bt_menu_list_add(u8 * name, u8 * mac, u8 rssi);
    bt_menu_list_add(name, addr, rssi);
    printf("name:%s,len:%d,class %x ,rssi %d\n", name, name_len, dev_class, rssi);
    return false;
#endif


#if (SEARCH_LIMITED_MODE == SEARCH_BD_NAME_LIMITED)
    return search_bd_name_filt(name, name_len, dev_class, rssi);
#endif

#if (SEARCH_LIMITED_MODE == SEARCH_BD_ADDR_LIMITED)
    return search_bd_addr_filt(addr);
#endif

#if (SEARCH_LIMITED_MODE == SEARCH_CUSTOM_LIMITED)
    /*以下为搜索结果自定义处理*/
    char bt_name[63] = {0};
    u8 len;
    if (name_len == 0) {
        puts("No_eir\n");
    } else {
        len = (name_len > 63) ? 63 : name_len;
        /* display bd_name */
        memcpy(bt_name, name, len);
        printf("name:%s,len:%d,class %x ,rssi %d\n", bt_name, name_len, dev_class, rssi);
    }

    /* display bd_addr */
    put_buf(addr, 6);

    /* You can connect the specified bd_addr by below api      */
    //user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR,6,addr);

    return FALSE;
#endif

#if (SEARCH_LIMITED_MODE == SEARCH_NULL_LIMITED)
    /*没有指定限制，则搜到什么就连接什么*/
    return TRUE;
#endif
}

typedef enum {
    AVCTP_OPID_VOLUME_UP   = 0x41,
    AVCTP_OPID_VOLUME_DOWN = 0x42,
    AVCTP_OPID_MUTE        = 0x43,
    AVCTP_OPID_PLAY        = 0x44,
    AVCTP_OPID_STOP        = 0x45,
    AVCTP_OPID_PAUSE       = 0x46,
    AVCTP_OPID_NEXT        = 0x4B,
    AVCTP_OPID_PREV        = 0x4C,
} AVCTP_CMD_TYPE;

/*
 *发射器收到接收器发过来的控制命令处理
 *根据实际需求可以在收到控制命令之后做相应的处理
 *蓝牙库里面定义的是weak函数，直接再定义一个同名可获取信息
 */
void emitter_rx_avctp_opid_deal(u8 cmd, u8 id)
{
    printf("avctp_rx_cmd:%x\n", cmd);
    switch (cmd) {
    case AVCTP_OPID_NEXT:
        puts("AVCTP_OPID_NEXT\n");
        app_common_key_var_2_event(KEY_MUSIC_NEXT);
        break;
    case AVCTP_OPID_PREV:
        puts("AVCTP_OPID_PREV\n");
        app_common_key_var_2_event(KEY_MUSIC_PREV);
        break;
    case AVCTP_OPID_PAUSE:
    case AVCTP_OPID_PLAY:
    case AVCTP_OPID_STOP:
        puts("AVCTP_OPID_PP\n");
        app_common_key_var_2_event(KEY_BT_EMITTER_SW);
        break;
    case AVCTP_OPID_VOLUME_UP:
        puts("AVCTP_OPID_VOLUME_UP\n");
        app_common_key_var_2_event(KEY_VOL_UP);
        break;
    case AVCTP_OPID_VOLUME_DOWN:
        puts("AVCTP_OPID_VOLUME_DOWN\n");
        app_common_key_var_2_event(KEY_VOL_DOWN);
        break;
    default:
        break;
    }
    return ;
}

void emitter_rx_vol_change(u8 vol)
{
    printf("vol_change:%d \n", vol);
}
typedef struct _EMITTER_INFO {
    volatile u8 role;
    u8 media_source;
    u8 source_record;/*统计当前有多少设备可用*/
    u8 reserve;
} EMITTER_INFO_T;

EMITTER_INFO_T emitter_info = {
    .role = 0/*EMITTER_ROLE_SLAVE*/,
};
extern void __emitter_send_media_toggle(u8 toggle);
void emitter_media_source(u8 source, u8 en)
{
    if (en) {
        /*关闭当前的source通道*/
        //emitter_media_source_close(emitter_info.media_source);
        emitter_info.source_record |= source;
        if (emitter_info.media_source == source) {
            return;
        }
        emitter_info.media_source = source;
        __emitter_send_media_toggle(1);
    } else {
        emitter_info.source_record &= ~source;
        if (emitter_info.media_source == source) {
            emitter_info.media_source = 0/*EMITTER_SOURCE_NULL*/;
            __emitter_send_media_toggle(0);
            //emitter_media_source_next();
        }
    }
    printf("current source: %x-%x\n", source, emitter_info.source_record);
}
#endif

#include "audio_config.h"
#include "audio_digital_vol.h"

extern u8 get_total_connect_dev(void);
extern int audio_sbc_enc_is_work(void);
extern int audio_sbc_enc_write(s16 *data, int len);

/* static u8 bt_emitter_on = 0; */

u8 bt_emitter_stu_get(void)
{
    return audio_sbc_enc_is_work();//bt_emitter_on;
}
u8 bt_emitter_stu_set(u8 on)
{
    if (bt_user_priv_var.emitter_or_receiver != BT_EMITTER_EN) {
        return 0;
    }
    printf("total con dev:%d ", get_total_connect_dev());
    if (on && (get_total_connect_dev() == 0)) {
        on = 0;
    }
    /* bt_emitter_on = on; */
    emitter_media_source(1, on);
    return on;
}
u8 bt_emitter_stu_sw(void)
{
    if (bt_user_priv_var.emitter_or_receiver != BT_EMITTER_EN) {
        return 0;
    }
    if (get_total_connect_dev() == 0) {
        //如果没有连接就启动一下搜索
        bt_search_device();
        return 0;
    }
    if (!(get_curr_channel_state() & A2DP_CH)) {
        return 0;
    }
    return bt_emitter_stu_set(!bt_emitter_stu_get());
}




//////////////////////////////////////////////////////////////////////
// mic
#include "audio_enc.h"

#define ESCO_ADC_BUF_NUM        2
#define ESCO_ADC_IRQ_POINTS     256
#define ESCO_ADC_BUFS_SIZE      (ESCO_ADC_BUF_NUM * ESCO_ADC_IRQ_POINTS)

struct bt_emitter_mic_hdl {
    struct audio_adc_output_hdl adc_output;
    struct adc_mic_ch mic_ch;
    s16 adc_buf[ESCO_ADC_BUFS_SIZE];    //align 4Bytes
    s16 temp_data[ESCO_ADC_IRQ_POINTS * 2];
    volatile u32 start : 1;
    };
    static struct bt_emitter_mic_hdl *bt_emitter_mic = NULL;

    extern struct audio_adc_hdl adc_hdl;

    extern int audio_sbc_enc_get_rate(void);
    extern int audio_sbc_enc_get_channel_num(void);

    static void adc_mic_output_handler(void *priv, s16 *data, int len)
{
    if (!bt_emitter_mic || !bt_emitter_mic->start) {
        return ;
    }
    u8 ch = audio_sbc_enc_get_channel_num();
    if (ch == 2) {
        //单变双
        u16 points = len >> 1;
        for (int i = 0; i < points ; i++) {
            bt_emitter_mic->temp_data[i * 2] = data[i];
            bt_emitter_mic->temp_data[i * 2 + 1] = data[i];
        }
        u32 wlen = audio_sbc_enc_write(bt_emitter_mic->temp_data, len * 2);
        /* if (wlen != (len*2)) { */
        /* putchar('A'); */
        /* } else { */
        /* putchar('O');	 */
        /* } */
    } else {
        u32 wlen = audio_sbc_enc_write(data, len);
        /* if (wlen != len) { */
        /* putchar('B'); */
        /* } else { */
        /* putchar('O');	 */
        /* } */
    }
}

void bt_emitter_mic_close(void)
{
    if (!bt_emitter_mic) {
        return ;
    }
    printf("bt emitter mic close \n");
    bt_emitter_mic->start = 0;
    audio_adc_mic_close(&bt_emitter_mic->mic_ch);
    audio_adc_del_output_handler(&adc_hdl, &bt_emitter_mic->adc_output);
    free(bt_emitter_mic);
    bt_emitter_mic = NULL;
}

extern void bt_emitter_set_vol(u8 vol);
extern int audio_sbc_enc_reset_buf(u8 flag);
int bt_emitter_mic_open(void)
{
    struct audio_fmt fmt = {0};
    if (bt_user_priv_var.emitter_or_receiver != BT_EMITTER_EN) {
        return 0;
    }
    if (bt_emitter_mic) {
        bt_emitter_mic_close();
    }
    printf("bt emitter mic open \n");
    bt_emitter_mic = zalloc(sizeof(*bt_emitter_mic));
    if (!bt_emitter_mic) {
        return -1;
    }
    bt_emitter_set_vol(app_var.music_volume);
    audio_sbc_enc_reset_buf(1);

    fmt.sample_rate = audio_sbc_enc_get_rate();//44100;
    audio_adc_mic_open(&bt_emitter_mic->mic_ch, AUDIO_ADC_MIC_CH, &adc_hdl);
    audio_adc_mic_set_sample_rate(&bt_emitter_mic->mic_ch, fmt.sample_rate);
    audio_adc_mic_set_gain(&bt_emitter_mic->mic_ch, app_var.aec_mic_gain);
    audio_adc_mic_set_buffs(&bt_emitter_mic->mic_ch, bt_emitter_mic->adc_buf,
                            ESCO_ADC_IRQ_POINTS * 2, ESCO_ADC_BUF_NUM);
    bt_emitter_mic->adc_output.handler = adc_mic_output_handler;
    audio_adc_add_output_handler(&adc_hdl, &bt_emitter_mic->adc_output);

    audio_adc_mic_start(&bt_emitter_mic->mic_ch);

    bt_emitter_mic->start = 1;
    return 0;
}

void audio_sbc_enc_open_exit(void)
{
    if (app_cur_task_check(APP_NAME_BT) == true) {
        bt_emitter_mic_open();
    }
}
void audio_sbc_enc_close_enter(void)
{
    bt_emitter_mic_close();
}

#else

void emitter_media_source(u8 source, u8 en)
{

}
void emitter_or_receiver_switch(u8 flag)
{
}
#endif



