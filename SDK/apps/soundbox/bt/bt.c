#include "system/includes.h"
#include "media/includes.h"
#include "tone_player.h"

#include "app_config.h"
#include "app_action.h"

#include "btstack/avctp_user.h"
#include "btstack/ble_api.h"
#include "btstack/btstack_task.h"
#include "btctrler/btctrler_task.h"
#include "user_cfg.h"
#include "aec_user.h"
#include "lmp_api.h"
#include "audio_reverb.h"
#include "bt_common.h"

#include "ui/ui_api.h"
#include "fm_emitter/fm_emitter_manage.h"

#include "bt_tws.h"

#include "bt_ble.h"

#include "app_chargestore.h"

#include "asm/charge.h"
#include "app_charge.h"
#include "ui_manage.h"

#include "app_chargestore.h"
#include "app_online_cfg.h"
#include "app_main.h"
#include "app_power_manage.h"
#include "gSensor/gSensor_manage.h"
#include "key_event_deal.h"
#include "classic/tws_api.h"
#include "asm/pwm_led.h"

#include "vol_sync.h"

#include "math.h"

#include "dma_deal.h"
#include "rcsp_bluetooth.h"
#include "clock_cfg.h"
#include "rcsp_adv_bluetooth.h"
#include "loud_speaker.h"

#define LOG_TAG_CONST       EARPHONE
#define LOG_TAG             "[EARPHONE]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
#define BACKGROUND_GOBACK         1   //后台链接是否跳回蓝牙 1：跳回
#else
#define BACKGROUND_GOBACK         1   //后台链接是否跳回蓝牙 1：跳回
#endif

#define TIMEOUT_CONN_TIME         60 //超时断开之后回连的时间s
#define POWERON_AUTO_CONN_TIME    12  //开机去回连的时间
#define TWS_RETRY_CONN_TIMEOUT    ((rand32() & BIT(0)) ? 200 : 400)
#define PHONE_CALL_FORCE_POWEROFF 0

//#define PHONE_CALL_DEFAULT_MAX_VOL

extern u8 get_esco_coder_busy_flag();
extern void dac_power_off(void);
extern void bredr_fcc_init(u8 mode);
extern void bredr_set_dut_enble(u8 en);
extern void bt_production_test(u8 en);
extern void bt_set_rxtx_status_enable(u8 en);
extern void set_wait_a2dp_start(u8 flag);

extern void emitter_search_noname(u8 status, u8 *addr, u8 *name);

extern u8 bt_emitter_stu_set(u8 on);
extern u8 bt_emitter_stu_get(void);
extern int bt_emitter_mic_open(void);
extern void bt_emitter_mic_close(void);

void sys_auto_shut_down_disable(void);

BT_USER_COMM_VAR bt_user_comm_var;
BT_USER_PRIV_VAR bt_user_priv_var;

int phone_call_begin(void *priv);
int phone_call_end(void *priv);
int earphone_a2dp_audio_codec_open(int media_type);
void earphone_a2dp_audio_codec_close();
extern int earphone_a2dp_codec_get_low_latency_mode();
extern void earphone_a2dp_codec_set_low_latency_mode(int enable);
u8 bt_audio_is_running(void);
u8 bt_media_is_running(void);
u8 bt_phone_dec_is_running();
void tws_try_connect_disable(void);
u8 hci_standard_connect_check(void);
extern int lmp_private_esco_suspend_resume(int flag);
extern void bt_pll_para(u32 osc, u32 sys, u8 low_power, u8 xosc);
void set_stack_exiting(u8 exit);
void user_set_tws_box_mode(u8 mode);

int a2dp_media_packet_codec_type(u8 *data);
int a2dp_dec_open(int media_type);
int a2dp_dec_close();
int esco_dec_open(void *, u8);
void esco_dec_close();
int esco_enc_open(void *param);
void esco_enc_close();
static int bt_must_work(void);

extern void tws_conn_switch_role();

extern int local_tws_dec_create(void);
extern int local_tws_dec_open(u32 dec_type);
extern int local_tws_dec_close(u8 drop_frame_start);

extern int dec2tws_tws_event_deal(struct bt_event *evt);

#define SBC_FILTER_TIME_MS			1000	//后台音频过滤时间ms
#define SBC_ZERO_TIME_MS			500		//静音多长时间认为已经退出
#define NO_SBC_TIME_MS				200		//无音频时间ms
extern u32 timer_get_ms(void);

/* #if TCFG_BLUETOOTH_BACK_MODE */
struct app_bt_opr {
    u8 init_ok : 1;		// 1-初始化完成
    u8 call_flag : 1;	// 1-由于蓝牙打电话命令切回蓝牙模式
    u8 exit_flag : 1;	// 1-可以退出蓝牙标志
    u8 exiting : 1;	// 1-正在退出蓝牙模式
    u8 wait_exit : 1;	// 1-等退出蓝牙模式
    u8 a2dp_decoder_type: 3;	// 从后台返回时记录解码格式用
    u8 cmd_flag ;	// 1-由于蓝牙命令切回蓝牙模式
    u8 ignore_discon_tone;	// 1-退出蓝牙模式， 不响应discon提示音
    u8 sbc_packet_valid_cnt;	// 有效sbc包计数
    u8 sbc_packet_valid_cnt_max;// 最大有效sbc包计数
    u8 sbc_packet_lose_cnt;	// sbc丢失的包数
    u8 sbc_packet_step;	// 0-正常；1-退出中；2-后台
    u32 sbc_packet_filter_to;	// 过滤超时
    u32 no_sbc_packet_to;	// 无声超时
    u32 init_ok_time;	// 初始化完成时间
    u32 auto_exit_limit_time;	// 自动退出时间限制
    int timer;
    int tmr_cnt;
    u8 tws_local_back_role;
    u8 a2dp_start_flag;
    int back_mode_systime;
};
static struct app_bt_opr app_bt_hdl = {.exit_flag = 1};
#define __this 	(&app_bt_hdl)
static DEFINE_SPINLOCK(lock);
/* #endif */

#define SYS_BT_EVENT_FROM_KEY       (('K' << 24) | ('E' << 16) | ('Y' << 8) | '\0')

u8 bt_back_flag = 0;		// 1:auto exit  2:back exit
static void bt_auto_exit(void)
{
#if TWFG_APP_POWERON_IGNORE_DEV
    log_info(">>>>ok time:%d, cur:%d auto_exit:%d\n", __this->init_ok_time, timer_get_ms(), __this->auto_exit_limit_time);
    if ((__this->init_ok_time == 0)
        || ((timer_get_ms() - __this->init_ok_time) < __this->auto_exit_limit_time)
       ) {
        return ;
    }
#endif
    if (get_total_connect_dev()) {
        return ;
    }
    log_info("bt_auto_exit \n");
    if (true == app_cur_task_check(APP_NAME_BT)) {
        bt_back_flag = 1;
        struct sys_event e = {0};
        e.type = SYS_BT_EVENT;
        e.arg = (void *)SYS_BT_EVENT_FROM_KEY;
        e.u.key.event = KEY_CHANGE_MODE;
        sys_event_notify(&e);
        /* log_info("KEY_CHANGE_MODE\n"); */
    }
}


static int bt_switch_back(void *p)
{
    if (!__this->call_flag) {
        return 0;
    }

    if (bt_must_work()) {
        sys_timeout_add(NULL, bt_switch_back, 10);
        return 0;
    }

    if (bt_phone_dec_is_running()) {
        sys_timeout_add(NULL, bt_switch_back, 10);
        return 0;
    }
    __this->call_flag = 0;
    bt_back_flag = 2;
    app_task_switch_last();
    return 0;
}

u8 get_a2dp_start_flag()
{
    return __this->a2dp_start_flag;
}

///////////////////////////////////////////////////
void a2dp_media_free_packet(void *_packet);
int a2dp_media_get_packet(u8 **frame);
void __a2dp_drop_frame(void *p);
u8 is_local_tws_dec_open();
u8  is_a2dp_dec_open();
extern int a2dp_media_get_packet_num();

static u32 bt_timer = 0;

static void bt_a2dp_drop_frame(void *p)
{
    int len;
    u8 *frame;
    /* putchar('@'); */
    __a2dp_drop_frame(NULL);

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    if (tone_get_status() || is_local_tws_dec_open()) {
#else
    if (tone_get_status()) {
#endif
        bt_timer = sys_timeout_add(NULL, bt_a2dp_drop_frame, 100);
    } else {
        local_irq_disable();
        bt_timer = 0;
        local_irq_enable();
    }

}

void bt_drop_a2dp_frame_start(void)
{
    log_info("bt_drop_frame_start %d\n", bt_timer);
    local_irq_disable();
    if (bt_timer == 0) {
        bt_timer = sys_timeout_add(NULL, bt_a2dp_drop_frame, 100);
    }
    local_irq_enable();

}

static void bt_drop_a2dp_frame_stop()
{
    log_info("bt_drop_frame_stop %d\n", bt_timer);
    local_irq_disable();
    if (bt_timer) {
        sys_timeout_del(bt_timer);
        bt_timer = 0;
    }
    local_irq_enable();
}

/*******************************************************
 *
 *  蓝牙模式播放提示音接口，防止提示音播放时候打开 a2dp 解码不及时，
 *  导致底层 a2dp buff 阻塞，手机超时断开蓝牙.
 *
 * ****************************************************/
int bt_tone_play_index(u8 index, u8 preemption, void *priv)
{
    bt_drop_a2dp_frame_start();
    return tone_play_index_with_callback(index, preemption, NULL, priv);
}
///////////////////////////////////////////////////


//传0时用上一个保存的状态
void bt_set_led_status(u8 status)
{
    static u8 bt_status = STATUS_BT_INIT_OK;
    struct application *app = get_current_app();
    if (status) {
        bt_status = status;
    }
    if (app == NULL || !strcmp(app->name, APP_NAME_BT)) { //蓝牙模式或者state_machine
        pwm_led_mode_set(PWM_LED_ALL_OFF);
        ui_update_status(bt_status);
    }
}

static u8 bt_search_busy = 0;
void bt_search_device(void)
{
    if (! __this->init_ok) {
        log_info("bt on init >>>>>>>>>>>>>>>>>>>>>>>\n");
        return;
    }
    if (bt_search_busy) {
        log_info("bt_search_busy >>>>>>>>>>>>>>>>>>>>>>>\n");
        //return;
    }
    bt_search_busy = 1;
    u8 inquiry_length = 20;   // inquiry_length * 1.28s
    user_send_cmd_prepare(USER_CTRL_SEARCH_DEVICE, 1, &inquiry_length);
    log_info("bt_search_start >>>>>>>>>>>>>>>>>>>>>>>\n");
}

u8 bt_search_status()
{
    return bt_search_busy;
}

/*开关可发现可连接的函数接口*/
void bt_wait_phone_connect_control(u8 enable)
{
#if TCFG_USER_TWS_ENABLE
    return;
#endif

    if (enable && app_var.goto_poweroff_flag && (__this->exiting)) {
        return;
    }

    if (enable) {
        log_info("is_1t2_connection:%d \t total_conn_dev:%d\n", is_1t2_connection(), get_total_connect_dev());
        if (is_1t2_connection()) {
            /*达到最大连接数，可发现(0)可连接(0)*/
            user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
            user_send_cmd_prepare(USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);
        } else {
            if (get_total_connect_dev() == 1) {
                /*支持连接2台，只连接一台的情况下，可发现(0)可连接(1)*/
                user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
                user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
            } else {
                /*可发现(1)可连接(1)*/
#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
#if (TCFG_USER_TWS_ENABLE == 0)
                bt_ble_icon_open(ICON_TYPE_INQUIRY);
#endif
#endif
                user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_ENABLE, 0, NULL);
                user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
            }
        }
    } else {
        user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);
    }
}

void bt_send_keypress(u8 key)
{
    user_send_cmd_prepare(USER_CTRL_KEYPRESS, 1, &key);
}

void bt_send_pair(u8 en)
{
    user_send_cmd_prepare(USER_CTRL_PAIR, 1, &en);
}

void bt_init_ok_search_index(void)
{
    if (!bt_user_priv_var.auto_connection_counter && get_current_poweron_memory_search_index(bt_user_priv_var.auto_connection_addr)) {
        log_info("bt_wait_connect_and_phone_connect_switch\n");
        clear_current_poweron_memory_search_index(1);
        bt_user_priv_var.auto_connection_counter = POWERON_AUTO_CONN_TIME * 1000; //8000ms
    }
}

