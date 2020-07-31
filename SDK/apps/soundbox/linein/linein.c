
#include "system/app_core.h"
#include "system/includes.h"
#include "server/server_core.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_action.h"
#include "tone_player.h"
#include "asm/charge.h"
#include "app_charge.h"
#include "app_main.h"
#include "app_online_cfg.h"
#include "app_power_manage.h"
#include "gSensor/gSensor_manage.h"
#include "ui_manage.h"
#include "vm.h"
#include "app_chargestore.h"
#include "key_event_deal.h"
#include "linein/linein_dev.h"
#include "linein/linein.h"
#include "asm/pwm_led.h"
#include "user_cfg.h"
#include "asm/audio_linein.h"
#include "audio_reverb.h"
#include "ui/ui_api.h"
#include "fm_emitter/fm_emitter_manage.h"
#include "clock_cfg.h"
#include "loud_speaker.h"

#if TCFG_APP_LINEIN_EN

#define LOG_TAG_CONST       APP_LINEIN
#define LOG_TAG             "[APP_LINEIN]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


extern struct dac_platform_data dac_data;

struct linein_opr {
    void *rec_dev;
    u8 volume;
    u8 onoff : 1;
};
static struct linein_opr linein_hdl = {0};
#define __this 	(&linein_hdl)

extern void tone_event_to_user(u8 event);

extern void linein_pcm_enc_stop(void);
extern int linein_pcm_enc_start(void);
extern bool linein_pcm_enc_check();
extern void bt_tws_sync_volume();


static int linein_volume_pp(void);

static u8 linein_last_onoff = (u8) - 1;
static u8 linein_bt_back_flag = 0;
extern u8 bt_back_flag;

// other task call it
int linein_sys_event_handler(struct sys_event *event)
{
    if ((u32)event->arg == DEVICE_EVENT_FROM_LINEIN) {
        if (event->u.dev.event == DEVICE_EVENT_IN) {
            log_info("linein online \n");
            return true;
        } else if (event->u.dev.event == DEVICE_EVENT_OUT) {
            log_info("linein offline \n");
        }
    }
    return false;
}


u8 linein_get_status(void)
{
    return __this->onoff;
}


// linein volume
static int linein_volume_set(u8 vol)
{
    if (TCFG_LINEIN_LR_CH == AUDIO_LIN_DACL_CH) {
        app_audio_output_ch_analog_gain_set(BIT(0), 0);
        app_audio_output_ch_analog_gain_set(BIT(1), vol);
    } else if (TCFG_LINEIN_LR_CH == AUDIO_LIN_DACR_CH) {
        app_audio_output_ch_analog_gain_set(BIT(0), vol);
        app_audio_output_ch_analog_gain_set(BIT(1), 0);
    } else {
        app_audio_set_volume(APP_AUDIO_STATE_MUSIC, vol, 1);
    }
    log_info("linein vol: %d", __this->volume);

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
    bt_tws_sync_volume();
#endif

    return true;
}


int linein_dec_open(u8 source, u32 sample_rate);
void linein_dec_close(void);
// linein stop
void linein_stop(void)
{

    if (__this->onoff == 0) {
        log_info("linein is aleady stop\n");
        return;
    }
#if (TCFG_LINEIN_INPUT_WAY == LINEIN_INPUT_WAY_ADC)
    linein_dec_close();
#elif (TCFG_LINEIN_INPUT_WAY == LINEIN_INPUT_WAY_ANALOG)

    if (TCFG_LINEIN_LR_CH & (BIT(0) | BIT(1))) {
        audio_linein0_close(TCFG_LINEIN_LR_CH, 0);
    } else if (TCFG_LINEIN_LR_CH & (BIT(2) | BIT(3))) {
        audio_linein1_close(TCFG_LINEIN_LR_CH, 0);
    } else if (TCFG_LINEIN_LR_CH & (BIT(4) | BIT(5))) {
        audio_linein2_close(TCFG_LINEIN_LR_CH, 0);
    }
#elif (TCFG_LINEIN_INPUT_WAY == LINEIN_INPUT_WAY_DAC)
    audio_linein_via_dac_close(TCFG_LINEIN_LR_CH, 0);
#endif
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, __this->volume, 1);
    __this->onoff = 0;
}

