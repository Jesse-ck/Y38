#include "system/includes.h"
#include "media/includes.h"

#include "app_config.h"
#include "app_action.h"
#include "app_main.h"

#include "audio_config.h"
#include "audio_digital_vol.h"

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
#include "fm_emitter/fm_emitter_manage.h"
#endif

#define LOG_TAG             "[APP_AUDIO]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#define DEFAULT_DIGTAL_VOLUME   16384

struct app_audio_config {
    u8 state;
    u8 prev_state;
    u8 mute_when_vol_zero;
    volatile u8 fade_gain_l;
    volatile u8 fade_gain_r;
    volatile s16 fade_dgain_l;
    volatile s16 fade_dgain_r;
    volatile s16 fade_dgain_step_l;
    volatile s16 fade_dgain_step_r;
    volatile int fade_timer;
    volatile int save_vol_timer;
    volatile u8  save_vol_cnt;
    s16 digital_volume;
    atomic_t ref;
    s16 max_volume[APP_AUDIO_STATE_WTONE + 1];
};
static const char *audio_state[] = {
    "idle",
    "music",
    "call",
    "tone",
    "err",
};

static struct app_audio_config app_audio_cfg = {0};

#define __this      (&app_audio_cfg)
extern struct dac_platform_data dac_data;
struct audio_dac_hdl dac_hdl;
extern struct audio_adc_hdl adc_hdl;
OS_SEM dac_sem;

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC)
s16 dac_buff[4 * 1024];
#endif




/*
 *************************************************************
 *
 *	audio volume save
 *
 *************************************************************
 */

static void app_audio_volume_save_do(void *priv)
{
    /* log_info("app_audio_volume_save_do %d\n", __this->save_vol_cnt); */
    local_irq_disable();
    if (++__this->save_vol_cnt >= 5) {
        sys_hi_timer_del(__this->save_vol_timer);
        __this->save_vol_timer = 0;
        __this->save_vol_cnt = 0;
        local_irq_enable();
        log_info("VOL_SAVE\n");
        syscfg_write(CFG_MUSIC_VOL, &app_var.music_volume, 1);
        return;
    }
    local_irq_enable();
}

static void app_audio_volume_change(void)
{
    local_irq_disable();
    __this->save_vol_cnt = 0;
    if (__this->save_vol_timer == 0) {
        __this->save_vol_timer = sys_hi_timer_add(NULL, app_audio_volume_save_do, 1000);
    }
    local_irq_enable();
}

/*
 *************************************************************
 *
 *	audio volume fade
 *
 *************************************************************
 */
static void audio_fade_timer(void *priv)
{
    u8 gain_l = dac_hdl.vol_l;
    u8 gain_r = dac_hdl.vol_r;

    //printf("<fade:%d-%d-%d-%d>", gain_l, gain_r, __this->fade_gain_l, __this->fade_gain_r);
    local_irq_disable();
    if ((gain_l == __this->fade_gain_l) && (gain_r == __this->fade_gain_r)
       ) {
        sys_hi_timer_del(__this->fade_timer);
        __this->fade_timer = 0;
        /*音量为0的时候mute住*/
        audio_dac_set_L_digital_vol(&dac_hdl, gain_l ? __this->digital_volume : 0);
        audio_dac_set_R_digital_vol(&dac_hdl, gain_r ? __this->digital_volume : 0);

        if ((gain_l == 0) && (gain_r == 0)) {
            if (__this->mute_when_vol_zero) {
                __this->mute_when_vol_zero = 0;
                audio_dac_mute(&dac_hdl, 1);
            }
        }

        local_irq_enable();
        /* log_info("dac_fade_end, VOL : 0x%x 0x%x 0x%x  %d\n", JL_ANA->DAA_CON1, JL_AUDIO->DAC_VL0, JL_AUDIO->DAC_VL1, __this->fade_gain_l); */
        return;
    }
    if (gain_l > __this->fade_gain_l) {
        gain_l--;
    } else if (gain_l < __this->fade_gain_l) {
        gain_l++;
    }

    if (gain_r > __this->fade_gain_r) {
        gain_r--;
    } else if (gain_r < __this->fade_gain_r) {
        gain_r++;
    }

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
    fm_emitter_manage_set_vol(gain_l);
#else

    int err = audio_dac_set_analog_vol(&dac_hdl, gain_l);
    if (err < 0) {
        local_irq_disable();
        sys_hi_timer_del(__this->fade_timer);
        __this->fade_timer = 0;
        local_irq_enable();
    }
#endif

    local_irq_enable();
}

static int audio_fade_timer_add(u8 gain_l, u8 gain_r)
{
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
    return 0;
#endif
    /* r_printf("dac_fade_begin:0x%x\n", __this->fade_timer); */
    local_irq_disable();
    if (__this->fade_timer == 0) {
        __this->fade_gain_l = gain_l;
        __this->fade_gain_r = gain_r;
        __this->fade_timer = sys_hi_timer_add((void *)0, audio_fade_timer, 2);
        /* y_printf("fade_timer:0x%x", __this->fade_timer); */
    } else {
        /* audio_dac_set_analog_vol(&dac_hdl, (__this->fade_gain_l + gain_l) / 2); */
        __this->fade_gain_l = gain_l;
        __this->fade_gain_r = gain_r;

    }
    local_irq_enable();

    return 0;
}


#if (SYS_VOL_TYPE == 2)

#define DGAIN_SET_MAX_STEP (300)
#define DGAIN_SET_MIN_STEP (30)

static unsigned short combined_vol_list[31][2] = {
    { 0,     0}, //0: None
    { 0, 16384}, // 1:-40.92 db
    { 2, 14124}, // 2:-39.51 db
    { 3, 14240}, // 3:-38.10 db
    { 4, 14326}, // 4:-36.69 db
    { 5, 14427}, // 5:-35.28 db
    { 6, 14562}, // 6:-33.87 db
    { 7, 14681}, // 7:-32.46 db
    { 8, 14802}, // 8:-31.05 db
    { 9, 14960}, // 9:-29.64 db
    {10, 15117}, // 10:-28.22 db
    {11, 15276}, // 11:-26.81 db
    {12, 15366}, // 12:-25.40 db
    {13, 15528}, // 13:-23.99 db
    {14, 15675}, // 14:-22.58 db
    {15, 15731}, // 15:-21.17 db
    {16, 15535}, // 16:-19.76 db
    {17, 15609}, // 17:-18.35 db
    {18, 15684}, // 18:-16.93 db
    {19, 15777}, // 19:-15.52 db
    {20, 15851}, // 20:-14.11 db
    {21, 15945}, // 21:-12.70 db
    {22, 16002}, // 22:-11.29 db
    {23, 16006}, // 23:-9.88 db
    {24, 16050}, // 24:-8.47 db
    {25, 16089}, // 25:-7.06 db
    {26, 16154}, // 26:-5.64 db
    {27, 16230}, // 27:-4.23 db
    {28, 16279}, // 28:-2.82 db
    {29, 16328}, // 29:-1.41 db
    {30, 16384}, // 30:0.00 db
};
static unsigned short call_combined_vol_list[16][2] = {
    { 0,     0}, //0: None
    { 0, 16384}, // 1:-40.92 db
    { 2, 15345}, // 2:-38.79 db
    { 4, 14374}, // 3:-36.66 db
    { 5, 15726}, // 4:-34.53 db
    { 7, 14782}, // 5:-32.40 db
    { 8, 16191}, // 6:-30.27 db
    {10, 15271}, // 7:-28.14 db
    {12, 14336}, // 8:-26.00 db
    {13, 15739}, // 9:-23.87 db
    {15, 14725}, // 10:-21.74 db
    {16, 15799}, // 11:-19.61 db
    {18, 14731}, // 12:-17.48 db
    {19, 16098}, // 13:-15.35 db
    {21, 15027}, // 14:-13.22 db
    {22, 16384}, // 15:-11.09 db
};