int bt_wait_connect_and_phone_connect_switch(void *p)
{
    int ret = 0;
    int timeout = 0;
    s32 wait_timeout;

    if (__this->exiting) {
        return 0;
    }

    if (get_remote_test_flag()) {
        return 0;
    }


    log_info("connect_switch: %d, %d\n", (int)p, bt_user_priv_var.auto_connection_counter);

    if (bt_user_priv_var.auto_connection_counter <= 0) {
        bt_user_priv_var.auto_connection_timer = 0;
        bt_user_priv_var.auto_connection_counter = 0;
        user_send_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
        if (get_current_poweron_memory_search_index(NULL)) {
            bt_init_ok_search_index();
            return bt_wait_connect_and_phone_connect_switch(0);
        } else {
            bt_wait_phone_connect_control(1);
            bt_auto_exit();
            return 0;
        }
    }
    /* log_info(">>>phone_connect_switch=%d\n",bt_user_priv_var.auto_connection_counter ); */
    if ((int)p == 0) {
        if (bt_user_priv_var.auto_connection_counter) {
            timeout = 6000;
            bt_wait_phone_connect_control(0);
            user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6, bt_user_priv_var.auto_connection_addr);
            ret = 1;
        }
    } else {
        timeout = 2000;
        user_send_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
        bt_wait_phone_connect_control(1);
    }
    if (bt_user_priv_var.auto_connection_counter) {
        bt_user_priv_var.auto_connection_counter -= timeout ;
        log_info("do=%d\n", bt_user_priv_var.auto_connection_counter);
    }
    bt_user_priv_var.auto_connection_timer = sys_timeout_add((void *)(!(int)p),
            bt_wait_connect_and_phone_connect_switch, timeout);

    return ret;
}

void bt_close_page_scan(void *p)
{
    log_info(">>>%s\n", __func__);
    bt_wait_phone_connect_control(0);
    sys_timer_del(app_var.auto_stop_page_scan_timer);
}


extern bool get_esco_busy_flag();
extern u8  get_cur_battery_level(void);
static int bt_get_battery_value()
{
    //取消默认蓝牙定时发送电量给手机，需要更新电量给手机使用USER_CTRL_HFP_CMD_UPDATE_BATTARY命令
    /*电量协议的是0-9个等级，请比例换算*/
    return get_cur_battery_level();
}

static void bt_fast_test_api(void)
{
    log_info("------------bt_fast_test_api---------\n");
    //进入快速测试模式，用户根据此标志判断测试，如测试按键-开按键音  、测试mic-开扩音 、关闭蓝牙进入powerdown、关闭可发现可连接
    bt_user_priv_var.fast_test_mode = 1;
    /* sin_tone_toggle(1);//enable key_tone */

    /* sound_automute_set(0, -1, -1, -1); // 关自动mute */
    /* dac_toggle(1); */
    /* set_sys_vol(20, 20, FADE_OFF);   */
    /* microphone_open(20, 0); */
#ifdef AUDIO_MIC_TEST
    mic_test_start();
#endif
}

static void bt_dut_api(void)
{
    log_info("bt in dut \n");
    sys_auto_shut_down_disable();
#if TCFG_USER_TWS_ENABLE
    extern 	void tws_cancle_all_noconn();
    tws_cancle_all_noconn() ;
#endif
#if TCFG_USER_BLE_ENABLE
    bt_ble_adv_enable(0);
#endif
}

void bt_fix_fre_api()
{
    bt_dut_api();
    bredr_fcc_init(BT_FRE);
}

void spp_data_handler(u8 packet_type, u16 ch, u8 *packet, u16 size)
{
    switch (packet_type) {
    case 1:
        log_info("spp connect\n");
        break;
    case 2:
        log_info("spp disconnect\n");
        break;
    case 7:
        //log_info("spp_rx:");
        //put_buf(packet,size);
#if AEC_DEBUG_ONLINE
        aec_debug_online(packet, size);
#endif
        break;
    }
}

static void bt_set_music_device_volume(int volume)
{
    if (true == app_cur_task_check(APP_NAME_BT)) {
        set_music_device_volume(volume);
    }
}

static void bt_read_remote_name(u8 status, u8 *addr, u8 *name)
{
    if (status) {
        printf("remote_name fail \n");
    } else {
        printf("remote_name : %s \n", name);
    }

    put_buf(addr, 6);

#if TCFG_USER_EMITTER_ENABLE
    emitter_search_noname(status, addr, name);
#endif
}


extern void user_spp_data_handler(u8 packet_type, u16 ch, u8 *packet, u16 size);
extern void bt_set_tx_power(u8 txpower);
void bredr_handle_register()
{
#if (GMA_EN || TRANS_DATA_EN)
    spp_data_deal_handle_register(user_spp_data_handler);
#else
    spp_data_deal_handle_register(spp_data_handler);
#endif

    bt_fast_test_handle_register(bt_fast_test_api);//测试盒快速测试接口
#if BT_SUPPORT_MUSIC_VOL_SYNC
    music_vol_change_handle_register(bt_set_music_device_volume, phone_get_device_vol);
#endif
#if BT_SUPPORT_DISPLAY_BAT
    get_battery_value_register(bt_get_battery_value);   /*电量显示获取电量的接口*/
#endif

    bt_dut_test_handle_register(bt_dut_api);

    read_remote_name_handle_register(bt_read_remote_name);

#if TCFG_USER_EMITTER_ENABLE
    void bt_emitter_init() ;
    bt_emitter_init();
    extern u8 emitter_search_result(char *name, u8 name_len, u8 * addr, u32 dev_class, char rssi);
    inquiry_result_handle_register(emitter_search_result);
    extern void emitter_or_receiver_switch(u8 flag);
    emitter_or_receiver_switch(BT_EMITTER_EN);
#endif
}

extern void transport_spp_init(void);
extern void lib_make_ble_address(u8 *ble_address, u8 *edr_address);
extern int le_controller_set_mac(void *addr);
extern void bt_set_ldos(u8 mode);
void bt_function_select_init()
{
    __set_user_ctrl_conn_num(TCFG_BD_NUM);
    __set_support_msbc_flag(1);
#if TCFG_BT_SUPPORT_AAC
    __set_support_aac_flag(1);
#else
    __set_support_aac_flag(0);
#endif

#if BT_SUPPORT_DISPLAY_BAT
    __bt_set_update_battery_time(60);
#else
    __bt_set_update_battery_time(0);
#endif
    __set_page_timeout_value(8000); /*回连搜索时间长度设置,可使用该函数注册使用，ms单位,u16*/
    __set_super_timeout_value(8000); /*回连时超时参数设置。ms单位。做主机有效*/
#if (TCFG_BD_NUM == 2)
    __set_auto_conn_device_num(2);
#endif
#if BT_SUPPORT_MUSIC_VOL_SYNC
    vol_sys_tab_init();
#endif

    __set_user_background_goback(BACKGROUND_GOBACK); // 后台链接是否跳回蓝牙 1:跳回

    //io_capabilities ; /*0: Display only 1: Display YesNo 2: KeyboardOnly 3: NoInputNoOutput*/
    //authentication_requirements: 0:not protect  1 :protect
    __set_simple_pair_param(3, 0, 2);

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
    bt_set_ldos(0);
#endif


#if TCFG_BT_SNIFF_DISABLE
    void lmp_set_sniff_disable(void);
    lmp_set_sniff_disable();
#endif


    /*
                TX     RX
       AI800x   PA13   PA12
       AC692x   PA13   PA12
       AC693x   PA8    PA9
       AC695x   PA9    PA10
       AC694x   PB1    PB2
    */
    /* bt_set_rxtx_status_enable(1); */

#if TCFG_USER_BLE_ENABLE
    {
        u8 tmp_ble_addr[6];
#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
        /* bt_set_tx_power(9);//ble txpwer level:0~9 */
        memcpy(tmp_ble_addr, (void *)bt_get_mac_addr(), 6);
#else
        lib_make_ble_address(tmp_ble_addr, (void *)bt_get_mac_addr());
#endif //
        le_controller_set_mac((void *)tmp_ble_addr);
        printf("\n-----edr + ble 's address-----");
        printf_buf((void *)bt_get_mac_addr(), 6);
        printf_buf((void *)tmp_ble_addr, 6);
    }
#endif // TCFG_USER_BLE_ENABLE

#if (USER_SUPPORT_PROFILE_SPP==1)
#if (TRANS_DATA_EN)
    transport_spp_init();
#elif (RCSP_BTMATE_EN || RCSP_ADV_EN)
    rcsp_init();
#endif
#endif
}

void phone_sync_vol(void)
{
    user_send_cmd_prepare(USER_CTRL_HFP_CALL_SET_VOLUME, 1, &bt_user_priv_var.phone_vol);
}

/*配置通话时前面丢掉的数据包包数*/
#define ESCO_DUMP_PACKET_ADJUST		1	/*配置使能*/
#define ESCO_DUMP_PACKET_DEFAULT	0
#define ESCO_DUMP_PACKET_CALL		120 /*0~0xFF*/
static u8 esco_dump_packet = ESCO_DUMP_PACKET_CALL;
#if ESCO_DUMP_PACKET_ADJUST
u8 get_esco_packet_dump(void)
{
    //log_info("esco_dump_packet:%d\n", esco_dump_packet);
    return esco_dump_packet;
}
#endif


#define  SNIFF_CNT_TIME               5/////<空闲5S之后进入sniff模式

#define SNIFF_MAX_INTERVALSLOT        800
#define SNIFF_MIN_INTERVALSLOT        100
#define SNIFF_ATTEMPT_SLOT            4
#define SNIFF_TIMEOUT_SLOT            1

int exit_sniff_timer = 0;
void bt_check_exit_sniff()
{
    sys_timeout_del(exit_sniff_timer);
    exit_sniff_timer = 0;

#if CONFIG_USER_TWS_ENABLE
    if (tws_api_get_role() == TWS_ROLE_SLAVE) {
        return;
    }
#endif
    user_send_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL);
}


void bt_check_enter_sniff()
{
    struct sniff_ctrl_config_t sniff_ctrl_config;
    u8 addr[12];
    u8 conn_cnt = 0;
    u8 i = 0;
    /*putchar('H');*/
    conn_cnt = bt_api_enter_sniff_status_check(SNIFF_CNT_TIME, addr);

    ASSERT(conn_cnt <= 2);

    for (i = 0; i < conn_cnt; i++) {
        log_info("-----USER SEND SNIFF IN %d %d\n", i, conn_cnt);
        sniff_ctrl_config.sniff_max_interval = SNIFF_MAX_INTERVALSLOT;
        sniff_ctrl_config.sniff_mix_interval = SNIFF_MIN_INTERVALSLOT;
        sniff_ctrl_config.sniff_attemp = SNIFF_ATTEMPT_SLOT;
        sniff_ctrl_config.sniff_timeout  = SNIFF_TIMEOUT_SLOT;
        memcpy(sniff_ctrl_config.sniff_addr, addr + i * 6, 6);
#if TCFG_USER_TWS_ENABLE
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {
            return;
        }
#endif
        user_send_cmd_prepare(USER_CTRL_SNIFF_IN, sizeof(struct sniff_ctrl_config_t), (u8 *)&sniff_ctrl_config);
    }

}
void sys_auto_sniff_controle(u8 enable, u8 *addr)
{
#if TCFG_BT_SNIFF_DISABLE
    return;
#endif

    if (addr) {
        if (bt_api_conn_mode_check(enable, addr) == 0) {
            log_info("sniff ctr not change\n");
            return;
        }
    }

    if (enable) {
        if (addr) {
            log_info("sniff cmd timer init\n");
            user_cmd_timer_init();
        }

        if (bt_user_priv_var.sniff_timer == 0) {
            log_info("check_sniff_enable\n");
            bt_user_priv_var.sniff_timer = sys_timer_add(NULL, bt_check_enter_sniff, 1000);
        }
    } else {

        if (addr) {
            log_info("sniff cmd timer remove\n");
            remove_user_cmd_timer();
        }

        if (bt_user_priv_var.sniff_timer) {
            log_info("check_sniff_disable\n");
            sys_timeout_del(bt_user_priv_var.sniff_timer);
            bt_user_priv_var.sniff_timer = 0;

            if (exit_sniff_timer == 0) {
                /* exit_sniff_timer = sys_timer_add(NULL, bt_check_exit_sniff, 5000); */
            }
        }
    }
}

void wait_exit_btstack_flag(void *priv)
{
    sys_timer_del(app_var.wait_exit_timer);
    app_var.wait_exit_timer = 0;
    if (priv == NULL) {
        app_task_switch(APP_NAME_IDLE, ACTION_APP_MAIN, NULL);
    } else if (priv == (void *)1) {
        log_info("cpu_reset!!!\n");
        cpu_reset();
    }
}


