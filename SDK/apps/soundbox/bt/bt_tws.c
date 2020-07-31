#include "system/includes.h"
#include "media/includes.h"
#include "device/vm.h"
#include "tone_player.h"

#include "app_config.h"
#include "app_action.h"

#include "btstack/avctp_user.h"
#include "btstack/btstack_task.h"
#include "user_cfg.h"
#include "bt_tws.h"
#include "asm/charge.h"
#include "app_charge.h"
#include "ui_manage.h"

#include "app_chargestore.h"
#include "app_online_cfg.h"
#include "app_main.h"
#include "app_power_manage.h"
#include "audio_config.h"
#include "user_cfg.h"

#include "btcontroller_config.h"
#include "asm/pwm_led.h"

#include "bt_common.h"

#include "bt_ble.h"

#ifdef CONFIG_NEW_BREDR_ENABLE
#define CONFIG_NEW_QUICK_CONN
#endif


#if TCFG_USER_TWS_ENABLE

#define LOG_TAG_CONST       BT_TWS
#define LOG_TAG             "[BT-TWS]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


#define    BT_TWS_UNPAIRED                      0x0001
#define    BT_TWS_PAIRED                        0x0002
#define    BT_TWS_WAIT_SIBLING_SEARCH           0x0004
#define    BT_TWS_SEARCH_SIBLING                0x0008
#define    BT_TWS_CONNECT_SIBLING               0x0010
#define    BT_TWS_SIBLING_CONNECTED             0x0020
#define    BT_TWS_PHONE_CONNECTED               0x0040
#define    BT_TWS_POWER_ON                      0x0080
#define    BT_TWS_TIMEOUT                       0x0100
#define    BT_TWS_AUDIO_PLAYING                 0x0200


struct tws_user_var {
    u8 addr[6];
    u16 state;
    s16 auto_pair_timeout;
    int pair_timer;
    u32 connect_time;
    u8  device_role;  //tws 记录那个是active device 活动设备，音源控制端
    u8 bt_task;   ///标志对箱在bt task情况，BIT(0):local  BIT(1):remote
};

struct tws_user_var  gtws;

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
typedef struct {
    u8 miss_flag: 1;
    u8 exchange_bat: 2;
    u8 poweron_flag: 1;
    u8 reserver: 4;
} icon_ctl_t;
static icon_ctl_t ble_icon_contrl;
#endif

extern u8 check_tws_le_aa(void);
extern const char *bt_get_local_name();
extern u8 get_bredr_link_state();
extern u8 get_esco_coder_busy_flag();
void bt_init_ok_search_index(void);
extern void sys_auto_shut_down_enable(void);
extern void sys_auto_shut_down_disable(void);
extern void phone_num_play_timer(void *priv);
extern u8 phone_ring_play_start(void);
extern bool get_esco_busy_flag();
extern void tws_api_set_connect_aa(int);
extern void tws_api_clear_connect_aa();
extern u8 tws_remote_state_check(void);
extern void tws_remote_state_clear(void);
extern u16 bt_get_tws_device_indicate(u8 *tws_device_indicate);
void tws_le_acc_generation_init(void);
static void bt_tws_connect_and_connectable_switch();
extern void bt_set_led_status(u8 status);
extern int earphone_a2dp_codec_get_low_latency_mode();
extern void earphone_a2dp_codec_set_low_latency_mode(int enable);
extern u32 get_bt_slot_time(u8 type, u32 time, int *ret_time, int (*local_us_time)(void));
extern u32 get_sync_rec_instant_us_time();
static u32 local_us_time = 0;
static u32 local_instant_us_time = 0;

extern int local_tws_dec_create(void);
extern int local_tws_dec_open(u32 dec_type);
extern int local_tws_dec_close(u8 drop_frame_start);
extern u8 bt_get_exit_flag();

int  a2dp_dec_close();
extern void app_mode_tone_play_by_tws(int cmd);
void user_set_tws_box_mode(u8 mode);
void tws_local_back_to_bt_mode(u8 mode, u8 value);
void bt_tws_sync_volume();
u8 back_box_role;

#define    TWS_LOCAL_IN_BT()      (gtws.bt_task |=BIT(0))
#define    TWS_LOCAL_OUT_BT()   (gtws.bt_task &=~BIT(0))
#define    TWS_REMOTE_IN_BT()      (gtws.bt_task |=BIT(1))
#define    TWS_REMOTE_OUT_BT()    (gtws.bt_task &=~BIT(1))


u8 is_tws_all_in_bt()
{
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    int state = tws_api_get_tws_state();
    if (state & TWS_STA_SIBLING_CONNECTED) {
        if ((gtws.bt_task & 0x03) == 0x03) {
            return 1;
        }
        return 0;
    }
#endif
    return 1;
}

u8 is_tws_active_device(void)
{
    /* log_info("\n    ------  tws device ------   %x \n",gtws.device_role); */
    int state = tws_api_get_tws_state();
    if (state & TWS_STA_SIBLING_CONNECTED) {
        if (gtws.device_role == TWS_ACTIVE_DEIVCE) {
            return 1;
        }
        return 0;
    }
    return 1;
}

u8 tws_network_audio_was_started(void)
{
    if (gtws.state & BT_TWS_AUDIO_PLAYING) {
        return 1;
    }

    return 0;
}

void tws_network_local_audio_start(void)
{
    gtws.state &= ~BT_TWS_AUDIO_PLAYING;
}


static void tws_sync_call_fun(int cmd)
{
    struct sys_event event;

    event.type = SYS_BT_EVENT;
    event.arg = (void *)SYS_BT_EVENT_FROM_TWS;

    event.u.bt.event = TWS_EVENT_SYNC_FUN_CMD;
    event.u.bt.args[0] = 0;
    event.u.bt.args[1] = 0;
    event.u.bt.args[2] = cmd;

#if (defined CONFIG_CPU_BR18 || \
     CONFIG_CPU_BR21)
    extern int sync_play_tone_paly_int(int time_us);
    if ((cmd == SYNC_CMD_TWS_CONN_TONE) ||
        (cmd == SYNC_CMD_PHONE_CONN_TONE)) {
        sync_play_tone_paly_int(300 * 1000);
    } else if ((cmd == SYNC_CMD_PHONE_RING_TONE) ||
               (cmd == SYNC_CMD_PHONE_SYNC_NUM_RING_TONE)) {
        if (!bt_user_priv_var.inband_ringtone && bt_user_priv_var.phone_ring_flag) {
            sync_play_tone_paly_int(600 * 1000); //600ms

        }
    } else if (cmd == SYNC_CMD_PHONE_NUM_TONE) {
        if (!bt_user_priv_var.inband_ringtone && bt_user_priv_var.phone_ring_flag) {
            sync_play_tone_paly_int(800 * 1000); //800ms
        }

    } else if (cmd == SYNC_CMD_POWER_OFF_TOGETHER) {
        sync_play_tone_paly_int(1300 * 1000); //800ms
    }
#endif

    sys_event_notify(&event);
}

TWS_SYNC_CALL_REGISTER(tws_tone_sync) = {
    .uuid = 'T',
    .func = tws_sync_call_fun,
};

static void tws_sync_call_tranid_fun(int cmd)
{
    struct sys_event event;

    event.type = SYS_BT_EVENT;
    event.arg = (void *)SYS_BT_EVENT_FROM_TWS;

    event.u.bt.event = TWS_EVENT_SYNC_FUN_TRANID;
    event.u.bt.args[0] = 0;
    event.u.bt.args[1] = 0;
    event.u.bt.args[2] = cmd;

    sys_event_notify(&event);
}

TWS_SYNC_CALL_REGISTER(tws_tranid_sync) = {
    .uuid = 'D',
    .func = tws_sync_call_tranid_fun,
};

u32 get_local_us_time(u32 *instant_time)
{
    *instant_time = local_instant_us_time;
    return local_us_time;
}

#define msecs_to_bt_slot_clk(m)     (((m + 1)* 1000) / 625)
u32 bt_tws_master_slot_clk(void);
u32 bt_tws_future_slot_time(u32 msecs)
{
    return bt_tws_master_slot_clk() + msecs_to_bt_slot_clk(msecs);
}

u16 tws_host_get_battery_voltage()
{
    return get_vbat_level();
}

int tws_host_channel_match(char remote_channel)
{
    /*r_printf("tws_host_channel_match: %c, %c\n", remote_channel,
             bt_tws_get_local_channel());*/

#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_MASTER_AS_LEFT)
    return 1;
#endif
    if (remote_channel != bt_tws_get_local_channel() || remote_channel == 'U') {
        return 1;
    }
#if CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_LEFT_START_PAIR || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_RIGHT_START_PAIR
    if (gtws.state & BT_TWS_SEARCH_SIBLING) {
        return 1;
    }
#endif

    return 0;
}

