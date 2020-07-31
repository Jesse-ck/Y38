#include "key_event_deal.h"
#include "key_driver.h"
#include "app_config.h"
#include "board_config.h"


static const u8 app_common_key_io_table[KEY_IO_NUM_MAX][KEY_EVENT_MAX] = {
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
        /*SHORT*/ KEY_MUSIC_NEXT,
        /*LONG*/  KEY_VOL_UP,
        /*HOLD*/  KEY_VOL_UP,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [2] =
    {
        /*SHORT*/ KEY_MUSIC_PREV,
        /*LONG*/  KEY_VOL_DOWN,
        /*HOLD*/  KEY_VOL_DOWN,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [3] =
    {
        /*SHORT*/ KEY_CHANGE_MODE,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [4] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [5] =
    {
        /*SHORT*/ KEY_CHANGE_MODE,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
};

static const u8 app_common_key_ad_table[KEY_AD_NUM_MAX][KEY_EVENT_MAX] = {
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
        /*SHORT*/ KEY_MUSIC_NEXT,
        /*LONG*/  KEY_VOL_UP,
        /*HOLD*/  KEY_VOL_UP,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_REVERB_DEEPVAL_UP,
        /*TRIBLE*/KEY_NULL,
    },
    [2] =
    {
        /*SHORT*/ KEY_MUSIC_PREV,
        /*LONG*/  KEY_VOL_DOWN,
        /*HOLD*/  KEY_VOL_DOWN,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_REVERB_DEEPVAL_DOWN,
        /*TRIBLE*/KEY_NULL,
    },
    [3] =
    {
        /*SHORT*/ KEY_CHANGE_MODE,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
        /*DOUBLE*/KEY_BT_EMITTER_SW,
#else
        /*DOUBLE*/KEY_NULL,
#endif
        /*TRIBLE*/KEY_NULL,
    },
    [4] =
    {
        /*SHORT*/ KEY_FM_EMITTER_NEXT_FREQ,
        /*LONG*/  KEY_FM_EMITTER_NEXT_FREQ,
        /*HOLD*/  KEY_FM_EMITTER_NEXT_FREQ,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_REVERB_OPEN,
        /*TRIBLE*/KEY_NULL,

    },
    [5] =
    {

        /*SHORT*/ KEY_SWITCH_PITCH_MODE,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_ENC_START,
        /*TRIBLE*/KEY_NULL,
    },

    [6] =
    {
        /*SHORT*/ KEY_REVERB_GAIN0_UP,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [7] =
    {
        /*SHORT*/ KEY_REVERB_GAIN1_UP,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [8] =
    {
        /*SHORT*/ KEY_REVERB_GAIN2_UP,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },

    [9] =
    {
        /*SHORT*/ KEY_REVERB_DEEPVAL_UP,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_REVERB_DEEPVAL_UP,
        /*TRIBLE*/KEY_NULL,
    },

};

static const u8 app_common_key_ir_table[KEY_IR_NUM_MAX][KEY_EVENT_MAX] = {
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
        /*SHORT*/ KEY_MUSIC_PP,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_CALL_LAST_NO,
        /*TRIBLE*/KEY_NULL,
    },
    [4] =
    {
        /*SHORT*/ KEY_MUSIC_PREV,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_VOL_UP,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [5] =
    {
        /*SHORT*/ KEY_MUSIC_NEXT,
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
        /*SHORT*/ KEY_IR_NUM_0,
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
        /*SHORT*/ KEY_IR_NUM_1,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [13] =
    {
        /*SHORT*/ KEY_IR_NUM_2,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [14] =
    {
        /*SHORT*/ KEY_IR_NUM_3,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [15] =
    {
        /*SHORT*/ KEY_IR_NUM_4,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [16] =
    {
        /*SHORT*/ KEY_IR_NUM_5,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [17] =
    {
        /*SHORT*/ KEY_IR_NUM_6,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [18] =
    {
        /*SHORT*/ KEY_IR_NUM_7,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [19] =
    {
        /*SHORT*/ KEY_IR_NUM_8,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [20] =
    {
        /*SHORT*/ KEY_IR_NUM_9,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
};

static const u8 app_common_key_touch_table[KEY_TOUCH_NUM_MAX][KEY_EVENT_MAX] = {
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
        /*SHORT*/ KEY_MUSIC_NEXT,
        /*LONG*/  KEY_VOL_UP,
        /*HOLD*/  KEY_VOL_UP,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [2] =
    {
        /*SHORT*/ KEY_MUSIC_PREV,
        /*LONG*/  KEY_VOL_DOWN,
        /*HOLD*/  KEY_VOL_DOWN,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [3] =
    {
        /*SHORT*/ KEY_CHANGE_MODE,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [4] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
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
};

static const u8 common_key_rdec_table[KEY_RDEC_NUM_MAX][KEY_EVENT_MAX] = {
    [0] =
    {
        /*SHORT*/ KEY_FM_EMITTER_NEXT_FREQ,
        /*LONG*/  KEY_FM_EMITTER_NEXT_FREQ,
        /*HOLD*/  KEY_FM_EMITTER_NEXT_FREQ,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [1] =
    {
        /*SHORT*/ KEY_FM_EMITTER_PERV_FREQ,
        /*LONG*/  KEY_FM_EMITTER_PERV_FREQ,
        /*HOLD*/  KEY_FM_EMITTER_PERV_FREQ,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [2] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/    KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
};

static const u8 app_common_key_ctmu_touch_table[KEY_CTMU_TOUCH_NUM_MAX][KEY_EVENT_MAX] = {
    [0] =
    {
        /*SHORT*/ KEY_MUSIC_PP,
        /*LONG*/  KEY_POWEROFF,
        /*HOLD*/  KEY_POWEROFF_HOLD,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_ENC_START,
        /*TRIBLE*/KEY_NULL,
    },
    [1] =
    {
        /*SHORT*/ KEY_MUSIC_NEXT,
        /*LONG*/  KEY_VOL_UP,
        /*HOLD*/  KEY_VOL_UP,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [2] =
    {
        /*SHORT*/ KEY_MUSIC_PREV,
        /*LONG*/  KEY_VOL_DOWN,
        /*HOLD*/  KEY_VOL_DOWN,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [3] =
    {
        /*SHORT*/ KEY_CHANGE_MODE,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
        /*UP*/	  KEY_NULL,
        /*DOUBLE*/KEY_NULL,
        /*TRIBLE*/KEY_NULL,
    },
    [4] =
    {
        /*SHORT*/ KEY_NULL,
        /*LONG*/  KEY_NULL,
        /*HOLD*/  KEY_NULL,
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
        /*DOUBLE*/KEY_ENC_START,
        /*TRIBLE*/KEY_NULL,
    },
};



u8 __attribute__((weak))app_common_key_event_get(struct key_event *key)
{
    if (key == NULL) {
        return KEY_NULL;
    }
    u8 key_event = KEY_NULL;
    switch (key->type) {
    case KEY_DRIVER_TYPE_IO:
        key_event = app_common_key_io_table[key->value][key->event];
        break;
    case KEY_DRIVER_TYPE_AD:
    case KEY_DRIVER_TYPE_RTCVDD_AD:
        key_event = app_common_key_ad_table[key->value][key->event];
        break;
    case KEY_DRIVER_TYPE_IR:
        key_event = app_common_key_ir_table[key->value][key->event];
        break;
    case KEY_DRIVER_TYPE_TOUCH:
        key_event = app_common_key_touch_table[key->value][key->event];
        break;
    case KEY_DRIVER_TYPE_CTMU_TOUCH:
        key_event = app_common_key_ctmu_touch_table[key->value][key->event];
        break;
    default:
        break;
    }
    r_printf("app_common key_event:%d %d %d\n", key_event, key->value, key->event);
    return key_event;
}

u8 app_common_key_var_2_event(u32 key_var)
{
    u8 key_event = 0;
    u8 key_value = 0;
    struct sys_event e = {0};
    for (; key_value < KEY_AD_NUM_MAX; key_value++) {
        for (key_event = 0; key_event < KEY_EVENT_MAX; key_event++) {
            if (app_common_key_ad_table[key_value][key_event] == key_var) {
                e.type = SYS_KEY_EVENT;
                e.u.key.type = KEY_DRIVER_TYPE_AD;
                e.u.key.event = key_event;
                e.u.key.value = key_value;
                /* e.u.key.tmr = timer_get_ms(); */
                e.arg  = (void *)DEVICE_EVENT_FROM_KEY;
                sys_event_notify(&e);
                printf("key2event:%d %d %d\n", key_var, key_value, key_event);
                return true;
            }
        }
    }
    return false;
}

