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
#include "asm/pwm_led.h"
#include "user_cfg.h"
#include "record/record.h"
#include "storage_dev/storage_dev.h"
#include "file_operate/file_operate.h"
#include "audio_enc.h"
#include "ui/ui_api.h"
#include "fm_emitter/fm_emitter_manage.h"
#include "audio_reverb.h"
#include "clock_cfg.h"
#include "loud_speaker.h"
#if TCFG_APP_RECORD_EN

#define LOG_TAG_CONST       APP_RECORD
#define LOG_TAG             "[APP_RECORD]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


struct record_opr {
	u8 record;
	u8 cycle_play;
};
struct record_opr record_hdl;
static struct record_opr *__this = &record_hdl;

extern u8 record_key_event_get(struct key_event *key);
extern int file_opr_dev_total(void);
extern int record_file_play_prev(void);
extern int record_file_play_next(void);

#define RECORD_MIC_CUT_HEAD_TIME			(300L)//300ms
#define RECORD_MIC_CUT_TAIL_TIME			(300L)//300ms
#define RECORD_MIC_FILE_SIZE_LIMIT			(3000L)

#if FLASH_INSIDE_REC_ENABLE
   struct storage_dev dev_sdfile = {"sdfile_rec_logo",NULL,MOUNT_SDFILE_REC_PATH,MOUNT_SDFILE_REC_PATH"/C/", "rec_sdfile", NULL, 0 };
#endif

void record_mic_start(void)
{
    int ret = record_player_encode_start(
                  NULL/*使用默认的fmt参数*/,
#if TCFG_NOR_FS_ENABLE
                  storage_dev_check("nor_fs"),  //使用这个需要把上面的dev_last注释
#else
#if FLASH_INSIDE_REC_ENABLE
                  &dev_sdfile,
#else
                  storage_dev_last(),
#endif
#endif
                  ENCODE_SOURCE_MIC,
                  RECORD_MIC_CUT_HEAD_TIME,
                  RECORD_MIC_CUT_TAIL_TIME,
                  RECORD_MIC_FILE_SIZE_LIMIT
              );
}

static void record_mic_stop(void)
{
    record_player_encode_stop();
}

u8 get_record_replay_mode(void)
{
	return __this->cycle_play;	
}
void set_record_replay_mode(u8 mark)
{
	__this->cycle_play = mark?1:0;	
}

static int _key_event_opr(struct sys_event *event)
{
    int ret = false;
    int err = 0;
    u8 vol;
    struct key_event *key = &event->u.key;

#if TCFG_APP_FM_EMITTER_EN
#if TCFG_UI_ENABLE
    if (ui_get_app_menu(GRT_CUR_MENU) == MENU_FM_SET_FRE) {
        return ret;
    }

#endif
#endif

	u8 key_event = record_key_event_get(key);
	log_info("key_event:%d \n", key_event);
	switch (key_event) {
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))
		case KEY_MUSIC_PP:
			if (!record_player_is_encoding()) {
				if(__this->cycle_play == 0){
					tone_play_stop();
					record_file_play();
					__this->cycle_play =1;
				}else{
					__this->cycle_play =0;
					tone_play_stop();
				}
			}
			return true;
		case KEY_VOL_UP:
			switch_holwing_en();
			/* set_speaker_gain_up(1); */
			/* __this->cycle_play =0; */
			/* record_file_play_next(); */
			return true;
		case KEY_VOL_DOWN:
			switch_holwing_en();
			/* __this->cycle_play =0; */
			/* record_file_play_prev(); */
			/* set_speaker_gain_down(1); */
			return true;
		case KEY_SPEAKER_OPEN:
			if (!record_player_is_encoding()) {
				if(speaker_if_working()){
					stop_loud_speaker();	
				}else{
					start_loud_speaker(NULL);	
				}
			}
			return true;
#endif	
		case KEY_ENC_START:
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))
			if(speaker_if_working()){
				stop_loud_speaker();	
			}
#endif
			if (record_player_is_encoding()) {
				log_info("mic record stop && replay\n");
				record_mic_stop();
				record_file_play();
			} else {
				tone_play_stop();
				record_mic_start();
				log_info("mic record start\n");
			}

			return true;

#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
		case KEY_REVERB_OPEN:
		case KEY_REVERB_DEEPVAL_UP:
		case KEY_REVERB_DEEPVAL_DOWN:
			return true;
			break;
#endif
		default:
			break;
	}

	return ret;
}