static void audio_combined_fade_timer(void *priv)
{
    u8 gain_l = dac_hdl.vol_l;
    u8 gain_r = dac_hdl.vol_r;
    s16 dgain_l = dac_hdl.d_volume[DA_LEFT];
    s16 dgain_r = dac_hdl.d_volume[DA_RIGHT];

    __this->fade_dgain_step_l = __builtin_abs(dgain_l - __this->fade_dgain_l) / \
                                (__builtin_abs(gain_l - __this->fade_gain_l) + 1);
    if (__this->fade_dgain_step_l > DGAIN_SET_MAX_STEP) {
        __this->fade_dgain_step_l = DGAIN_SET_MAX_STEP;
    } else if (__this->fade_dgain_step_l < DGAIN_SET_MIN_STEP) {
        __this->fade_dgain_step_l = DGAIN_SET_MIN_STEP;
    }

    __this->fade_dgain_step_r = __builtin_abs(dgain_r - __this->fade_dgain_r) / \
                                (__builtin_abs(gain_r - __this->fade_gain_r) + 1);
    if (__this->fade_dgain_step_r > DGAIN_SET_MAX_STEP) {
        __this->fade_dgain_step_r = DGAIN_SET_MAX_STEP;
    } else if (__this->fade_dgain_step_r < DGAIN_SET_MIN_STEP) {
        __this->fade_dgain_step_r = DGAIN_SET_MIN_STEP;
    }

    /* log_info("<a:%d-%d-%d-%d d:%d-%d-%d-%d-%d-%d>\n", \ */
    /* gain_l, gain_r, __this->fade_gain_l, __this->fade_gain_r, \ */
    /* dgain_l, dgain_r, __this->fade_dgain_l, __this->fade_dgain_r, \ */
    /* __this->fade_dgain_step_l, __this->fade_dgain_step_r); */

    local_irq_disable();

    if ((gain_l == __this->fade_gain_l) \
        && (gain_r == __this->fade_gain_r) \
        && (dgain_l == __this->fade_dgain_l)\
        && (dgain_r == __this->fade_dgain_r)) {
        sys_hi_timer_del(__this->fade_timer);
        __this->fade_timer = 0;
        /*音量为0的时候mute住*/
        if ((gain_l == 0) && (gain_r == 0)) {
            if (__this->mute_when_vol_zero) {
                __this->mute_when_vol_zero = 0;
                audio_dac_mute(&dac_hdl, 1);
            }
        }

        local_irq_enable();
        /* log_info("dac_fade_end,VOL:0x%x-0x%x-%d-%d-%d-%d\n", \ */
        /* JL_ANA->DAA_CON1, JL_AUDIO->DAC_VL0,  \ */
        /* __this->fade_gain_l, __this->fade_gain_r, \ */
        /* __this->fade_dgain_l, __this->fade_dgain_r); */
        return;
    }
    if ((gain_l != __this->fade_gain_l) \
        || (gain_r != __this->fade_gain_r)) {
        if (gain_l > __this->fade_gain_l) {
            gain_l--;
        } else if (gain_l < __this->fade_gain_l) {
            gain_l++;
        }

        if (gain_r > __this->fade_gain_r) {
            gain_r--;
        } else if (gain_r < __this->fade_gain_r) {
            gain_r++;
        }

        audio_dac_set_analog_vol(&dac_hdl, gain_l);
    }

    if ((dgain_l != __this->fade_dgain_l) \
        || (dgain_r != __this->fade_dgain_r)) {

        if (gain_l != __this->fade_gain_l) {
            if (dgain_l > __this->fade_dgain_l) {
                if ((dgain_l - __this->fade_dgain_l) >= __this->fade_dgain_step_l) {
                    dgain_l -= __this->fade_dgain_step_l;
                } else {
                    dgain_l = __this->fade_dgain_l;
                }
            } else if (dgain_l < __this->fade_dgain_l) {
                if ((__this->fade_dgain_l - dgain_l) >= __this->fade_dgain_step_l) {
                    dgain_l += __this->fade_dgain_step_l;
                } else {
                    dgain_l = __this->fade_dgain_l;
                }
            }
        } else {
            dgain_l = __this->fade_gain_l;
        }

        if (gain_r != __this->fade_gain_r) {
            if (dgain_r > __this->fade_dgain_r) {
                if ((dgain_r - __this->fade_dgain_r) >= __this->fade_dgain_step_r) {
                    dgain_r -= __this->fade_dgain_step_r;
                } else {
                    dgain_r = __this->fade_dgain_r;
                }
            } else if (dgain_r < __this->fade_dgain_r) {
                if ((__this->fade_dgain_r - dgain_r) >= __this->fade_dgain_step_r) {
                    dgain_r += __this->fade_dgain_step_r;
                } else {
                    dgain_r = __this->fade_dgain_r;
                }
            }
        } else {
            dgain_r = __this->fade_gain_r;
        }
        audio_dac_set_digital_vol(&dac_hdl, dgain_l);
    }

    local_irq_enable();
}


static int audio_combined_fade_timer_add(u8 gain_l, u8 gain_r)
{
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
    return 0;
#endif
    u8  target_again_l = 0;
    u8  target_again_r = 0;
    u16 target_dgain_l = 0;
    u16 target_dgain_r = 0;

    if (__this->state == APP_AUDIO_STATE_CALL) {
        target_again_l = call_combined_vol_list[gain_l][0];
        target_again_r = call_combined_vol_list[gain_r][0];
        target_dgain_l = call_combined_vol_list[gain_l][1];
        target_dgain_r = call_combined_vol_list[gain_r][1];
    } else {
        target_again_l = combined_vol_list[gain_l][0];
        target_again_r = combined_vol_list[gain_r][0];
        target_dgain_l = combined_vol_list[gain_l][1];
        target_dgain_r = combined_vol_list[gain_r][1];
    }

    r_printf("dac_fade_begin:0x%x\n", __this->fade_timer);

    local_irq_disable();

    __this->fade_gain_l  = target_again_l;
    __this->fade_gain_r  = target_again_r;
    __this->fade_dgain_l = target_dgain_l;
    __this->fade_dgain_r = target_dgain_r;

    if (__this->fade_timer == 0) {
        __this->fade_timer = sys_hi_timer_add((void *)0, audio_combined_fade_timer, 2);
        y_printf("combined_fade_timer:0x%x", __this->fade_timer);
    }

    local_irq_enable();

    return 0;
}

#endif  // (SYS_VOL_TYPE == 2)

static void set_audio_device_volume(u8 type, s16 vol)
{
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
    fm_emitter_manage_set_vol(vol);
#else
    audio_dac_set_analog_vol(&dac_hdl, vol);
#endif
}

static int get_audio_device_volume(u8 vol_type)
{
#if 0
    void *dev;
    struct audio_volume volume;
    int vol;

    volume.type = vol_type;
    dev = dev_open("audio", (void *)"play0");
    if (dev) {
        dev_ioctl(dev, IOCTL_GET_VOLUME, (u32)&volume);
        dev_close(dev);
        return volume.value;
    } else {
        log_info("no audio dev\n");
        return -1;
    }
#endif
    return 0;
}

void volume_up_down_direct(s8 value)
{
#if 0
    s16 volume;
    volume = get_audio_device_volume(AUDIO_SYS_VOL);
    volume += value;
    if (volume < 0) {
        volume = 0;
    }
    set_audio_device_volume(AUDIO_SYS_VOL, volume);
#endif
}

