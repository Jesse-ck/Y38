#include "tm_gma_type.h"
#include "tm_gma.h"
#include "tm_gma_hw_driver.h"
#include "cpu.h"
//#include "speech.h"
#include "uart.h"
//#include "audio.h"
//#include "audio/dac_api.h"
#include "btstack/avctp_user.h"
//#include "power.h"
#include "audio_config.h"
#include "app_power_manage.h"
#include "key_event_deal.h"
#include "gma_include.h"
#include "3th_profile_api.h"
//#include "sdk_cfg.h"
//#include "common.h"
//#include "msg.h"
#if FMTX_EN
#include "fmtx_api.h"
#endif

#if (GMA_EN)
#define TM_GMA_APP_DEBUG  1

#if TM_GMA_APP_DEBUG
#define TM_DBG		printf
#else
#define TM_DBG
#endif

/**************************************************************
	moudle:			cirtical
**************************************************************/
extern void local_irq_disable();
extern void local_irq_enable();

#define CPU_INT_DIS()       local_irq_disable()
#define CPU_INT_EN()        local_irq_enable()

static void TM_ENTER_CRITICAL(void)
{
    CPU_INT_DIS();
}

static void TM_EXIT_CRITICAL(void)
{
    CPU_INT_EN();
}

/**************************************************************
	moudle:		      keysent
**************************************************************/
static void tm_key_msg_notif(u16 msg, u8 *data, u16 len)
{
    struct sys_event e;

    e.type = SYS_BT_AI_EVENT;
    e.u.key.event = msg;
    e.u.key.value = 0;
    sys_event_notify(&e);

}

/**************************************************************
	moudle:			exchange device information
**************************************************************/
/*
phone to firmware
*/
static void gma_hw_set_phone_sys_info(_uint8 sys)
{
    switch (sys) {
    case _GMA_PHONE_TYPE_IOS:
        TM_DBG(" IOS SYSTEM \n");
        break;
    case _GMA_PHONE_TYPE_ANDROID:
        TM_DBG(" ANDROID SYSTEM \n");
        break;

    default:
        break;
    }
}
/*
firmware to phone
*/
#define GMA_DEV_ABILITY_SUPPORT_WAKEUP		BIT(0)
#define GMA_DEV_ABILITY_SUPPORT_VAD			BIT(1)
#define GMA_DEV_ABILITY_SUPPORT_AEC			BIT(2)
#define GMA_DEV_ABILITY_SUPPORT_DNC			BIT(3)
#define GMA_DEV_ABILITY_SUPPORT_A2DP		BIT(4)
#define GMA_DEV_ABILITY_SUPPORT_FM_TX		BIT(5)
#define GMA_DEV_ABILITY_SUPPORT_HFP			BIT(6)
/*reserved BIT(7)  ~  BIT(15)*/
static _uint16 gma_hw_get_fw_abilities(void)
{
#if (!FMTX_EN)
    _uint16 fw_abilities = GMA_DEV_ABILITY_SUPPORT_WAKEUP | GMA_DEV_ABILITY_SUPPORT_A2DP | GMA_DEV_ABILITY_SUPPORT_HFP;
#else
    _uint16 fw_abilities = GMA_DEV_ABILITY_SUPPORT_WAKEUP | GMA_DEV_ABILITY_SUPPORT_A2DP | GMA_DEV_ABILITY_SUPPORT_HFP | GMA_DEV_ABILITY_SUPPORT_FM_TX;
#endif

    return fw_abilities;
}

static _uint8 gma_hw_get_enc_type(void)
{
    _uint8 enc_type = _GMA_ENC_TYPE_OPUS;

    return enc_type;
}

void gma_bt_mac_addr_get(uint8_t *buf);
static _uint8 bt_addr[6];
static _uint8 *gma_hw_get_bt_mac_addr(void)
{
    /*
    copy edr bt mac addr to bt_mac_addr buffer
    */
    gma_bt_mac_addr_get(bt_addr);
    return bt_addr;
}