char tws_host_get_local_channel()
{
    char channel;

#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_RIGHT_START_PAIR)
    if (gtws.state & BT_TWS_SEARCH_SIBLING) {
        return 'R';
    }
#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_LEFT_START_PAIR)
    if (gtws.state & BT_TWS_SEARCH_SIBLING) {
        return 'L';
    }
#endif
    channel = bt_tws_get_local_channel();
    if (channel != 'R') {
        channel = 'L';
    }
    /*y_printf("tws_host_get_local_channel: %c\n", channel);*/

    return channel;
}


/*
 * 设置自动回连的识别码 6个byte
 * */
u8 auto_pair_code[6] = {0x34, 0x66, 0x33, 0x87, 0x09, 0x42};

u8 *tws_set_auto_pair_code(void)
{
    u16 code = bt_get_tws_device_indicate(NULL);
    auto_pair_code[0] = code >> 8;
    auto_pair_code[1] = code & 0xff;
    return auto_pair_code;
}
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_FAST_CONN
u8 tws_auto_pair_enable = 1;
#else
u8 tws_auto_pair_enable = 0;
#endif


static u8 tws_get_sibling_addr(u8 *addr)
{
    u8 all_ff[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    int len = syscfg_read(CFG_TWS_REMOTE_ADDR, addr, 6);
    if (len != 6 || !memcmp(addr, all_ff, 6)) {
        return -ENOENT;
    }
    return 0;
}

void lmp_hci_write_local_address(const u8 *addr);

/*
 * 获取左右耳信息
 * 'L': 左耳
 * 'R': 右耳
 * 'U': 未知
 */
char bt_tws_get_local_channel()
{
    char channel = 'U';

    syscfg_read(CFG_TWS_CHANNEL, &channel, 1);

    return channel;
}

int get_bt_tws_connect_status()
{
    if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
        return 1;
    }

    return 0;
}



void tws_sync_phone_num_play_wait(void *priv)
{
    puts("tws_sync_phone_num_play_wait\n");
    if (bt_user_priv_var.phone_con_sync_num_ring) {
        return;
    }
    if (bt_user_priv_var.phone_num_flag) {
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            tws_api_sync_call_by_uuid('T', SYNC_CMD_PHONE_NUM_TONE, TWS_SYNC_TIME_DO * 2);
        } else { //从机超时还没获取到
            phone_ring_play_start();
        }
    } else {
        /*电话号码还没有获取到，定时查询*/
        if (bt_user_priv_var.get_phone_num_timecnt > 100) {
            bt_user_priv_var.get_phone_num_timecnt = 0;
            tws_api_sync_call_by_uuid('T', SYNC_CMD_PHONE_SYNC_NUM_RING_TONE, TWS_SYNC_TIME_DO);
        } else {
            bt_user_priv_var.phone_timer_id = sys_timeout_add(NULL, tws_sync_phone_num_play_wait, 10);
        }
    }
    bt_user_priv_var.get_phone_num_timecnt++;

}

int bt_tws_sync_phone_num(void *priv)
{
    int state = tws_api_get_tws_state();
    if ((state & TWS_STA_SIBLING_CONNECTED) && (state & TWS_STA_PHONE_CONNECTED)) {
        puts("bt_tws_sync_phone_num\n");
#if BT_PHONE_NUMBER
        bt_user_priv_var.get_phone_num_timecnt = 0;
        if (tws_api_get_role() == TWS_ROLE_SLAVE && bt_user_priv_var.phone_con_sync_num_ring) { //从机同步播来电ring,不在发起sync_play
            puts("phone_con_sync_num_ring_ing return\n");
            return 1;
        }
#endif
        if (tws_api_get_role() == TWS_ROLE_SLAVE || bt_user_priv_var.phone_con_sync_num_ring) {

            if (!(state & TWS_STA_MONITOR_START)) {
                puts(" not monitor ring_tone\n");
                /* tws_api_sync_call_by_uuid('T', SYNC_CMD_PHONE_RING_TONE, TWS_SYNC_TIME_DO * 5); */
                bt_user_priv_var.phone_timer_id = sys_timeout_add(NULL, (void (*)(void *priv))bt_tws_sync_phone_num, 10);
                return 1;
            }
#if BT_PHONE_NUMBER
            if (bt_user_priv_var.phone_con_sync_num_ring) {
                puts("<<<<<<<<<<<send_SYNC_CMD_PHONE_SYNC_NUM_RING_TONE\n");
                tws_api_sync_call_by_uuid('T', SYNC_CMD_PHONE_SYNC_NUM_RING_TONE, TWS_SYNC_TIME_DO / 2);

            } else {
                bt_user_priv_var.phone_timer_id = sys_timeout_add(NULL, tws_sync_phone_num_play_wait, 10);
            }
#else
            tws_api_sync_call_by_uuid('T', SYNC_CMD_PHONE_RING_TONE, TWS_SYNC_TIME_DO * 2);
#endif
        } else {
#if BT_PHONE_NUMBER
            if (!bt_user_priv_var.phone_timer_id) { //从机超时还没获取到
                /* bt_user_priv_var.phone_timer_id = sys_timeout_add(NULL, tws_sync_phone_num_play_wait, TWS_SYNC_TIME_DO * 2); */
            }
#endif
        }
        return 1;
    }
    return 0;
}


static void bt_tws_delete_pair_timer()
{
    if (gtws.pair_timer) {
        sys_timeout_del(gtws.pair_timer);
        gtws.pair_timer = 0;
    }
}

/*
 * 根据配对码搜索TWS设备
 * 搜索超时会收到事件: TWS_EVENT_SEARCH_TIMEOUT
 * 搜索到连接超时会收到事件: TWS_EVENT_CONNECTION_TIMEOUT
 * 搜索到并连接成功会收到事件: TWS_EVENT_CONNECTED
 */
static void bt_tws_search_sibling_and_pair()
{
    u8 mac_addr[6];

    bt_tws_delete_pair_timer();
    tws_api_get_local_addr(gtws.addr);
#if CONFIG_TWS_USE_COMMMON_ADDR
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
    get_random_number(mac_addr, 6);
    lmp_hci_write_local_address(mac_addr);
#endif
#endif
    tws_api_search_sibling_by_code(bt_get_tws_device_indicate(NULL), 15000);
}

/*
 *打开可发现, 可连接，可被手机和tws搜索到
 */
static void bt_tws_wait_pair_and_phone_connect()
{
    bt_tws_delete_pair_timer();

    tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
}

static void bt_tws_wait_sibling_connect(int timeout)
{
    bt_tws_delete_pair_timer();
    tws_api_wait_connection(timeout);
}

static void bt_tws_connect_sibling(int timeout)
{
    int err;

    bt_tws_delete_pair_timer();
    err = tws_api_create_connection(timeout * 1000);
    if (err) {
        bt_tws_connect_and_connectable_switch();
    }
}


static int bt_tws_connect_phone()
{
    int timeout = 6500;

    bt_user_priv_var.auto_connection_counter -= timeout ;
    tws_api_cancle_wait_pair();
    tws_api_cancle_create_connection();
    user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6,
                          bt_user_priv_var.auto_connection_addr);
    return timeout;
}


/*
 * TWS 回连手机、开可发现可连接、回连已配对设备、搜索tws设备 4个状态间定时切换函数
 *
 */
#ifdef CONFIG_NEW_BREDR_ENABLE
static void connect_and_connectable_switch(void *_sw)
{
    int timeout = 0;
    int sw = (int)_sw;
    int end_sw = 1;

    gtws.pair_timer = 0;

    log_info("switch: %d, state = %x, %d\n", sw, gtws.state,
             bt_user_priv_var.auto_connection_counter);

    if (sw == 0) {
__again:
        if (bt_user_priv_var.auto_connection_counter > 0) {
            timeout = 4000 + (rand32() % 4 + 1) * 500;
            bt_user_priv_var.auto_connection_counter -= timeout ;
            tws_api_cancle_wait_pair();
            tws_api_cancle_create_connection();
            user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6,
                                  bt_user_priv_var.auto_connection_addr);
        } else {
            bt_user_priv_var.auto_connection_counter = 0;

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
#if (CONFIG_NO_DISPLAY_BUTTON_ICON || !TCFG_CHARGESTORE_ENABLE)
            //last do
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                if (ble_icon_contrl.miss_flag) {
                    ble_icon_contrl.miss_flag = 0;
                    puts("ble_icon_contrl.miss_flag...\n");
                } else {
                    printf("switch_icon_ctl00...\n");
                    bt_ble_icon_open(ICON_TYPE_INQUIRY);
                }
            }
#endif
#endif

            if (gtws.state & BT_TWS_POWER_ON) {
                /*
                 * 开机回连,获取下一个设备地址
                 */
                if (get_current_poweron_memory_search_index(NULL)) {
                    bt_init_ok_search_index();
                    goto __again;
                }
                gtws.state &= ~BT_TWS_POWER_ON;
            }

            if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
                tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
                return;
            }

