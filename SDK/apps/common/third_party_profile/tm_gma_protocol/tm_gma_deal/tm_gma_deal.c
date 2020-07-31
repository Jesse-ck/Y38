#include "tm_gma_type.h"
#include "tm_frame_mg.h"
#include "tm_gma_hw_driver.h"
//#include "msg.h"
#include "btstack/avctp_user.h"
//#include "warning_tone.h"
//#include "sdk_cfg.h"
#include "gma.h"
#include "gma_include.h"
#include "tm_gma_hw_driver.h"

#if (GMA_EN)

#define TM_GMA_APP_DEBUG  1

#if TM_GMA_APP_DEBUG
#define TM_DBG		printf
#else
#define TM_DBG
#endif


bool tm_gma_msg_deal(u32 param)
{
    static u8 send_state = 0;
    int ret = 0;
    void printf_buf(u8 * buf, u32 len);
    switch (param) {

    case KEY_SEND_SPEECH_START:
        printf("gma MSG_SEND_SPEECH_START\n");

        if (BT_STATUS_TAKEING_PHONE == get_bt_connect_status()) {
            printf("phone ing...\n");
            break;
        }

        if (gma_connect_success() && (get_curr_channel_state()&A2DP_CH)) {
        } else {
            if (get_curr_channel_state()&A2DP_CH) {
                //<GMA未连接， 但是A2DP已连接， 点击唤醒键， 提示TTS【请打开小度APP】
            } else {
                //<蓝牙完全关闭状态， 用户按唤醒键， 提示TTS【蓝牙未连接， 请用手机蓝牙和我连接吧】
            }
            break;
        }

        printf("gma start mic status report \n");
        if (gma_mic_status_report(START_MIC) == 0) {
            //gma_hw_api.start_speech();
        }
        break;

    case KEY_SEND_SPEECH_STOP:
        printf("MSG_SPEECH_STOP\n");
        gma_hw_stop_speech();
        break;

    default:
        break;
    }

    return FALSE;
}

void gma_event_post(u32 type, u8 event)
{
    struct sys_event e;
    e.type = SYS_BT_EVENT;
    e.arg  = (void *)type;
    e.u.bt.event = event;
    sys_event_notify(&e);
}

#endif