// linein start
int linein_start(void)
{
    if (__this->onoff == 1) {
        log_info("linein is aleady start\n");
        return true;
    }

#if (TCFG_LINEIN_INPUT_WAY != LINEIN_INPUT_WAY_ADC)
    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());
#endif

#if (TCFG_LINEIN_INPUT_WAY != LINEIN_INPUT_WAY_ADC)
    if (!app_audio_get_volume(APP_AUDIO_STATE_MUSIC)) {
        audio_linein_mute(1);    //模拟输出时候，dac为0也有数据
    }
#endif

#if (TCFG_LINEIN_INPUT_WAY == LINEIN_INPUT_WAY_ADC)
#if (TCFG_LINEIN_MULTIPLEX_WITH_FM && (defined(CONFIG_CPU_BR25)))
    linein_dec_open(AUDIO_LIN1R_CH, 44100);		//696X 系列FM 与 LINEIN复用脚，绑定选择AUDIO_LIN1R_CH
#elif ((TCFG_LINEIN_LR_CH & AUDIO_LIN1R_CH ) && (defined(CONFIG_CPU_BR25)))		//FM 与 LINEIN 复用未使能，不可选择AUDIO_LIN1R_CH
    log_e("FM is not multiplexed with linein. channel selection err\n");
    ASSERT(0, "err\n");
#else
    linein_dec_open(TCFG_LINEIN_LR_CH, 44100);
#endif
#elif (TCFG_LINEIN_INPUT_WAY == LINEIN_INPUT_WAY_ANALOG)

    if (TCFG_LINEIN_LR_CH & (BIT(0) | BIT(1))) {
        audio_linein0_open(TCFG_LINEIN_LR_CH, 1);
    } else if (TCFG_LINEIN_LR_CH & (BIT(2) | BIT(3))) {
        audio_linein1_open(TCFG_LINEIN_LR_CH, 1);
    } else if (TCFG_LINEIN_LR_CH & (BIT(4) | BIT(5))) {
        audio_linein2_open(TCFG_LINEIN_LR_CH, 1);
    }
    audio_linein_gain(1);   // high gain
#elif (TCFG_LINEIN_INPUT_WAY == LINEIN_INPUT_WAY_DAC)
    if ((TCFG_LINEIN_LR_CH == AUDIO_LIN_DACL_CH) \
        || (TCFG_LINEIN_LR_CH == AUDIO_LIN_DACR_CH)) {
        audio_linein_via_dac_open(TCFG_LINEIN_LR_CH, 1);
    } else {
        ASSERT(0, "linein ch err\n");
    }
    linein_volume_set(app_audio_get_volume(APP_AUDIO_STATE_MUSIC));
#endif

#if TCFG_UI_ENABLE
    ui_menu_reflash(false);//刷新主页并且支持打断显示
#endif

    __this->volume = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);


#if (TCFG_LINEIN_INPUT_WAY != LINEIN_INPUT_WAY_ADC)
    if (app_audio_get_volume(APP_AUDIO_STATE_MUSIC)) {
        audio_linein_mute(0);
        app_audio_set_volume(APP_AUDIO_STATE_MUSIC, app_audio_get_volume(APP_AUDIO_STATE_MUSIC), 1);//防止无法调整
    }
    //模拟输出时候，dac为0也有数据
#endif
    __this->onoff = 1;
    return true;
}

// linein volume
static int linein_volume_pp(void)
{
    if (__this->onoff) {
        linein_stop();
    } else {
        linein_start();
    }
    log_info("pp:%d \n", __this->onoff);
    return true;
}

