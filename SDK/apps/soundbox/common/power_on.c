#include "system/includes.h"
#include "media/includes.h"
#include "app_config.h"
#include "app_action.h"
#include "tone_player.h"
#include "asm/charge.h"
#include "app_charge.h"
#include "app_main.h"
#include "ui_manage.h"
#include "vm.h"
#include "app_chargestore.h"
#include "user_cfg.h"
#include "ui/ui_api.h"


#define LOG_TAG_CONST       APP_IDLE
#define LOG_TAG             "[APP_IDLE]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


static int poweron_event_handler(struct application *app, struct sys_event *event)
{
    switch (event->type) {
    case SYS_KEY_EVENT:
        break;
    case SYS_BT_EVENT:
        break;
    case SYS_DEVICE_EVENT:
        break;
    default:
        return true;
    }
    return false;
}

static int power_on_init(void)
{
    ///有些需要在开机提示完成之后再初始化的东西， 可以在这里初始化
#if(defined TCFG_APP_BOX_EN) && (TCFG_APP_BOX_EN)
    app_task_switch(APP_NAME_BOX, ACTION_APP_MAIN, NULL);
#else
    app_task_switch(APP_NAME_BT, ACTION_APP_MAIN, NULL);
    /* app_task_switch(APP_NAME_MUSIC, ACTION_APP_MAIN, NULL); */
#endif

    return 0;
}

static int power_on_unint(void)
{
    return 0;
}

static int poweron_state_machine(struct application *app, enum app_state state,
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
            power_on_init();
            break;
        }
        break;
    case APP_STA_PAUSE:
        break;
    case APP_STA_RESUME:
        break;
    case APP_STA_STOP:
        break;
    case APP_STA_DESTROY:
        power_on_unint();
        break;
    }
    return 0;
}

static int  poweron_tone_prepare()
{

#if TCFG_UI_ENABLE
    printf("~~~~~~~~~~~~ %s \n", __FUNCTION__, __LINE__);
    ui_set_tmp_menu(MENU_POWER_UP, 0, 0, NULL);
#endif
    return 0;
}



static const struct application_reg app_poweron_reg = {
    .tone_name = TONE_POWER_ON,//如果不需要开机提示音， 直接置空
    .tone_play_check = NULL,
    .tone_prepare = poweron_tone_prepare,
    .enter_check = NULL,
    .exit_check = NULL,
    .user_msg = NULL,
};


static const struct application_operation app_poweron_ops = {
    .state_machine  = poweron_state_machine,
    .event_handler 	= poweron_event_handler,
};

REGISTER_APPLICATION(app_app_poweron) = {
    .name 	= APP_NAME_POWERON,
    .action	= ACTION_APP_MAIN,
    .ops 	= &app_poweron_ops,
    .state  = APP_STA_DESTROY,
    .private_data = (void *) &app_poweron_reg,
};