/**************************************************************
	moudle:			alive device
**************************************************************/

/**************************************************************
	moudle:			audio send
**************************************************************/

/**************************************************************
	moudle:			app notify state
**************************************************************/
/*phone to firmware*/
static void gma_hw_app_notify_state(_uint8 state)
{
    switch (state) {
    case _GMA_DEV_ENTER_CONNECT:
        TM_DBG("_GMA_DEV_ENTER_CONNECT \n");
        break;
    case _GMA_DEV_START_RECORD:
        TM_DBG("_GMA_DEV_START_RECORD \n");
        /*start record*/
        gma_hw_api.start_speech();
        break;
    case _GMA_DEV_STOP_RECORD:
        TM_DBG("_GMA_DEV_STOP_RECORD \n");
        /*stop record*/
        gma_hw_api.stop_speech();
        break;
    case _GMA_DEV_START_PLAY_TTS:
        TM_DBG("_GMA_DEV_START_PLAY_TTS \n");
        break;
    case _GMA_DEV_STOP_PLAY_TTS:
        TM_DBG("_GMA_DEV_STOP_PLAY_TTS \n");
        break;

    default:
        break;
    }
}

/*firmware to phone*/

/**************************************************************
	moudle:			firmware notify state
**************************************************************/

/**************************************************************
	moudle:			audio control
**************************************************************/
/*phone to firmware*/
static void gma_hw_set_sys_vol(_uint8 vol)
{
    u8 vol_l = (vol * get_max_sys_vol()) / 0x64;
    u8 vol_r = (vol * get_max_sys_vol()) / 0x64;

    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, APP_AUDIO_STATE_MUSIC, vol_r);
    /*set system volume*/
}

/*firmware to phone*/
#include "btstack/avctp_user.h"
bool a2dp_is_conn(void)
{
    return (BT_STATUS_WAITINT_CONN != get_bt_connect_status());
}
static _uint8 gma_hw_a2dp_connected_state(void)
{
    return a2dp_is_conn() ? _GMA_SUCCEED : _GMA_FAIL;
}

static _uint8 gma_hw_audio_state(struct _gma_app_audio_ctl info)
{

    _uint8 succeed_or_fail = _GMA_FAIL;
    switch (info.type) {
    case _GMA_AUDIO_CTL_A2DP_STATE:
        succeed_or_fail = gma_hw_a2dp_connected_state();
        TM_DBG("_GMA_AUDIO_CTL_A2DP_STATE \n");
        break;
    case _GMA_AUDIO_CTL_SEL_VOL:
        TM_DBG("_GMA_AUDIO_CTL_SEL_VOL \n");
        gma_hw_api.set_sys_vol(info.value);
        succeed_or_fail = _GMA_SUCCEED;
        break;
    case _GMA_AUDIO_CTL_PLAY:
        TM_DBG("_GMA_AUDIO_CTL_PLAY \n");
        user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PLAY, 0, NULL);
        succeed_or_fail = _GMA_SUCCEED;
        break;
    case _GMA_AUDIO_CTL_PAUSE:
        TM_DBG("_GMA_AUDIO_CTL_PAUSE \n");
        user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PAUSE, 0, NULL);
        succeed_or_fail = _GMA_SUCCEED;
        break;
    case _GMA_AUDIO_CTL_NEXT_FILE:
        TM_DBG("_GMA_AUDIO_CTL_NEXT_FILE \n");
        user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_NEXT, 0, NULL);
        succeed_or_fail = _GMA_SUCCEED;
        break;
    case _GMA_AUDIO_CTL_PREV_FILE:
        TM_DBG("_GMA_AUDIO_CTL_PREV_FILE \n");
        user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_PREV, 0, NULL);
        succeed_or_fail = _GMA_SUCCEED;
        break;
    case _GMA_AUDIO_CTL_STOP:
        TM_DBG("_GMA_AUDIO_CTL_STOP \n");
        user_send_cmd_prepare(USER_CTRL_AVCTP_OPID_STOP, 0, NULL);
        succeed_or_fail = _GMA_SUCCEED;
        break;
    default:
        break;
    }

    return succeed_or_fail;
}