// key
extern u8 app_get_audio_state(void);
extern u8 linein_key_event_get(struct key_event *key);
static int _key_event_opr(struct sys_event *event)
{
    int ret = true;
    int err = 0;
    u8 vol;
    struct key_event *key = &event->u.key;

#if TCFG_UI_ENABLE
#if (TCFG_APP_FM_EMITTER_EN == ENABLE_THIS_MOUDLE)
    if (ui_get_app_menu(GRT_CUR_MENU) == MENU_FM_SET_FRE) {
        return false;
    }
#endif
#endif

    u8 key_event = linein_key_event_get(key);
    log_info("key_event:%d \n", key_event);


    switch (key_event) {
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))		
	case KEY_SPEAKER_OPEN:
		if(speaker_if_working()){
			stop_loud_speaker();	
		}else{
			start_loud_speaker(NULL);	
		}
		return true;
#endif
    case  KEY_MUSIC_PP:
        log_info("KEY_MUSIC_PP\n");
        linein_volume_pp();
        linein_last_onoff = __this->onoff;

#if TCFG_UI_ENABLE
        ui_menu_reflash(true);
#endif
        break;
    case  KEY_VOL_UP:
#if (TCFG_LINEIN_INPUT_WAY != LINEIN_INPUT_WAY_ADC)
        if (!__this->volume) {
            audio_linein_mute(0);
        }
#endif
        if (__this->volume < get_max_sys_vol()) {
            __this->volume ++;
            linein_volume_set(__this->volume);
        } else {
            linein_volume_set(__this->volume);
            if (tone_get_status() == 0) {
                /* tone_play(TONE_MAX_VOL); */
#if TCFG_MAX_VOL_PROMPT
                STATUS *p_tone = get_tone_config();
                tone_play_index(p_tone->max_vol, 1);
#endif
            }
        }
#if TCFG_UI_ENABLE
        vol = __this->volume;
        ui_set_tmp_menu(MENU_MAIN_VOL, 1000, vol, NULL);
#endif //TCFG_UI_ENABLE

        log_info("vol+:%d\n", __this->volume);
        break;

    case  KEY_VOL_DOWN:
        if (__this->volume) {
            __this->volume --;
            linein_volume_set(__this->volume);
        }

#if (TCFG_LINEIN_INPUT_WAY != LINEIN_INPUT_WAY_ADC)
        if (!__this->volume) {
            audio_linein_mute(1);
        }
#endif

#if TCFG_UI_ENABLE
        vol = __this->volume;
        ui_set_tmp_menu(MENU_MAIN_VOL, 1000, vol, NULL);
#endif //TCFG_UI_ENABLE

        log_info("vol-:%d\n", __this->volume);
        break;

    default:
        ret = false;
        break;
    }

    return ret;
}

// linein play event
static int _linein_event_opr(struct device_event *dev)
{
    int ret = false;
    int err = 0;

    switch (dev->event) {
    case AUDIO_PLAY_EVENT_END:
        log_info("AUDIO_PLAY_EVENT_END\n");
        break;
    case AUDIO_PLAY_EVENT_ERR:
        break;
    }

    return ret;
}

// tone play event
static int _tone_event_opr(struct device_event *dev)
{
    int ret = false;

    switch (dev->event) {
    case AUDIO_PLAY_EVENT_END:
#if (TCFG_LINEIN_INPUT_WAY == LINEIN_INPUT_WAY_DAC)
        app_audio_set_volume(APP_AUDIO_STATE_MUSIC, __this->volume, 1);
        // dac 做linein 会和原来音量系统不同步 -HB
#endif
        break;
    }

    return ret;
}

static int linein_event_handler(struct application *app, struct sys_event *event)
{
    int err = 0;
    switch (event->type) {
    case SYS_KEY_EVENT:
        return _key_event_opr(event);
        break;

    case SYS_DEVICE_EVENT:
        if ((u32)event->arg == DEVICE_EVENT_FROM_TONE) {
            _tone_event_opr(&event->u.dev);
        } else if ((u32)event->arg == DEVICE_EVENT_FROM_LINEIN) {
            if (event->u.dev.event == DEVICE_EVENT_IN) {
                log_info("linein online \n");
            } else if (event->u.dev.event == DEVICE_EVENT_OUT) {
                log_info("linein offline \n");
                app_task_next();
            }
            return true;
        }
        return false;
    default:
        return false;
    }
}