void audio_fade_in_fade_out(u8 left_gain, u8 right_gain)
{
#if (SYS_VOL_TYPE == 2)
    audio_combined_fade_timer_add(left_gain, right_gain);
#else
    audio_fade_timer_add(left_gain, right_gain);
#endif
}

void app_audio_set_volume(u8 state, s8 volume, u8 fade)
{
    switch (state) {
    case APP_AUDIO_STATE_IDLE:
    case APP_AUDIO_STATE_MUSIC:
        app_var.music_volume = volume;
        if (app_var.music_volume > get_max_sys_vol()) {
            app_var.music_volume = get_max_sys_vol();
        }
        volume = app_var.music_volume;
        break;
    case APP_AUDIO_STATE_CALL:
        app_var.call_volume = volume;
        if (app_var.call_volume > app_var.aec_dac_gain) {
            app_var.call_volume = app_var.aec_dac_gain;
        }
        volume = app_var.call_volume;
#if TCFG_CALL_USE_DIGITAL_VOLUME
        audio_digital_vol_set(volume);
        return;
#endif
        break;
    case APP_AUDIO_STATE_WTONE:
#if (APP_AUDIO_STATE_WTONE_BY_MUSIC == 1)
        app_var.wtone_volume = app_var.music_volume;
        if (app_var.wtone_volume < 5) {
            app_var.wtone_volume = 5;
        }
#else
        app_var.wtone_volume = volume;
#endif
        if (app_var.wtone_volume > get_max_sys_vol()) {
            app_var.wtone_volume = get_max_sys_vol();
        }
        volume = app_var.wtone_volume;
        break;
    default:
        return;
    }
    if (state == __this->state) {
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
        fm_emitter_manage_set_vol(volume);
#else
        audio_dac_set_volume(&dac_hdl, volume, volume);
        if (audio_dac_get_status(&dac_hdl)) {
            if (fade) {
                audio_fade_in_fade_out(volume, volume);
            } else {
                audio_dac_set_analog_vol(&dac_hdl, volume);
            }
        }
#endif
    }
    app_audio_volume_change();
}

void app_audio_volume_init(void)
{
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, app_var.music_volume, 1);
}

s8 app_audio_get_volume(u8 state)
{
    s8 volume = 0;
    switch (state) {
    case APP_AUDIO_STATE_IDLE:
    case APP_AUDIO_STATE_MUSIC:
        volume = app_var.music_volume;
        break;
    case APP_AUDIO_STATE_CALL:
        volume = app_var.call_volume;
        break;
    case APP_AUDIO_STATE_WTONE:
#if (APP_AUDIO_STATE_WTONE_BY_MUSIC == 1)
        volume = app_var.music_volume;
        break;
#else
        volume = app_var.wtone_volume;
        /* if (!volume) { */
        /* volume = app_var.music_volume; */
        /* } */
        break;
#endif
    case APP_AUDIO_CURRENT_STATE:
        volume = app_audio_get_volume(__this->state);
        break;
    default:
        break;
    }
    /* printf("app_audio_get_volume %d %d\n", state, volume); */
    return volume;
}


static const char *audio_mute_string[] = {
    "mute_default",
    "unmute_default",
    "mute_L",
    "unmute_L",
    "mute_R",
    "unmute_R",
};

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
#define AUDIO_MUTE_FADE			0
#define AUDIO_UMMUTE_FADE		0
#else
#define AUDIO_MUTE_FADE			1
#define AUDIO_UMMUTE_FADE		1
#endif

void app_audio_mute(u8 value)
{
    u8 volume = 0;
    printf("audio_mute:%s", audio_mute_string[value]);
    switch (value) {
    case AUDIO_MUTE_DEFAULT:

#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
        fm_emitter_manage_set_vol(0);
#else

#if AUDIO_MUTE_FADE
        audio_fade_in_fade_out(0, 0);
        __this->mute_when_vol_zero = 1;
#else
        audio_dac_set_analog_vol(&dac_hdl, 0);
        audio_dac_mute(&dac_hdl, 1);
#endif

#endif
        break;
    case AUDIO_UNMUTE_DEFAULT:
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM)
        volume = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
        fm_emitter_manage_set_vol(volume);
#else

#if AUDIO_UMMUTE_FADE
        audio_dac_mute(&dac_hdl, 0);
        volume = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
        audio_fade_in_fade_out(volume, volume);
#else
        audio_dac_mute(&dac_hdl, 0);
        volume = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
        audio_dac_set_analog_vol(&dac_hdl, volume);
#endif

#endif
        break;
    }
}


void app_audio_volume_up(u8 value)
{
    s16 volume = 0;
    switch (__this->state) {
    case APP_AUDIO_STATE_IDLE:
    case APP_AUDIO_STATE_MUSIC:
        app_var.music_volume += value;
        if (app_var.music_volume > get_max_sys_vol()) {
            app_var.music_volume = get_max_sys_vol();
        }
        volume = app_var.music_volume;
        break;
    case APP_AUDIO_STATE_CALL:
        app_var.call_volume += value;
        if (app_var.call_volume > app_var.aec_dac_gain) {
            app_var.call_volume = app_var.aec_dac_gain;
        }
        volume = app_var.call_volume;
#if TCFG_CALL_USE_DIGITAL_VOLUME
        audio_digital_vol_set(volume);
        return;
#endif
        break;
    case APP_AUDIO_STATE_WTONE:
#if (APP_AUDIO_STATE_WTONE_BY_MUSIC == 1)
        app_var.wtone_volume = app_var.music_volume;
#endif
        app_var.wtone_volume += value;
        if (app_var.wtone_volume > get_max_sys_vol()) {
            app_var.wtone_volume = get_max_sys_vol();
        }
        volume = app_var.wtone_volume;
#if (APP_AUDIO_STATE_WTONE_BY_MUSIC == 1)
        app_var.music_volume = app_var.wtone_volume;
#endif
        break;
    default:
        return;
    }

    app_audio_set_volume(__this->state, volume, 1);
}

void app_audio_volume_down(u8 value)
{
    s16 volume = 0;
    switch (__this->state) {
    case APP_AUDIO_STATE_IDLE:
    case APP_AUDIO_STATE_MUSIC:
        app_var.music_volume -= value;
        if (app_var.music_volume < 0) {
            app_var.music_volume = 0;
        }
        volume = app_var.music_volume;
        break;
    case APP_AUDIO_STATE_CALL:
        app_var.call_volume -= value;
        if (app_var.call_volume < 0) {
            app_var.call_volume = 0;
        }
        volume = app_var.call_volume;
#if TCFG_CALL_USE_DIGITAL_VOLUME
        audio_digital_vol_set(volume);
        return;
#endif
        break;
    case APP_AUDIO_STATE_WTONE:
#if (APP_AUDIO_STATE_WTONE_BY_MUSIC == 1)
        app_var.wtone_volume = app_var.music_volume;
#endif
        app_var.wtone_volume -= value;
        if (app_var.wtone_volume < 0) {
            app_var.wtone_volume = 0;
        }
        volume = app_var.wtone_volume;
#if (APP_AUDIO_STATE_WTONE_BY_MUSIC == 1)
        app_var.music_volume = app_var.wtone_volume;
#endif
        break;
    default:
        return;
    }

    app_audio_set_volume(__this->state, volume, 1);
}