void sys_enter_soft_poweroff(void *priv)
{
    int detach_phone = 1;
    struct sys_event clear_key_event = {.type =  SYS_KEY_EVENT, .arg = "key"};

    log_info("%s", __func__);

    bt_user_priv_var.emitter_or_receiver = 0;
    if (app_var.goto_poweroff_flag) {
        return;
    }

#if TCFG_BLUETOOTH_BACK_MODE
    if (__this->exit_flag == 1) {
        __this->cmd_flag = 1;
        app_task_switch(APP_NAME_BT, ACTION_APP_MAIN, NULL);
    }
#endif

    ui_update_status(STATUS_POWEROFF);

    app_var.goto_poweroff_flag = 1;
    app_var.goto_poweroff_cnt = 0;


#if 0
    sys_key_event_disable();
    sys_event_clear(&clear_key_event);
    sys_auto_shut_down_disable();
    sys_auto_sniff_controle(0, NULL);


#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
#if (!TCFG_CHARGESTORE_ENABLE)
    //非智能充电仓时，做停止广播操作
    if ((bt_ble_icon_get_adv_state() != ADV_ST_NULL)
        && (bt_ble_icon_get_adv_state() != ADV_ST_END)) {
        bt_ble_icon_close(1);
        os_time_dly(50);//盒盖时间，根据效果调整时间
    }
#endif
    bt_ble_exit();
#endif


#if TCFG_CHARGESTORE_ENABLE
    if (chargestore_get_earphone_online() != 2)
#else
    if (1)
#endif
    {
#if TCFG_USER_TWS_ENABLE
        detach_phone = bt_tws_poweroff();
#endif
    }

    /* #if TCFG_USER_TWS_ENABLE */
    /* detach_phone = bt_tws_poweroff(); */
    /* #if TCFG_CHARGESTORE_ENABLE */
    /* if (chargestore_get_earphone_online() == 2) { */
    /* detach_phone += 1; */
    /* } */
    /* #endif // TCFG_CHARGESTORE_ENABLE */
    /* #endif */

    log_info("detach_phone = %d\n", detach_phone);

    if (detach_phone) {
        user_send_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);
    }
#endif

    if (app_var.wait_exit_timer == 0) {
        app_var.wait_exit_timer = sys_timer_add(priv, wait_exit_btstack_flag, 50);
    }
}

void sys_auto_shut_down_enable(void)
{

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_TRANS_DATA)
    extern u16 bt_ble_is_connected(void);
    if (bt_ble_is_connected()) {
        return;
    }
#endif


#if TCFG_AUTO_SHUT_DOWN_TIME
    log_info("sys_auto_shut_down_enable\n");
    struct application *app = get_current_app();
    if (app_var.auto_shut_down_timer == 0) {
        if (app) {
            if (!strcmp(app->name, APP_NAME_BT)) {
                app_var.auto_shut_down_timer = sys_timeout_add(NULL, sys_enter_soft_poweroff, (app_var.auto_off_time * 1000));
            } else {
                log_info("cur_app:%s, return", app->name);
            }
        } else {//在切换到蓝牙任务APP_STA_START中，current_app为空
            app_var.auto_shut_down_timer = sys_timeout_add(NULL, sys_enter_soft_poweroff, (app_var.auto_off_time * 1000));
        }
    }
#endif
}

void sys_auto_shut_down_disable(void)
{
#if TCFG_AUTO_SHUT_DOWN_TIME
    log_info("sys_auto_shut_down_disable\n");
    if (app_var.auto_shut_down_timer) {
        sys_timeout_del(app_var.auto_shut_down_timer);
        app_var.auto_shut_down_timer = 0;
    }
#endif
}

/*static u32 len_lst[34];*/

static const u32 num0_9[] = {
    (u32)TONE_NUM_0,
    (u32)TONE_NUM_1,
    (u32)TONE_NUM_2,
    (u32)TONE_NUM_3,
    (u32)TONE_NUM_4,
    (u32)TONE_NUM_5,
    (u32)TONE_NUM_6,
    (u32)TONE_NUM_7,
    (u32)TONE_NUM_8,
    (u32)TONE_NUM_9,
} ;
static u8 check_phone_income_idle(void)
{
    if (bt_user_priv_var.phone_ring_flag) {
        return 0;
    }
    return 1;
}
REGISTER_LP_TARGET(phone_incom_lp_target) = {
    .name       = "phone_check",
    .is_idle    = check_phone_income_idle,
};

static void number_to_play_list(char *num, u32 *lst)
{
    u8 i = 0;

    if (num) {
        for (; i < strlen(num); i++) {
            lst[i] = num0_9[num[i] - '0'] ;
        }
    }
    lst[i++] = (u32)TONE_REPEAT_BEGIN(-1);
    lst[i++] = (u32)TONE_RING;
    lst[i++] = (u32)TONE_REPEAT_END();
    lst[i++] = (u32)NULL;
}

void phone_num_play_timer(void *priv)
{
    char *len_lst[34];

    if (get_call_status() == BT_CALL_HANGUP) {
        log_info("hangup,--phone num play return\n");
        return;
    }
    log_info("%s\n", __FUNCTION__);

    if (bt_user_priv_var.phone_num_flag) {
        tone_play_stop();
        number_to_play_list((char *)(bt_user_priv_var.income_phone_num), len_lst);
        tone_file_list_play(len_lst, 1);
    } else {
        /*电话号码还没有获取到，定时查询*/
        bt_user_priv_var.phone_timer_id = sys_timeout_add(NULL, phone_num_play_timer, 200);
    }
}

static void phone_num_play_start(void)
{
    /* check if support inband ringtone */
    if (!bt_user_priv_var.inband_ringtone) {
        bt_user_priv_var.phone_num_flag = 0;
        bt_user_priv_var.phone_timer_id = sys_timeout_add(NULL, phone_num_play_timer, 500);
    }
}

void phone_ring_play_start(void)
{
    char *len_lst[34];

    if (get_call_status() == BT_CALL_HANGUP) {
        log_info("hangup,--phone ring play return\n");
        return;
    }
    log_info("%s\n", __FUNCTION__);
    /* check if support inband ringtone */
    if (!bt_user_priv_var.inband_ringtone) {
        tone_play_stop();
        number_to_play_list(NULL, len_lst);
        tone_file_list_play(len_lst, 1);
    }
}


static u16 play_poweron_ok_timer_id = 0;

static void play_poweron_ok_timer(void *priv)
{
    play_poweron_ok_timer_id = 0;

    log_d("\n-------play_poweron_ok_timer-------\n", priv);
    if (!tone_get_status()) {
#if TCFG_USER_TWS_ENABLE
        bt_tws_poweron();
#else
        bt_wait_connect_and_phone_connect_switch(0);
#endif
        return;
    }

    play_poweron_ok_timer_id = sys_timeout_add(priv, play_poweron_ok_timer, 400);
}


static void a2dp_paly_check()
{
    if (bt_audio_is_running()) {
        sys_timeout_add((void *)1, sys_enter_soft_poweroff, 5000);
    } else {
        user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
        sys_timeout_add(NULL, a2dp_paly_check, 3000);
    }
}

static void connect_phone_test(void *p)
{
    user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
    sys_timeout_add(NULL, a2dp_paly_check, 3000);
}

static void play_poweron_ok_timer_init(void)
{
    /*
     * 等待开机提示音播放结束，然后开始tws连接和回连手机
     */
    if (play_poweron_ok_timer_id) {
        sys_timeout_del(play_poweron_ok_timer_id);
    }

    if (tone_get_status()) {
        play_poweron_ok_timer_id = sys_timeout_add(NULL, play_poweron_ok_timer, 400);
    } else {
#if TCFG_USER_TWS_ENABLE
        bt_tws_poweron();
#else
        bt_wait_connect_and_phone_connect_switch(0);
#endif
    }
}

//=============================================================================//
//NOTE by MO: For fix Make, should be implemented later;
__attribute__((weak))
int earphone_a2dp_codec_get_low_latency_mode()
{
    return 0;
}

__attribute__((weak))
void earphone_a2dp_codec_set_low_latency_mode(int enable)
{
    return;
}
//=============================================================================//



int bt_get_low_latency_mode()
{
    return earphone_a2dp_codec_get_low_latency_mode();
}

void bt_set_low_latency_mode(int enable)
{
#if TCFG_USER_TWS_ENABLE
    if (tws_api_get_role() == TWS_ROLE_MASTER) {
        if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {
            if (enable) {
                tws_api_sync_call_by_uuid('T', SYNC_CMD_LOW_LATENCY_ENABLE, 300);
            } else {
                tws_api_sync_call_by_uuid('T', SYNC_CMD_LOW_LATENCY_DISABLE, 300);
            }
        } else {
            earphone_a2dp_codec_set_low_latency_mode(enable);
        }
    }
#else
    earphone_a2dp_codec_set_low_latency_mode(enable);
#endif
}

u8 bt_sco_state(void)
{
    return bt_user_priv_var.phone_call_dec_begin;
}
void phonebook_packet_handler(u8 type, const u8 *name, const u8 *number, const u8 *date)
{
    static u16 number_cnt = 0;
    printf("NO.%d:", number_cnt);
    number_cnt++;
    printf("type:%d ", type);
    if (type == 0xff) {
        number_cnt = 0;
    }
    if (name) {
        printf(" NAME:%s  ", name);
    }
    if (number) {
        printf("number:%s  ", number);
    }
    if (date) {
        printf("date:%s ", date);
    }
    putchar('\n');
}
int tone_dec_wait_stop(u32 timeout_ms);
extern void bredr_bulk_change(u8 mode);
/*
 * 对应原来的状态处理函数，连接，电话状态等
 */
static int bt_connction_status_event_handler(struct bt_event *bt)
{
    STATUS *p_tone = get_tone_config();
    u8 *phone_number = NULL;

    log_info("-----------------------bt_connction_status_event_handler %d", bt->event);


    if (app_var.goto_poweroff_flag) {
        if (bt->event != BT_STATUS_A2DP_MEDIA_STOP) {
            return false;
        }
    }


    switch (bt->event) {
    case BT_STATUS_INIT_OK:
        /*
         * 蓝牙初始化完成
         */
        log_info("BT_STATUS_INIT_OK\n");

        __this->init_ok = 1;
        if (__this->init_ok_time == 0) {
            __this->init_ok_time = timer_get_ms();
            __this->auto_exit_limit_time = POWERON_AUTO_CONN_TIME * 1000;
            /* tone_dec_wait_stop(3000); */
        }
#if TCFG_USER_BLE_ENABLE
        bt_ble_init();
#endif


#if (TCFG_SPI_LCD_ENABLE)
#if TCFG_USER_EMITTER_ENABLE//带有屏幕的方案根据UI选项连接
        u8 device_num = 0;
        if (bt_user_priv_var.emitter_or_receiver == BT_EMITTER_EN) {
            bredr_bulk_change(0);
            device_num = get_current_poweron_memory_search_index(NULL);
            if (device_num) {
                puts("start reconnect\n");
                user_send_cmd_prepare(USER_CTRL_START_CONNECTION, 0, NULL);
            } else {
                /* bt_search_device(); */
            }
            break;
        }

#endif
#endif

#if TCFG_USER_EMITTER_ENABLE
        if (bt_user_priv_var.emitter_or_receiver == BT_EMITTER_EN) {
            bredr_bulk_change(0);
            if (get_current_poweron_memory_search_index(NULL)) {
                puts("start reconnect\n");
                user_send_cmd_prepare(USER_CTRL_START_CONNECTION, 0, NULL);
            } else {
                bt_search_device();
            }
            break;
        }
#endif

#if GMA_EN
        extern void gma_spp_init(void);
        gma_spp_init();
#elif TRANS_DATA_EN
        extern void transport_spp_init(void);
        transport_spp_init();
#endif


        bt_init_ok_search_index();
        bt_set_led_status(STATUS_BT_INIT_OK);
#if TCFG_CHARGESTORE_ENABLE || TCFG_TEST_BOX_ENABLE
        chargestore_set_bt_init_ok(1);
#endif


#if ((CONFIG_BT_MODE == BT_BQB)||(CONFIG_BT_MODE == BT_PER))
        bt_wait_phone_connect_control(1);
#else
#if TCFG_USER_TWS_ENABLE
        bt_tws_poweron();
#else
        bt_wait_connect_and_phone_connect_switch(0);
#endif
#endif
        /* if (app_var.play_poweron_tone) { */
        /* bt_tone_play_index(p_tone->bt_init_ok, 1, NULL); */
        /* } */
        break;

    case BT_STATUS_START_CONNECTED:
        break;

    case BT_STATUS_ENCRY_COMPLETE:
        break;

    case BT_STATUS_SECOND_CONNECTED:
        clear_current_poweron_memory_search_index(0);
    case BT_STATUS_FIRST_CONNECTED:
        log_info("BT_STATUS_CONNECTED\n");
        sys_auto_shut_down_disable();


#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
        {
            u8 connet_type;
            if (get_auto_connect_state(bt->args)) {
                connet_type = ICON_TYPE_RECONNECT;
            } else {
                connet_type = ICON_TYPE_CONNECTED;
            }

#if TCFG_USER_TWS_ENABLE
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                bt_ble_icon_open(connet_type);
            } else {
                //maybe slave already open
                bt_ble_icon_close(0);
            }

#else
            bt_ble_icon_open(connet_type);
#endif
        }
#endif


#if (TCFG_BD_NUM == 2)
        if (get_current_poweron_memory_search_index(NULL) == 0) {
            bt_wait_phone_connect_control(1);
        }
#endif

#if TCFG_USER_TWS_ENABLE
        if (!get_bt_tws_connect_status()) {   //如果对耳还未连接就连上手机，要切换闪灯状态
            bt_set_led_status(STATUS_BT_CONN);
        }

        if (bt_tws_phone_connected()) {
            break;
        }
#else
        bt_set_led_status(STATUS_BT_CONN);    //单台在此处设置连接状态,对耳的连接状态需要同步，在bt_tws.c中去设置
#if (TCFG_AUTO_STOP_PAGE_SCAN_TIME && TCFG_BD_NUM == 2)
        if (get_total_connect_dev() == 1) {   //当前有一台连接上了
            if (app_var.auto_stop_page_scan_timer == 0) {
                app_var.auto_stop_page_scan_timer = sys_timeout_add(NULL, bt_close_page_scan, (TCFG_AUTO_STOP_PAGE_SCAN_TIME * 1000)); //2
            }
        } else {
            if (app_var.auto_stop_page_scan_timer) {
                sys_timeout_del(app_var.auto_stop_page_scan_timer);
                app_var.auto_stop_page_scan_timer = 0;
            }
        }
#endif   //endif AUTO_STOP_PAGE_SCAN_TIME
#endif
        /* tone_play(TONE_CONN); */



        log_info("tone status:%d\n", tone_get_status());
        if (get_call_status() == BT_CALL_HANGUP) {
            bt_tone_play_index(p_tone->bt_connect_ok, 1, NULL);
        }

        /*int timeout = 5000 + rand32() % 10000;
        sys_timeout_add(NULL, connect_phone_test,  timeout);*/
        /*sys_timer_add(NULL, auto_switch_mode_test,  5000);*/
        break;
    case BT_STATUS_FIRST_DISCONNECT:
    case BT_STATUS_SECOND_DISCONNECT:
        __this->call_flag = 0;
        log_info("BT_STATUS_DISCONNECT\n");

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
        bt_emitter_stu_set(0);
#endif

#if(TCFG_BD_NUM == 2)               //对耳在bt_tws同步播放提示音
        if (!app_var.goto_poweroff_flag) { /*关机不播断开提示音*/
            if (!__this->ignore_discon_tone) {
                tone_play_index(p_tone->bt_disconnect, 1);
            }
        }
#else
#if TCFG_USER_TWS_ENABLE
        if (!get_bt_tws_connect_status())
#endif
        {
            if (!app_var.goto_poweroff_flag) { /*关机不播断开提示音*/
                if (!__this->ignore_discon_tone) {
                    bt_tone_play_index(p_tone->bt_disconnect, 1, NULL);
                }
            }
        }
#endif

#if TCFG_USER_TWS_ENABLE
        STATUS *p_led = get_led_config();
        if (get_bt_tws_connect_status()) {
#if TCFG_CHARGESTORE_ENABLE
            chargestore_set_phone_disconnect();
#endif
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                tws_api_sync_call_by_uuid('T', SYNC_CMD_LED_PHONE_DISCONN_STATUS, 400);
            }
        } else {
            //断开手机时，如果对耳未连接，要把LED时钟切到RC（因为单台会进SNIFF）
            if (!app_var.goto_poweroff_flag) { /*关机时不改UI*/
                pwm_led_clk_set(PWM_LED_CLK_RC32K);
                bt_set_led_status(STATUS_BT_DISCONN);
            }
        }
        __this->tws_local_back_role = 0;