#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
            if (gtws.state & BT_TWS_UNPAIRED) {
#if (CONFIG_TWS_PAIR_ALL_WAY == 1)
                tws_api_wait_pair_by_ble(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
                return;
#else
                tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
                return;
#endif
            }
#endif
            sw = 1;
        }
    }

    if (sw == 1) {
        tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
        if (!(gtws.state & BT_TWS_SIBLING_CONNECTED)) {
            if (!(gtws.state & BT_TWS_UNPAIRED)) {
#ifdef CONFIG_TWS_AUTO_PAIR_WITHOUT_UNPAIR
                end_sw = 2;
#endif
            }
            tws_api_create_connection(0);
        }
        timeout = 2000 + (rand32() % 4 + 1) * 500;

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
#if (CONFIG_NO_DISPLAY_BUTTON_ICON || !TCFG_CHARGESTORE_ENABLE)
        //last do
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            printf("switch_icon_ctl11...\n");
            bt_ble_icon_open(ICON_TYPE_INQUIRY);
        }
#endif
#endif
        if (bt_user_priv_var.auto_connection_counter <= 0 && end_sw == 1) {
            return;
        }

    } else if (sw == 2) {
        u8 addr[6];
        timeout = 2000 + (rand32() % 4 + 1) * 500;
        memcpy(addr, tws_api_get_quick_connect_addr(), 6);
        tws_api_set_quick_connect_addr(tws_set_auto_pair_code());
        tws_api_create_connection(0);
        tws_api_set_quick_connect_addr(addr);
    }
    if (++sw > end_sw) {
        sw = 0;
    }

    gtws.pair_timer = sys_timeout_add((void *)sw, connect_and_connectable_switch, timeout);
}

#else

static void connect_and_connectable_switch(void *_sw)
{
    int timeout = 0;
    int sw = (int)_sw;

    gtws.pair_timer = 0;

    log_info("switch: %d, state = %x, %d\n", sw, gtws.state,
             bt_user_priv_var.auto_connection_counter);

#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_AUTO
    if (tws_auto_pair_enable == 1) {
        tws_auto_pair_enable = 0;
        tws_le_acc_generation_init();
    }
#endif

    if (tws_remote_state_check()) {
        if (sw == 0) {
            sw = 1;
        } else if (sw == 3) {
            sw = 2;
        }
        puts("tws_exist\n");
    }

    if (sw == 0) {
__again:
        if (bt_user_priv_var.auto_connection_counter > 0) {
            timeout = 4000 + (rand32() % 4 + 1) * 500;
            bt_user_priv_var.auto_connection_counter -= timeout ;
            tws_api_cancle_wait_pair();
            tws_api_cancle_create_connection();
            user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6,
                                  bt_user_priv_var.auto_connection_addr);
        } else {
            bt_user_priv_var.auto_connection_counter = 0;

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
#if (CONFIG_NO_DISPLAY_BUTTON_ICON || !TCFG_CHARGESTORE_ENABLE)
            //last do
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                if (ble_icon_contrl.miss_flag) {
                    ble_icon_contrl.miss_flag = 0;
                    puts("ble_icon_contrl.miss_flag...\n");
                } else {
                    printf("switch_icon_ctl00...\n");
                    bt_ble_icon_open(ICON_TYPE_INQUIRY);
                }
            }
#endif
#endif

            if (gtws.state & BT_TWS_POWER_ON) {
                /*
                 * 开机回连,获取下一个设备地址
                 */
                if (get_current_poweron_memory_search_index(NULL)) {
                    bt_init_ok_search_index();
                    goto __again;
                }
                gtws.state &= ~BT_TWS_POWER_ON;
            }

            if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
                tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
                return;
            }
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
            if (gtws.state & BT_TWS_UNPAIRED) {
                tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
                return;
            }
#endif
            sw = 1;
        }
    }
    if (sw == 1) {
        if (tws_remote_state_check()) {
            timeout = 1000 + (rand32() % 4 + 1) * 500;
        } else {
            timeout = 2000 + (rand32() % 4 + 1) * 500;
        }
        tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
#if (CONFIG_NO_DISPLAY_BUTTON_ICON || !TCFG_CHARGESTORE_ENABLE)
        //last do
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            printf("switch_icon_ctl11...\n");
            bt_ble_icon_open(ICON_TYPE_INQUIRY);
        }
#endif
#endif

    } else if (sw == 2) {
        if (tws_remote_state_check()) {
            timeout = 4000;
        } else {
            timeout = 1000 + (rand32() % 4 + 1) * 500;
        }
        tws_remote_state_clear();
        tws_api_create_connection(0);
    } else if (sw == 3) {
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_AUTO
        timeout = 2000 + (rand32() % 4 + 1) * 500;
        tws_auto_pair_enable = 1;
        tws_le_acc_generation_init();
        tws_api_create_connection(0);
#endif
    }

    if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
        if (++sw > 1) {
            sw = 0;
        }
    } else {
        int end_sw = 2;
#ifdef CONFIG_TWS_AUTO_PAIR_WITHOUT_UNPAIR
        end_sw = 3;
#elif CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_AUTO
        if (gtws.state & BT_TWS_UNPAIRED) {
            end_sw = 3;
        }
#endif
        if (++sw > end_sw) {
            sw = 0;
        }
    }

    gtws.pair_timer = sys_timeout_add((void *)sw, connect_and_connectable_switch, timeout);

}
#endif

static void bt_tws_connect_and_connectable_switch()
{
    bt_tws_delete_pair_timer();

    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        connect_and_connectable_switch(0);
    }
}

#if (CONFIG_TWS_PAIR_ALL_WAY == 1)
u8 get_task_run_slot(void)
{
    return 72;
}
#endif

int bt_tws_start_search_sibling()
{
    int state = get_bt_connect_status();

#if (CONFIG_TWS_PAIR_ALL_WAY == 0)
    if (gtws.state & BT_TWS_PHONE_CONNECTED) {
        return 0;
    }
#endif

    if ((get_call_status() == BT_CALL_ACTIVE) ||
        (get_call_status() == BT_CALL_OUTGOING) ||
        (get_call_status() == BT_CALL_ALERT) ||
        (get_call_status() == BT_CALL_INCOMING)) {
        return 0;//通话过程不允许
    }

    if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
#ifdef CONFIG_TWS_REMOVE_PAIR_ENABLE
        puts("===========remove_pairs\n");
        tws_api_remove_pairs();
#endif
        return 0;
    }



    /* #if (CONFIG_TWS_PAIR_ALL_WAY == 1) */
    /*     if (gtws.state & BT_TWS_PAIRED) { */
    /*         return 0; */
    /*     } */
    /* #endif */


    if (check_tws_le_aa()) {
        return 0;
    }

#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
    log_i("bt_tws_start_search_sibling\n");
    gtws.state |= BT_TWS_SEARCH_SIBLING;
#if CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_LEFT_START_PAIR || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_RIGHT_START_PAIR || \
    CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_MASTER_AS_LEFT
    tws_api_set_local_channel('U');
#endif
    bt_tws_search_sibling_and_pair();
    return 1;
#endif

    return 0;
}


/*
 * 自动配对状态定时切换函数
 */