void app_audio_state_switch(u8 state, s16 max_volume)
{
    r_printf("audio state old:%s,new:%s,vol:%d\n", audio_state[__this->state], audio_state[state], max_volume);

    __this->prev_state = __this->state;
    __this->state = state;
#if TCFG_CALL_USE_DIGITAL_VOLUME
    if (__this->state == APP_AUDIO_STATE_CALL) {
        audio_digital_vol_open(max_volume, max_volume, 4);
        /*调数字音量的时候，模拟音量定最大*/
        audio_dac_set_analog_vol(&dac_hdl, max_volume);
    }
#else
    app_audio_set_volume(__this->state, app_audio_get_volume(__this->state), 1);
#endif

    /*限制最大音量*/
    __this->digital_volume = DEFAULT_DIGTAL_VOLUME;
    __this->max_volume[state] = max_volume;

#if (SYS_VOL_TYPE == 2)
    if (__this->state == APP_AUDIO_STATE_CALL) {
        __this->max_volume[state] = 15;
    }
#endif

}



void app_audio_state_exit(u8 state)
{
#if TCFG_CALL_USE_DIGITAL_VOLUME
    if (__this->state == APP_AUDIO_STATE_CALL) {
        audio_digital_vol_close();
    }
#endif

    r_printf("audio state now:%s,prev:%s\n", audio_state[__this->state], audio_state[__this->prev_state]);
    if (state == __this->state) {
        __this->state = __this->prev_state;
        __this->prev_state = APP_AUDIO_STATE_IDLE;
    } else if (state == __this->prev_state) {
        __this->prev_state = APP_AUDIO_STATE_IDLE;
    }
    app_audio_set_volume(__this->state, app_audio_get_volume(__this->state), 1);
}
u8 app_audio_get_state(void)
{
    return __this->state;
}

s16 app_audio_get_max_volume(void)
{
    if (__this->state == APP_AUDIO_STATE_IDLE) {
        return get_max_sys_vol();
    }
    return __this->max_volume[__this->state];
}

void app_audio_set_mix_volume(u8 front_volume, u8 back_volume)
{
    /*set_audio_device_volume(AUDIO_MIX_FRONT_VOL, front_volume);
    set_audio_device_volume(AUDIO_MIX_BACK_VOL, back_volume);*/
}
#if 0

void audio_vol_test()
{
    app_set_sys_vol(10, 10);
    log_info("sys vol %d %d\n", get_audio_device_volume(AUDIO_SYS_VOL) >> 16, get_audio_device_volume(AUDIO_SYS_VOL) & 0xffff);
    log_info("ana vol %d %d\n", get_audio_device_volume(AUDIO_ANA_VOL) >> 16, get_audio_device_volume(AUDIO_ANA_VOL) & 0xffff);
    log_info("dig vol %d %d\n", get_audio_device_volume(AUDIO_DIG_VOL) >> 16, get_audio_device_volume(AUDIO_DIG_VOL) & 0xffff);
    log_info("max vol %d %d\n", get_audio_device_volume(AUDIO_MAX_VOL) >> 16, get_audio_device_volume(AUDIO_MAX_VOL) & 0xffff);

    app_set_max_vol(30);
    app_set_ana_vol(25, 24);
    app_set_dig_vol(90, 90);

    log_info("sys vol %d %d\n", get_audio_device_volume(AUDIO_SYS_VOL) >> 16, get_audio_device_volume(AUDIO_SYS_VOL) & 0xffff);
    log_info("ana vol %d %d\n", get_audio_device_volume(AUDIO_ANA_VOL) >> 16, get_audio_device_volume(AUDIO_ANA_VOL) & 0xffff);
    log_info("dig vol %d %d\n", get_audio_device_volume(AUDIO_DIG_VOL) >> 16, get_audio_device_volume(AUDIO_DIG_VOL) & 0xffff);
    log_info("max vol %d %d\n", get_audio_device_volume(AUDIO_MAX_VOL) >> 16, get_audio_device_volume(AUDIO_MAX_VOL) & 0xffff);
}
#endif

#ifdef AUDIO_MIC_TEST
#if 0
static const u8 pcm_wav_header[] = {
    'R', 'I', 'F', 'F',         //rid
    0xff, 0xff, 0xff, 0xff,     //file length
    'W', 'A', 'V', 'E',         //wid
    'f', 'm', 't', ' ',         //fid
    0x14, 0x00, 0x00, 0x00,     //format size
    0x01, 0x00,                 //format tag
    0x01, 0x00,                 //channel num
    0x80, 0x3e, 0x00, 0x00,     //sr 16K
    0x00, 0x7d, 0x00, 0x00,     //avgbyte
    0x02, 0x00,                 //blockalign
    0x10, 0x00,                 //persample
    0x02, 0x00,
    0x00, 0x00,
    'f', 'a', 'c', 't',         //f2id
    0x40, 0x00, 0x00, 0x00,     //flen
    0xff, 0xff, 0xff, 0xff,     //datalen
    'd', 'a', 't', 'a',         //"data"
    0xff, 0xff, 0xff, 0xff,     //sameple  size
};
#endif

#define AUDIO_MIC_PLAY_SIZE     AUDIO_FIXED_SIZE / 2
#define AUDIO_MIC_REC_SIZE      AUDIO_FIXED_SIZE / 2
#define AUDIO_MIC_PLAY_BUFFER   (audio_dma_buffer)
#define AUDIO_MIC_REC_BUFFER    (audio_dma_buffer + AUDIO_MIC_PLAY_SIZE)
#define AUDIO_MIC_PLAY_SAMPLE_RATE   16000

struct mic_rec_play {
    void *dec_server;
    void *rec_dev;
    int  file_ptr;
};
static struct mic_rec_play mic_play;

static void *mic_play_fopen(const char *path, const char *mode)
{
    int err;
    void *rec_dev;
    struct audio_format f = {0};
    int bindex;

    /*audio rec设备*/
    log_info("open_audio_dev\n");
    rec_dev = dev_open("audio", (void *)"rec");
    if (!rec_dev) {
        log_error("---dev_open: faild\n");
        return NULL;
    }
    f.sample_source = "mic";
    f.sample_rate = AUDIO_MIC_PLAY_SAMPLE_RATE;
    f.volume = 20;
    f.channel = 1;
    err = dev_ioctl(rec_dev, AUDIOC_SET_FMT, (u32)&f);
    if (err) {
        log_error("audio set fmt err\n");
        goto __err;
    }
    struct audio_reqbufs breq;
    breq.buf = AUDIO_MIC_REC_BUFFER;
    breq.size = AUDIO_MIC_REC_SIZE;
    err = dev_ioctl(rec_dev, AUDIOC_REQBUFS, (unsigned int)&breq);
    if (err) {
        log_error("audio rec req bufs err\n");
        goto __err;
    }

    err = dev_ioctl(rec_dev, AUDIOC_STREAM_ON, (u32)&bindex);
    if (err) {
        log_error("audio rec stream on err\n");
        goto __err;
    }
    return rec_dev;
__err:
    dev_close(rec_dev);
    return 0;
}

static int mic_play_fread(void *file, void *buf, u32 len)
{
#if 0
    u8  head_len = 0;
    if (mic_play.file_ptr < sizeof(pcm_wav_header)) {
        head_len = sizeof(pcm_wav_header);
        head_len -= mic_play.file_ptr;
        if (len < head_len) {
            head_len = len;
        }
        memcpy(buf, &pcm_wav_header[mic_play.file_ptr], head_len);
        mic_play.file_ptr += head_len;
        buf += head_len;
    }
#endif
    return dev_read(file, buf, len);
}

static int mic_play_fseek(void *file, u32 offset, int seek_mode)
{
    mic_play.file_ptr = 0;
    return 0;
}

static int mic_play_flen(void *file)
{
    return 0xffffffff;
}

static int mic_play_fclose(void *file)
{
    void *dev = file;

    if (dev) {
        return dev_ioctl(dev, AUDIOC_STREAM_OFF, 0);
    }
    return 0;
}

static const struct audio_vfs_ops mic_play_vfs_ops = {
    .fopen = mic_play_fopen,
    .fread = mic_play_fread,
    .fseek = mic_play_fseek,
    .flen  = mic_play_flen,
    .fclose = mic_play_fclose,
};