#else
        if (get_total_connect_dev() == 0) {    //已经没有设备连接
            if (!app_var.goto_poweroff_flag) { /*关机时不改UI*/
                bt_set_led_status(STATUS_BT_DISCONN);
            }
            sys_auto_shut_down_enable();
        }
#endif
        break;

    //phone status deal
    case BT_STATUS_PHONE_INCOME:
        log_info("BT_STATUS_PHONE_INCOME\n");
        /* #if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE)) */
        /* reverb_pause(); */
        /* #endif */
        esco_dump_packet = ESCO_DUMP_PACKET_CALL;
        ui_update_status(STATUS_PHONE_INCOME);
        u8 tmp_bd_addr[6];
        memcpy(tmp_bd_addr, bt->args, 6);
        /*
         *(1)1t2有一台通话的时候，另一台如果来电不要提示
         *(2)1t2两台同时来电，现来的题示，后来的不播
         */
        if ((check_esco_state_via_addr(tmp_bd_addr) != BD_ESCO_BUSY_OTHER) && (bt_user_priv_var.phone_ring_flag == 0)) {
#if BT_INBAND_RINGTONE
            extern u8 get_device_inband_ringtone_flag(void);
            bt_user_priv_var.inband_ringtone = get_device_inband_ringtone_flag();
#else
            bt_user_priv_var.inband_ringtone = 0 ;
            lmp_private_esco_suspend_resume(3);
#endif

            bt_user_priv_var.phone_ring_flag = 1;
            bt_user_priv_var.phone_income_flag = 1;

#if TCFG_USER_TWS_ENABLE
#if BT_SYNC_PHONE_RING
            if (!bt_tws_sync_phone_num(NULL))
#else
            if (tws_api_get_role() == TWS_ROLE_MASTER)
#endif
#endif
            {
#if BT_PHONE_NUMBER
                phone_num_play_start();
#else
                phone_ring_play_start();
#endif

            }
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_CURRENT, 0, NULL); //发命令获取电话号码
        } else {
            log_info("SCO busy now:%d,%d\n", check_esco_state_via_addr(tmp_bd_addr), bt_user_priv_var.phone_ring_flag);
        }
        break;
    case BT_STATUS_PHONE_OUT:
        log_info("BT_STATUS_PHONE_OUT\n");
        lmp_private_esco_suspend_resume(4);
        esco_dump_packet = ESCO_DUMP_PACKET_CALL;
        ui_update_status(STATUS_PHONE_OUT);
        bt_user_priv_var.phone_income_flag = 0;
        user_send_cmd_prepare(USER_CTRL_HFP_CALL_CURRENT, 0, NULL); //发命令获取电话号码
        break;
    case BT_STATUS_PHONE_ACTIVE:
        log_info("BT_STATUS_PHONE_ACTIVE\n");
        ui_update_status(STATUS_PHONE_ACTIV);
        if (bt_user_priv_var.phone_call_dec_begin) {
            log_info("call_active,dump_packet clear\n");
            esco_dump_packet = ESCO_DUMP_PACKET_DEFAULT;
        }
        if (bt_user_priv_var.phone_ring_flag) {
            bt_user_priv_var.phone_ring_flag = 0;
            tone_play_stop();
            if (bt_user_priv_var.phone_timer_id) {
                sys_timeout_del(bt_user_priv_var.phone_timer_id);
                bt_user_priv_var.phone_timer_id = 0;
            }
        }
        lmp_private_esco_suspend_resume(4);
        bt_user_priv_var.phone_income_flag = 0;
        bt_user_priv_var.phone_num_flag = 0;
        bt_user_priv_var.phone_con_sync_num_ring = 0;
        bt_user_priv_var.phone_con_sync_ring = 0;
        bt_user_priv_var.phone_vol = 15;
        log_info("phone_active:%d\n", app_var.call_volume);
#ifdef PHONE_CALL_DEFAULT_MAX_VOL
        phone_sync_vol();
        app_audio_set_volume(APP_AUDIO_STATE_CALL, app_var.aec_dac_gain, 1);
#else
        app_audio_set_volume(APP_AUDIO_STATE_CALL, app_var.call_volume, 1);
#endif
        break;
    case BT_STATUS_PHONE_HANGUP:
        esco_dump_packet = ESCO_DUMP_PACKET_CALL;
        log_info("phone_handup\n");
        if (bt_user_priv_var.phone_ring_flag) {
            bt_user_priv_var.phone_ring_flag = 0;
            tone_play_stop();
            if (bt_user_priv_var.phone_timer_id) {
                sys_timeout_del(bt_user_priv_var.phone_timer_id);
                bt_user_priv_var.phone_timer_id = 0;
            }
        }
        bt_user_priv_var.phone_num_flag = 0;
        bt_user_priv_var.phone_con_sync_num_ring = 0;
        bt_user_priv_var.phone_con_sync_ring = 0;
        if (__this->call_flag) {
            sys_timeout_add(NULL, bt_switch_back, 10);
        }
        break;
    case BT_STATUS_PHONE_NUMBER:
        log_info("BT_STATUS_PHONE_NUMBER\n");
        phone_number = (u8 *)bt->value;
        //put_buf(phone_number, strlen((const char *)phone_number));
        if (bt_user_priv_var.phone_num_flag == 1) {
            break;
        }
        bt_user_priv_var.income_phone_len = 0;
        memset(bt_user_priv_var.income_phone_num, '\0', sizeof(bt_user_priv_var.income_phone_num));
        for (int i = 0; i < strlen((const char *)phone_number); i++) {
            if (phone_number[i] >= '0' && phone_number[i] <= '9') {
                //过滤，只有数字才能报号
                bt_user_priv_var.income_phone_num[bt_user_priv_var.income_phone_len++] = phone_number[i];
                if (bt_user_priv_var.income_phone_len >= sizeof(bt_user_priv_var.income_phone_num)) {
                    break;    /*buffer 空间不够，后面不要了*/
                }
            }
        }
        if (bt_user_priv_var.income_phone_len > 0) {
            bt_user_priv_var.phone_num_flag = 1;
        } else {
            log_info("PHONE_NUMBER len err\n");
        }
        break;
    case BT_STATUS_INBAND_RINGTONE:
        log_info("BT_STATUS_INBAND_RINGTONE\n");
#if BT_INBAND_RINGTONE
        bt_user_priv_var.inband_ringtone = bt->value;
#else
        bt_user_priv_var.inband_ringtone = 0;
#endif
        break;


    case BT_STATUS_BEGIN_AUTO_CON:
        log_info("BT_STATUS_BEGIN_AUTO_CON\n");
        break;
    case BT_STATUS_A2DP_MEDIA_START:
        r_printf(" BT_STATUS_A2DP_MEDIA_START:0x%x\n", bt->value);
        __this->call_flag = 0;
        __this->a2dp_start_flag = 1;
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        bt_drop_a2dp_frame_stop();
        if (is_a2dp_dec_open()) {
            puts("is_a2dp_dec_open break\n");
            break;
        }
        if (local_tws_dec_close(1)) {
            bt_a2dp_drop_frame(NULL);
            tws_user_sync_box(TWS_BOX_NOTICE_A2DP_BACK_TO_BT_MODE, 0);

        }
#endif
        //earphone_a2dp_audio_codec_open(0x0);//A2DP_CODEC_SBC);
        a2dp_dec_open(bt->value);
#if TCFG_USER_TWS_ENABLE
        //bt_tws_sync_led_status();
#endif
        break;
    case BT_STATUS_A2DP_MEDIA_STOP:
        log_info(" BT_STATUS_A2DP_MEDIA_STop");

        __this->a2dp_start_flag = 0;
        /*earphone_a2dp_audio_codec_close();*/
#if(defined(TCFG_MIXERCH_REC_EN) && (TCFG_MIXERCH_REC_EN))
        extern int mixer_recorder_encoding(void);
        extern void mixer_recorder_stop(void);
        if (mixer_recorder_encoding()) {
            g_printf("mixer_recorder_stop\n");
            mixer_recorder_stop();
        }
#endif
        bt_drop_a2dp_frame_stop();
        a2dp_dec_close();

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        if (is_tws_active_device()) {
            break;
        }
#endif
#if TCFG_USER_TWS_ENABLE
        if (get_bt_tws_connect_status()) {
        }
#endif

        break;
    case BT_STATUS_SCO_STATUS_CHANGE:
        r_printf(" BT_STATUS_SCO_STATUS_CHANGE len:%d ,type:%d", (bt->value >> 16), (bt->value & 0x0000ffff));
        if (bt->value != 0xff) {
            puts("<<<<<<<<<<<esco_dec_stat\n");
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
#if (!TCFG_CALLING_EN_REVERB)
            reverb_pause();
#endif
#endif
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))
            if (speaker_if_working()) {
                stop_loud_speaker();
            }
#endif

#if 0   //debug
            void mic_capless_feedback_toggle(u8 toggle);
            mic_capless_feedback_toggle(0);
#endif

#if PHONE_CALL_USE_LDO15
            if ((get_chip_version() & 0xff) <= 0x04) { //F版芯片以下的，通话需要使用LDO模式
                power_set_mode(PWR_LDO15);
            }
#endif
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
            if (local_tws_dec_close(1)) {
                bt_a2dp_drop_frame(NULL);
            }

#endif
            /*phone_call_begin(&bt->value);*/

#if TCFG_USER_TWS_ENABLE
#if	TCFG_USER_ESCO_SLAVE_MUTE
            if (tws_api_get_role() == TWS_ROLE_SLAVE) {
                esco_dec_open(&bt->value, 1);
            } else
#endif
#endif
            {
                esco_dec_open(&bt->value, 0);
            }
            bt_user_priv_var.phone_call_dec_begin = 1;
            if (get_call_status() == BT_CALL_ACTIVE) {
                log_info("dec_begin,dump_packet clear\n");
                esco_dump_packet = ESCO_DUMP_PACKET_DEFAULT;
            }
