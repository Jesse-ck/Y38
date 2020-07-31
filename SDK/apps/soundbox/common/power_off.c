#include "common/power_off.h"
#include "bt_tws.h"
#include <stdlib.h>
#include "app_power_manage.h"
#include "app_chargestore.h"
#include "btstack/avctp_user.h"

#define LOG_TAG_CONST       APP_ACTION
#define LOG_TAG             "[APP_ACTION]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"


#define POWER_OFF_CNT       10

extern bool get_tws_sibling_connect_state(void);
extern void sys_enter_soft_poweroff(void *priv);

static u8 goto_poweroff_cnt = 0;
static u8 goto_poweroff_flag = 0;

void power_off_deal(struct sys_event *event, u8 step)
{
    switch (step) {
    case 0:
        goto_poweroff_cnt = 0;
        goto_poweroff_flag = 0;

        if ((BT_STATUS_CONNECTING == get_bt_connect_status()) ||
            (BT_STATUS_TAKEING_PHONE == get_bt_connect_status()) ||
            (BT_STATUS_PLAYING_MUSIC == get_bt_connect_status())) {
            /* if (get_call_status() != BT_CALL_HANGUP) {
               log_info("call hangup\n");
               user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
               goto_poweroff_flag = 0;
               break;
               } */
            if ((get_call_status() == BT_CALL_INCOMING) ||
                (get_call_status() == BT_CALL_OUTGOING)) {
                log_info("key call reject\n");
                user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
                goto_poweroff_flag = 0;
                break;
            } else if (get_call_status() == BT_CALL_ACTIVE) {
                log_info("key call hangup\n");
                user_send_cmd_prepare(USER_CTRL_HFP_CALL_HANGUP, 0, NULL);
                goto_poweroff_flag = 0;
                break;
            }
        }

#if (TCFG_USER_TWS_ENABLE && CONFIG_TWS_POWEROFF_SAME_TIME == 0)
        if ((u32)event->arg == KEY_EVENT_FROM_TWS) {
            break;
        }
#endif
        goto_poweroff_flag = 1;
        user_send_cmd_prepare(USER_CTRL_ALL_SNIFF_EXIT, 0, NULL);
        break;
    case 1:
#if (TCFG_USER_TWS_ENABLE && CONFIG_TWS_POWEROFF_SAME_TIME == 0)
        if ((u32)event->arg == KEY_EVENT_FROM_TWS) {
            break;
        }
#endif
        log_info("poweroff flag:%d cnt:%d\n", goto_poweroff_flag, goto_poweroff_cnt);

        if (goto_poweroff_flag) {
            goto_poweroff_cnt++;

#if CONFIG_TWS_POWEROFF_SAME_TIME
            if (goto_poweroff_cnt == POWER_OFF_CNT) {
                if (get_tws_sibling_connect_state()) {
                    if ((u32)event->arg != KEY_EVENT_FROM_TWS) {
                        tws_api_sync_call_by_uuid('T', SYNC_CMD_POWER_OFF_TOGETHER, TWS_SYNC_TIME_DO);
                    }
                } else {
                    sys_enter_soft_poweroff(NULL);
                }
            }
#else
            if (goto_poweroff_cnt >= POWER_OFF_CNT) {
                goto_poweroff_cnt = 0;
                sys_enter_soft_poweroff(NULL);
            }
#endif //CONFIG_TWS_POWEROFF_SAME_TIME

        }
        break;;
    }
}