int mic_rec_play_start()
{
    union audio_dec_req req = {0};

    if (mic_play.dec_server) {
        return -EBUSY;
    }
    req.play.cmd = AUDIO_DEC_OPEN;
    req.play.output_buf = AUDIO_MIC_PLAY_BUFFER;
    req.play.output_buf_len = AUDIO_MIC_PLAY_SIZE;
    req.play.priority = TONE_PRIORITY;
    req.play.fade_en = 1;
    req.play.volume = app_audio_get_volume(APP_AUDIO_STATE_WTONE);
    req.play.file = mic_play_fopen(NULL, "r");
    req.play.vfs_ops = &mic_play_vfs_ops;
    req.play.channel = 1;
    req.play.sample_rate = AUDIO_MIC_PLAY_SAMPLE_RATE;
    req.play.format = "pcm";

    if (req.play.file == NULL) {
        log_error("open file err\n");
        return -ENOENT;
    }

    mic_play.rec_dev = req.play.file;
    mic_play.dec_server = server_open("audio_dec", NULL);
    if (!mic_play.dec_server) {
        log_error("audio decoder server open error\n");
        mic_play_fclose(req.play.file);
        return -EINVAL;
    }
    mic_play.file_ptr = 0;

    req.play.cmd = AUDIO_DEC_OPEN;
    server_request(mic_play.dec_server, AUDIO_REQ_DEC, &req);

    req.play.cmd = AUDIO_DEC_START;
    req.play.auto_dec = 1;
    server_request(mic_play.dec_server, AUDIO_REQ_DEC, &req);

    return 0;
}

int mic_rec_play_stop()
{
    union audio_dec_req req = {0};
    if (mic_play.dec_server == NULL) {
        return -ENODEV;
    }

    req.play.cmd = AUDIO_DEC_STOP;
    server_request(mic_play.dec_server, AUDIO_REQ_DEC, &req);
    server_close(mic_play.dec_server);
    mic_play.dec_server = NULL;
    if (mic_play.rec_dev == NULL) {
        return -1;
    }

    mic_play_fclose(mic_play.rec_dev);
    dev_close(mic_play.rec_dev);
    mic_play.rec_dev = NULL;
    return 0;
}

#endif

void dac_power_on(void)
{
    log_info(">>>dac_power_on:%d", __this->ref.counter);
    if (atomic_inc_return(&__this->ref) == 1) {
        audio_dac_open(&dac_hdl);
    }

}
void dac_sniff_power_off(void)
{
    audio_dac_close(&dac_hdl);
}

void dac_power_off(void)
{
    log_info(">>>dac_power_off:%d", __this->ref.counter);
    if (atomic_dec_return(&__this->ref)) {
        return;
    }
#if 1
    app_audio_mute(AUDIO_MUTE_DEFAULT);
    if (dac_hdl.vol_l || dac_hdl.vol_r) {
        u8 fade_time = dac_hdl.vol_l * 2 / 10 + 1;
        os_time_dly(fade_time);
        printf("fade_time:%d ms", fade_time);
    }
#endif
    audio_dac_close(&dac_hdl);
}

//#define LADC_CAPLESS_INFO_DEBUG
#ifdef LADC_CAPLESS_INFO_DEBUG
/*
 * adcdso:正负1000之内
 * dacr32(正常范围:稳定在正负28000之内)
 */
void ladc_capless_info(s16 adcdso, s32 dacr32, s32 pout, s32 tmps8)
{
    printf("[%d, %d, %d, %d]\n", adcdso, dacr32, pout, tmps8);
}
#endif

static void mic_capless_feedback_toggle(u8 toggle);

#define LADC_CAPLESS_ADJUST_SAVE
#ifdef LADC_CAPLESS_ADJUST_SAVE
#define DIFF_RANGE		50
#define CFG_DIFF_RANGE	200
#define CHECK_INTERVAL  7
#define DACR32_DEFAULT	32767

#define MIC_CAPLESS_ADJUST_BUD_DEFAULT	0
#define MIC_CAPLESS_ADJUST_BUD			100
/*不支持自动校准，使用快速收敛*/
#if TCFG_MC_BIAS_AUTO_ADJUST
u8	mic_capless_adjust_bud = MIC_CAPLESS_ADJUST_BUD_DEFAULT;
#else
u8	mic_capless_adjust_bud = MIC_CAPLESS_ADJUST_BUD;
#endif

s16 read_capless_DTB(void)
{
    s16 dacr32 = 32767;
    int ret = syscfg_read(CFG_DAC_DTB, &dacr32, 2);
    printf("cfg DAC_DTB:%d,ret = %d\n", dacr32, ret);
    /*没有记忆值,使用默认值*/
    if (ret != 2) {
        /*没有收敛值的时候，使用快速收敛*/
        //printf("DAC_DTB NULL,use fast feedback");
        mic_capless_adjust_bud = MIC_CAPLESS_ADJUST_BUD;
        return 32767;
    }
    return dacr32;
}

s16 read_vm_capless_DTB(void)
{
    s16 vm_dacr32 = 32767;
    int ret = syscfg_read(CFG_DAC_DTB, &vm_dacr32, 2);
    printf("vm DAC_DTB:%d,ret = %d\n", vm_dacr32, ret);
    if (ret != 2) {
        return DACR32_DEFAULT;
    }
    return vm_dacr32;
}

s16 save_dacr32 = DACR32_DEFAULT;
static u8 adjust_complete = 0;
void save_capless_DTB()
{
    s16 diff;
    //printf("save_capless_DTB\n");
    if ((save_dacr32 != DACR32_DEFAULT) && adjust_complete) {
        /*比较是否需要更新配置*/
        s16 cfg_dacr32 = read_vm_capless_DTB();
        adjust_complete = 0;
        diff = save_dacr32 - cfg_dacr32;
        if ((cfg_dacr32 == DACR32_DEFAULT) || ((diff < -CFG_DIFF_RANGE) || (diff > CFG_DIFF_RANGE))) {
            log_info("dacr32 write:%d\n", save_dacr32);
            syscfg_write(CFG_DAC_DTB, &save_dacr32, 2);

            /* s16 tmp_dacr32;
            syscfg_read(CFG_DAC_DTB,&tmp_dacr32,2);
            printf("dacr32 read:%d\n",tmp_dacr32); */
        } else {
            log_info("dacr32 need't update:%d,diff:%d\n", save_dacr32, diff);
        }
    } else {
        log_info("dacr32 adjust uncomplete:%d,complete:%d\n", save_dacr32, adjust_complete);
    }
}

void ladc_capless_adjust_post(s32 dacr32, u8 begin)
{
    static s32 last_dacr32 = 0;
    static u8 check_cnt = 0;

    s32 dacr32_diff;

    /*adjust_begin,clear*/
    if (begin) {
        last_dacr32 = 0;
        adjust_complete = 0;
        check_cnt = 0;
        save_dacr32 = DACR32_DEFAULT;
        return;
    }
    //printf("<%d>",dacr32);
    if (adjust_complete == 0) {
        if (++check_cnt > CHECK_INTERVAL) {
            check_cnt = 0;
            dacr32_diff = dacr32 - last_dacr32;
            //printf("[capless:%d-%d-%d]",dacr32,last_dacr32,dacr32_diff);
            last_dacr32 = dacr32;
            if (adjust_complete == 0) {
                save_dacr32 = dacr32;
            }
            /*调整稳定*/
            if ((dacr32_diff > -DIFF_RANGE) && (dacr32_diff < DIFF_RANGE)) {
                log_info("adjust_OK:%d\n", dacr32);
                adjust_complete = 1;
#if TCFG_MC_BIAS_AUTO_ADJUST
                mic_capless_feedback_toggle(0);
#endif
            }
        }
    }
}
#endif