#if TCFG_USER_TWS_ENABLE
            tws_page_scan_deal_by_esco(1);
#endif
        } else {
            puts("<<<<<<<<<<<esco_dec_stop\n");
            bt_user_priv_var.phone_call_dec_begin = 0;
            esco_dump_packet = ESCO_DUMP_PACKET_CALL;
#if(defined(TCFG_MIXERCH_REC_EN) && (TCFG_MIXERCH_REC_EN))
            extern int mixer_recorder_encoding(void);
            extern void mixer_recorder_stop(void);
            if (mixer_recorder_encoding()) {
                g_printf("mixer_recorder_stop\n");
                mixer_recorder_stop();
            }
#endif
            esco_dec_close();
#if TCFG_USER_TWS_ENABLE
            tws_page_scan_deal_by_esco(0);
#endif
#if PHONE_CALL_USE_LDO15
            if ((get_chip_version() & 0xff) <= 0x04) { //F版芯片以下的，通话需要使用LDO模式
                power_set_mode(TCFG_LOWPOWER_POWER_SEL);
            }
#endif
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
            if (is_tws_active_device()) {
                break;
            }
#endif
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
#if (!TCFG_CALLING_EN_REVERB)
            reverb_resume();
#endif
#endif
        }
        break;
    case BT_STATUS_CALL_VOL_CHANGE:
        log_info(" BT_STATUS_CALL_VOL_CHANGE %d", bt->value);
        u8 volume = app_audio_get_max_volume() * bt->value / 15;
        u8 call_status = get_call_status();
        bt_user_priv_var.phone_vol = bt->value;
        if ((call_status == BT_CALL_ACTIVE) || (call_status == BT_CALL_OUTGOING) || app_var.siri_stu) {
            app_audio_set_volume(APP_AUDIO_STATE_CALL, volume, 1);
        } else if (call_status != BT_CALL_HANGUP) {
            /*只保存，不设置到dac*/
            app_var.call_volume = volume;
        }
        break;
    case BT_STATUS_SNIFF_STATE_UPDATE:
        log_info(" BT_STATUS_SNIFF_STATE_UPDATE %d\n", bt->value);    //0退出SNIFF
        if (bt->value == 0) {
#if (TCFG_USER_TWS_ENABLE == 1)
            pwm_led_clk_set(PWM_LED_CLK_BTOSC_24M);
#endif
            sys_auto_sniff_controle(1, bt->args);
        } else {
#if (TCFG_USER_TWS_ENABLE == 1)
            pwm_led_clk_set(PWM_LED_CLK_RC32K);
#endif
            sys_auto_sniff_controle(0, bt->args);
        }
        break;

    case BT_STATUS_LAST_CALL_TYPE_CHANGE:
        log_info("BT_STATUS_LAST_CALL_TYPE_CHANGE:%d\n", bt->value);
        bt_user_priv_var.last_call_type = bt->value;
        break;

    case BT_STATUS_CONN_A2DP_CH:
#if TCFG_USER_EMITTER_ENABLE
        if (bt_user_priv_var.emitter_or_receiver == BT_EMITTER_EN) {
            extern u8 app_common_key_var_2_event(u32 key_var);
            app_common_key_var_2_event(KEY_BT_EMITTER_SW);
        }
#endif
    case BT_STATUS_CONN_HFP_CH:

        if ((!is_1t2_connection()) && (get_current_poweron_memory_search_index(NULL))) { //回连下一个device
            if (get_esco_coder_busy_flag()) {
                clear_current_poweron_memory_search_index(0);
            } else {
                user_send_cmd_prepare(USER_CTRL_START_CONNECTION, 0, NULL);
            }
        }
        break;
    case BT_STATUS_PHONE_MANUFACTURER:
        log_info("BT_STATUS_PHONE_MANUFACTURER:%d\n", bt->value);
        extern const u8 hid_conn_depend_on_dev_company;
        app_var.remote_dev_company = bt->value;
        if (hid_conn_depend_on_dev_company) {
            if (bt->value) {
                //user_send_cmd_prepare(USER_CTRL_HID_CONN, 0, NULL);
            } else {
                user_send_cmd_prepare(USER_CTRL_HID_DISCONNECT, 0, NULL);
            }
        }
        break;
    case BT_STATUS_VOICE_RECOGNITION:
        log_info(" BT_STATUS_VOICE_RECOGNITION:%d \n", bt->value);
        esco_dump_packet = ESCO_DUMP_PACKET_DEFAULT;
        /* put_buf(bt, sizeof(struct bt_event)); */
        app_var.siri_stu = bt->value;
        if (__this->call_flag && (app_var.siri_stu == 0)) {
            sys_timeout_add(NULL, bt_switch_back, 10);
        }
        break;
    case BT_STATUS_AVRCP_INCOME_OPID:
#define AVC_VOLUME_UP			0x41
#define AVC_VOLUME_DOWN			0x42
        log_info("BT_STATUS_AVRCP_INCOME_OPID:%d\n", bt->value);
        if (bt->value == AVC_VOLUME_UP) {

        }
        if (bt->value == AVC_VOLUME_DOWN) {

        }
        break;
    default:
        log_info(" BT STATUS DEFAULT\n");
        break;
    }
    return 0;
}




#define HCI_EVENT_INQUIRY_COMPLETE                            0x01
#define HCI_EVENT_CONNECTION_COMPLETE                         0x03
#define HCI_EVENT_DISCONNECTION_COMPLETE                      0x05
#define HCI_EVENT_PIN_CODE_REQUEST                            0x16
#define HCI_EVENT_IO_CAPABILITY_REQUEST                       0x31
#define HCI_EVENT_USER_CONFIRMATION_REQUEST                   0x33
#define HCI_EVENT_USER_PASSKEY_REQUEST                        0x34
#define HCI_EVENT_USER_PRESSKEY_NOTIFICATION			      0x3B
#define HCI_EVENT_VENDOR_NO_RECONN_ADDR                       0xF8
#define HCI_EVENT_VENDOR_REMOTE_TEST                          0xFE
#define BTSTACK_EVENT_HCI_CONNECTIONS_DELETE                  0x6D


#define ERROR_CODE_SUCCESS                                    0x00
#define ERROR_CODE_PAGE_TIMEOUT                               0x04
#define ERROR_CODE_AUTHENTICATION_FAILURE                     0x05
#define ERROR_CODE_PIN_OR_KEY_MISSING                         0x06
#define ERROR_CODE_CONNECTION_TIMEOUT                         0x08
#define ERROR_CODE_SYNCHRONOUS_CONNECTION_LIMIT_TO_A_DEVICE_EXCEEDED  0x0A
#define ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS                      0x0B
#define ERROR_CODE_CONNECTION_REJECTED_DUE_TO_LIMITED_RESOURCES       0x0D
#define ERROR_CODE_CONNECTION_REJECTED_DUE_TO_UNACCEPTABLE_BD_ADDR    0x0F
#define ERROR_CODE_CONNECTION_ACCEPT_TIMEOUT_EXCEEDED         0x10
#define ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION          0x13
#define ERROR_CODE_CONNECTION_TERMINATED_BY_LOCAL_HOST        0x16

#define CUSTOM_BB_AUTO_CANCEL_PAGE                            0xFD  //// app cancle page
#define BB_CANCEL_PAGE                                        0xFE  //// bb cancle page


static void sys_time_auto_connection_deal(void *arg)
{
    if (bt_user_priv_var.auto_connection_counter) {
        log_info("------------------------auto_timeout conuter %d", bt_user_priv_var.auto_connection_counter);
        bt_user_priv_var.auto_connection_counter--;
        user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6, bt_user_priv_var.auto_connection_addr);
    }
}

static void bt_hci_event_inquiry(struct bt_event *bt)
{
#if TCFG_USER_EMITTER_ENABLE
    if (bt_user_priv_var.emitter_or_receiver == BT_EMITTER_EN) {
        extern void emitter_search_stop();
        emitter_search_stop();
    }
#endif
}

static void bt_hci_event_connection(struct bt_event *bt)
{
    log_info("tws_conn_state=%d\n", bt_user_priv_var.tws_conn_state);

#if TCFG_USER_TWS_ENABLE
    bt_tws_hci_event_connect();
#ifndef CONFIG_NEW_BREDR_ENABLE
    tws_try_connect_disable();
#endif
#else
    if (bt_user_priv_var.auto_connection_timer) {
        sys_timeout_del(bt_user_priv_var.auto_connection_timer);
        bt_user_priv_var.auto_connection_timer = 0;
    }
    bt_user_priv_var.auto_connection_counter = 0;
    bt_wait_phone_connect_control(0);
    user_send_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL);
#endif
}

extern void bt_get_vm_mac_addr(u8 *addr);
static void bt_hci_event_disconnect(struct bt_event *bt)
{
    u8 local_addr[6];
    if (app_var.goto_poweroff_flag || __this->exiting) {
        return;
    }

    if (get_total_connect_dev() == 0) {    //已经没有设备连接
        sys_auto_shut_down_enable();
    }


#if TCFG_TEST_BOX_ENABLE
    if (chargestore_get_testbox_status()) {
        log_info("<<<<<<<<<<<<<<<<<wait bt discon,then page_scan>>>>>>>>>>>>>\n");
        bt_get_vm_mac_addr(local_addr);
        //log_info("local_adr\n");
        //put_buf(local_addr,6);
        lmp_hci_write_local_address(local_addr);
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
        return;
    }
#endif

#if (TCFG_AUTO_STOP_PAGE_SCAN_TIME && TCFG_BD_NUM == 2)
    if (get_total_connect_dev() == 1) {   //当前有一台连接上了
        if (app_var.auto_stop_page_scan_timer == 0) {
            app_var.auto_stop_page_scan_timer = sys_timeout_add(NULL, bt_close_page_scan, (TCFG_AUTO_STOP_PAGE_SCAN_TIME * 1000)); //2
        }
    } else {
        if (app_var.auto_stop_page_scan_timer) {
            sys_timeout_del(app_var.auto_stop_page_scan_timer);
            app_var.auto_stop_page_scan_timer = 0;
        }
    }
#endif   //endif AUTO_STOP_PAGE_SCAN_TIME

#if (TCFG_BD_NUM == 2)
    if ((bt->value == ERROR_CODE_CONNECTION_REJECTED_DUE_TO_UNACCEPTABLE_BD_ADDR) ||
        (bt->value == ERROR_CODE_CONNECTION_ACCEPT_TIMEOUT_EXCEEDED)) {
        /*
         *连接接受超时
         *如果支持1t2，可以选择继续回连下一台，除非已经回连完毕
         */
        if (get_current_poweron_memory_search_index(NULL)) {
            user_send_cmd_prepare(USER_CTRL_START_CONNECTION, 0, NULL);
            return;
        }
    }
#endif

#if TCFG_USER_TWS_ENABLE
    bt_tws_phone_disconnected();
#else
    if (bt_user_priv_var.emitter_or_receiver != BT_EMITTER_EN) {
        bt_wait_phone_connect_control(1);
    } else {
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
    }
#endif
}

static void bt_hci_event_linkkey_missing(struct bt_event *bt)
{
#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
#if (CONFIG_NO_DISPLAY_BUTTON_ICON && TCFG_CHARGESTORE_ENABLE)
    //已取消配对了
    if (bt_ble_icon_get_adv_state() == ADV_ST_RECONN) {
        //切换广播
        log_info("switch_INQ\n");
        bt_ble_icon_open(ICON_TYPE_INQUIRY);
    }
#endif
#endif
}

static void bt_hci_event_page_timeout()
{
    log_info("------------------------HCI_EVENT_PAGE_TIMEOUT conuter %d", bt_user_priv_var.auto_connection_counter);
#if TCFG_USER_TWS_ENABLE
    bt_tws_phone_page_timeout();
#else
    /* if (!bt_user_priv_var.auto_connection_counter) { */
    if (bt_user_priv_var.auto_connection_timer) {
        sys_timer_del(bt_user_priv_var.auto_connection_timer);
        bt_user_priv_var.auto_connection_timer = 0;
    }

#if (TCFG_BD_NUM == 2)
    if (get_current_poweron_memory_search_index(NULL)) {
        user_send_cmd_prepare(USER_CTRL_START_CONNECTION, 0, NULL);
        return;
    }
#endif
    if (bt_user_priv_var.emitter_or_receiver != BT_EMITTER_EN) {
        bt_wait_phone_connect_control(1);
    } else {
        user_send_cmd_prepare(USER_CTRL_WRITE_CONN_ENABLE, 0, NULL);
    }
#endif
}