/**************************************************************
	moudle:			HFP control
**************************************************************/
/*firmware to phone*/
static _uint8 gma_hw_get_HFP_state(struct _gma_app_HFP_ctl info)
{
    _uint8 succeed_or_fail = _GMA_FAIL;
    switch (info.type) {
    case _GMA_HFP_CONNECT_STATE:
        TM_DBG("_GMA_HFP_CONNECT_STATE \n");
        succeed_or_fail = _GMA_FAIL;
        if (get_curr_channel_state()&HFP_CH) {
            succeed_or_fail = _GMA_SUCCEED;
        }
        break;
    case _GMA_HFP_PHONE_NUMS:
        TM_DBG("_GMA_HFP_PHONE_NUMS \n");
        TM_DBG("phone numbers: \n");
        user_send_cmd_prepare(USER_CTRL_DIAL_NUMBER, info.length, (u8 *)info.phone_nums);
        succeed_or_fail = _GMA_SUCCEED;
        break;

    case _GMA_HFP_ASW_A_PHONE:
        TM_DBG("_GMA_HFP_ASW_A_PHONE \n");
        user_send_cmd_prepare(USER_CTRL_HFP_CALL_ANSWER, 0, NULL);
        succeed_or_fail = _GMA_SUCCEED;
        break;
    case _GMA_HFP_HANG_UP_A_PHONE:
        TM_DBG("_GMA_HFP_HANG_UP_A_PHONE \n");
        user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
        succeed_or_fail = _GMA_SUCCEED;
        break;
    case _GMA_HFP_RECALL_LAST_PHONE:
        TM_DBG("_GMA_HFP_RECALL_LAST_PHONE \n");
        user_send_cmd_prepare(USER_CTRL_HFP_CALL_LAST_NO, 0, NULL);
        succeed_or_fail = _GMA_SUCCEED;
        break;
    case _GMA_HFP_ASW_ANOTHER_HPONE:
        TM_DBG("_GMA_HFP_ASW_ANOTHER_HPONE \n");
        user_send_cmd_prepare(USER_CTRL_HFP_THREE_WAY_ANSWER1, 0, NULL);
        succeed_or_fail = _GMA_SUCCEED;
        break;
    case _GMA_HFP_HANG_UP_HOLDING_PHONE:
        TM_DBG("_GMA_HFP_HANG_UP_HOLDING_PHONE \n");
        user_send_cmd_prepare(USER_CTRL_HFP_THREE_WAY_REJECT, 0, NULL);
        succeed_or_fail = _GMA_SUCCEED;
        break;
    case _GMA_HFP_HOLD_CUR_ASW_ANOTHER_HPONE:
        TM_DBG("_GMA_HFP_HOLD_CUR_ASW_ANOTHER_HPONE \n");
        user_send_cmd_prepare(USER_CTRL_HFP_THREE_WAY_ANSWER2, 0, NULL);
        succeed_or_fail = _GMA_SUCCEED;
        break;
    case _GMA_HFP_ADD_HOLD2CUR_HPONE:
        TM_DBG("_GMA_HFP_ADD_HOLD2CUR_HPONE \n");
        user_send_cmd_prepare(USER_CTRL_HFP_THREE_WAY_ANSWER3, 0, NULL);
        succeed_or_fail = _GMA_SUCCEED;
        break;

    default:
        succeed_or_fail = _GMA_FAIL;
        break;
    }

    return succeed_or_fail;
}

/**************************************************************
	moudle:			device base information
**************************************************************/
/*phone t firmware*/