#if 0
/*
 *写到dac buf的数据接口
 */
void audio_write_data_hook(void *data, u32 len)
{

}
#endif

/*
 *dac快速校准
 */
//#define DAC_TRIM_FAST_EN
#ifdef DAC_TRIM_FAST_EN
u8 dac_trim_fast_en()
{
    return 1;
}
#endif

/*
 *自定义dac上电延时时间，具体延时多久应通过示波器测量
 */
#if 1
void dac_power_on_delay()
{
#if TCFG_MC_BIAS_AUTO_ADJUST
    void mic_capless_auto_adjust_init();
    mic_capless_auto_adjust_init();
#endif
    os_time_dly(50);
}
#endif

/*
 *capless模式一开始不要的数据包数量
 */
u16 get_ladc_capless_dump_num(void)
{
    return 10;
}

/*
 *mic省电容模式自动收敛
 */
u8 mic_capless_feedback_sw = 0;
static u8 audio_mc_idle_query(void)
{
    return (mic_capless_feedback_sw ? 0 : 1);
}
REGISTER_LP_TARGET(audio_mc_device_lp_target) = {
    .name = "audio_mc_device",
    .is_idle = audio_mc_idle_query,
};

/*快调慢调边界*/
u16 get_ladc_capless_bud(void)
{
    //printf("mc_bud:%d",mic_capless_adjust_bud);
    return mic_capless_adjust_bud;
}

extern int audio_adc_mic_init(u16 sr);
extern void audio_adc_mic_exit(void);
int audio_mic_capless_feedback_control(u8 en, u16 sr)
{
    int ret = 0;
    if (en) {
        ret = audio_adc_mic_init(sr);
    } else {
        audio_adc_mic_exit();
    }
    return ret;
}

OS_SEM mc_sem;
/*收敛的前提是偏置电压合法*/
static void mic_capless_feedback_toggle(u8 toggle)
{
    int ret = 0;
    log_info("mic_capless_feedback_toggle:%d-%d\n", mic_capless_feedback_sw, toggle);
    if (toggle && (mic_capless_feedback_sw == 0)) {
        mic_capless_feedback_sw = 1;
        ret = audio_mic_capless_feedback_control(1, 32000);
        if (ret == 0) {
            mic_capless_adjust_bud = MIC_CAPLESS_ADJUST_BUD;
        }
        os_sem_create(&mc_sem, 0);
    } else if (mic_capless_feedback_sw) {
        os_sem_post(&mc_sem);
        mic_capless_adjust_bud = MIC_CAPLESS_ADJUST_BUD_DEFAULT;
    } else {
        log_info("Nothing to do\n");
    }
}

extern struct adc_platform_data adc_data;

#if TCFG_MC_BIAS_AUTO_ADJUST
static const u8 mic_bias_tab[] = {0, 20, 12, 28, 4, 18, 10, 26, 2, 22, 14, 30, 17, 21, 6, 25, 29, 27, 31, 5, 3, 7};
extern void delay_2ms(int cnt);
extern void wdt_clear(void);
extern void mic_analog_init(u8 mic_ldo_vsel, u8 mic_bias);
extern void mic_analog_close(struct adc_platform_data *pd);
void mic_capless_auto_adjust_init()
{
    if (adc_data.mic_capless == 0) {
        return;
    }
    log_info("mic_capless_bias_adjust_init:%d-%d\n", adc_data.mic_ldo_vsel, adc_data.mic_bias_res);
    mic_analog_init(adc_data.mic_ldo_vsel, adc_data.mic_bias_res);
}

void mic_capless_auto_adjust_exit()
{
    if (adc_data.mic_capless == 0) {
        return;
    }
    log_info("mic_capless_bias_adjust_exit\n");
    mic_analog_close(&adc_data);
}

/*AC696x系列只支持高压模式*/
#define MIC_BIAS_HIGH_UPPER_LIMIT	200	/*高压上限：2.00v*/
#define MIC_BIAS_HIGH_LOWER_LIMIT	135	/*高压下限：1.35v*/

#define ADC_MIC_IO			IO_PORTA_01
#define ADC_MIC_CH			AD_CH_PA1
#define MIC_BIAS_RSEL(x) 	SFR(JL_ANA->ADA_CON0, 6, 5, x)


/*
 *return -1:非省电容模式
 *return -2:校准失败
 *return  0:默认值合法，不用校准
 *return  1:默认值非法，启动校准
 */
s8 mic_capless_auto_adjust(void)
{
    u16 mic_bias_val = 0;
    u8 mic_bias_idx = adc_data.mic_bias_res;
    u8 mic_bias_compare = 0;
    u16 bias_upper_limit = MIC_BIAS_HIGH_UPPER_LIMIT;
    u16 bias_lower_limit = MIC_BIAS_HIGH_LOWER_LIMIT;
    s8 ret = 0;
    u8 err_cnt = 0;
    u8 mic_ldo_idx = 0;

    //printf("mic_capless_bias_adjust:%d\n",adc_data.mic_capless);

    if (adc_data.mic_capless == 0) {
        return -1;
    }

    log_info("mic_bias idx:%d,rsel:%d\n", mic_bias_idx, mic_bias_tab[mic_bias_idx]);

    gpio_set_die(ADC_MIC_IO, 0);
    gpio_direction_input(ADC_MIC_IO);
    gpio_set_pull_up(ADC_MIC_IO, 0);
    gpio_set_pull_down(ADC_MIC_IO, 0);

    adc_add_sample_ch(ADC_MIC_CH);


#if 0
    /*
     *调试使用
     *如果mic的偏置电压mic_bias_val稳定，则表示延时足够，否则加大延时知道电压值稳定
     */
    while (1) {
        wdt_clear();
        MIC_BIAS_RSEL(mic_bias_tab[mic_bias_idx]);
        delay_2ms(50);//延时等待偏置电压稳定
        mic_bias_val = adc_get_voltage(ADC_MIC_CH) / 10;
        log_info("mic_bias_val:%d,idx:%d,rsel:%d\n", mic_bias_val, mic_bias_idx, mic_bias_tab[mic_bias_idx]);
    }
#endif

    while (1) {
        wdt_clear();
        MIC_BIAS_RSEL(mic_bias_tab[mic_bias_idx]);
        delay_2ms(50);

        mic_bias_val = adc_get_voltage(ADC_MIC_CH) / 10;
        log_info("mic_bias_val:%d,idx:%d,rsel:%d\n", mic_bias_val, mic_bias_idx, mic_bias_tab[mic_bias_idx]);

        if (mic_bias_val < bias_lower_limit) {
            /*电压偏小，调小内部上拉偏置*/
            mic_bias_compare |= BIT(0);
            mic_bias_idx++;
            if (mic_bias_idx >= sizeof(mic_bias_tab)) {
                log_error("mic_bias_auto_adjust faild 0\n");
                /*校准失败，使用快速收敛*/
                //mic_capless_adjust_bud = MIC_CAPLESS_ADJUST_BUD;
                ret = -2;
                //break;
            }
        } else if (mic_bias_val > bias_upper_limit) {
            /*电压偏大，调大内部上拉偏置*/
            mic_bias_compare |= BIT(1);
            if (mic_bias_idx) {
                mic_bias_idx--;
            } else {
                log_error("mic_bias_auto_adjust faild 1\n");
                /*校准失败，使用快速收敛*/
                //mic_capless_adjust_bud = MIC_CAPLESS_ADJUST_BUD;
                ret = -2;
                //break;
            }
        } else {
            if (mic_bias_compare) {
                /*超出范围，调整过的值,保存*/
                adc_data.mic_bias_res = mic_bias_idx;
                log_info("mic_bias_adjust ok,idx:%d,rsel:%d\n", mic_bias_idx, mic_bias_tab[mic_bias_idx]);
                /*记住校准过的值*/
                ret = syscfg_write(CFG_MC_BIAS, &adc_data.mic_bias_res, 1);
                log_info("mic_bias_adjust save ret = %d\n", ret);
                ret = 1;
            }

            /*原本的MICLDO档位不合适，保存新的MICLDO档位*/
            if (err_cnt) {
                adc_data.mic_ldo_vsel = mic_ldo_idx;
                log_info("mic_ldo_vsel fix:%d\n", adc_data.mic_ldo_vsel);
                //log_info("mic_bias:%d,idx:%d\n",adc_data.mic_bias_res,mic_bias_idx);
                ret = syscfg_write(CFG_MIC_LDO_VSEL, &mic_ldo_idx, 1);
                log_info("mic_ldo_vsel save ret = %d\n", ret);
                ret = 1;
            }
            log_info("mic_bias valid:%d,idx:%d,res:%d\n", mic_bias_val, mic_bias_idx, mic_bias_tab[mic_bias_idx]);
            break;
        }

        /*
         *当前MICLDO分不出合适的偏置电压
         * 选择1、修改MICLDO档位，重新校准
         * 选择2、直接退出，跳出自动校准
         */
        if ((mic_bias_compare == (BIT(0) | BIT(1))) || (ret == -2)) {
            log_info("mic_bias_trim err,adjust micldo vsel\n");
            ret = 0;
#if 1	/*选择1*/
            /*从0开始遍历查询*/
            if (err_cnt) {
                mic_ldo_idx++;
            }
            err_cnt++;
            /*跳过默认的ldo电压档*/
            if (mic_ldo_idx == adc_data.mic_ldo_vsel) {
                mic_ldo_idx++;
            }
            /*遍历结束，没有合适的MICLDO电压档*/
            if (mic_ldo_idx > 3) {
                log_info("mic_bias_adjust tomeout\n");
                mic_capless_adjust_bud = MIC_CAPLESS_ADJUST_BUD;
                ret = -3;
                break;
            }
            log_info("mic_ldo_idx:%d", mic_ldo_idx);
            JL_ANA->ADA_CON0 = BIT(0) | BIT(1) | (mic_ldo_idx << 2); //MICLDO_EN,MICLDO_ISEL,MIC_LDO_VSEL
            /*修改MICLDO电压档，等待电压稳定*/
            os_time_dly(20);
            /*复位偏置电阻档位*/
            mic_bias_idx = adc_data.mic_bias_res;
            /*复位校准标志位*/
            mic_bias_compare = 0;
#else	/*选择2*/
            log_info("mic_bias_trim err,break loop\n");
            mic_capless_adjust_bud = MIC_CAPLESS_ADJUST_BUD;
            ret = -3;
            break;
#endif
        }
    }
    mic_capless_auto_adjust_exit();
    return ret;
}
#endif