static void bt_hci_event_connection_timeout(struct bt_event *bt)
{
    if (!get_remote_test_flag() && !get_esco_busy_flag()) {
        bt_user_priv_var.auto_connection_counter = (TIMEOUT_CONN_TIME * 1000);
        memcpy(bt_user_priv_var.auto_connection_addr, bt->args, 6);
#if TCFG_USER_TWS_ENABLE
        bt_tws_phone_connect_timeout();
#else

#if ((CONFIG_BT_MODE == BT_BQB)||(CONFIG_BT_MODE == BT_PER))
        bt_wait_phone_connect_control(1);
#else
        user_send_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
        bt_wait_connect_and_phone_connect_switch(0);
#endif
        //user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6, bt->args);
#endif
    } else {
#if TCFG_USER_TWS_ENABLE
        bt_tws_phone_disconnected();
#else
        bt_wait_phone_connect_control(1);
#endif
    }

}

static void bt_hci_event_connection_exist(struct bt_event *bt)
{
    if (!get_remote_test_flag() && !get_esco_busy_flag()) {
        bt_user_priv_var.auto_connection_counter = (8 * 1000);
        memcpy(bt_user_priv_var.auto_connection_addr, bt->args, 6);
#if TCFG_USER_TWS_ENABLE
        bt_tws_phone_connect_timeout();
#else
        if (bt_user_priv_var.auto_connection_timer) {
            sys_timer_del(bt_user_priv_var.auto_connection_timer);
            bt_user_priv_var.auto_connection_timer = 0;
        }
        bt_wait_connect_and_phone_connect_switch(0);
        //user_send_cmd_prepare(USER_CTRL_START_CONNEC_VIA_ADDR, 6, bt->args);
#endif
    } else {
#if TCFG_USER_TWS_ENABLE
        bt_tws_phone_disconnected();
#else
        bt_wait_phone_connect_control(1);
#endif
    }

}

extern void set_remote_test_flag(u8 own_remote_test);
static int bt_hci_event_handler(struct bt_event *bt)
{
    //对应原来的蓝牙连接上断开处理函数  ,bt->value=reason
    log_info("------------------------bt_hci_event_handler reason %x %x", bt->event, bt->value);

    if (bt->event == HCI_EVENT_VENDOR_REMOTE_TEST) {
        if (0 == bt->value) {
            set_remote_test_flag(0);
            log_info("clear_test_box_flag");
            return 0;
        } else {

#if TCFG_USER_BLE_ENABLE
            extern void bt_ble_adv_enable(u8 enable);
            bt_ble_adv_enable(0);
#endif

#if TCFG_USER_TWS_ENABLE
            bt_tws_poweroff();
#endif
        }
    }

    if ((bt->event != HCI_EVENT_CONNECTION_COMPLETE) ||
        ((bt->event == HCI_EVENT_CONNECTION_COMPLETE) && (bt->value != ERROR_CODE_SUCCESS))) {
#if TCFG_TEST_BOX_ENABLE
        if (chargestore_get_testbox_status()) {
            if (get_remote_test_flag()) {
                chargestore_clear_connect_status();
            }
            //return 0;
        }
#endif
        if (get_remote_test_flag() \
            && !(HCI_EVENT_DISCONNECTION_COMPLETE == bt->event) \
            && !(HCI_EVENT_VENDOR_REMOTE_TEST == bt->event)) {
            log_info("cpu reset\n");
            cpu_reset();
        }
    }

    switch (bt->event) {
    case HCI_EVENT_INQUIRY_COMPLETE:
        log_info(" HCI_EVENT_INQUIRY_COMPLETE \n");
        bt_hci_event_inquiry(bt);
        bt_search_busy = 0;
        break;

    case HCI_EVENT_IO_CAPABILITY_REQUEST:
        log_info(" HCI_EVENT_IO_CAPABILITY_REQUEST \n");
        clock_add_set(BT_CONN_CLK);
        break;

    case HCI_EVENT_USER_CONFIRMATION_REQUEST:
        log_info(" HCI_EVENT_USER_CONFIRMATION_REQUEST \n");
        ///<可通过按键来确认是否配对 1：配对   0：取消
        bt_send_pair(1);
        clock_remove_set(BT_CONN_CLK);
        break;
    case HCI_EVENT_USER_PASSKEY_REQUEST:
        log_info(" HCI_EVENT_USER_PASSKEY_REQUEST \n");
        ///<可以开始输入6位passkey
        break;
    case HCI_EVENT_USER_PRESSKEY_NOTIFICATION:
        log_info(" HCI_EVENT_USER_PRESSKEY_NOTIFICATION %x\n", bt->value);
        ///<可用于显示输入passkey位置 value 0:start  1:enrer  2:earse   3:clear  4:complete
        break;
    case HCI_EVENT_PIN_CODE_REQUEST :
        log_info("HCI_EVENT_PIN_CODE_REQUEST  \n");
        bt_send_pair(1);
        break;

    case HCI_EVENT_VENDOR_NO_RECONN_ADDR :
        log_info("HCI_EVENT_VENDOR_NO_RECONN_ADDR \n");
        bt_hci_event_disconnect(bt) ;
        break;

    case HCI_EVENT_DISCONNECTION_COMPLETE :
        log_info("HCI_EVENT_DISCONNECTION_COMPLETE \n");
        bt_hci_event_disconnect(bt) ;
        clock_remove_set(BT_CONN_CLK);
        break;

    case BTSTACK_EVENT_HCI_CONNECTIONS_DELETE:
    case HCI_EVENT_CONNECTION_COMPLETE:
        log_info(" HCI_EVENT_CONNECTION_COMPLETE \n");
        switch (bt->value) {
        case ERROR_CODE_SUCCESS :
            log_info("ERROR_CODE_SUCCESS  \n");
            bt_hci_event_connection(bt);
            break;
        case ERROR_CODE_PIN_OR_KEY_MISSING:
            log_info(" ERROR_CODE_PIN_OR_KEY_MISSING \n");
            bt_hci_event_linkkey_missing(bt);

        case ERROR_CODE_SYNCHRONOUS_CONNECTION_LIMIT_TO_A_DEVICE_EXCEEDED :
        case ERROR_CODE_CONNECTION_REJECTED_DUE_TO_LIMITED_RESOURCES:
        case ERROR_CODE_CONNECTION_REJECTED_DUE_TO_UNACCEPTABLE_BD_ADDR:
        case ERROR_CODE_CONNECTION_ACCEPT_TIMEOUT_EXCEEDED  :
        case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION   :
        case ERROR_CODE_CONNECTION_TERMINATED_BY_LOCAL_HOST :
        case ERROR_CODE_AUTHENTICATION_FAILURE :
        case CUSTOM_BB_AUTO_CANCEL_PAGE:
            bt_hci_event_disconnect(bt) ;
            break;

        case ERROR_CODE_PAGE_TIMEOUT:
            log_info(" ERROR_CODE_PAGE_TIMEOUT \n");
            bt_hci_event_page_timeout(bt);
            break;

        case ERROR_CODE_CONNECTION_TIMEOUT:
            log_info(" ERROR_CODE_CONNECTION_TIMEOUT \n");
            bt_hci_event_connection_timeout(bt);
            break;

        case ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS  :
            log_info("ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS   \n");
            bt_hci_event_connection_exist(bt);
            break;
        default:
            break;

        }
        break;
    default:
        break;

    }
    return 0;
}

#if (DUEROS_DMA_EN)
extern void tws_ble_slave_dueros_task_create();
extern void tws_ble_slave_dueros_task_del();
static int bt_ai_event_handler(struct bt_event *bt)
{
    switch (bt->event) {
    case  KEY_CALL_LAST_NO:
        log_info("KEY_CALL_LAST_NO \n");
#if TCFG_USER_TWS_ENABLE
        if (bt_tws_start_search_sibling()) {
            tone_sin_play(250, 1);
            break;
        }
#endif

        if ((get_call_status() == BT_CALL_ACTIVE) ||
            (get_call_status() == BT_CALL_OUTGOING) ||
            (get_call_status() == BT_CALL_ALERT) ||
            (get_call_status() == BT_CALL_INCOMING)) {
            break;//通话过程不允许回拨
        }

        if (bt_user_priv_var.last_call_type ==  BT_STATUS_PHONE_INCOME) {
            user_send_cmd_prepare(USER_CTRL_DIAL_NUMBER, bt_user_priv_var.income_phone_len,
                                  bt_user_priv_var.income_phone_num);
        } else {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_LAST_NO, 0, NULL);
        }
        break;
    case  KEY_CALL_HANG_UP:
        log_info("KEY_CALL_HANG_UP \n");
        if ((get_call_status() == BT_CALL_ACTIVE) ||
            (get_call_status() == BT_CALL_OUTGOING) ||
            (get_call_status() == BT_CALL_ALERT) ||
            (get_call_status() == BT_CALL_INCOMING)) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
        }
        break;
    case  KEY_CALL_ANSWER:
        log_info("KEY_CALL_ANSWER \n");
        if (get_call_status() == BT_CALL_INCOMING) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_ANSWER, 0, NULL);
        }
        break;
    case  KEY_TWS_BLE_DUEROS_CONNECT:
        log_info("KEY_TWS_BLE_DUEROS_CONNECT \n");
        ///mqc mark
        //tws_ble_slave_dueros_task_create();
        //set_ble_connect_type(TYPE_SLAVE_BLE);
        break;
    case  KEY_TWS_BLE_DUEROS_DISCONNECT:
        log_info("KEY_TWS_BLE_DUEROS_DISCONNECT \n");
        //mqc mark
        //set_ble_connect_type(TYPE_NULL);
        //tws_ble_slave_dueros_task_del();
        break;
    default:
        break;
    }

    return 0;
}
#endif

static void bt_resume_deal(void)
{
    sys_key_event_enable();
    sys_auto_sniff_controle(1, NULL);
    if (get_call_status() != BT_CALL_HANGUP) {
        log_info("background return by call");
        return;
    }
    bt_set_led_status(0);
    if (get_total_connect_dev() == 0) {
        sys_auto_shut_down_enable();
    }

}

static int bt_must_work(void)
{
    if (app_var.siri_stu) {
        // siri不退出
        return true;
    }

    if ((get_call_status() == BT_CALL_OUTGOING)
        || (get_call_status() == BT_CALL_ALERT)
        || (get_call_status() == BT_CALL_INCOMING)
        || (get_call_status() == BT_CALL_ACTIVE)
       ) {
        // 通话不退出
        return true;
    }
    return false;
}


static int a2dp_media_packet_play_start(void *p)
{
    __this->back_mode_systime = 0;
    if ((__this->exit_flag == 0) || (__this->sbc_packet_step == 2)) {
        printf(" a2dp back \n");
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        tws_user_sync_box(TWS_BOX_A2DP_BACK_TO_BT_MODE_START, __this->a2dp_decoder_type);
        set_wait_a2dp_start(1);
#endif
        app_task_switch(APP_NAME_BT, ACTION_APP_MAIN, NULL);
        struct sys_event event;
        event.type = SYS_BT_EVENT;
        event.arg = (void *)SYS_BT_EVENT_TYPE_CON_STATUS;
        event.u.bt.event = BT_STATUS_A2DP_MEDIA_START;
        event.u.bt.value = __this->a2dp_decoder_type;
        sys_event_notify(&event);
    }
    return 0;
}

static int a2dp_media_packet_cmd_pp(void *p)
{
    if ((__this->exit_flag == 1) && (__this->sbc_packet_step == 1)) {
        log_info("send pp ");
        user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
    }
    return 0;
}


u8 bt_get_exit_flag()
{
    return __this->exit_flag;
}

int sbc_energy_check(u8 *packet, u16 size);
void tws_local_back_to_bt_mode(u8 mode, u8 value)
{
    r_printf("rx_tws_local_back_to_bt_mode=%d\n", mode);
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    if (mode == TWS_BOX_NOTICE_A2DP_BACK_TO_BT_MODE) {
        if (__this->tws_local_back_role == 1) {
            set_wait_a2dp_start(1);
            __this->tws_local_back_role = 0;
            if (__this->back_mode_systime == 0) {
                __this->cmd_flag = 1;
                __this->back_mode_systime = sys_timeout_add(NULL, a2dp_media_packet_play_start, 1);
            }
            r_printf("tws_local_back_role ==role_new back mode_switch=%d\n", __this->back_mode_systime);
        }

    } else if (mode == TWS_BOX_A2DP_BACK_TO_BT_MODE_START) {
        __this->tws_local_back_role = 0;
        struct sys_event event;
        event.type = SYS_BT_EVENT;
        event.arg = (void *)SYS_BT_EVENT_TYPE_CON_STATUS;
        event.u.bt.event = BT_STATUS_A2DP_MEDIA_START;
        event.u.bt.value = value;
        sys_event_notify(&event);

    } else if (mode == TWS_BOX_EXIT_BT) {
        __this->a2dp_start_flag = 0;
        if (value == TWS_UNACTIVE_DEIVCE) {

            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                printf("\n ----- master pause  -------  \n");
                user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PAUSE, 0, NULL);
            }
            if (a2dp_dec_close()) {
                bt_drop_a2dp_frame_start();
            }
            if (bt_get_exit_flag()) {
                __this->cmd_flag = 2;
            }
            __this->tws_local_back_role = 2;
            printf("__this->cmd_flag=0x%x,%d\n", __this->tws_local_back_role, __this->cmd_flag);
            app_task_switch(APP_NAME_BT, ACTION_APP_MAIN, NULL);
        } else {
            __this->exiting = 0;
            app_task_switch_target();
        }
    } else if (mode == TWS_BOX_ENTER_BT) {
        __this->tws_local_back_role = 0;
        if (!local_tws_dec_close(1)) {
            tws_api_local_media_trans_clear();
        }
    }
