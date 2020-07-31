#include "key_event_deal.h"
#include "btstack/avctp_user.h"
#include "event.h"
#include "app_power_manage.h"
#include "app_main.h"
#include "tone_player.h"
#include "audio_config.h"
#include "user_cfg.h"
#include "system/timer.h"
#include "vol_sync.h"
#include "media/includes.h"
#include "app_action.h"
#include "app_config.h"

#include "ui/ui_api.h"
#include "fm_emitter/fm_emitter_manage.h"
#include "bt_tws.h"
#include "audio_reverb.h"
#include "loud_speaker.h"
#define LOG_TAG_CONST       KEY_EVENT_DEAL
#define LOG_TAG             "[KEY_EVENT_DEAL]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#define POWER_OFF_CNT       10

static u8 goto_poweroff_cnt = 0;
static u8 goto_poweroff_flag = 0;
extern u8 bt_sco_state(void);
extern u8 get_max_sys_vol(void);
extern bool get_tws_sibling_connect_state(void);
extern u8 common_key_event_get(struct key_event *key);
extern u8 bt_key_event_get(struct key_event *key);
#if(defined(TCFG_MIXERCH_REC_EN) && (TCFG_MIXERCH_REC_EN))
extern bool mixer_record_check();
extern int mixer_record_start(void);
extern int mixer_record_stop(void);
#endif
extern int bt_get_low_latency_mode();
extern void bt_set_low_latency_mode(int enable);
extern u8 is_local_tws_dec_open();
extern u8 is_tws_all_in_bt();
void sys_enter_soft_poweroff(void *priv);
u8 bt_phone_dec_is_running();
u8 is_a2dp_dec_open();

u8 poweroff_sametime_flag = 0;
#if CONFIG_TWS_POWEROFF_SAME_TIME
enum {
    FROM_HOST_POWEROFF_CNT_ENOUGH = 1,
    FROM_TWS_POWEROFF_CNT_ENOUGH,
    POWEROFF_CNT_ENOUGH,
};

static u16 poweroff_sametime_timer_id = 0;
static void poweroff_sametime_timer(void *priv)
{
    log_info("poweroff_sametime_timer\n");
    int state = tws_api_get_tws_state();
    if (!(state & TWS_STA_SIBLING_CONNECTED)) {
        sys_enter_soft_poweroff(NULL);
        return;
    }
    if (priv == NULL || poweroff_sametime_flag == POWEROFF_CNT_ENOUGH) {
        poweroff_sametime_timer_id = sys_timeout_add((void *)1, poweroff_sametime_timer, 500);
    } else {
        poweroff_sametime_timer_id = 0;
    }
}

static void poweroff_sametime_timer_init(void)
{
    if (poweroff_sametime_timer_id) {
        return;
    }

    poweroff_sametime_timer_id = sys_timeout_add(NULL, poweroff_sametime_timer, 500);
}
#endif

#if TCFG_USER_TWS_ENABLE
u8 replay_tone_flag = 1;
int max_tone_timer_hdl = 0;
static void max_tone_timer(void *priv)
{
    if (!tone_get_status()) {
        max_tone_timer_hdl = 0;
        replay_tone_flag = 1;
    } else {
        max_tone_timer_hdl = sys_timeout_add(NULL, max_tone_timer, TWS_SYNC_TIME_DO + 100);
    }
}
#endif