/*firmware to phone*/
/*range:0~100  mean  0%~100%*/
#define POWER_FLOOR_LEVEL   32
static _uint8 gma_hw_get_battery_value(void)
{
    TM_DBG("battery :%d \n", get_vbat_percent());
    return get_vbat_percent();
}

/*
_GMA_FAIL:LOW POWER
_GMA_SUCCEED:NORMAL MODE
*/
bool vbat_is_low_power(void);
static _uint8 gma_hw_low_power_state(void)
{
    return vbat_is_low_power() ? _GMA_SUCCEED : _GMA_FAIL;
}

static _uint8 gma_hw_battery_state(void)
{
    return _GMA_DEV_CHARGE_STATE_USING_BATTERY;
}

/*
fm hardware moudle
Description:range:88.0~108.0MHZ
 * */
#define FM_MINIMUM_FRE		880
#define FM_MAXIMUM_FRE		1080
/*
 *common
 * */
static int gma_hw_fm_fre_strtoint(_uint8 *fre_str)
{
#define IS_VALID_CHAR(chr) ((chr)>='0' && (chr)<='9')
#define CHARTOINT(chr) ((chr) - '0')
    int fre_int = 0;
    ///string to int
    while (*fre_str != '\0') {
        if (!IS_VALID_CHAR(*fre_str)) {
            fre_str++;
            continue;
        }
        fre_int *= 10;
        fre_int += CHARTOINT(*fre_str++);
    }

    return fre_int;
}

static int gma_hw_fm_fre_inttostr(int fre_int, _uint8 *fre_str)
{
#define INTTOCHAR(digit) ((digit) + '0')
#define CHR_SWAP(chr1,chr2) \
{   \
    char temp = chr1;\
    chr1 = chr2;\
    chr2 = temp;\
}

    int decimal_bit = 0;
    _uint8 *fre_str_store = fre_str;

    //integer to string
    while (fre_int != 0) {
        //get a decimal_bit
        decimal_bit = fre_int % 10;
        *fre_str++ = INTTOCHAR(decimal_bit);

        //delete one decimal bit
        fre_int /= 10;
    }
    fre_str = fre_str_store;
    //TM_DBG("4fre_str:%s \n",fre_str);
    //string reverse
    {
        fre_str = fre_str_store;
        int index = strlen((const char *)fre_str);
        //TM_DBG("index:%d \n",index);
        index ? index-- : index;
        char chr = 0;
        for (int i = 0; i <= index / 2; i++) {
            ///character swap
            CHR_SWAP(fre_str[index - i], fre_str[i]);
        }
    }

    //TM_DBG("2fre_str:%s \n",fre_str);
    ///add character '.'
    {
        fre_str = fre_str_store;
        int index = strlen((const char *)fre_str);
        //TM_DBG("index:%d \n",index);
        index ? index-- : index;
        fre_str[index + 1] = fre_str[index];
        fre_str[index] = '.';
    }


    return 0;
}

/*body*/
static _sint32 gma_hw_set_fm_fre(_uint8 *fre_str, _uint8 length)
{
    if (length > 5) {
        TM_DBG("illegal fm_fre !!! \n");
        return _GMA_FAIL;
    }

    ///fre_str get
    static _uint8 fm_fre_string[6];
    memset(fm_fre_string, 0x00, sizeof(fm_fre_string));
    memcpy(fm_fre_string, fre_str, length);

#if FMTX_EN
    TM_DBG("1fm_fre:%s \n", fm_fre_string);
    int fm_fre = gma_hw_fm_fre_strtoint(fm_fre_string);
    TM_DBG("3fm_fre:%d \n", fm_fre);
    if (fm_fre < FM_MINIMUM_FRE || fm_fre > FM_MAXIMUM_FRE) {
        TM_DBG("illegal fm_fre !!! \n");
        return _GMA_FAIL;
    }

    fmtx_setfre(FREQ_SEL, fm_fre);
    return _GMA_SUCCEED;
#endif

    return _GMA_FAIL;
}

