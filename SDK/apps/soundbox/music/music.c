#include <stdlib.h>
#include "system/app_core.h"
#include "system/includes.h"
#include "server/server_core.h"
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
#include "music/music.h"
#include "asm/pwm_led.h"
#include "user_cfg.h"
#include "usb/otg.h"
#include "usb/host/usb_host.h"
#include "device/sdmmc.h"
#include "music/music_player_api.h"
#include "common/user_msg.h"
#include "pitchshifter/pitchshifter_api.h"
#include "ui/ui_api.h"
#include "fm_emitter/fm_emitter_manage.h"
#include "audio_reverb.h"
#include "clock_cfg.h"
#include "loud_speaker.h"
#if TCFG_APP_MUSIC_EN

#define LOG_TAG_CONST       APP_MUSIC
#define LOG_TAG             "[APP_MUSIC]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


#if TCFG_SPEED_PITCH_ENABLE
static PITCH_SHIFT_PARM init_parm;

extern void update_audio_effect_parm(PITCH_SHIFT_PARM *parm);
// old param
#define PS_PITCHT_DEFAULT_VAL (32768L) ///变调参数说明：>32768音调高，<32768音调低，变调比例pitchV/32768
#define PS_SPEED_DEFAULT_VAL (80L) ///变数参数说明，>80变快，<80变慢,建议范围：40_160, 但是20-200也有效

const u16 speed_tab[] = {
    PS_SPEED_DEFAULT_VAL,
    PS_SPEED_DEFAULT_VAL + 10,
    PS_SPEED_DEFAULT_VAL + 20,
    PS_SPEED_DEFAULT_VAL - 10,
    PS_SPEED_DEFAULT_VAL - 20,
};
const u16 pitch_tab[] = {
    PS_PITCHT_DEFAULT_VAL,
    PS_PITCHT_DEFAULT_VAL + 4000,
    PS_PITCHT_DEFAULT_VAL + 6000,
    PS_PITCHT_DEFAULT_VAL - 3000,
    PS_PITCHT_DEFAULT_VAL - 6000,
};

void init_pitch_shift_parm()
{
    init_parm.shiftv = 5600 ;             //pitch rate:  <8192(pitch up), >8192(pitch down)   ：调节范围4000到16000
    init_parm.sr = 16000;                             //配置输入audio的采样率
    init_parm.effect_v = EFFECT_PITCH_SHIFT;          //选移频效果
    init_parm.formant_shift = 8192;      //3000到16000，或者0【省buf，效果只选EFFECT_PITCH_SHIFT】
}
PITCH_SHIFT_PARM *get_effect_parm(void)
{
    return &init_parm;
}
#endif


#if (TCFG_SPI_LCD_ENABLE)
#include "ui/ui_style.h"
extern int ui_hide_main(int id);
extern int ui_show_main(int id);
extern int ui_server_msg_post(int argc, ...);
#endif

// key
extern u8 music_key_event_get(struct key_event *key);
static int _key_event_opr(struct sys_event *event)
{
    int ret = true;
    int err = 0;
    u8 vol;
    struct key_event *key = &event->u.key;

#if TCFG_APP_FM_EMITTER_EN
#if TCFG_UI_ENABLE
    if (ui_get_app_menu(GRT_CUR_MENU) == MENU_FM_SET_FRE) {
        return false;
    }
#endif
#endif

    u8 key_event = music_key_event_get(key);

    switch (key_event) {

#if (TCFG_SPI_LCD_ENABLE)//lcd屏使用

    case  KEY_VOL_UP:
        app_audio_volume_up(1);
        log_info("music vol+: %d", app_audio_get_volume(APP_AUDIO_CURRENT_STATE));

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        bt_tws_sync_volume();
#endif

        ui_server_msg_post(3, ID_WINDOW_MUSIC, "vol", NULL);
        /* vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE); */
        break;
    case  KEY_VOL_DOWN:
        app_audio_volume_down(1);
        log_info("music vol-: %d", app_audio_get_volume(APP_AUDIO_CURRENT_STATE));

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
        bt_tws_sync_volume();
#endif

        ui_server_msg_post(3, ID_WINDOW_MUSIC, "vol", NULL);
        /* vol = app_audio_get_volume(APP_AUDIO_CURRENT_STATE); */
        break;

#endif

    case  KEY_MUSIC_PP:
        /* app_task_msg_post(USER_MSG_TEST, 5, 1,2,3,4,5); */
        err = music_play_file_pp();
        break;
    case  KEY_MUSIC_PREV:
        err = music_play_file_prev();
        y_printf(">>>[test]:KEY_MUSIC_PREV\n");
        break;
    case  KEY_MUSIC_NEXT:
        err = music_play_file_next();
        y_printf(">>>[test]:KEY_MUSIC_NEXT\n");
        break;
    case  KEY_MUSIC_FF:
        err = music_play_ff(3);
        break;
    case  KEY_MUSIC_FR:
        err = music_play_fr(3);
        break;
    case  KEY_MUSIC_CHANGE_REPEAT:
        err = music_play_set_repeat_mode_auto();
        break;
    case  KEY_MUSIC_CHANGE_DEV:
        err = music_play_change_dev_next();
        break;
    case  KEY_MUSIC_CHANGE_DEV_REPEAT:
        err = music_play_change_dev_repeat_mode_auto();
        break;
#if TCFG_SPEED_PITCH_ENABLE
    case KEY_MUSIC_SET_PITCH:
        update_audio_effect_parm(&init_parm);
        break;
    case KEY_MUSIC_SET_SPEED:
        /* update_audio_effect_parm(&init_parm); */
        break;
#endif

    case KEY_MUSIC_PLAYE_BY_DEV_FILENUM:
        /* err = music_play_file_by_dev_filenum((char *)"udisk", 2);///this is a demo */
        //err = music_play_file_by_dev_filenum((char *)"norflash", 1);///this is a demo
#if TCFG_NOR_FS_ENABLE
        err = music_flash_play_file_by_dev_filenum(1);
#endif
        break;

    case KEY_MUSIC_PLAYE_BY_DEV_SCLUST:
        err = music_play_file_by_dev_sclust((char *)"udisk", 2);///this is a demo
        break;

    case KEY_MUSIC_PLAYE_BY_DEV_PATH:
        err = music_play_file_by_dev_path((char *)"udisk", "/sin.wav");///this is a demo
        break;

    case KEY_MUSIC_PLAYE_PREV_FOLDER:
        err = music_play_file_by_change_folder(0);
        break;

    case KEY_MUSIC_PLAYE_NEXT_FOLDER:
        err = music_play_file_by_change_folder(1);
        break;

    case KEY_MUSIC_PLAYE_REC_FOLDER_SWITCH:
        err = music_play_record_folder_file_switch();
        break;
#if 0
    case KEY_REVERB_OPEN:
        printf("\n KEY_REVERB_OPEN \n");
        if (reverb_if_working()) {
            stop_reverb_mic2dac();
        } else {
            start_reverb_mic2dac(NULL);
        }
        break;
#endif
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))		
	case KEY_SPEAKER_OPEN:
		if(speaker_if_working()){
			stop_loud_speaker();	
		}else{
			start_loud_speaker(NULL);	
		}
		return true;