static void linein_app_init(void)
{
#if (defined(TCFG_APP_BT_EN) && TCFG_APP_BT_EN)
    linein_bt_back_flag = bt_back_flag;
    bt_back_flag = 0;
#else
    linein_bt_back_flag = 0;
#endif

#if TCFG_UI_ENABLE
    ui_set_main_menu(UI_AUX_MENU_MAIN);
    ui_set_tmp_menu(MENU_AUX, 1000, 0, NULL);
#endif
    sys_key_event_enable();
    ui_update_status(STATUS_LINEIN_MODE);

    clock_idle(LINEIN_IDLE_CLOCK);

    log_info("linein_bt_back_flag == %d linein_last_onoff = %d\n", \
             linein_bt_back_flag, linein_last_onoff);

    if ((linein_bt_back_flag == 2) && (linein_last_onoff == 0)) {
        /* linein_volume_pp(); */
    } else {
        linein_start();
    }
    linein_last_onoff = __this->onoff;

    /* #if TCFG_UI_ENABLE */
    /* ui_menu_reflash(true); */
    /* #endif */
}

static void linein_app_uninit(void)
{
    linein_stop();
    tone_play_stop();
}

static int linein_state_machine(struct application *app, enum app_state state,
                                struct intent *it)
{
    int ret;
    switch (state) {
    case APP_STA_CREATE:
        break;
    case APP_STA_START:
        if (!it) {
            break;
        }
        switch (it->action) {
        case ACTION_APP_MAIN:
            log_info("ACTION_APP_MAIN\n");
            linein_app_init();
            break;
        }
        break;
    case APP_STA_PAUSE:
        log_info("APP_STA_PAUSE\n");
        linein_stop();
        break;
    case APP_STA_RESUME:
        log_info("APP_STA_RESUME\n");
        break;
    case APP_STA_STOP:
        log_info("APP_STA_STOP\n");
        break;
    case APP_STA_DESTROY:
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))		
		if(speaker_if_working()){
			stop_loud_speaker();	
		}
#endif
        log_info("APP_STA_DESTROY\n");
        linein_app_uninit();
        break;
    }

    return 0;
}

static int linein_app_check(void)
{
#if TCFG_LINEIN_ENABLE
    if (linein_is_online()) {
        return true;
    }
    return false;
#else
    return false;
#endif
}

static int linein_user_msg_deal(int msg, int argc, int *argv)
{
    switch (msg) {
    case USER_MSG_SYS_MIXER_RECORD_SWITCH:
#if (TCFG_LINEIN_INPUT_WAY != LINEIN_INPUT_WAY_ADC)
        return 1;
#endif
        break;
    }
    return 0;
}

static const struct application_reg app_linein_reg = {
    .tone_name = TONE_LINEIN,
    .tone_play_check = NULL,
    .tone_prepare = NULL,
    .enter_check = linein_app_check,
    .exit_check = NULL,
    .user_msg = linein_user_msg_deal,
#if (defined(TCFG_TONE2TWS_ENABLE) && (TCFG_TONE2TWS_ENABLE))
    .tone_tws_cmd = SYNC_CMD_MODE_LINEIN,
#endif
};

static const struct application_operation app_linein_ops = {
    .state_machine  = linein_state_machine,
    .event_handler 	= linein_event_handler,
};

REGISTER_APPLICATION(app_app_linein) = {
    .name 	= APP_NAME_LINEIN,
    .action	= ACTION_APP_MAIN,
    .ops 	= &app_linein_ops,
    .state  = APP_STA_DESTROY,
    .private_data = (void *) &app_linein_reg,
};


#endif
