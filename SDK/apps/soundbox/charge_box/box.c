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
#include "clock_cfg.h"

#if(defined TCFG_APP_BOX_EN) && (TCFG_APP_BOX_EN)

#define LOG_TAG_CONST       APP_BOX
#define LOG_TAG             "[APP_BOX]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"





// key
extern u8 box_key_event_get(struct key_event *key);
static int _key_event_opr(struct sys_event *event)
{
    int ret = true;
    int err = 0;
    u8 vol;
    struct key_event *key = &event->u.key;

    u8 key_event = box_key_event_get(key);

    switch (key_event) {
    case  KEY_MUSIC_PP:
        break;
    default:
        ret = false;
        break;
    }
    return ret;
}


static int box_event_handler(struct application *app, struct sys_event *event)
{
    const char *logo = NULL;
    int err = 0;
    switch (event->type) {
    case SYS_KEY_EVENT:
        return _key_event_opr(event);
    default://switch (event->type)
        break;
    }

    return false;
}

static void box_app_init(void *logo)
{
    int err = 0;
    clock_idle(BOX_IDLE_CLOCK);
}

static void box_app_uninit(void)
{

}

static int box_state_machine(struct application *app, enum app_state state,
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
            box_app_init();
            break;
        }
        break;
    case APP_STA_STOP:
        log_info("APP_STA_STOP\n");
        break;
    case APP_STA_DESTROY:
        log_info("APP_STA_DESTROY\n");
        box_app_uninit();
        break;
    }

    return 0;
}



//test
static int box_user_msg_deal(int msg, int argc, int *argv)
{
    return 0;
}

static const struct application_reg app_box_reg = {
    .tone_name = NULL,
    .tone_play_check = NULL,
    .tone_prepare = NULL,
    .enter_check = NULL,
    .exit_check = NULL,
    .user_msg = box_user_msg_deal,
};

static const struct application_operation app_box_ops = {
    .state_machine  = box_state_machine,
    .event_handler 	= box_event_handler,
};

REGISTER_APPLICATION(app_box) = {
    .name 	= APP_NAME_BOX,
    .action	= ACTION_APP_MAIN,
    .ops 	= &app_box_ops,
    .state  = APP_STA_DESTROY,
    .private_data = (void *) &app_box_reg,
};

#endif//TCFG_APP_BOX_EN

