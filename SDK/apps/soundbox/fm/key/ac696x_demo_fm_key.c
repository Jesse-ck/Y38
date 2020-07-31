
#include "key_event_deal.h"
#include "key_driver.h"
#include "app_config.h"
#include "board_config.h"


#if TCFG_APP_FM_EN
#if (defined(CONFIG_BOARD_AC696X_DEMO) || defined(CONFIG_BOARD_AC696X_TWS) || defined(CONFIG_BOARD_AC696X_TWS_BOX))
static const u8 fm_key_io_table[KEY_IO_NUM_MAX][KEY_EVENT_MAX] = {
    [0] =
    {
        /*SHORT*/ KEY_MUSIC_PP,
        /*LONG*/  KEY_POWEROFF,
        /*HOLD*/  KEY_POWEROFF_HOLD,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [1] =
    {
        /*SHORT*/ KEY_FM_NEXT_STATION,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [2] =
    {
        /*SHORT*/ KEY_FM_PREV_STATION,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [3] =
    {
        /*SHORT*/ KEY_CHANGE_MODE,
        /*LONG*/  KEY_FM_SCAN_ALL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [4] =
    {
        /*SHORT*/ KEY_FM_NEXT_FREQ,
        /*LONG*/  KEY_FM_SCAN_ALL_DOWN,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [5] =
    {
        /*SHORT*/ KEY_FM_PREV_FREQ,
        /*LONG*/  KEY_FM_SCAN_ALL_UP,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
};

static const u8 fm_key_ad_table[KEY_AD_NUM_MAX][KEY_EVENT_MAX] = {
    [0] =
    {
        /*SHORT*/ KEY_MUSIC_PP,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_FM_SCAN_ALL_UP,
        /*TRIBLE*/KEY_NULL,
    },
    [1] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [2] =
    {
        /*SHORT*/ KEY_CHANGE_MODE,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_FM_SCAN_ALL_DOWN,
        /*TRIBLE*/KEY_NULL,
    },
    [3] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_SPEAKER_OPEN,
        /*TRIBLE*/KEY_NULL,
    },
    [4] =
    {
        /*SHORT*/ KEY_FM_PREV_STATION,
        /*LONG*/  KEY_VOL_DOWN,
        /*HOLD*/  KEY_VOL_DOWN,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [5] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [6] =
    {
        /*SHORT*/ KEY_FM_NEXT_STATION,
        /*LONG*/  KEY_VOL_UP,
        /*HOLD*/  KEY_VOL_UP,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
};

static const u8 fm_key_ir_table[KEY_IR_NUM_MAX][KEY_EVENT_MAX] = {
    [0] =
    {
        /*SHORT*/ KEY_CHANGE_MODE,
        /*LONG*/  KEY_POWEROFF,
        /*HOLD*/  KEY_POWEROFF_HOLD,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [1] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [2] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [3] =
    {
        /*SHORT*/ KEY_FM_EMITTER_MENU,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [4] =
    {
        /*SHORT*/ KEY_FM_EMITTER_NEXT_FREQ,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_VOL_UP,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [5] =
    {
        /*SHORT*/ KEY_FM_EMITTER_PERV_FREQ,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_VOL_DOWN,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [6] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [7] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [8] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [9] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [10] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [11] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [12] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [13] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [14] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [15] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [16] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [17] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [18] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [19] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [20] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
};

static const u8 fm_key_touch_table[KEY_TOUCH_NUM_MAX][KEY_EVENT_MAX] = {
    [0] =
    {
        /*SHORT*/ KEY_MUSIC_PP,
        /*LONG*/  KEY_POWEROFF,
        /*HOLD*/  KEY_POWEROFF_HOLD,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [1] =
    {
        /*SHORT*/ KEY_FM_PREV_STATION,
        /*LONG*/  KEY_VOL_UP,
        /*HOLD*/  KEY_VOL_UP,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [2] =
    {
        /*SHORT*/ KEY_FM_NEXT_STATION,
        /*LONG*/  KEY_VOL_DOWN,
        /*HOLD*/  KEY_VOL_DOWN,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [3] =
    {
        /*SHORT*/ KEY_CHANGE_MODE,
        /*LONG*/  KEY_FM_SCAN_ALL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [4] =
    {
        /*SHORT*/ KEY_FM_PREV_FREQ,
        /*LONG*/  KEY_FM_SCAN_ALL_UP,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [5] =
    {
        /*SHORT*/ KEY_FM_NEXT_FREQ,
        /*LONG*/  KEY_FM_SCAN_ALL_DOWN,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
};


u8 fm_key_event_get(struct key_event *key)
{
    if (key == NULL) {
        return KEY_NULL;
    }
    u8 key_event = KEY_NULL;
    switch (key->type) {
    case KEY_DRIVER_TYPE_IO:
        key_event = fm_key_io_table[key->value][key->event];
        break;
    case KEY_DRIVER_TYPE_AD:
    case KEY_DRIVER_TYPE_RTCVDD_AD:
        key_event = fm_key_ad_table[key->value][key->event];
        break;
    case KEY_DRIVER_TYPE_IR:
        key_event = fm_key_ir_table[key->value][key->event];
        break;
    case KEY_DRIVER_TYPE_TOUCH:
        key_event = fm_key_touch_table[key->value][key->event];
        break;
    default:
        break;
    }
    /* printf("fm key_event:%d %d %d\n", key_event, key->value, key->event); */
    return key_event;
}
#endif// CONFIG_BOARD_AC696X_DEMO
#endif//TCFG_APP_FM_EN