static void __bt_tws_auto_pair_switch(void *priv)
{
    u32 timeout;
    int sw = (int)priv;

    gtws.pair_timer = 0;

    printf("bt_tws_auto_pair: %d, %d\n", gtws.auto_pair_timeout, bt_user_priv_var.auto_connection_counter);

    if (gtws.auto_pair_timeout < 0 && bt_user_priv_var.auto_connection_counter > 0) {
        gtws.auto_pair_timeout = 4000;
        timeout = bt_tws_connect_phone();
    } else {

        timeout = 1000 + ((rand32() % 10) * 200);

        gtws.auto_pair_timeout -= timeout;

        if (sw == 0) {
            tws_api_wait_pair_by_code(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
        } else if (sw == 1) {
            bt_tws_search_sibling_and_pair();
        }

        if (++sw > 1) {
            sw = 0;
        }
    }

    gtws.pair_timer = sys_timeout_add((void *)sw, __bt_tws_auto_pair_switch, timeout);
}

static void bt_tws_auto_pair_switch(int timeout)
{
    bt_tws_delete_pair_timer();

    gtws.auto_pair_timeout = timeout;
    __bt_tws_auto_pair_switch(NULL);
}

void __set_sbc_cap_bitpool(u8 sbc_cap_bitpoola);


static u8 set_channel_by_code_or_res(void)
{
    u8 count = 0;
    char channel = 0;
    char last_channel = 0;
#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_EXTERN_UP_AS_LEFT)
    gpio_set_direction(CONFIG_TWS_CHANNEL_CHECK_IO, 1);
    gpio_set_pull_down(CONFIG_TWS_CHANNEL_CHECK_IO, 1);
    gpio_set_die(CONFIG_TWS_CHANNEL_CHECK_IO, 1);
    for (int i = 0; i < 5; i++) {
        os_time_dly(2);
        if (gpio_read(CONFIG_TWS_CHANNEL_CHECK_IO)) {
            count++;
        }
    }
    channel = (count >= 3) ? 'L' : 'R';
#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_EXTERN_DOWN_AS_LEFT)
    gpio_set_direction(CONFIG_TWS_CHANNEL_CHECK_IO, 1);
    gpio_set_die(CONFIG_TWS_CHANNEL_CHECK_IO, 1);
    gpio_set_pull_up(CONFIG_TWS_CHANNEL_CHECK_IO, 1);
    for (int i = 0; i < 5; i++) {
        os_time_dly(2);
        if (gpio_read(CONFIG_TWS_CHANNEL_CHECK_IO)) {
            count++;
        }
    }
    channel = (count >= 3) ? 'R' : 'L';
#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_AS_LEFT_CHANNEL)
    channel = 'L';
#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_AS_RIGHT_CHANNEL)
    channel = 'R';
#elif (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_SECECT_BY_CHARGESTORE)
    syscfg_read(CFG_CHARGESTORE_TWS_CHANNEL, &channel, 1);
#endif

#if CONFIG_TWS_SECECT_CHARGESTORE_PRIO
    syscfg_read(CFG_CHARGESTORE_TWS_CHANNEL, &channel, 1);
#endif

    if (channel) {
        syscfg_read(CFG_TWS_CHANNEL, &last_channel, 1);
        if (channel != last_channel) {
            syscfg_write(CFG_TWS_CHANNEL, &channel, 1);
        }
        tws_api_set_local_channel(channel);
        return 1;
    }
    return 0;
}
/*
 * 开机tws初始化
 */
__BANK_INIT_ENTRY
int bt_tws_poweron()
{
    int err;
    u8 addr[6];
    char channel;

    gtws.state = BT_TWS_POWER_ON;
    gtws.connect_time = 0;
    gtws.device_role = TWS_ACTIVE_DEIVCE;
    TWS_LOCAL_IN_BT();

#if CONFIG_TWS_USE_COMMMON_ADDR
    tws_api_common_addr_en(1);
#else
    tws_api_common_addr_en(0);
    tws_api_auto_role_switch_disable();
#endif

#if CONFIG_TWS_PAIR_ALL_WAY
    tws_api_pair_all_way(1);
#endif

    __set_sbc_cap_bitpool(38);


#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
    memset(&ble_icon_contrl, 0, sizeof(icon_ctl_t));
    ble_icon_contrl.poweron_flag = 1;
#endif

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    tws_api_auto_role_switch_disable();
#endif

    err = tws_get_sibling_addr(addr);
    if (err == 0) {
        /*
         * 获取到对方地址, 开始连接
         */
        printf("\n ---------have tws info----------\n");
        gtws.state |= BT_TWS_PAIRED;

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            bt_ble_set_control_en(1);
        } else {
            //slave close
            bt_ble_set_control_en(0);
        }
#endif

        tws_api_set_sibling_addr(addr);
        if (set_channel_by_code_or_res() == 0) {
            channel = bt_tws_get_local_channel();
            tws_api_set_local_channel(channel);
        }
#if TCFG_TEST_BOX_ENABLE
        if (chargestore_get_testbox_status()) {
        } else
#endif
        {
#ifdef CONFIG_NEW_QUICK_CONN
            if (bt_tws_get_local_channel() == 'L') {
                syscfg_read(CFG_TWS_LOCAL_ADDR, addr, 6);
            }
            tws_api_set_quick_connect_addr(addr);
#endif
            bt_tws_connect_sibling(CONFIG_TWS_CONNECT_SIBLING_TIMEOUT);
        }

    } else {
        printf("\n ---------no tws info----------\n");

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
        /* if (tws_api_get_role() == TWS_ROLE_MASTER) { */
        /* bt_ble_set_control_en(1); */
        /* bt_ble_icon_open(icon_type_inquiry); */
        /* } */
        /* else */
        /* { */
        /* bt_ble_set_control_en(0); */
        /* }		 */
#endif

        gtws.state |= BT_TWS_UNPAIRED;
        if (set_channel_by_code_or_res() == 0) {
            tws_api_set_local_channel('U');
        }
#if TCFG_TEST_BOX_ENABLE
        if (chargestore_get_testbox_status()) {
        } else
#endif
        {

#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_AUTO
            /*
             * 未配对, 开始自动配对
             */
#ifdef CONFIG_NEW_BREDR_ENABLE
            tws_api_set_quick_connect_addr(tws_set_auto_pair_code());
#else
            tws_auto_pair_enable = 1;
            tws_le_acc_generation_init();
#endif
            bt_tws_connect_sibling(6);

#else
            /*
             * 未配对, 等待发起配对
             */
            bt_tws_connect_and_connectable_switch();
#endif
        }
    }

#ifndef CONFIG_NEW_BREDR_ENABLE
    tws_remote_state_clear();
#endif

    return 0;
}

/*
 * 手机开始连接
 */
void bt_tws_hci_event_connect()
{
    printf("bt_tws_hci_event_connect: %x\n", gtws.state);

    gtws.state &= ~BT_TWS_POWER_ON;

    bt_user_priv_var.auto_connection_counter = 0;

    bt_tws_delete_pair_timer();
    sys_auto_shut_down_disable();

#ifndef CONFIG_NEW_BREDR_ENABLE
    tws_remote_state_clear();
#endif
}

int bt_tws_phone_connected()
{
    int state;

    printf("bt_tws_phone_connected: %x\n", gtws.state);

#if TCFG_TEST_BOX_ENABLE
    if (chargestore_get_testbox_status()) {
        return 1;
    }
#endif

    gtws.state |= BT_TWS_PHONE_CONNECTED;

    if (gtws.state & BT_TWS_UNPAIRED) {
#if (CONFIG_TWS_PAIR_ALL_WAY == 1)
        if (get_call_status() == BT_CALL_HANGUP) {
            tws_api_wait_pair_by_ble(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
        }
#endif
        return 0;
    }

    if (!(gtws.state & BT_TWS_SIBLING_CONNECTED)) {
        bt_tws_wait_sibling_connect(0);
        return 0;
    }

    /*
     * 获取tws状态，如果正在播歌或打电话则返回1,不播连接成功提示音
     */

    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        tws_api_sync_call_by_uuid('T', SYNC_CMD_LED_PHONE_CONN_STATUS, TWS_SYNC_TIME_DO - 500);   //此时手机已经连接上，同步PHONE_CONN状态

        /* bt_tws_sync_volume(); */
        state = tws_api_get_tws_state();
        if (state & (TWS_STA_SBC_OPEN | TWS_STA_ESCO_OPEN) ||
            (get_call_status() != BT_CALL_HANGUP)) {
            return 1;
        }


        log_info("[SYNC] TONE SYNC");
        tws_api_sync_call_by_uuid('T', SYNC_CMD_PHONE_CONN_TONE, TWS_SYNC_TIME_DO);
    }

    return 1;
}

void bt_tws_phone_disconnected()
{
    gtws.state &= ~BT_TWS_PHONE_CONNECTED;

    printf("bt_tws_phone_disconnected: %x\n", gtws.state);

    sys_auto_shut_down_enable();

    bt_user_priv_var.auto_connection_counter = 0;
    if (!(gtws.state & BT_TWS_SIBLING_CONNECTED)) {
        bt_tws_connect_and_connectable_switch();
    }
}

void bt_tws_phone_page_timeout()
{
    printf("bt_tws_phone_page_timeout: %x\n", gtws.state);

    bt_tws_phone_disconnected();
}


void bt_tws_phone_connect_timeout()
{
    log_d("bt_tws_phone_connect_timeout: %x, %d\n", gtws.state, gtws.pair_timer);

    /*
     * 跟手机超时断开,如果对耳未连接则优先连接对耳
     */
    if (gtws.state & (BT_TWS_UNPAIRED | BT_TWS_SIBLING_CONNECTED)) {
        bt_tws_connect_and_connectable_switch();
    } else {
        bt_tws_connect_sibling(CONFIG_TWS_CONNECT_SIBLING_TIMEOUT);
    }
}