static void volume_up(void)
{
    u8 test_box_vol_up = 0x41;
    s8 cur_vol = 0;
    u8 call_status = get_call_status();

    if (tone_get_status()) {
        if (get_call_status() == BT_CALL_INCOMING) {
            app_audio_volume_up(1);
        }
        return;
    }

    /*打电话出去彩铃要可以调音量大小*/
    if ((call_status == BT_CALL_ACTIVE) || (call_status == BT_CALL_OUTGOING)) {
        cur_vol = app_audio_get_volume(APP_AUDIO_STATE_CALL);
    } else {
        cur_vol = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
    }
    if (get_remote_test_flag()) {
        user_send_cmd_prepare(USER_CTRL_TEST_KEY, 1, &test_box_vol_up); //音量加
    }

    if (cur_vol >= app_audio_get_max_volume()) {
#if TCFG_USER_TWS_ENABLE
        if (get_tws_sibling_connect_state()) {
            if (tws_api_get_role() == TWS_ROLE_MASTER && replay_tone_flag) {
                replay_tone_flag = 0;               //防止提示音被打断标志
                tws_api_sync_call_by_uuid('T', SYNC_CMD_MAX_VOL, TWS_SYNC_TIME_DO);
                max_tone_timer_hdl = sys_timeout_add(NULL, max_tone_timer, TWS_SYNC_TIME_DO + 100);  //同步在TWS_SYNC_TIME_DO之后才会播放提示音，所以timer需要在这个时间之后才去检测提示音状态
            }
        } else
#endif
        {
#if TCFG_MAX_VOL_PROMPT
            STATUS *p_tone = get_tone_config();
            tone_play_index(p_tone->max_vol, 1);
#endif
        }

        if (get_call_status() != BT_CALL_HANGUP) {
            /*本地音量最大，如果手机音量还没最大，继续加，以防显示不同步*/
            if (bt_user_priv_var.phone_vol < 15) {
                //user_send_cmd_prepare(USER_CTRL_HFP_CALL_VOLUME_UP, 0, NULL);
                user_send_cmd_prepare(USER_CTRL_HID_VOL_UP, 0, NULL);     //使用HID调音量
            }
            return;
        }
#if BT_SUPPORT_MUSIC_VOL_SYNC
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        if (!is_tws_all_in_bt()) {
            printf(">>>>>>>tws is not all in bt mode !!!\n");
        } else
#endif
        {
            opid_play_vol_sync_fun(&app_var.music_volume, 1);
            user_send_cmd_prepare(USER_CTRL_CMD_SYNC_VOL_INC, 0, NULL);
        }
#endif/*BT_SUPPORT_MUSIC_VOL_SYNC*/
        return;
    }

    opid_play_vol_sync_fun(&app_var.music_volume, 1);
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, app_var.music_volume, 1);
    log_info("vol+: %d", app_audio_get_volume(APP_AUDIO_CURRENT_STATE));
    if (get_call_status() != BT_CALL_HANGUP) {
        //user_send_cmd_prepare(USER_CTRL_HFP_CALL_VOLUME_UP, 0, NULL);
        user_send_cmd_prepare(USER_CTRL_HID_VOL_UP, 0, NULL);     //使用HID调音量
    } else {
#if BT_SUPPORT_MUSIC_VOL_SYNC
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        if (!is_tws_all_in_bt()) {
            printf(">>>>>>>tws is not all in bt mode !!!\n");
        } else
#endif
        {
            /* opid_play_vol_sync_fun(&app_var.music_volume, 0); */

#if TCFG_USER_TWS_ENABLE
            user_send_cmd_prepare(USER_CTRL_CMD_SYNC_VOL_INC, 0, NULL);     //使用HID调音量
            //user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_SEND_VOL, 0, NULL);
#else
            user_send_cmd_prepare(USER_CTRL_CMD_SYNC_VOL_INC, 0, NULL);
#endif/*TCFG_USER_TWS_ENABLE*/
        }
#endif/*BT_SUPPORT_MUSIC_VOL_SYNC*/
    }
}
static void volume_down(void)
{
    u8 test_box_vol_down = 0x42;

    if (tone_get_status()) {
        if (get_call_status() == BT_CALL_INCOMING) {
            app_audio_volume_down(1);
        }
        return;
    }
    if (get_remote_test_flag()) {
        user_send_cmd_prepare(USER_CTRL_TEST_KEY, 1, &test_box_vol_down); //音量减
    }

    if (app_audio_get_volume(APP_AUDIO_CURRENT_STATE) <= 0) {
        if (get_call_status() != BT_CALL_HANGUP) {
            /*
             *本地音量最小，如果手机音量还没最小，继续减
             *注意：有些手机通话最小音量是1(GREE G0245D)
             */
            if (bt_user_priv_var.phone_vol > 1) {
                //user_send_cmd_prepare(USER_CTRL_HFP_CALL_VOLUME_UP, 0, NULL);
                user_send_cmd_prepare(USER_CTRL_HID_VOL_DOWN, 0, NULL);     //使用HID调音量
            }
            return;
        }
#if BT_SUPPORT_MUSIC_VOL_SYNC
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        if (!is_tws_all_in_bt()) {
            printf(">>>>>>>tws is not all in bt mode !!!\n");
        } else
#endif
        {
            opid_play_vol_sync_fun(&app_var.music_volume, 0);
            user_send_cmd_prepare(USER_CTRL_CMD_SYNC_VOL_DEC, 0, NULL);
        }
#endif
        return;
    }

    opid_play_vol_sync_fun(&app_var.music_volume, 0);
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, app_var.music_volume, 1);
    r_printf("vol-: %d", app_audio_get_volume(APP_AUDIO_CURRENT_STATE));
    if (get_call_status() != BT_CALL_HANGUP) {
        //user_send_cmd_prepare(USER_CTRL_HFP_CALL_VOLUME_DOWN, 0, NULL);
        user_send_cmd_prepare(USER_CTRL_HID_VOL_DOWN, 0, NULL);
    } else {
#if BT_SUPPORT_MUSIC_VOL_SYNC
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        if (!is_tws_all_in_bt()) {
            printf(">>>>>>>tws is not all in bt mode !!!\n");
        } else
#endif
        {
            /* opid_play_vol_sync_fun(&app_var.music_volume, 0); */
            if (app_audio_get_volume(APP_AUDIO_CURRENT_STATE) == 0) {
                app_audio_volume_down(0);
            }

#if TCFG_USER_TWS_ENABLE
            user_send_cmd_prepare(USER_CTRL_CMD_SYNC_VOL_DEC, 0, NULL);
            //user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_SEND_VOL, 0, NULL);
#else
            user_send_cmd_prepare(USER_CTRL_CMD_SYNC_VOL_DEC, 0, NULL);
#endif
        }
#endif
    }
}