#endif

}

/*
 * 后台模式下sbc丢包加能量检测, 返回-INVALUE表示要丢掉此包数据
 */
int a2dp_media_packet_user_handler(u8 *data, u16 size)
{
    if ((get_call_status() != BT_CALL_HANGUP) || bt_phone_dec_is_running() || (__this->call_flag)) {
        if (get_call_status() != BT_CALL_HANGUP) {
            /* puts("call!=hangup"); */

        }
        /* __this->sbc_packet_lose_cnt = 0; */
        return -EINVAL;
    }

    /* #if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        if (is_tws_active_device()) {
            __this->sbc_packet_lose_cnt = 1;
            return -EINVAL;
        }
    #endif */

    if (__this->tws_local_back_role == 2) {

        putchar('$');
        /* r_printf("wait active_dev check_exit_back\n"); */
        return -EINVAL;
    }
    spin_lock(&lock);
    if (__this->exit_flag == 0) {
        if ((!bt_media_is_running()) && (__this->sbc_packet_lose_cnt)) {
            __this->sbc_packet_lose_cnt++;
            if (__this->sbc_packet_lose_cnt > 10) {
                __this->sbc_packet_lose_cnt = 0;
                __this->cmd_flag = 0;
                __this->sbc_packet_step = 0;
                spin_unlock(&lock);
                __this->a2dp_decoder_type = a2dp_media_packet_codec_type(data);
                log_info("sbc lose over %d", __this->a2dp_decoder_type);
                if (__this->back_mode_systime == 0) {
                    __this->back_mode_systime = sys_timeout_add(NULL, a2dp_media_packet_play_start, 1);
                }
                return 0;
            }
        } else {
            __this->sbc_packet_lose_cnt = 0;
        }
        spin_unlock(&lock);
        return 0;
    }
    u32 media_type = a2dp_media_packet_codec_type(data);
    u32 cur_time = timer_get_ms();
    int energy = 0;
    if (media_type == 0/* A2DP_CODEC_SBC */) {//后台返回蓝牙
        energy = sbc_energy_check(data, size);
    } else {
        if (a2dp_get_status() == BT_MUSIC_STATUS_STARTING) {
            energy = 2000;
            //其它格式不方便做能量检测。简易判断，该标志成立就直接认为是有效声音
        }
    }
    /* log_info("sbc_filter: %d ", energy); */
    if (__this->sbc_packet_step == 1) {
        // 退出
        if ((cur_time > __this->no_sbc_packet_to) || ((cur_time > __this->sbc_packet_filter_to))) {
            // 转换为后台模式
            log_info("goto back mode \n");
            __this->sbc_packet_step = 2;
            __this->no_sbc_packet_to = cur_time;
            __this->sbc_packet_filter_to = cur_time + SBC_FILTER_TIME_MS;
            __this->a2dp_decoder_type = a2dp_media_packet_codec_type(data);
        } else {
            // 还在退出
            /* log_info("exit,, "); */
            __this->no_sbc_packet_to = cur_time + NO_SBC_TIME_MS;
            if (energy > 1000) {
                __this->sbc_packet_filter_to = cur_time + SBC_ZERO_TIME_MS;
                __this->sbc_packet_valid_cnt ++;
                if (__this->sbc_packet_valid_cnt > __this->sbc_packet_valid_cnt_max) {
                    __this->sbc_packet_valid_cnt = 0;
                    if (__this->sbc_packet_valid_cnt_max < 80) {
                        __this->sbc_packet_valid_cnt_max += 10;
                    } else {
                        log_info("goto back mode0 \n");
                        __this->sbc_packet_step = 2;
                        __this->no_sbc_packet_to = cur_time;
                        __this->sbc_packet_filter_to = cur_time + SBC_FILTER_TIME_MS;
                    }
                    ///在退出的时候已经发送暂停
                    /* sys_timeout_add(NULL, a2dp_media_packet_cmd_pp, 1); */
                }
            } else {
                __this->sbc_packet_valid_cnt = 0;
            }
        }
    } else if (__this->sbc_packet_step == 2) {
        // 后台
        if (cur_time >= __this->no_sbc_packet_to) {
            // 新的开始
            if (energy > 1000) {
                log_info("new back mode \n");
                if (__this->tws_local_back_role) {
                    /* puts("tws_new_back_mode\n"); */
                    __this->no_sbc_packet_to = cur_time + NO_SBC_TIME_MS * 2;
                    __this->sbc_packet_filter_to = cur_time + SBC_FILTER_TIME_MS / 4;
                } else {
                    __this->no_sbc_packet_to = cur_time + NO_SBC_TIME_MS;
                    __this->sbc_packet_filter_to = cur_time + SBC_FILTER_TIME_MS;
                }
            } else {
                /* log_info("energy limit \n"); */
            }
        } else {
            /* log_info("bkm:%d, ", __this->sbc_packet_filter_to - cur_time); */
            if (__this->tws_local_back_role) {
                __this->no_sbc_packet_to = cur_time + NO_SBC_TIME_MS * 2;
            } else {
                __this->no_sbc_packet_to = cur_time + NO_SBC_TIME_MS;
            }
            if (cur_time > __this->sbc_packet_filter_to) {
                //过滤时间耗完
                __this->no_sbc_packet_to = cur_time;
                if (energy > 1000) {
                    log_info("start back mode \n");
                    __this->cmd_flag = 1;
                    __this->tws_local_back_role = 0;
                    spin_unlock(&lock);
                    __this->a2dp_decoder_type = a2dp_media_packet_codec_type(data);
                    if (__this->back_mode_systime == 0) {
                        __this->back_mode_systime = sys_timeout_add(NULL, a2dp_media_packet_play_start, 1);
                    }
                    return -EINVAL;
                }
            }
        }
    } else {
        spin_unlock(&lock);
        return 0;
    }
    spin_unlock(&lock);
    return -EINVAL;
}


extern int btstack_exit();
static void bt_no_background_exit_check(void *priv)
{
    if (play_poweron_ok_timer_id) {
        sys_timeout_del(play_poweron_ok_timer_id);
    }
    if (bt_user_priv_var.auto_connection_timer) {
        sys_timeout_del(bt_user_priv_var.auto_connection_timer);
        bt_user_priv_var.auto_connection_timer = 0;
    }

    if (__this->init_ok == 0) {
        putchar('#');
        return;
    }

    if (bt_audio_is_running()) {
        putchar('$');
        return;
    }

#if TCFG_USER_BLE_ENABLE
    bt_ble_exit();
#endif
#if TCFG_USER_TWS_ENABLE
    bt_tws_poweroff();
#endif
    btstack_exit();
    puts("bt_exit_check ok\n");
    __this->exit_flag = 1;
    sys_timer_del(__this->timer);
    __this->init_ok = 0;
    __this->timer = 0;
    __this->exiting = 0;
    set_stack_exiting(0);
    app_task_switch_target();
}

static u8 bt_background_exit()
{
    u8 suepend_rx_bulk = 1;
    __this->cmd_flag = 0;
    __this->call_flag = 0;

    u32 cur_time = timer_get_ms();
    spin_lock(&lock);
    __this->exit_flag = 1;
    __this->sbc_packet_step = 1;
    __this->sbc_packet_valid_cnt = 0;
    __this->sbc_packet_valid_cnt_max = 2;
    __this->no_sbc_packet_to = cur_time + NO_SBC_TIME_MS;
    __this->sbc_packet_filter_to = cur_time + SBC_ZERO_TIME_MS;
    spin_unlock(&lock);
    /* earphone_a2dp_audio_codec_close(); */

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    int state = tws_api_get_tws_state();
    suepend_rx_bulk = 0;
    __this->tws_local_back_role = 0;
    if (state & TWS_STA_SIBLING_CONNECTED) {
        __this->tws_local_back_role = 1;
        /* tws_user_sync_box(TWS_BOX_EXIT_BT, 0); */
        tws_api_sync_call_by_uuid('D', SYNC_CMD_BOX_INIT_EXIT_BT, 0); // delay不宜太长，避免两边结束时间差异大
        /* suepend_rx_bulk = 0; */
        local_tws_dec_close(1);
    } else {
        user_set_tws_box_mode(1);
    }
    r_printf("__this->tws_local_back_role=0x%x\n", __this->tws_local_back_role);

#endif
    a2dp_dec_close();
    extern int btctrler_suspend(u8 suepend_rx_bulk);
    btctrler_suspend(suepend_rx_bulk);
    extern int bredr_suspend();
    bredr_suspend();

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    if (state & TWS_STA_SIBLING_CONNECTED) {
        __this->exiting = 1;
        return -EINVAL;
    }
#endif
    return 0;
}

static u8 bt_background_poweroff_exit()
{
    __this->exiting = 1;
    set_stack_exiting(1);

    user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
    user_send_cmd_prepare(USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);
    user_send_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
    user_send_cmd_prepare(USER_CTRL_CONNECTION_CANCEL, 0, NULL);
    user_send_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);

    if (__this->exit_flag) {
        __this->sbc_packet_step = 0;
        __this->no_sbc_packet_to = 0;
        __this->sbc_packet_filter_to = 0;
        __this->sbc_packet_lose_cnt = 1;
    }

    if (__this->timer == 0) {
        __this->tmr_cnt = 0;
        __this->timer = sys_timer_add(NULL, bt_no_background_exit_check, 10);
        printf("set exit timer\n");
    }
    return -EINVAL;
}

static u8 bt_nobackground_exit()
{
    __this->exiting = 1;
    set_stack_exiting(1);
    __a2dp_drop_frame(NULL);//临时解决非后台退出杂音问题
    user_send_cmd_prepare(USER_CTRL_WRITE_SCAN_DISABLE, 0, NULL);
    user_send_cmd_prepare(USER_CTRL_WRITE_CONN_DISABLE, 0, NULL);
    user_send_cmd_prepare(USER_CTRL_PAGE_CANCEL, 0, NULL);
    user_send_cmd_prepare(USER_CTRL_CONNECTION_CANCEL, 0, NULL);
    user_send_cmd_prepare(USER_CTRL_POWER_OFF, 0, NULL);

    if (__this->timer == 0) {
        __this->tmr_cnt = 0;
        __this->timer = sys_timer_add(NULL, bt_no_background_exit_check, 10);
        printf("set exit timer\n");
    }

    return -EINVAL;
}

void esco_check_state(void *priv)
{
    /* if (bt_sco_state()) { */
    if (true == bt_phone_dec_is_running()) {
        sys_timeout_add(NULL, esco_check_state, 20);
    } else {
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        extern bool get_tws_sibling_connect_state(void);
        if (get_tws_sibling_connect_state()) {
            if (tws_api_get_role() == TWS_ROLE_MASTER) {
                tws_api_sync_call_by_uuid('T', SYNC_CMD_POWER_OFF_TOGETHER, TWS_SYNC_TIME_DO);
            }
        } else
#endif
        {
            sys_enter_soft_poweroff(NULL);
        }
    }
}
int bt_app_exit(void)
{
    struct sys_event clear_key_event = {.type =  SYS_KEY_EVENT, .arg = "key"};

    log_info("bt_app_exit");

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
    bt_emitter_mic_close();
#endif

    if (__this->exit_flag) {
        // 正在退出就不用重新设置
        // 避免快速调用两次导致发了两次pp，结束不了解码
        log_info("exit flag ");
#if TCFG_BLUETOOTH_BACK_MODE
        if ((app_var.goto_poweroff_flag == 0) && (__this->exiting == 1)) {
            return -EINVAL;
        } else {
            return 0;
        }
#else
        return 0;
#endif
    }
#if PHONE_CALL_FORCE_POWEROFF
    if (true == bt_phone_dec_is_running()) {
        log_info("bt_phone_dec_is_running");
        if (app_var.goto_poweroff_flag) {
            user_send_cmd_prepare(USER_CTRL_DISCONN_SCO, 0, NULL);
            sys_timeout_add(NULL, esco_check_state, 20);
        }
        app_var.goto_poweroff_flag = 0;
        return -EINVAL;
    }
#else
    if (true == bt_must_work()) {
        log_info("bt_must_work");
        app_var.goto_poweroff_flag = 0;
        return -EINVAL;
    }

    if (true == bt_phone_dec_is_running()) {
        log_info("bt_phone_dec_is_running");
        app_var.goto_poweroff_flag = 0;
        return -EINVAL;
    }
#endif
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))
    if (speaker_if_working()) {
        stop_loud_speaker();
    }