void bt_get_vm_mac_addr(u8 *addr);

void bt_page_scan_for_test()
{
    u8 local_addr[6];

    int state = tws_api_get_tws_state();

    log_info("\n\n\n\n -------------bt test page scan\n");

    bt_tws_delete_pair_timer();

    tws_api_cancle_wait_pair();
    tws_api_cancle_create_connection();
    user_send_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);

    tws_api_detach(TWS_DETACH_BY_POWEROFF);

    user_send_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);

    if (0 == get_total_connect_dev()) {
        log_info("<<<<<<<<<<<<<<<<no phone connect,instant page_scan>>>>>>>>>>>>>>>>\n");
        bt_get_vm_mac_addr(local_addr);
        //log_info("local_adr\n");
        //put_buf(local_addr,6);
        lmp_hci_write_local_address(local_addr);
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
    }

    gtws.state = 0;
}

int bt_tws_poweroff()
{
    log_info("bt_tws_poweroff\n");

    bt_tws_delete_pair_timer();

    tws_api_cancle_wait_pair();
    tws_api_cancle_create_connection();
    user_send_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);

    tws_api_detach(TWS_DETACH_BY_POWEROFF);

    tws_profile_exit();

    if (tws_api_get_tws_state() & TWS_STA_SIBLING_DISCONNECTED) {
        return 1;
    }

    return 0;
}