#if ONE_KEY_CTL_DIFF_FUNC
static void lr_diff_otp_deal(u8 opt, char channel)
{
    log_info("lr_diff_otp_deal:%d", opt);
    switch (opt) {
    case ONE_KEY_CTL_NEXT_PREV:
        if (channel == 'L') {
            user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_NEXT, 0, NULL);
        } else if (channel == 'R') {
            user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PREV, 0, NULL);
        } else {
            user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_NEXT, 0, NULL);
        }
        break;
    case ONE_KEY_CTL_VOL_UP_DOWN:
        if (channel == 'L') {
            volume_up();
        } else if (channel == 'R') {
            volume_down();
        }
        break;
    default:
        break;
    }
}

void key_tws_lr_diff_deal(struct sys_event *event, u8 opt)
{
    u8 channel = 'U';
    if (get_bt_tws_connect_status()) {
        channel = tws_api_get_local_channel();
        if ('L' == channel) {
            channel = (u32)event->arg == KEY_EVENT_FROM_TWS ? 'R' : 'L';
        } else {
            channel = (u32)event->arg == KEY_EVENT_FROM_TWS ? 'L' : 'R';
        }
    }
    lr_diff_otp_deal(opt, channel);
}
#else
void key_tws_lr_diff_deal(struct sys_event *event, u8 opt)
{
}
#endif

u8 is_tws_key_filter(void);
int app_earphone_key_event_handler(struct sys_event *event)
{
    int ret = true;
    struct key_event *key = &event->u.key;
    u8 vol;
#if TCFG_APP_FM_EMITTER_EN
#if TCFG_UI_ENABLE

    if (ui_get_app_menu(GRT_CUR_MENU) == MENU_FM_SET_FRE) {
        return false;
    }

#endif
#endif

#if (TCFG_SPI_LCD_ENABLE)
    extern int key_is_ui_takeover();
    if (key_is_ui_takeover()) {
        return false;
    }
#endif


    u8 key_event = bt_key_event_get(key);//key_table[key->value][key->event];

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    u8 is_tws_all_in_bt();
    if ((key_event != KEY_POWEROFF) && \
        (key_event != KEY_POWEROFF_HOLD) && \
        (key_event != KEY_CHANGE_MODE)) {
        if (!is_tws_all_in_bt()) {
            return true;
        }
    }
#endif

#if RCSP_ADV_EN
    extern void set_key_event_by_rcsp_info(struct sys_event * event, u8 * key_event);
    set_key_event_by_rcsp_info(event, &key_event);
#endif

    r_printf("bt key_event:%d %d %d %d\n", key_event, key->value, key->event, key->init);

    switch (key_event) {
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))		
		case KEY_SPEAKER_OPEN:
			if ((get_call_status() == BT_CALL_ACTIVE) ||
					(get_call_status() == BT_CALL_OUTGOING) ||
					(get_call_status() == BT_CALL_ALERT) ||
					(get_call_status() == BT_CALL_INCOMING)) {
				//通话过程不允许开/关扩音
				break;
			}
			if(speaker_if_working()){
				stop_loud_speaker();	
			}else{
				start_loud_speaker(NULL);	
			}
			return true;