static void record_app_init(void)
{
#if TCFG_UI_ENABLE
    ui_set_main_menu(UI_RECORD_MENU_MAIN);
    /* ui_set_tmp_menu(MENU_WAIT,100,0,NULL); */
#endif
	memset(__this,0,sizeof(struct record_opr));
    sys_key_event_enable();
    ui_update_status(STATUS_RECORD_MODE);

    clock_idle(REC_IDLE_CLOCK);
}

static void record_app_uninit(void)
{
    record_mic_stop();
}


static int record_state_machine(struct application *app, enum app_state state,
		struct intent *it)
{
    int ret;
    switch (state) {
    case APP_STA_CREATE:
        /* server_load(audio_rec); */
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
        reverb_pause();
#endif
        break;
    case APP_STA_START:
        if (!it) {
            break;
        }
        switch (it->action) {
        case ACTION_APP_MAIN:
            log_info("----   ACTION_APP_MAIN\n");
            record_app_init();
            break;
        }
        break;
    case APP_STA_PAUSE:
        log_info("APP_STA_PAUSE\n");
        break;
    case APP_STA_RESUME:
        log_info("APP_STA_RESUME\n");
        break;
    case APP_STA_STOP:
        log_info("APP_STA_STOP\n");
        break;
    case APP_STA_DESTROY:
		log_info("APP_STA_DESTROY\n");
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))
		if(speaker_if_working()){
			stop_loud_speaker();	
		}
#endif
		record_app_uninit();
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
		reverb_resume();
#endif

		break;
	}

    return 0;
}

static int record_event_handler(struct application *app, struct sys_event *event)
{
    const char *logo = NULL;
    int err = 0;
    switch (event->type) {
    case SYS_KEY_EVENT:
        return _key_event_opr(event);
    case SYS_DEVICE_EVENT:
        ///所有设备相关的事件不能返回true， 必须给留给公共处理的地方响应设备上下线
        switch ((u32)event->arg) {
        case DRIVER_EVENT_FROM_SD0:
        case DRIVER_EVENT_FROM_SD1:
        case DRIVER_EVENT_FROM_SD2:
        case DEVICE_EVENT_FROM_USB_HOST:
            logo = evt2dev_map_logo((u32)event->arg);
            if (logo == NULL) {
                break;
            }
            if (event->u.dev.event == DEVICE_EVENT_IN) {

            } else if (event->u.dev.event == DEVICE_EVENT_OUT) {
                record_player_device_offline_check(logo);
                if (0 == file_opr_dev_total()) {
                    //没有设别才切换
                    app_task_next();
                }
            }
            break;//DEVICE_EVENT_FROM_USB_HOST
        }//switch((u32)event->arg)
        break;//SYS_DEVICE_EVENT
    default:
        break;;
    }//switch (event->type)

    return false;
}

static int record_app_check(void)
{
#if TCFG_APP_RECORD_EN
#if FLASH_INSIDE_REC_ENABLE
    return true;
#endif

    if (file_opr_dev_total()) {
        return true;
    }
    return false;
#else
    return false;
#endif
}

static int record_user_msg_deal(int msg, int argc, int *argv)
{
    switch (msg) {
    case USER_MSG_SYS_MIXER_RECORD_SWITCH:
        ///录音模式不响应混合录音,只响应录mic
        return 1;
    default:
        break;
    }
    return 0;
}

static const struct application_reg app_record_reg = {
    .tone_name = TONE_RECORD,
    .tone_play_check = NULL,
    .tone_prepare = NULL,
    .enter_check = record_app_check,
    .exit_check = NULL,
    .user_msg = record_user_msg_deal,
#if (defined(TCFG_TONE2TWS_ENABLE) && (TCFG_TONE2TWS_ENABLE))
    .tone_tws_cmd = SYNC_CMD_MODE_ENC,
#endif
};

static const struct application_operation app_record_ops = {
    .state_machine  = record_state_machine,
    .event_handler 	= record_event_handler,
};

REGISTER_APPLICATION(app_app_record) = {
    .name 	= APP_NAME_RECORD,
    .action	= ACTION_APP_MAIN,
    .ops 	= &app_record_ops,
    .state  = APP_STA_DESTROY,
    .private_data = (void *) &app_record_reg,
};
#if 0
int record_file_play(void);
int enc_test_tt(void *p)
{
    /* return 0; */
    static u32 cnt = 0;
    if (!cnt) {
        tone_play_stop();
        record_mic_start();
        log_info("mic record start\n");
    }
    if (cnt == 20) {

        log_info("mic record stop && replay\n");
        record_mic_stop();
        record_file_play();
    }
    if (cnt == 25) {
        log_info("play last file \n");
        record_file_play();
    }
    cnt++;
    return 0;
}
#endif

#endif