extern int tws_api_cancle_wait_pair_internal();
void tws_page_scan_deal_by_esco(u8 esco_flag)
{
    if (gtws.state & BT_TWS_UNPAIRED) {
#if (CONFIG_TWS_PAIR_ALL_WAY == 1)
        if (esco_flag) {
            bt_tws_delete_pair_timer();
            tws_api_cancle_wait_pair_internal();
        } else {
            tws_api_wait_pair_by_ble(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
        }
#endif
        return;
    }

    if (esco_flag) {
        bt_tws_delete_pair_timer();
        gtws.state &= ~BT_TWS_CONNECT_SIBLING;
        tws_api_cancle_create_connection();
        tws_api_connect_in_esco();
        puts("close scan\n");
    }

    if (!esco_flag && !(gtws.state & BT_TWS_SIBLING_CONNECTED)) {
        puts("open scan22\n");
        tws_api_cancle_connect_in_esco();
        tws_api_wait_connection(0);
    }
}

/*
 * 解除配对，清掉对方地址信息和本地声道信息
 */
static void bt_tws_remove_pairs()
{
    u8 mac_addr[6];
    char channel = 'U';
    char tws_channel = 0;
    int role = 0;

#if (CONFIG_TWS_USE_COMMMON_ADDR == 0)
    syscfg_write(VM_TWS_ROLE, &role, 1);
#endif

    memset(mac_addr, 0xFF, 6);
    syscfg_write(CFG_TWS_COMMON_ADDR, mac_addr, 6);
    syscfg_write(CFG_TWS_REMOTE_ADDR, mac_addr, 6);
    syscfg_read(CFG_BT_MAC_ADDR, mac_addr, 6);
    lmp_hci_write_local_address(mac_addr);

#if (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_LEFT_START_PAIR) ||\
    (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_RIGHT_START_PAIR) ||\
    (CONFIG_TWS_CHANNEL_SELECT == CONFIG_TWS_MASTER_AS_LEFT)

#if CONFIG_TWS_SECECT_CHARGESTORE_PRIO
    syscfg_read(CFG_CHARGESTORE_TWS_CHANNEL, &tws_channel, 1);
#endif
    if ((tws_channel != 'L') && (tws_channel != 'R')) {
        syscfg_write(CFG_TWS_CHANNEL, &channel, 1);
        tws_api_set_local_channel(channel);
    }
#endif

    tws_api_clear_connect_aa();

    bt_tws_wait_pair_and_phone_connect();

}
#if TCFG_ADSP_UART_ENABLE
extern void adsp_deal_sibling_uart_command(u8 type, u8 cmd);
void tws_sync_uart_command(u8 type, u8 cmd)
{
    struct tws_sync_info_t sync_adsp_uart_command;
    sync_adsp_uart_command.type = TWS_SYNC_ADSP_UART_CMD;
    sync_adsp_uart_command.u.adsp_cmd[0] = type;
    sync_adsp_uart_command.u.adsp_cmd[1] = cmd;
    tws_api_send_data_to_sibling((u8 *)&sync_adsp_uart_command, sizeof(sync_adsp_uart_command));
}
#endif


static void bt_tws_vol_sync(void *_data, u16 len, bool rx)
{
    if (rx) {
        u8 *data = (u8 *)_data;
        app_audio_set_volume(APP_AUDIO_STATE_MUSIC, data[0], 1);
        app_audio_set_volume(APP_AUDIO_STATE_CALL, data[1], 1);
        r_printf("----   bt_tws_sync_volume %d %d \n", data[0], data[1]);
        r_printf("vol: %d", app_audio_get_volume(APP_AUDIO_CURRENT_STATE));
    }

}


REGISTER_TWS_FUNC_STUB(app_vol_sync_stub) = {
    .func_id = TWS_FUNC_ID_VOL_SYNC,
    .func    = bt_tws_vol_sync,
};

void bt_tws_sync_volume()
{
    u8 data[2];

    data[0] = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
    data[1] = app_audio_get_volume(APP_AUDIO_STATE_CALL);

    r_printf("---- tx  bt_tws_sync_volume %d %d \n", data[0], data[1]);

    /* tws_api_send_data_to_slave(data, 2, TWS_FUNC_ID_VOL_SYNC); */
    tws_api_send_data_to_sibling(data, 2, TWS_FUNC_ID_VOL_SYNC);
}

void tws_conn_auto_test(void *p)
{
    cpu_reset();
}

static void bt_tws_box_sync(void *_data, u16 len, bool rx)
{
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    if (rx) {
        u8 *data = (u8 *)_data;
        if (data[0] == TWS_BOX_NOTICE_A2DP_BACK_TO_BT_MODE
            || data[0] == TWS_BOX_A2DP_BACK_TO_BT_MODE_START) {
            tws_local_back_to_bt_mode(data[0], data[1]);
        }
    }
#endif
}

REGISTER_TWS_FUNC_STUB(app_box_sync_stub) = {
    .func_id = TWS_FUNC_ID_BOX_SYNC,
    .func    = bt_tws_box_sync,
};


void tws_user_sync_box(u8 cmd, u8 value)
{
    u8 data[2];

    data[0] = cmd;
    data[1] = value;
    tws_api_send_data_to_sibling(data, 2, TWS_FUNC_ID_BOX_SYNC);
}

#if (CONFIG_TWS_USE_COMMMON_ADDR == 0)
int tws_host_get_local_role()
{
    int role = 0;

    if (syscfg_read(VM_TWS_ROLE, &role, 1) == 1) {
        if (role == TWS_ROLE_SLAVE) {


            //printf("\n\n\nrole is slave\n\n\n");

            return TWS_ROLE_SLAVE;
        }
    }
    return TWS_ROLE_MASTER;
}
#endif

/*
 * tws事件状态处理函数
 */
int bt_tws_connction_status_event_handler(struct bt_event *evt)
{
    u8 addr[4][6];
    int state;
    int pair_suss = 0;
    int role = evt->args[0];
    int phone_link_connection = evt->args[1];
    int reason = evt->args[2];
    u16 random_num = 0;
    char channel = 0;
    u8 mac_addr[6];
    STATUS *p_tone = get_tone_config();
    STATUS *p_led = get_led_config();

    log_info("tws-user: role= %d, phone_link_connection %d, reason=%d,event= %d\n",
             role, phone_link_connection, reason, evt->event);

    switch (evt->event) {
    case TWS_EVENT_CONNECTED:
        r_printf("-----  tws_event_pair_suss: %x\n", gtws.state);



#if (CONFIG_TWS_USE_COMMMON_ADDR == 0)
        lmp_hci_write_local_address(bt_get_mac_addr());
#endif


        syscfg_read(CFG_TWS_REMOTE_ADDR, addr[0], 6);
        syscfg_read(CFG_TWS_COMMON_ADDR, addr[1], 6);
        tws_api_get_sibling_addr(addr[2]);
        tws_api_get_local_addr(addr[3]);

        /*
         * 记录对方地址
         */
        if (memcmp(addr[0], addr[2], 6)) {
            syscfg_write(CFG_TWS_REMOTE_ADDR, addr[2], 6);
            pair_suss = 1;
            log_info("rec tws addr\n");
        }
        if (memcmp(addr[1], addr[3], 6)) {
            syscfg_write(CFG_TWS_COMMON_ADDR, addr[3], 6);
            pair_suss = 1;
            log_info("rec comm addr\n");
#if (CONFIG_TWS_USE_COMMMON_ADDR == 0)
            syscfg_write(VM_TWS_ROLE, &role, 1);
#endif
        }


        if (pair_suss) {
#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
            printf("comm address.....");
            put_buf((void *)addr[0], 6);
            put_buf((void *)addr[1], 6);
            put_buf((void *)addr[2], 6);
            put_buf((void *)addr[3], 6);
            bt_ble_icon_set_comm_address((void *)addr[3]);
#endif
            gtws.state = BT_TWS_PAIRED;

#if CONFIG_TWS_USE_COMMMON_ADDR
            extern void bt_update_mac_addr(u8 * addr);
            bt_update_mac_addr((void *)addr[3]);
#endif
        }

        /*
         * 记录左右声道
         */
        channel = tws_api_get_local_channel();
        if (channel != bt_tws_get_local_channel()) {
            syscfg_write(CFG_TWS_CHANNEL, &channel, 1);
        }
        r_printf("tws_local_channel: %c\n", channel);

#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_AUTO
        if (tws_auto_pair_enable) {
            tws_auto_pair_enable = 0;
            tws_le_acc_generation_init();
        }
#endif

#ifdef CONFIG_NEW_BREDR_ENABLE
        if (channel == 'L') {
            syscfg_read(CFG_TWS_LOCAL_ADDR, addr[2], 6);
        }
        tws_api_set_quick_connect_addr(addr[2]);
#else
        tws_api_set_connect_aa(channel);
        tws_remote_state_clear();
#endif
        if (reason & (TWS_STA_ESCO_OPEN | TWS_STA_SBC_OPEN)) {
            if (role == TWS_ROLE_SLAVE) {
                gtws.state |= BT_TWS_AUDIO_PLAYING;
            }
        }

        if (!phone_link_connection ||
            (reason & (TWS_STA_ESCO_OPEN | TWS_STA_SBC_OPEN))) {
            pwm_led_clk_set(PWM_LED_CLK_BTOSC_24M);
        }
        /*
         * TWS连接成功, 主机尝试回连手机
         */
        if (gtws.connect_time) {
            if (role == TWS_ROLE_MASTER) {
                bt_user_priv_var.auto_connection_counter = gtws.connect_time;
            }
            gtws.connect_time = 0;
        }

        gtws.state &= ~BT_TWS_TIMEOUT;
        gtws.state |= BT_TWS_SIBLING_CONNECTED;

        state = evt->args[2];
        if (role == TWS_ROLE_MASTER) {
            if (!phone_link_connection) { //!(gtws.state & TWS_STA_PHONE_CONNECTED)) {    //还没连上手机
                log_info("[SYNC] LED SYNC");
                tws_api_sync_call_by_uuid('T', SYNC_CMD_LED_TWS_CONN_STATUS, TWS_SYNC_TIME_DO - 500);
            } else {
                tws_api_sync_call_by_uuid('T', SYNC_CMD_LED_PHONE_CONN_STATUS, TWS_SYNC_TIME_DO - 500);
            }
            if (bt_get_exit_flag() || ((get_call_status() == BT_CALL_HANGUP) && !(state & TWS_STA_SBC_OPEN))) {  //通话状态不播提示音
                log_info("send tws_conn_tone_sync 1\n");
                log_info("[SYNC] TONE SYNC");
                tws_api_sync_call_by_uuid('T', SYNC_CMD_TWS_CONN_TONE, TWS_SYNC_TIME_DO / 2);
            }
#if BT_SYNC_PHONE_RING
            if (get_call_status() == BT_CALL_INCOMING) {
                if (bt_user_priv_var.phone_ring_flag && !bt_user_priv_var.inband_ringtone) {
                    printf("<<<<<<<<<phone_con_sync_num_ring \n");
                    bt_user_priv_var.phone_con_sync_ring = 1;//主机来电过程，从机连接加入
#if BT_PHONE_NUMBER
                    bt_user_priv_var.phone_con_sync_num_ring = 1;//主机来电过程，从机连接加入
                    /* tone_file_list_stop(); */
                    bt_tws_sync_phone_num(NULL);
#endif
                }
            }
#endif
            bt_tws_sync_volume();
        }
#if TCFG_CHARGESTORE_ENABLE
        chargestore_sync_chg_level();//同步充电舱电量
#endif
        tws_sync_bat_level(); //同步电量到对耳
        bt_tws_delete_pair_timer();
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        //state&TWS_STA_LOCAL_TWS_OPEN 对方是否开启local_tws
        extern u8 is_local_tws_dec_open();
        r_printf("tws_conn_state=0x%x\n", state);

        u8 cur_task = app_cur_task_check(APP_NAME_BT);

        if (cur_task) {
            TWS_LOCAL_IN_BT();
        } else {
            TWS_LOCAL_OUT_BT();
        }

        if (state & TWS_STA_LOCAL_TWS_OPEN) {
            TWS_REMOTE_OUT_BT();
        } else {
            TWS_REMOTE_IN_BT();
        }


        if (state & TWS_STA_LOCAL_TWS_OPEN) {
            user_set_tws_box_mode(1);
            clear_current_poweron_memory_search_index(0);
            bt_user_priv_var.auto_connection_counter = 0;
            if (!cur_task) {
                gtws.device_role = TWS_ACTIVE_DEIVCE;
                log_info("\n    ------  tws active device 0 ------   \n");
            } else {
                gtws.device_role = TWS_UNACTIVE_DEIVCE;
                log_info("\n    ------  tws unactive device0 ------   \n");
            }
        } else if (!cur_task) {
            clear_current_poweron_memory_search_index(0);
            bt_user_priv_var.auto_connection_counter = 0;
            gtws.device_role = TWS_ACTIVE_DEIVCE;
            log_info("\n    ------  tws active device 1 ------   \n");
        } else {
            if (role == TWS_ROLE_MASTER) {
                gtws.device_role = TWS_ACTIVE_DEIVCE;
                log_info("\n    ------  tws active device 2 ------   \n");
            } else {
                gtws.device_role = TWS_UNACTIVE_DEIVCE;
                log_info("\n    ------  tws unactive device2 ------   \n");
            }

        }
#endif

        if (phone_link_connection == 0) {

            if (role == TWS_ROLE_MASTER) {
                bt_tws_connect_and_connectable_switch();
            } else {
                tws_api_cancle_wait_pair();
            }
        }

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
        bt_ble_icon_slave_en(1);
        if (tws_api_get_role() == TWS_ROLE_MASTER) {
            //master enable
            log_info("master do icon_open\n");
            bt_ble_set_control_en(1);

            if (phone_link_connection) {
                bt_ble_icon_open(ICON_TYPE_RECONNECT);
            } else {
#if (TCFG_CHARGESTORE_ENABLE && !CONFIG_NO_DISPLAY_BUTTON_ICON)
                bt_ble_icon_open(ICON_TYPE_RECONNECT);
#else
                if (ble_icon_contrl.poweron_flag) { //上电标记
                    if (bt_user_priv_var.auto_connection_counter > 0) {
                        //有回连手机动作
                        /* g_printf("ICON_TYPE_RECONNECT"); */
                        /* bt_ble_icon_open(ICON_TYPE_RECONNECT); //没按键配对的话，等回连成功的时候才显示电量。如果在这里显示，手机取消配对后耳机开机，会显示出按键的界面*/
                    } else {
                        //没有回连，设可连接
                        /* g_printf("ICON_TYPE_INQUIRY"); */
                        bt_ble_icon_open(ICON_TYPE_INQUIRY);
                    }

                }
#endif
            }
        } else {
            //slave disable
            bt_ble_set_control_en(0);
        }
        ble_icon_contrl.poweron_flag = 0;
#endif

        //ui_update_status(STATUS_BT_TWS_CONN);
        /* pbg_user_set_tws_state(1); */
#if RCSP_ADV_EN
        extern void sync_setting_by_time_stamp(void);
        sync_setting_by_time_stamp();
#endif
        break;

    case TWS_EVENT_SEARCH_TIMEOUT:
        /*
         * 发起配对超时, 等待手机连接
         */
        gtws.state &= ~BT_TWS_SEARCH_SIBLING;
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
        tws_api_set_local_channel(bt_tws_get_local_channel());
        lmp_hci_write_local_address(gtws.addr);
#endif

#if (CONFIG_TWS_PAIR_ALL_WAY == 1)
        if (gtws.state & BT_TWS_UNPAIRED) {
            if (phone_link_connection) {
                tws_api_wait_pair_by_ble(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
            } else {
                bt_tws_connect_and_connectable_switch();
            }
        } else {
            if (phone_link_connection) {
                bt_tws_wait_sibling_connect(0);
            } else {
                bt_tws_connect_and_connectable_switch();
            }
        }
#else
        bt_tws_connect_and_connectable_switch();
#endif
        /* pbg_user_set_tws_state(0); */
        break;
    case TWS_EVENT_CONNECTION_TIMEOUT:
        /*
        * TWS连接超时
        */
        log_info("tws_event_pair_timeout: %x\n", gtws.state);
        /* pbg_user_set_tws_state(0); */

#if TCFG_TEST_BOX_ENABLE
        if (chargestore_get_testbox_status()) {
            break;
        }
#endif

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
        bt_ble_icon_slave_en(0);
#endif

        if (gtws.state & BT_TWS_UNPAIRED) {
            /*
             * 配对超时
             */
            gtws.state &= ~BT_TWS_SEARCH_SIBLING;
#if CONFIG_TWS_PAIR_MODE == CONFIG_TWS_PAIR_BY_CLICK
            tws_api_set_local_channel(bt_tws_get_local_channel());
            lmp_hci_write_local_address(gtws.addr);
#endif

#if (CONFIG_TWS_PAIR_ALL_WAY == 1)
            if (phone_link_connection) {
                tws_api_wait_pair_by_ble(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
            } else {
                bt_tws_connect_and_connectable_switch();
            }
#else
            bt_tws_connect_and_connectable_switch();
#endif
        } else {
            if (phone_link_connection) {
                bt_tws_wait_sibling_connect(0);
            } else {
                if (gtws.state & BT_TWS_TIMEOUT) {
                    gtws.state &= ~BT_TWS_TIMEOUT;
                    gtws.connect_time = bt_user_priv_var.auto_connection_counter;
                    bt_user_priv_var.auto_connection_counter = 0;
                }
                bt_tws_connect_and_connectable_switch();
            }
        }

#if (CONFIG_TWS_USE_COMMMON_ADDR == 0)
        syscfg_read(CFG_BT_MAC_ADDR, mac_addr, 6);
        lmp_hci_write_local_address(mac_addr);
#endif

        break;
    case TWS_EVENT_CONNECTION_DETACH:
        /*
         * TWS连接断开
         */
        log_info("tws_event_connection_detach: state: %x\n", gtws.state);
        /* pbg_user_set_tws_state(0); */

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        if (app_cur_task_check(APP_NAME_BT)) {  //tws断开时清除local_tws标志，防止tws重新配对后状态错误
            user_set_tws_box_mode(0);
        }
#endif

        gtws.device_role = TWS_ACTIVE_DEIVCE;
        app_power_set_tws_sibling_bat_level(0xff, 0xff);

#if TCFG_CHARGESTORE_ENABLE
        chargestore_set_sibling_chg_lev(0xff);
#endif

        if ((!a2dp_get_status()) && (get_call_status() == BT_CALL_HANGUP)) {  //如果当前正在播歌，切回RC会导致下次从机开机回连后灯状态不同步
            pwm_led_clk_set((!TCFG_LOWPOWER_BTOSC_DISABLE) ? PWM_LED_CLK_RC32K : PWM_LED_CLK_BTOSC_24M);
            //pwm_led_clk_set(PWM_LED_CLK_RC32K);
        }

#if TCFG_TEST_BOX_ENABLE
        if (chargestore_get_testbox_status()) {
            break;
        }
#endif

        if (phone_link_connection) {
            ui_update_status(STATUS_BT_CONN);
            extern void power_event_to_user(u8 event);
            power_event_to_user(POWER_EVENT_POWER_CHANGE);
            user_send_cmd_prepare(USER_CTRL_HFP_CMD_UPDATE_BATTARY, 0, NULL);           //对耳断开后如果手机还连着，主动推一次电量给手机
        } else {
            ui_update_status(STATUS_BT_TWS_DISCONN);
        }

        if ((gtws.state & BT_TWS_SIBLING_CONNECTED) && (!app_var.goto_poweroff_flag)) {
#if CONFIG_TWS_POWEROFF_SAME_TIME
            extern u8 poweroff_sametime_flag;
            if (!app_var.goto_poweroff_flag && !poweroff_sametime_flag && !phone_link_connection && (reason != TWS_DETACH_BY_REMOVE_PAIRS)) { /*关机不播断开提示音*/
                tone_play_index(p_tone->tws_disconnect, 1);
            }
#else
            if (!app_var.goto_poweroff_flag && !phone_link_connection && (reason != TWS_DETACH_BY_REMOVE_PAIRS)) {
                tone_play_index(p_tone->tws_disconnect, 1);
            }
#endif
            gtws.state &= ~BT_TWS_SIBLING_CONNECTED;

#if (CONFIG_TWS_USE_COMMMON_ADDR == 0)
            syscfg_read(CFG_BT_MAC_ADDR, mac_addr, 6);
            lmp_hci_write_local_address(mac_addr);
#endif
        }

        if (reason == TWS_DETACH_BY_REMOVE_PAIRS) {
            gtws.state = BT_TWS_UNPAIRED;
            printf("<<<<<<<<<<<<<<<<<<<<<<tws detach by remove pairs>>>>>>>>>>>>>>>>>>>\n");
#if (CONFIG_TWS_PAIR_ALL_WAY == 1)
            tws_api_wait_pair_by_ble(bt_get_tws_device_indicate(NULL), bt_get_local_name(), 0);
#endif
            break;
        }

        if (get_esco_coder_busy_flag()) {
            tws_api_connect_in_esco();
            break;
        }

        //非测试盒在仓测试，直连蓝牙
        if (reason == TWS_DETACH_BY_TESTBOX_CON) {
            puts(">>>>>>>>>>>>>>>>TWS_DETACH_BY_TESTBOX_CON<<<<<<<<<<<<<<<<<<<\n");
            gtws.state &= ~BT_TWS_PAIRED;
            gtws.state |= BT_TWS_UNPAIRED;
            //syscfg_read(CFG_BT_MAC_ADDR, mac_addr, 6);
            if (!phone_link_connection) {
#if CONFIG_TWS_USE_COMMMON_ADDR
                get_random_number(mac_addr, 6);
                lmp_hci_write_local_address(mac_addr);
#endif

                bt_tws_wait_pair_and_phone_connect();
            }
            break;
        }

        if (phone_link_connection) {
            bt_tws_wait_sibling_connect(0);
        } else {
            if (reason == TWS_DETACH_BY_SUPER_TIMEOUT) {
                if (role == TWS_ROLE_SLAVE) {
                    gtws.state |= BT_TWS_TIMEOUT;
                    gtws.connect_time = bt_user_priv_var.auto_connection_counter;
                    bt_user_priv_var.auto_connection_counter = 0;
                }
                bt_tws_connect_sibling(6);
            } else {
                bt_tws_connect_and_connectable_switch();
            }
        }
        break;
    case TWS_EVENT_PHONE_LINK_DETACH:
        /*
         * 跟手机的链路LMP层已完全断开, 只有tws在连接状态才会收到此事件
         */
        printf("tws_event_phone_link_detach: %x\n", gtws.state);

#if TCFG_TEST_BOX_ENABLE
        if (chargestore_get_testbox_status()) {
            break;
        }
#endif

        sys_auto_shut_down_enable();

        if ((reason != 0x08) && (reason != 0x0b))   {
            bt_user_priv_var.auto_connection_counter = 0;
        } else {
            if (reason == 0x0b) {
                bt_user_priv_var.auto_connection_counter = (8 * 1000);
            }
        }


#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
        if (reason == 0x0b) {
            //CONNECTION ALREADY EXISTS
            ble_icon_contrl.miss_flag = 1;
        } else {
            ble_icon_contrl.miss_flag = 0;
        }
#endif

        bt_tws_connect_and_connectable_switch();
        break;
    case TWS_EVENT_REMOVE_PAIRS:
        log_info("tws_event_remove_pairs\n");
        tone_play_index(p_tone->tws_disconnect, 1);
        bt_tws_remove_pairs();
        app_power_set_tws_sibling_bat_level(0xff, 0xff);
#if TCFG_CHARGESTORE_ENABLE
        chargestore_set_sibling_chg_lev(0xff);
#endif
        pwm_led_clk_set((!TCFG_LOWPOWER_BTOSC_DISABLE) ? PWM_LED_CLK_RC32K : PWM_LED_CLK_BTOSC_24M);
        //pwm_led_clk_set(PWM_LED_CLK_RC32K);
        //tone_play(TONE_TWS_DISCONN);
        /* pbg_user_set_tws_state(0); */
        break;

    case TWS_EVENT_SYNC_FUN_CMD:
        log_d("TWS_EVENT_SYNC_FUN_CMD: %x\n", reason);
        /*
         * 主从同步调用函数处理
         */
        if (reason == SYNC_CMD_TWS_CONN_TONE) {
            tone_play_index(p_tone->tws_connect_ok, 1);
        } else if (reason == SYNC_CMD_PHONE_CONN_TONE) {
            //ui_update_status(STATUS_BT_CONN);
            /* if(is_tws_all_in_bt()){ */
            tone_play_index(p_tone->bt_connect_ok, 1);
            /* } */
        } else if (reason == SYNC_CMD_PHONE_NUM_TONE) {

            if (bt_user_priv_var.phone_con_sync_num_ring) {
                break;
            }
            if (tws_api_get_role() == TWS_ROLE_SLAVE) {
                if (bt_user_priv_var.phone_timer_id) {
                    sys_timeout_del(bt_user_priv_var.phone_timer_id);
                    bt_user_priv_var.phone_timer_id = 0;
                }
            }
            if (bt_user_priv_var.phone_ring_flag) {
                phone_num_play_timer(NULL);
            }
        } else if (reason == SYNC_CMD_PHONE_RING_TONE) {
            if (bt_user_priv_var.phone_ring_flag) {
                if (bt_user_priv_var.phone_con_sync_ring) {
                    if (phone_ring_play_start()) {
                        bt_user_priv_var.phone_con_sync_ring = 0;
                        if (tws_api_get_role() == TWS_ROLE_MASTER) {
                            //播完一声来电铃声之后,关闭aec、msbc_dec之后再次同步播来电铃声
                            /* tws_api_sync_call_by_uuid('T', SYNC_CMD_PHONE_RING_TONE, TWS_SYNC_TIME_DO * 2 + 400); */
                        }
                    }
                } else {
                    phone_ring_play_start();
                }
            }
        } else if (reason == SYNC_CMD_PHONE_SYNC_NUM_RING_TONE) {
            if (bt_user_priv_var.phone_ring_flag) {
                if (phone_ring_play_start()) {
                    if (tws_api_get_role() == TWS_ROLE_MASTER) {
                        //播完一声来电铃声之后,关闭aec、msbc_dec之后再次同步播来电铃声
                        bt_user_priv_var.phone_con_sync_ring = 0;
                        /* tws_api_sync_call_by_uuid('T', SYNC_CMD_PHONE_RING_TONE, TWS_SYNC_TIME_DO * 2 + 400); */
                    }
                    bt_user_priv_var.phone_con_sync_num_ring = 1;

                }
            }

        } else if (reason == SYNC_CMD_LED_TWS_CONN_STATUS) {
            pwm_led_mode_set(PWM_LED_ALL_OFF);
            ui_update_status(STATUS_BT_TWS_CONN);
        } else if (reason == SYNC_CMD_LED_PHONE_CONN_STATUS) {
            pwm_led_mode_set(PWM_LED_ALL_OFF);
            ui_update_status(STATUS_BT_CONN);
        } else if (reason == SYNC_CMD_LED_PHONE_DISCONN_STATUS) {
            ui_update_status(STATUS_BT_TWS_CONN);
            if (!app_var.goto_poweroff_flag) { /*关机不播断开提示音*/
                /* tone_play(TONE_DISCONN); */
                if (is_tws_all_in_bt()) {
                    tone_play_index(p_tone->bt_disconnect, 1);
                }
            }
        } else if (reason == SYNC_CMD_POWER_OFF_TOGETHER) {
            //tone_play_index(p_tone->power_off);
            extern void sys_enter_soft_poweroff(void *priv);
            sys_enter_soft_poweroff(NULL);
        } else if (reason == SYNC_CMD_MAX_VOL) {
#if TCFG_MAX_VOL_PROMPT
            STATUS *p_tone = get_tone_config();
            if (p_tone->max_vol != IDEX_TONE_NONE) {
                tone_play_index(p_tone->max_vol, 0);
            } else {
                /* tone_sin_play(150, 1); */
            }
#endif
#if (defined(TCFG_TONE2TWS_ENABLE) && (TCFG_TONE2TWS_ENABLE))
        } else if ((reason >= SYNC_CMD_MODE_START) && (reason < SYNC_CMD_MODE_STOP)) {
            app_mode_tone_play_by_tws(reason);
#endif

        } else if (reason == SYNC_CMD_LOW_LATENCY_ENABLE) {
            earphone_a2dp_codec_set_low_latency_mode(1);
        } else if (reason == SYNC_CMD_LOW_LATENCY_DISABLE) {
            earphone_a2dp_codec_set_low_latency_mode(0);
        } else if (reason == SYNC_CMD_EARPHONE_CHAREG_START) {
            if (a2dp_get_status() != BT_MUSIC_STATUS_STARTING) {
                g_printf("TWS MUSIC PLAY");
                user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
            }
        } else if (reason == SYNC_CMD_IRSENSOR_EVENT_NEAR) {
            g_printf("TWS NEAR START MUSIC PLAY");
            g_printf("a2dp_get_status = %d", a2dp_get_status());
            if (a2dp_get_status() != BT_MUSIC_STATUS_STARTING) {
                r_printf("START...\n");
                user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
            }
        } else if (reason == SYNC_CMD_IRSENSOR_EVENT_FAR) {
            g_printf("TWS FAR STOP MUSIC PLAY");
            g_printf("a2dp_get_status = %d", a2dp_get_status());
            if (a2dp_get_status() == BT_MUSIC_STATUS_STARTING) {
                r_printf("STOP...\n");
                user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PAUSE, 0, NULL);
            }
        }
        break;
    case TWS_EVENT_SYNC_FUN_TRANID:
        log_d("--- TWS_EVENT_SYNC_FUN_TRANID: %x\n", reason);

        if (reason == SYNC_CMD_BOX_EXIT_BT) {
            user_set_tws_box_mode(1);
            r_printf("\n    ------   TWS_UNACTIVE_DEIVCE \n");
            TWS_LOCAL_IN_BT();
            TWS_REMOTE_OUT_BT();
            gtws.device_role =  TWS_UNACTIVE_DEIVCE;
            tws_local_back_to_bt_mode(TWS_BOX_EXIT_BT, gtws.device_role);
        } else if (reason == SYNC_CMD_BOX_INIT_EXIT_BT) {
            TWS_LOCAL_OUT_BT();
            TWS_REMOTE_IN_BT();
            user_set_tws_box_mode(2);
            r_printf("\n    ------   TWS_ACTIVE_DEIVCE \n");
            gtws.device_role =  TWS_ACTIVE_DEIVCE;
            tws_local_back_to_bt_mode(TWS_BOX_EXIT_BT, gtws.device_role);
        } else if (reason == SYNC_CMD_BOX_ENTER_BT) {
            r_printf("----  SYNC_CMD_BOX_ENTER_BT");
            TWS_LOCAL_IN_BT();
            TWS_REMOTE_IN_BT();
            user_set_tws_box_mode(0);
            tws_local_back_to_bt_mode(TWS_BOX_ENTER_BT, gtws.device_role);
        }
        break;

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    case TWS_EVENT_LOCAL_MEDIA_START:
        puts("TWS_EVENT_LOCAL_MEDIA_START\n");
        if (a2dp_dec_close()) {
            /* tws_api_local_media_trans_clear(); */
        }
        local_tws_dec_create();
        local_tws_dec_open(evt->value);
        break;
    case TWS_EVENT_LOCAL_MEDIA_STOP:
        puts("TWS_EVENT_LOCAL_MEDIA_STOP\n");
        if (local_tws_dec_close(0)) {
        }
        break;
#endif

    case TWS_EVENT_ROLE_SWITCH:
        if (role == TWS_ROLE_MASTER) {

        } else {

        }
        break;
    case TWS_EVENT_ESCO_ADD_CONNECT:
        if (role == TWS_ROLE_MASTER) {
            bt_tws_sync_volume();
        }
        break;
    }
    return 0;
}

void tws_cancle_all_noconn()
{

    bt_tws_delete_pair_timer();
    tws_api_cancle_wait_pair();
    tws_api_cancle_create_connection();
    user_send_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
}


bool get_tws_sibling_connect_state(void)
{
    if (gtws.state & BT_TWS_SIBLING_CONNECTED) {
        return TRUE;
    }
    return FALSE;
}


bool get_tws_phone_connect_state(void)
{
    if (gtws.state & BT_TWS_PHONE_CONNECTED) {
        return TRUE;
    }
    return FALSE;
}


#endif