static const _sint8 *gma_hw_get_fm_fre(void)
{
    static _uint8 fm_fre_string[6] = "88.0";
    ///get fm frequence
#if FMTX_EN
    int fm_fre = fmtx_get_freq();
#else
    int fm_fre = FM_MINIMUM_FRE;
#endif

    ///frequence limitation
    (fm_fre < FM_MINIMUM_FRE || fm_fre > FM_MAXIMUM_FRE) ?  fm_fre = FM_MAXIMUM_FRE : fm_fre;

    TM_DBG("--fm_fre:%d \n", fm_fre);
    ///fm frequence integer to string
    memset(fm_fre_string, 0x00, sizeof(fm_fre_string));
    gma_hw_fm_fre_inttostr(fm_fre, fm_fre_string);
    TM_DBG("fm_fre_string:%s \n", fm_fre_string);

    return (_sint8 *)fm_fre_string;
}

/*
bt name
 * */
static const _sint8 *gma_hw_get_bt_name(void)
{
    extern const char *bt_get_local_name();
    return (const _sint8 *)bt_get_local_name();
}

/*
mic state
 * */
static _uint8 gma_hw_get_mic_state(void)
{
    if (BT_STATUS_TAKEING_PHONE == get_bt_connect_status()) {
        TM_DBG("phone ing...\n");
        return _GMA_FAIL;///mic busy
    }

    int mic_coder_busy_flag(void);
    if (mic_coder_busy_flag()) {
        return _GMA_FAIL;///mic busy
    }

    return _GMA_SUCCEED;///mic available
}

/**************************************************************
	moudle:		speech
**************************************************************/
static volatile u8 speech_timeout_cnt = 0;
#define TIMEOUT_COUNTER_NUMBER		(15)
static void tm_gma_speech_timeout_det(void *priv)
{
    if (speech_timeout_cnt) {
        putchar('T');
        ///timeout
        if (speech_timeout_cnt == TIMEOUT_COUNTER_NUMBER) {
            ///speech stop
            tm_key_msg_notif(KEY_SEND_SPEECH_STOP, NULL, 0);
            ///timeout module stop
            speech_timeout_cnt = 0;
            return;
        }
        speech_timeout_cnt++;

        ///make sure speech_timeout_cnt will not overfull
        if (speech_timeout_cnt >= (TIMEOUT_COUNTER_NUMBER + 1)) {
            speech_timeout_cnt = (TIMEOUT_COUNTER_NUMBER + 1);
        }
    }
}

SYS_HI_TIMER_ADD(tm_gma_speech_timeout_det, NULL, 500);
#if 0
LOOP_DETECT_REGISTER(_tm_gma_speech_timeout_det) = {
    .time = 500,///500times -> 1000ms
    .fun  = (void *)tm_gma_speech_timeout_det,
};
#endif

extern int ai_mic_rec_start(void);
extern int ai_mic_rec_close(void);

static volatile int  tm_mic_coder_busy = 0;
volatile u8 *tm_tws_mic_pool = NULL;
#define TM_TWS_MEM_LEN 1024*2
int mic_coder_busy_flag(void)
{
    u8 tm_queque_is_busy(void);
    return tm_mic_coder_busy/* || tm_queque_is_busy()*/;
}

int gma_hw_start_speech(void)
{
#if(TCFG_USER_TWS_ENABLE)
    if (get_tws_sibling_connect_state()) { //对耳已连接，断开对耳
        if (tws_api_get_role() == TWS_ROLE_SLAVE) {///slave no mic
            return 0;
        }
    }
#endif

    ///mqc mark
    speech_timeout_cnt = 1;
    if (ai_mic_rec_start() == 0) {

        tm_mic_coder_busy = 1;


#if 0//TCFG_USER_TWS_ENABLE
        if (mic_get_data_source()) {
            //log_info("\n\n\n\ntws mic data init\n");

            if (tm_tws_mic_pool == 0) {
                tm_tws_mic_pool = malloc(TM_TWS_MEM_LEN);

                if (tm_tws_mic_pool == NULL) {
                    ASSERT(0, "dam_tws_mic pool is err\n");
                }

                tws_api_local_media_trans_start();
                tws_api_local_media_trans_set_buf(tm_tws_mic_pool, TM_TWS_MEM_LEN);
            }

        }
#endif

    }

    return 0;
}

