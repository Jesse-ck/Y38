#include "app_config.h"
#include "key_event_deal.h"
#include "system/includes.h"
#include "tone_player.h"
#include "app_action.h"
#include "tone_player.h"
#include "media/includes.h"
#include "asm/audio_spdif.h"
#include "clock_cfg.h"

#include "ui/ui_api.h"


#if TCFG_APP_SPDIF_EN

extern struct audio_spdif_hdl spdif_slave_hdl;
/* #define LOG_TAG_CONST    APP_SPDIF */
#define LOG_TAG             "[APP_SPDIF]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

struct spdif_opr {
    u8 input_sorce;
    u8 output_sorce;
    u8 status;
};


static struct spdif_opr *__this = NULL;

static void spdif_app_init(void)
{
    clock_idle(SPDIF_IDLE_CLOCK);
#if TCFG_UI_ENABLE
    ui_set_main_menu(UI_MENU_MAIN_NULL);
#endif

}

static u8 source = 0;
static void spdif_open(void)
{
#if	TCFG_SPDIF_OUTPUT_ENABLE
    spdif_slave_hdl.output_port = SPDIF_OUT_PORT_A;//PB11
#endif
    spdif_slave_hdl.input_port  = SPDIF_IN_PORT_A;//PA9
    source  = 0;
    audio_spdif_slave_open(&spdif_slave_hdl);

    audio_spdif_slave_start(&spdif_slave_hdl);
#if TCFG_HDMI_ARC_ENABLE
    extern void hdmi_cec_init(void);
    hdmi_cec_init();
#endif
}

static void spdif_close(void)
{
    audio_spdif_slave_close(&spdif_slave_hdl);
    log_info("APP_SPDIF_STOP1\n");
    extern void spdif_dec_close(void);
    spdif_dec_close();
#if TCFG_HDMI_ARC_ENABLE
    extern void hdmi_cec_close(void);
    hdmi_cec_close();
#endif
}

static void switch_spdif_input_port(void)
{
    source++;
    if (source > 2) {
        source = 0;
    }
    printf("\n--func=%s\n", __FUNCTION__);
    switch (source) {
    case 0:
        audio_spdif_slave_switch_port(&spdif_slave_hdl, SPDIF_IN_PORT_A);
        break;
    case 1:
        audio_spdif_slave_switch_port(&spdif_slave_hdl, SPDIF_IN_PORT_C);
        break;
    case 2:
        audio_spdif_slave_switch_port(&spdif_slave_hdl, SPDIF_IN_PORT_D);
        break;
    default:
        break;
    }
}

extern u8 spdif_key_event_get(struct key_event *key);
static int _key_event_opr(struct sys_event *event)
{
    int ret = true;
    int err = 0;
    struct key_event *key = &event->u.key;
    u8 key_event = spdif_key_event_get(key);

    log_info("key value:%d, event:%d \n", key->value, key->event);
    log_info("key_event:%d \n", key_event);


    switch (key_event) {
    case KEY_SPDIF_SW_SOURCE:
        switch_spdif_input_port();
        log_info("KEY_SPDIF_SW_SOURCE \n");
        break;
    default :
        ret = false;
        break;
    }
    return ret;
}

static int spdif_event_handler(struct application *app, struct sys_event *event)
{
    switch (event->type) {
    case SYS_KEY_EVENT:
        return _key_event_opr(event);
    case SYS_DEVICE_EVENT:
        return false;
    default:
        return false;
    }
    return false;
}

static int spdif_state_machine(struct application *app, enum app_state state, struct intent *it)
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
            log_info("ACTION_APP_SPDIF_MAIN\n");
            spdif_app_init();
            spdif_open();
            break;
        }
        break;
    case APP_STA_PAUSE:
        log_info("APP_SPDIF_PAUSE\n");
        break;
    case APP_STA_RESUME:
        log_info("APP_SPDIF_RESUME\n");
        break;
    case APP_STA_STOP:
        log_info("APP_SPDIF_STOP\n");
        spdif_close();
        break;
    case APP_STA_DESTROY:
        log_info("APP_SPDIF_DESTROY\n");
        break;
    }
    return 0;
}


static int spdif_user_msg_deal(int msg, int argc, int *argv)
{
    switch (msg) {
    case USER_MSG_SYS_MIXER_RECORD_SWITCH:
        ///spdif模式不响应混合录音
        return 1;
    default:
        break;
    }
    return 0;
}

static const struct application_reg app_spdif_reg = {
    .tone_name = TONE_SPDIF,
    .tone_play_check = NULL,
    .tone_prepare = NULL,
    .enter_check = NULL,//spdif_app_check,
    .exit_check = NULL,
    .user_msg = spdif_user_msg_deal,
#if (defined(TCFG_TONE2TWS_ENABLE) && (TCFG_TONE2TWS_ENABLE))
    .tone_tws_cmd = SYNC_CMD_MODE_SPDIF,
#endif
};

static const struct application_operation app_spdif_ops = {
    .state_machine  = spdif_state_machine,
    .event_handler 	= spdif_event_handler,
};

REGISTER_APPLICATION(app_app_spdif) = {
    .name 	= APP_NAME_SPDIF,
    .action	= ACTION_APP_MAIN,
    .ops 	= &app_spdif_ops,
    .state  = APP_STA_DESTROY,
    .private_data = (void *) &app_spdif_reg,
};


#endif