/*
 *检查mic偏置是否需要校准,以下情况需要重新校准：
 *1、power on reset
 *2、vm被擦除
 *3、每次开机都校准
 */
extern u8 power_reset_src;
u8 mc_bias_adjust_check()
{
#if(TCFG_MC_BIAS_AUTO_ADJUST == MC_BIAS_ADJUST_ALWAYS)
    return 1;
#elif (TCFG_MC_BIAS_AUTO_ADJUST == MC_BIAS_ADJUST_ONE)
    return 0;
#endif
    u8 por_flag = 0;
    int ret = syscfg_read(CFG_POR_FLAG, &por_flag, 1);
    if (ret == 1) {
        if (por_flag == 0xA5) {
            log_info("power on reset 1");
            por_flag = 0;
            ret = syscfg_write(CFG_POR_FLAG, &por_flag, 1);
            return 1;
        }
    }
    if (power_reset_src & BIT(0)) {
        log_info("power on reset 2");
        return 1;
    }
    if (read_vm_capless_DTB() == DACR32_DEFAULT) {
        log_info("vm format");
        return 1;
    }

    return 0;
}

/*
 *pos = 1:dac trim begin
 *pos = 2:dac trim end
 *pos = 3:dac已经trim过(开机)
 *pos = 4:dac已经读取过变量(过程)
 *pos = 5:dac已经trim过(开机,dac模块初始化)
 */
extern void audio_dac2micbias_en(struct audio_dac_hdl *dac, u8 en);
void dac_trim_hook(u8 pos)
{
#if TCFG_MC_BIAS_AUTO_ADJUST
    int ret = 0;
    log_info("dac_trim_hook:%d\n", pos);
    if ((adc_data.mic_capless == 0) || (pos == 0xFF)) {
        return;
    }

    if (pos == 1) {
        ret = mic_capless_auto_adjust();
        if (ret >= 0) {
            mic_capless_feedback_toggle(1);
        } else {
            /*校准出错的时候不做预收敛*/
            log_info("auto_adjust err:%d\n", ret);
        }
        return;
    } else if (pos == 2) {
        if (mic_capless_feedback_sw) {
            ret = os_sem_pend(&mc_sem, 250);
            if (ret == OS_TIMEOUT) {
                log_info("mc_trim1 timeout!\n");
            }
            audio_mic_capless_feedback_control(0, 16000);
        } else {
            log_info("auto_feedback disable");
        }
    } else if (pos == 5) {
        if (mc_bias_adjust_check()) {
            //printf("MC_BIAS_ADJUST...");
            void mic_capless_auto_adjust_init();
            mic_capless_auto_adjust_init();
            os_time_dly(25);
            ret = mic_capless_auto_adjust();
            /*
             *预收敛条件：
             *1、开机检查发现mic的偏置非法，则校准回来，同时重新收敛,比如中途更换mic头的情况
             *2、收敛值丢失（vm被擦除），重新收敛一次(前提是校准成功)
             */
            if ((ret == 1) || ((ret == 0) && (read_vm_capless_DTB() == DACR32_DEFAULT))) {
                audio_dac2micbias_en(&dac_hdl, 1);
                mic_capless_feedback_toggle(1);
                ret = os_sem_pend(&mc_sem, 250);
                if (ret == OS_TIMEOUT) {
                    log_info("mc_trim2 timeout!\n");
                }
                audio_mic_capless_feedback_control(0, 16000);
                audio_dac2micbias_en(&dac_hdl, 0);
            } else {
                log_info("auto_adjust err:%d\n", ret);
            }
        } else {
            log_info("MC_BIAS_OK...\n");
        }
    }
    mic_capless_feedback_sw = 0;
#endif/*TCFG_MC_BIAS_AUTO_ADJUST*/
}


/*******************************************************
* Function name	: app_audio_irq_handler_hook
* Description	: 音频设备硬件中断回调
* Parameter		:
*   @source			0:DAC 1:ADC
* Return        : None
********************* -HB ******************************/
__attribute__((weak))
void app_audio_irq_handler_hook(int source)
{
    return;
}


/*******************************************************
* Function name	: app_audio_irq_handler
* Description	: 音频设备硬件中断
* Return        : None
********************* -HB ******************************/
___interrupt
static void app_audio_irq_handler()
{
    /* putchar('A'); */
    if (JL_AUDIO->DAC_CON & BIT(7)) {
        JL_AUDIO->DAC_CON |= BIT(6);
        if (JL_AUDIO->DAC_CON & BIT(5)) {
            /*putchar('R');*/
            os_sem_post(&dac_sem);
            app_audio_irq_handler_hook(0);
            audio_dac_irq_handler(&dac_hdl);
            /*r_printf("resuem\n");*/
        }
    }

    if (JL_AUDIO->ADC_CON & BIT(7)) {
        JL_AUDIO->ADC_CON |= BIT(6);
        if ((JL_AUDIO->ADC_CON & BIT(5)) && (JL_AUDIO->ADC_CON & BIT(4))) {
            app_audio_irq_handler_hook(1);
            audio_adc_irq_handler(&adc_hdl);
        }
    }
}