int gma_hw_stop_speech(void)
{
    ///mqc mark
    speech_timeout_cnt = 0;
    ai_mic_rec_close();

#if 0//TCFG_USER_TWS_ENABLE
    if (tm_mic_coder_busy) {

        if (tws_api_get_tws_state() & TWS_STA_SIBLING_CONNECTED) {

            if (get_app_connect_type() == TYPE_SPP) {
                tm_mic_coder_busy = 0;
                TM_DBG("mic clear spp\n");
            } else {
                TM_DBG("\ntws connect mic busy will delay\n");
            }

        } else {
            tm_mic_coder_busy = 0;
            TM_DBG("mic c1\n");
        }

        if (mic_get_data_source()) {
            TM_DBG("\n\n\ntws mic data end\n");
            if (get_ble_connect_type() == TYPE_MASTER_BLE) {
                TM_DBG("\n\n\n slave start & master stop\n");
                app_tws_send_data(TWS_APP_DATA_SEND, TWS_AI_SPEECH_STOP, NULL, 0);
            }
            if (tm_tws_mic_pool) {
                TM_DBG("tws_api_local_media_trans_stop\n");
                tws_api_local_media_trans_stop();
                free(tm_tws_mic_pool);
                tm_tws_mic_pool = NULL;
            }
        } else {
            if (get_ble_connect_type() == TYPE_MASTER_BLE) {
                TM_DBG("\n\n\nmastar start & send stop\n");
                app_tws_send_data(TWS_APP_DATA_SEND, TWS_AI_SPEECH_STOP, NULL, 0);
            }
        }

        mic_set_data_source(SOURCE_TYPE);
    }

#else
    tm_mic_coder_busy = 0;
#endif


    return 0;
}

void phone_coming_ai_mic_close(void)
{
    TM_DBG(">>>>> gma phone coming ai mic close \n");
    if (mic_coder_busy_flag()) {
        int gma_hw_stop_speech(void);
        gma_hw_stop_speech();
    }
}


const struct __gma_hw_api gma_hw_api = {
    .ENTER_CRITICAL 		= TM_ENTER_CRITICAL,
    .EXIT_CRITICAL 			= TM_EXIT_CRITICAL,
    .set_phone_sys_info 	= gma_hw_set_phone_sys_info,
    .get_fw_abilities 		= gma_hw_get_fw_abilities,
    .get_enc_type 			= gma_hw_get_enc_type,
    .get_bt_mac_addr 		= gma_hw_get_bt_mac_addr,
    .app_notify_state 		= gma_hw_app_notify_state,
    .set_sys_vol 			= gma_hw_set_sys_vol,
    .a2dp_connected_state 	= gma_hw_a2dp_connected_state,
    .audio_state 			= gma_hw_audio_state,
    .get_HFP_state 			= gma_hw_get_HFP_state,
    .get_battery_value 		= gma_hw_get_battery_value,
    .low_power_state 		= gma_hw_low_power_state,
    .battery_state 			= gma_hw_battery_state,
    .set_fm_fre 			= gma_hw_set_fm_fre,
    .get_fm_fre 			= gma_hw_get_fm_fre,
    .get_bt_name 			= gma_hw_get_bt_name,
    .get_mic_state 			= gma_hw_get_mic_state,
    .start_speech 			= gma_hw_start_speech,
    .stop_speech 			= gma_hw_stop_speech,
};

#endif