#endif
	default:
        ret = false;
        break;
    }
    if (err) {
        printf("music _key_event_opr err = %x\n", err);
        /* app_task_next(); */
    }
    return ret;
}


#if (TCFG_SPI_LCD_ENABLE)
extern int key_is_ui_takeover();
#endif

static int music_event_handler(struct application *app, struct sys_event *event)
{
    const char *logo = NULL;
    int err = 0;
    switch (event->type) {
    case SYS_KEY_EVENT:

#if (TCFG_SPI_LCD_ENABLE)
        if (key_is_ui_takeover()) {
            return false;
        }
#endif

        return _key_event_opr(event);
    case SYS_DEVICE_EVENT:
        switch ((u32)event->arg) {
        case DRIVER_EVENT_FROM_SD0:
        case DRIVER_EVENT_FROM_SD1:
        case DRIVER_EVENT_FROM_SD2:
        case DEVICE_EVENT_FROM_USB_HOST:
            music_play_device_event_deal((u32)event->arg, event->u.dev.event);
            return true;
        default://switch((u32)event->arg)
            break;
        }
        break;//SYS_DEVICE_EVENT
    default://switch (event->type)
        break;
    }

    return false;
}

static void music_app_init(void *logo)
{
    int err = 0;
#if TCFG_UI_ENABLE
    ui_set_main_menu(UI_MUSIC_MENU_MAIN);
    ui_set_tmp_menu(MENU_WAIT, 1000, 0, NULL);
#endif

#if (TCFG_SPI_LCD_ENABLE)
    ui_show_main(ID_WINDOW_MUSIC);
#endif


    clock_idle(MUSIC_IDLE_CLOCK);

    ui_update_status(STATUS_MUSIC_MODE);
    sys_key_event_enable();
    err = music_play_first_start(logo);
    if (err) {
        printf("music_play_first_start %x\n ", err);
        app_task_next();
    }
}

static void music_app_uninit(void)
{

#if (TCFG_SPI_LCD_ENABLE)
    ui_hide_main(ID_WINDOW_MUSIC);
#endif

    /* clock_idle(MUSIC_IDLE_CLOCK); */
}

static int music_state_machine(struct application *app, enum app_state state,
                               struct intent *it)
{
    int ret;
    switch (state) {
    case APP_STA_CREATE:
        break;
    case APP_STA_START:
        music_play_init();
        if (!it) {
            break;
        }
        switch (it->action) {
        case ACTION_APP_MAIN:
            log_info("ACTION_APP_MAIN\n");
            music_app_init((void *)it->exdata);
            break;
        }
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
        music_play_uninit();
        music_app_uninit();
        break;
    }

    return 0;
}


static int music_app_check(void)
{
#if TCFG_APP_MUSIC_EN
    if (file_opr_dev_total()) {
        return true;
    }
    return false;
#else

    return false;
#endif
}

//test
static int music_user_msg_deal(int msg, int argc, int *argv)
{
    switch (msg) {
    case USER_MSG_TEST:
        printf("get user msg %d, msg_val_count = %d, msg_val:\n", msg, argc);
        for (int i = 0; i < argc; i++) {
            printf("%d ", argv[i]);
        }
        break;
    case USER_MSG_SYS_MIXER_RECORD_SWITCH:
        ///解码模式不响应混合录音
        return 1;
    default:
        break;
    }

    return 0;
}

static const struct application_reg app_music_reg = {
    .tone_name = TONE_MUSIC,
    .tone_play_check = NULL,
    .tone_prepare = NULL,
    .enter_check = music_app_check,
    .exit_check = NULL,
    .user_msg = music_user_msg_deal,
#if (defined(TCFG_TONE2TWS_ENABLE) && (TCFG_TONE2TWS_ENABLE))
    .tone_tws_cmd = SYNC_CMD_MODE_MUSIC,
#endif
};

static const struct application_operation app_music_ops = {
    .state_machine  = music_state_machine,
    .event_handler 	= music_event_handler,
};

REGISTER_APPLICATION(app_app_music) = {
    .name 	= APP_NAME_MUSIC,
    .action	= ACTION_APP_MAIN,
    .ops 	= &app_music_ops,
    .state  = APP_STA_DESTROY,
    .private_data = (void *) &app_music_reg,
};

#endif