/*******************************************************
* Function name	: app_audio_output_init
* Description	: 音频输出设备初始化
* Return        : None
********************* -HB ******************************/
void app_audio_output_init(void)
{
    os_sem_create(&dac_sem, 0);
    request_irq(IRQ_AUDIO_IDX, 2, app_audio_irq_handler, 0);
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC)
    audio_dac_init(&dac_hdl, &dac_data);

    s16 dacr32 = read_capless_DTB();

    audio_dac_set_capless_DTB(&dac_hdl, dacr32);

    audio_dac_set_buff(&dac_hdl, dac_buff, sizeof(dac_buff));

    struct audio_dac_trim dac_trim;
    int len = syscfg_read(CFG_DAC_TRIM_INFO, (void *)&dac_trim, sizeof(dac_trim));
    if (len != sizeof(dac_trim) || dac_trim.left == 0 || dac_trim.right == 0) {
        audio_dac_do_trim(&dac_hdl, &dac_trim, 0);
        syscfg_write(CFG_DAC_TRIM_INFO, (void *)&dac_trim, sizeof(dac_trim));
    } else {
        dac_trim_hook(5);
    }
    audio_dac_set_trim_value(&dac_hdl, &dac_trim);
    audio_dac_set_delay_time(&dac_hdl, 30, 50);
    audio_dac_set_analog_vol(&dac_hdl, 0);
    audio_dac_set_fade_handler(&dac_hdl, NULL, audio_fade_in_fade_out);
#endif
}

/*******************************************************
* Function name	: app_audio_output_sync_buff_init
* Description	: 设置音频输出设备同步功能 buf
* Parameter		:
*   @sync_buff		buf 起始地址
*   @len       		buf 长度
* Return        : None
********************* -HB ******************************/
void app_audio_output_sync_buff_init(void *sync_buff, int len)
{
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC)
    /*音频同步DA端buffer设置*/
    audio_dac_set_sync_buff(&dac_hdl, sync_buff, len);
#endif
}


/*******************************************************
* Function name	: app_audio_output_samplerate_select
* Description	: 将输入采样率与输出采样率进行匹配对比
* Parameter		:
*   @sample_rate    输入采样率
*   @high:          0 - 低一级采样率，1 - 高一级采样率
* Return        : 匹配后的采样率
********************* -HB ******************************/
int app_audio_output_samplerate_select(u32 sample_rate, u8 high)
{
    return audio_dac_sample_rate_select(&dac_hdl, sample_rate, high);
}

/*******************************************************
* Function name	: app_audio_output_samplerate_set
* Description	: 设置音频输出设备的采样率
* Parameter		:
*   @sample_rate	采样率
* Return        : 0 success, other fail
********************* -HB ******************************/
int app_audio_output_samplerate_set(int sample_rate)
{
#if AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC
    return audio_dac_set_sample_rate(&dac_hdl, sample_rate);
#endif
    return 0;
}

/*******************************************************
* Function name	: app_audio_output_samplerate_get
* Description	: 获取音频输出设备的采样率
* Return        : 音频输出设备的采样率
********************* -HB ******************************/
int app_audio_output_samplerate_get(void)
{
#if AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC
    return audio_dac_get_sample_rate(&dac_hdl);
#endif
    return 0;
}

/*******************************************************
* Function name	: app_audio_output_mode_get
* Description	: 获取当前硬件输出模式
* Return        : 输出模式
********************* -HB ******************************/
int app_audio_output_mode_get(void)
{
#if AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC
    return audio_dac_get_pd_output(&dac_hdl);
#endif
    return 0;
}

/*******************************************************
* Function name	: app_audio_output_mode_set
* Description	: 设置当前硬件输出模式
* Return        : 0 success, other fail
********************* -HB ******************************/
int app_audio_output_mode_set(u8 output)
{
#if AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC
    return audio_dac_set_pd_output(&dac_hdl, output);
#endif
    return 0;
}

/*******************************************************
* Function name	: app_audio_output_channel_get
* Description	: 获取音频输出设备输出通道数
* Return        : 通道数
********************* -HB ******************************/
int app_audio_output_channel_get(void)
{
#if AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC
    return audio_dac_get_channel(&dac_hdl);
#endif
    return 0;
}

/*******************************************************
* Function name	: app_audio_output_channel_set
* Description	: 设置音频输出设备输出通道数
* Parameter		:
*   @channel       	通道数
* Return        : 0 success, other fail
********************* -HB ******************************/
int app_audio_output_channel_set(u8 channel)
{
#if AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC
    return audio_dac_set_channel(&dac_hdl, channel);
#endif
    return 0;
}

/*******************************************************
* Function name	: app_audio_output_write
* Description	: 向音频输出设备写入需要输出的音频数据
* Parameter		:
*   @buf			写入音频数据的起始地址
*   @len			写入音频数据的长度
* Return        : 成功写入的长度
********************* -HB ******************************/
int app_audio_output_write(void *buf, int len)
{
#if AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC
    return audio_dac_write(&dac_hdl, buf, len);
#elif AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_FM
    return fm_emitter_cbuf_write(buf, len);
#endif
    return 0;
}


/*******************************************************
* Function name	: app_audio_output_start
* Description	: 音频输出设备输出打开
* Return        : 0 success, other fail
********************* -HB ******************************/
int app_audio_output_start(void)
{
#if AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC
    return audio_dac_start(&dac_hdl);
#endif
    return 0;
}

/*******************************************************
* Function name	: app_audio_output_stop
* Description	: 音频输出设备输出停止
* Return        : 0 success, other fail
********************* -HB ******************************/
int app_audio_output_stop(void)
{
#if AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC
    return audio_dac_stop(&dac_hdl);
#endif
    return 0;
}

/*******************************************************
* Function name	: app_audio_output_reset
* Description	: 音频输出设备重启
* Parameter		:
*   @msecs       	重启时间 ms
* Return        : 0 success, other fail
********************* -HB ******************************/
int app_audio_output_reset(u32 msecs)
{
#if AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC
    return audio_dac_sound_reset(&dac_hdl, msecs);
#endif
    return 0;
}

/*******************************************************
* Function name	: app_audio_output_get_cur_buf_points
* Description	: 获取当前音频输出buf还可以输出的点数
* Parameter		:
* Return        : 还可以输出的点数
********************* -HB ******************************/
int app_audio_output_get_cur_buf_points(void)
{
#if AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC
    return audio_dac_buf_pcm_number(&dac_hdl) >> PCM_PHASE_BIT;
#endif
    return 0;
}

int app_audio_output_ch_analog_gain_set(u8 ch, u8 again)
{
#if (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC) || (AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_BT)
    return audio_dac_ch_analog_gain_set(&dac_hdl, ch, again);
#endif
    return 0;
}

int app_audio_output_state_get(void)
{

#if AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC
    return 	audio_dac_get_status(&dac_hdl);
#endif
    return 0;
}

void app_audio_output_ch_mute(u8 ch, u8 mute)
{
    audio_dac_ch_mute(&dac_hdl, ch, mute);
}

int audio_output_sync_start(void)
{
#if AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC
    audio_dac_sync_start(&dac_hdl);
#endif
    return 0;
}

int audio_output_sync_stop(void)
{
#if AUDIO_OUTPUT_WAY == AUDIO_OUTPUT_WAY_DAC
    audio_dac_sync_stop(&dac_hdl);
#endif
    return 0;
}