#endif

    __this->wait_exit = 0;
    __this->ignore_discon_tone = 1;
    bt_drop_a2dp_frame_stop();
    sys_key_event_disable();
    sys_event_clear(&clear_key_event);
    sys_auto_shut_down_disable();
    tone_play_stop();
#if (TCFG_SPI_LCD_ENABLE)
#include "ui/ui_style.h"
    extern int ui_hide_main(int id);
    ui_hide_main(ID_WINDOW_BT);
#endif


#if TCFG_BLUETOOTH_BACK_MODE
    if (app_var.goto_poweroff_flag == 0) {
        return	bt_background_exit();
    } else {
        return bt_background_poweroff_exit();
    }
#else
    return bt_nobackground_exit();
#endif
}

int bt_is_resume(void)
{
    if (bt_audio_is_running()) {
        return false;
    }
#if TCFG_BLUETOOTH_BACK_MODE
#else
    if (__this->exit_flag == 0) {
        return false;
    } else {
        __this->exit_flag = 0;
    }
#endif
    return true;
}

int app_sys_bt_event_opr(struct sys_event *event)
{
#if TCFG_BLUETOOTH_BACK_MODE
    u8 enter_bt = 0;
    u8 bt_call = 0;
    STATUS *p_tone = get_tone_config();

    if ((u32)event->arg == SYS_BT_EVENT_TYPE_CON_STATUS) {
        printf("bt con event: %d \n", event->u.bt.event);
        switch (event->u.bt.event) {
        // 需要切换蓝牙的命令
        case BT_STATUS_A2DP_MEDIA_START:
            if (__this->sbc_packet_step != 0) {
                log_info("sbc_packet_step : %d \n", __this->sbc_packet_step);
                break;
            }

        case BT_STATUS_FIRST_DISCONNECT:
        case BT_STATUS_SECOND_DISCONNECT:

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
            enter_bt = 1;

#else

#if BACKGROUND_GOBACK
            /* enter_bt = 1; */
#else
            /* if (tws_api_get_role() == TWS_ROLE_MASTER) {
                tws_api_sync_call_by_uuid('T', SYNC_CMD_LED_PHONE_DISCONN_STATUS, 400);
            } */
#endif
#endif
            break;

        case BT_STATUS_SECOND_CONNECTED:
        case BT_STATUS_FIRST_CONNECTED:
#if BACKGROUND_GOBACK
            enter_bt = 1;
#else

#if TCFG_USER_TWS_ENABLE
            /* bt_tws_phone_connected(); */
#endif
#endif
            break;

        case BT_STATUS_START_CONNECTED:
#if BACKGROUND_GOBACK
            enter_bt = 1;
#endif
            break;

        case  BT_STATUS_ENCRY_COMPLETE:
            break;

        case BT_STATUS_SCO_STATUS_CHANGE:
#if BACKGROUND_GOBACK
            enter_bt = 1;
#endif
            break;
        case BT_STATUS_VOICE_RECOGNITION:
        case BT_STATUS_PHONE_INCOME:
        case BT_STATUS_PHONE_NUMBER:
        /* case BT_STATUS_PHONE_MANUFACTURER: */
        case BT_STATUS_PHONE_OUT:
        case BT_STATUS_PHONE_ACTIVE:
            /* case BT_STATUS_PHONE_HANGUP: */
            bt_call = 1;
            enter_bt = 1;
            break;
        // 不需要处理的命令
        case BT_STATUS_A2DP_MEDIA_STOP:
            bt_drop_a2dp_frame_stop();
        case BT_STATUS_CALL_VOL_CHANGE:
            /* case BT_STATUS_SNIFF_STATE_UPDATE: */
            break;
        // 按原方式处理的命令
        default:
            bt_connction_status_event_handler(&event->u.bt);
            break;
        }
    } else if ((u32)event->arg == SYS_BT_EVENT_TYPE_HCI_STATUS) {
        log_info("bt hci event: %d \n", event->u.bt.event);
        switch (event->u.bt.event) {
        case HCI_EVENT_IO_CAPABILITY_REQUEST:
            clock_add_set(BT_CONN_CLK);
#if BACKGROUND_GOBACK
            enter_bt = 1;
#endif
            break;

        default:
            bt_hci_event_handler(&event->u.bt);
            break;
        }
    }
#if TCFG_USER_TWS_ENABLE
    else if (((u32)event->arg == SYS_BT_EVENT_FROM_TWS)) {
        log_info("bt tws event: %d \n", event->u.bt.event);
        switch (event->u.bt.event) {
        /* case TWS_EVENT_CONNECTED: */
        /* enter_bt = 1; */
        /* break; */
        default:
            bt_tws_connction_status_event_handler(&event->u.bt);
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
            dec2tws_tws_event_deal(&event->u.bt);
#endif
            break;
        }
    }
#endif
#if (DUEROS_DMA_EN)
    else if (((u32)event->arg == SYS_BT_AI_EVENT_TYPE_STATUS)) {
        log_info("bt AI event: %d \n", event->u.bt.event);
    }
#endif
    else if ((u32)event->arg == SYS_EVENT_FROM_CTRLER) {
        switch (event->u.bt.event) {
        case  BTCTRLER_EVENT_RESUME_REQ:
            printf("-------   BTCTRLER_EVENT_RESUME_REQ \n");
            bt_call = 1;
            enter_bt = 1;
            break;
        }

    }

    if (enter_bt) {
        printf("enter bt \n");
        if (false == app_cur_task_check(APP_NAME_BT)) {
            __this->cmd_flag = 1;
            __this->call_flag = bt_call;
            app_task_switch(APP_NAME_BT, ACTION_APP_MAIN, NULL);
            sys_event_notify(event);
        }
    }

#endif

    return false;
}

extern int app_earphone_key_event_handler(struct sys_event *);

/*
 * 系统事件处理函数
 */
static int event_handler(struct application *app, struct sys_event *event)
{
    if (__this->exit_flag) {
        return false;
    }


    switch (event->type) {
    case SYS_KEY_EVENT:
        return app_earphone_key_event_handler(event);

    case SYS_BT_EVENT:
        if ((u32)event->arg == SYS_BT_EVENT_TYPE_CON_STATUS) {
            bt_connction_status_event_handler(&event->u.bt);
        } else if ((u32)event->arg == SYS_BT_EVENT_TYPE_HCI_STATUS) {
            bt_hci_event_handler(&event->u.bt);
        }
#if TCFG_USER_TWS_ENABLE
        else if (((u32)event->arg == SYS_BT_EVENT_FROM_TWS)) {
            bt_tws_connction_status_event_handler(&event->u.bt);
        }
#endif
        else if (((u32)event->arg == SYS_BT_EVENT_FROM_KEY)) {
            log_info("SYS_BT_EVENT_FROM_KEY\n");
            switch (event->u.key.event) {
            case KEY_CHANGE_MODE:
                /* log_info("KEY_CHANGE_MODE\n"); */
                app_task_next();
                break;
            }
        }
        return true;

    default:
        return false;
    }
    return false;
}

void bt_background_init()
{
    if (__this->back_mode_systime) {
        sys_timeout_del(__this->back_mode_systime);
        __this->back_mode_systime = 0;
        puts("__this->init_ok clear back_mode_systime\n");
    }
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    int state = tws_api_get_tws_state();
    if (state & TWS_STA_SIBLING_CONNECTED) {
        if (__this->cmd_flag != 2) {
            puts("tx_SYNC_CMD_BOX_ENTER_BT\n");
            tws_api_sync_call_by_uuid('D', SYNC_CMD_BOX_ENTER_BT, 150);
        }
    } else {
        /* user_set_tws_box_mode(2); */
        user_set_tws_box_mode(0);
    }
#endif
    spin_lock(&lock);
    __this->sbc_packet_step = 0;
    __this->no_sbc_packet_to = 0;
    __this->sbc_packet_filter_to = 0;
    __this->sbc_packet_lose_cnt = 1;
    __this->tws_local_back_role = 0;
    spin_unlock(&lock);
    extern void bredr_resume();//background resume
    bredr_resume();
    void btctrler_resume();
    btctrler_resume();

    bt_resume_deal();
    __this->cmd_flag = 0;
#if TCFG_UI_ENABLE
    ui_set_tmp_menu(MENU_BT, 1000, 0, NULL);
#endif

}

extern void bt_set_tx_power(u8 txpower);
extern void overlay_mode_clear(void);

static u8 led7_skip_vm_onoff = 1;
u8 led7_skip_vm_flag(void)      // 控制led7扫描中断是否需要在vm操作时跳过 0:跳过 1:不跳过 (蓝牙模式跳过防止在连接时闪屏)
{
    return led7_skip_vm_onoff;
}
/*
 * earphone 模式状态机, 通过start_app()控制状态切换
 */
static int state_machine(struct application *app, enum app_state state, struct intent *it)
{
    switch (state) {
    case APP_STA_CREATE:
        /* server_load(audio_dec); */
        /* server_load(audio_rec); */
        break;
    case APP_STA_START:
        //进入蓝牙模式,UI退出充电状态
        ui_update_status(STATUS_EXIT_LOWPOWER);
        if (!it) {
            break;
        }
        //overlay_mode_clear();
        switch (it->action) {
        case ACTION_APP_MAIN:
            log_info("ACTION_APP_MAIN\n");
            /*
             * earphone 模式初始化
             */
            if (__this->exit_flag == 0) {
                break;
            }

            led7_skip_vm_onoff = 0;
            clock_idle(BT_IDLE_CLOCK);
            u32 sys_clk =  clk_get("sys");
            bt_pll_para(TCFG_CLOCK_OSC_HZ, sys_clk, 0, 0);

            __this->ignore_discon_tone = 0;
            __this->exiting = 0;
            __this->exit_flag = 0;
            __this->wait_exit = 0;


#if TCFG_UI_ENABLE
            ui_set_main_menu(UI_BT_MENU_MAIN);
            ui_set_tmp_menu(MENU_BT, 1000, 0, NULL);
#endif


#if (TCFG_SPI_LCD_ENABLE)
#include "ui/ui_style.h"
            extern int ui_show_main(int id);
            ui_show_main(ID_WINDOW_BT);
#endif

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
            if (bt_emitter_stu_get() == true) {
                bt_emitter_mic_open();
            }
#endif

#if TCFG_BLUETOOTH_BACK_MODE
            if (__this->init_ok) {
                bt_background_init();
                break;
            }
#else
            app_var.goto_poweroff_flag = 0;
#endif
            bt_function_select_init();
            bredr_handle_register();
            btstack_init();
#if TCFG_USER_TWS_ENABLE
            tws_profile_init();
#endif

            /* 按键消息使能 */
            sys_key_event_enable();
            sys_auto_shut_down_enable();
            sys_auto_sniff_controle(1, NULL);

            break;
        }
        break;
    case APP_STA_PAUSE:
        break;
    case APP_STA_RESUME:
        __this->auto_exit_limit_time = (u32) - 1;
        break;
    case APP_STA_STOP:
        log_info("APP_STA_STOP\n");
        tone_play_stop();
        break;
    case APP_STA_DESTROY:
        log_info("APP_STA_DESTROY\n");
        led7_skip_vm_onoff = 1;
        if (__this->timer) {
            sys_timer_del(__this->timer);
            __this->timer = 0;
        }
        sys_auto_shut_down_disable();
        __this->auto_exit_limit_time = (u32) - 1;
        break;
    }

    return 0;
}


int bt_mode_tone_play_check(void)
{
    if (__this) {
        if (__this->init_ok) {
            if (__this->cmd_flag) {
                printf("no need to play bt mode tone!!!!!!!\n");
                return 0;
            }
            bt_drop_a2dp_frame_start();
        }
    }

    return 1;
}

static int  bt_tone_prepare()
{
#if TCFG_UI_ENABLE
    ui_set_tmp_menu(MENU_BT, 0, 0, NULL);
#endif
    return 0;
}

static int bt_user_msg_deal(int msg, int argc, int *argv)
{
#ifdef CONFIG_CPU_BR25
    switch (msg) {
    case USER_MSG_SYS_MIXER_RECORD_SWITCH:
        return 0;
    default:
        break;
    }
#endif

    return 0;
}

static const struct application_reg app_bt_reg = {
    .tone_name = TONE_BT_MODE,
    .tone_play_check = bt_mode_tone_play_check,
    .tone_prepare = NULL,//bt_tone_prepare,
    .enter_check = NULL,
    .exit_check = bt_app_exit,
    .user_msg = bt_user_msg_deal,
#if (defined(TCFG_TONE2TWS_ENABLE) && (TCFG_TONE2TWS_ENABLE))
    .tone_tws_cmd = SYNC_CMD_MODE_BT,
#endif
};

static const struct application_operation app_soundbox_ops = {
    .state_machine  = state_machine,
    .event_handler 	= event_handler,
};

REGISTER_APPLICATION(app_app_music) = {
    .name 	= APP_NAME_BT,
    .action	= ACTION_APP_MAIN,
    .ops 	= &app_soundbox_ops,
    .state  = APP_STA_DESTROY,
    .private_data = (void *) &app_bt_reg,
};

