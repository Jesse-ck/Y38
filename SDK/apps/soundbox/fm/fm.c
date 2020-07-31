#include "fm_server.h"
#include "loud_speaker.h"
#if TCFG_APP_FM_EN

// key
extern u8 fm_key_event_get(struct key_event *key);

extern u8 fm_get_scan_flag(void);
static int _key_event_opr(struct sys_event *event)
{
    int ret = true;
    int err = 0;
    int i, j;
    u8 temp = 0;
    struct key_event *key = &event->u.key;
    u8 key_event = fm_key_event_get(key);
    r_printf("key value:%d, event:%d \n", key->value, key->event);

    switch (key_event) {
#if (defined(TCFG_LOUDSPEAKER_ENABLE) && (TCFG_LOUDSPEAKER_ENABLE))		
	case KEY_SPEAKER_OPEN:
		if(fm_get_scan_flag())
			break;

		if(speaker_if_working()){
			stop_loud_speaker();	
		}else{
			start_loud_speaker(NULL);	
		}
		return true;
#endif
    case  KEY_MUSIC_PP:
        fm_server_msg_post(FM_MUSIC_PP);
        break;
    case  KEY_FM_SCAN_ALL_DOWN:
        fm_server_msg_post(FM_SCAN_ALL_DOWN);
        break;
    case  KEY_FM_SCAN_ALL_UP:
        fm_server_msg_post(FM_SCAN_ALL_UP);
        break;
    case  KEY_FM_SCAN_DOWN:
        fm_server_msg_post(FM_SCAN_DOWN);//半自动搜台
        break;
    case  KEY_FM_SCAN_UP:
        fm_server_msg_post(FM_SCAN_UP);//半自动搜台
        break;
    case  KEY_FM_PREV_STATION:
        fm_server_msg_post(FM_PREV_STATION);
        break;
    case  KEY_FM_NEXT_STATION:
        fm_server_msg_post(FM_NEXT_STATION);
        break;
    case  KEY_FM_PREV_FREQ:
        fm_server_msg_post(FM_PREV_FREQ);
        break;
    case  KEY_FM_NEXT_FREQ:
        fm_server_msg_post(FM_NEXT_FREQ);
        break;
    case KEY_VOL_UP:
        fm_server_msg_post(FM_VOLUME_UP);
        break;
    case KEY_VOL_DOWN:
        fm_server_msg_post(FM_VOLUME_DOWN);
        break;

#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
#if TCFG_FM_INSIDE_ENABLE
// 对箱+内部fm，不开混响
    case KEY_REVERB_OPEN:
    case KEY_REVERB_DEEPVAL_UP:
    case KEY_REVERB_DEEPVAL_DOWN:
        ret = true;
        break;
#endif
#endif
#endif

    default:
        ret = false;
        break;
    }
    /* printf("fm_freq_cur:%x\n", __this->fm_freq_cur); */
    /* printf("fm_freq_channel_cur:%x\n", __this->fm_freq_channel_cur); */
    /* printf("fm_total_channel:%x\n", __this->fm_total_channel); */
    return ret;
}

// fm play event
static int _fm_event_opr(struct device_event *dev)
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
        break;
    }

    return ret;
}

static int fm_event_handler(struct application *app, struct sys_event *event)
{
    int err = 0;
    switch (event->type) {
    case SYS_KEY_EVENT:
        return _key_event_opr(event);
        break;

    case SYS_DEVICE_EVENT:
        if ((u32)event->arg == DEVICE_EVENT_FROM_TONE) {
            _tone_event_opr(&event->u.dev);
        } else if ((u32)event->arg == DEVICE_EVENT_FROM_FM) {
            if (event->u.dev.event == DEVICE_EVENT_IN) {
                log_info("fm online \n");
            } else if (event->u.dev.event == DEVICE_EVENT_OUT) {
                log_info("fm offline \n");
                app_task_next();
            }
            return true;
        }
        return false;

    default:
        return false;
    }
}

static void fm_app_init(void)
{
    sys_key_event_enable();
    ui_update_status(STATUS_FM_MODE);
    clock_idle(FM_IDLE_CLOCK);
    fm_manage_init();
    fm_server_create();
}

static void fm_app_uninit(void)
{
    fm_sever_kill();
    fm_manage_close();
    tone_play_stop();
}


static int fm_state_machine(struct application *app, enum app_state state,
                            struct intent *it)
{
    int ret;
    switch (state) {
    case APP_STA_CREATE:
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
#if TCFG_FM_INSIDE_ENABLE
        reverb_pause();
#endif
#endif
#endif
        r_printf("APP_STA_CREATE\n");
        break;
    case APP_STA_START:
        if (!it) {
            break;
        }
        switch (it->action) {
        case ACTION_APP_MAIN:
            printf(" ACTION_FM_MAIN\n");
            fm_app_init();
            break;
        }
        break;
    case APP_STA_PAUSE:
        log_info("APP_STA_PAUSE\n");
        fm_manage_close();
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
        fm_app_uninit();
#if (defined(TCFG_DEC2TWS_ENABLE) && (TCFG_DEC2TWS_ENABLE))
#if (defined(TCFG_REVERB_ENABLE) && (TCFG_REVERB_ENABLE))
#if TCFG_FM_INSIDE_ENABLE
        reverb_resume();
#endif
#endif
#endif
        break;
    }

    return 0;
}

static int fm_user_msg_deal(int msg, int argc, int *argv)
{
    return 0;
}

static const struct application_reg app_fm_reg = {
    .tone_name = TONE_FM,
    .tone_play_check = NULL,
    .tone_prepare = NULL,
    .enter_check = NULL,
    .exit_check = NULL,
    .user_msg = fm_user_msg_deal,
#if (defined(TCFG_TONE2TWS_ENABLE) && (TCFG_TONE2TWS_ENABLE))
    .tone_tws_cmd = SYNC_CMD_MODE_FM,
#endif
};

static const struct application_operation app_fm_ops = {
    .state_machine  = fm_state_machine,
    .event_handler 	= fm_event_handler,
};

REGISTER_APPLICATION(app_app_fm) = {
    .name 	= APP_NAME_FM,
    .action	= ACTION_APP_MAIN,
    .ops 	= &app_fm_ops,
    .state  = APP_STA_DESTROY,
    .private_data = (void *) &app_fm_reg,
};


#endif