#endif
    case  KEY_MUSIC_PP:
        r_printf("   KEY_MUSIC_PP  \n");
        if ((get_call_status() == BT_CALL_OUTGOING) ||
            (get_call_status() == BT_CALL_ALERT)) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
        } else if (get_call_status() == BT_CALL_INCOMING) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_ANSWER, 0, NULL);
        } else if (get_call_status() == BT_CALL_ACTIVE) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
        } else {
            user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
        }
        break;
    case  KEY_MUSIC_PREV:

        r_printf("    KEY_MUSIC_PREV \n");


        user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PREV, 0, NULL);
        break;
    case  KEY_MUSIC_NEXT:
        r_printf("    KEY_MUSIC_NEXT \n");

#ifdef CONFIG_BOARD_AC6933B_LIGHTING
        if (get_call_status() == BT_CALL_INCOMING) {
            user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
            break;
        }
#endif
        user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_NEXT, 0, NULL);
        break;
    case  KEY_VOL_UP:
        if (get_call_status() == BT_CALL_ACTIVE && bt_sco_state() == 0) {
            break;
        }
        r_printf("   KEY_VOL_UP  \n");
        volume_up();
#if TCFG_UI_ENABLE
        vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
        ui_set_tmp_menu(MENU_MAIN_VOL, 1000, vol, NULL);
#endif //TCFG_UI_ENABLE
        break;
    case  KEY_VOL_DOWN:
        if (get_call_status() == BT_CALL_ACTIVE && bt_sco_state() == 0) {
            break;
        }
        r_printf("   KEY_VOL_DOWN  \n");
        volume_down();
#if TCFG_UI_ENABLE
        vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
        ui_set_tmp_menu(MENU_MAIN_VOL, 1000, vol, NULL);
#endif //TCFG_UI_ENABLE
        break;
    case  KEY_CALL_LAST_NO:
//充电仓不支持 双击回播电话,也不支持 双击配对和取消配对
#if (!TCFG_CHARGESTORE_ENABLE)
#if TCFG_USER_TWS_ENABLE
        if (bt_tws_start_search_sibling()) {
            tone_play_index(IDEX_TONE_NORMAL, 1);
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
#endif
        break;
    case  KEY_CALL_HANG_UP:
        printf("KEY_CALL_HANG_UP\n");
        user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
        break;
    case  KEY_CALL_ANSWER:

        break;
    case  KEY_OPEN_SIRI:
        user_send_cmd_prepare(USER_CTRL_HFP_GET_SIRI_OPEN, 0, NULL);
        break;
    case  KEY_HID_CONTROL:
        log_info("get_curr_channel_state:%x\n", get_curr_channel_state());
        if (get_curr_channel_state() & HID_CH) {
            log_info("KEY_HID_CONTROL\n");
            user_send_cmd_prepare(USER_CTRL_HID_IOS, 0, NULL);
        }
        break;
    case KEY_THIRD_CLICK:
        log_info("KEY_THIRD_CLICK");
        key_tws_lr_diff_deal(event, ONE_KEY_CTL_NEXT_PREV);
        break;
    case KEY_LOW_LANTECY:
        bt_set_low_latency_mode(!bt_get_low_latency_mode());
        break;
    case  KEY_NULL:
        ret = false;
#if TCFG_USER_TWS_ENABLE
        if ((u32)event->arg == KEY_EVENT_FROM_TWS) {
            break;
        }
#endif
        /* goto_poweroff_cnt = 0; */
        /* goto_poweroff_flag = 0; */
        break;

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
    case KEY_REVERB_OPEN:
        if ((get_call_status() == BT_CALL_ACTIVE) ||
            (get_call_status() == BT_CALL_OUTGOING) ||
            (get_call_status() == BT_CALL_ALERT) ||
            (get_call_status() == BT_CALL_INCOMING)) {
            //通话过程不允许开/关混响
            break;
        }
        ret = false;
        break;
#endif
    default:
        ret = false;
        break;
    }
    /* printf(">ret:%d\n", ret); */
    return ret;
}
