/*****************************************************************
>file name : audio_config.h
>author : lichao
>create time : Tue 26 Feb 2019 11:56:00 AM CST
*****************************************************************/
#ifndef _AUDIO_CONFIG_H_
#define _AUDIO_CONFIG_H_

#include "app_config.h"

#if TCFG_DEC_WMA_ENABLE
#define AUDIO_FIXED_SIZE        (1024*8)
#else
#define AUDIO_FIXED_SIZE        4096
#endif

/*提示音和电话解码buffer一致*/
#define AUDIO_COMMON_SIZE         240 * 10/*以通话解码输出为单位：max(120,240)*/
#define AUDIO_CALL_SIZE         AUDIO_COMMON_SIZE
#define AUDIO_TONE_SIZE         AUDIO_COMMON_SIZE / 4
#define AUDIO_SBC_TONE_SIZE     AUDIO_COMMON_SIZE
#define AUDIO_MP3_TONE_SIZE     AUDIO_COMMON_SIZE

#define AUDIO_REC_FRAME_SIZE    0
#define AUDIO_REC_DEV_SIZE      (AUDIO_FIXED_SIZE - AUDIO_CALL_SIZE - AUDIO_REC_FRAME_SIZE)

#define MUSIC_PRIORITY          1
#define PHONE_CALL_PRIORITY     2
#define TONE_PRIORITY           3

#define SYS_VOL_TYPE            (1)  //0使用数字音量做系统音量 1：使用模拟音量做系统音量 2:使用联合音量
#define MAX_COM_VOL             (22)    // 具体数值应小于联合音量等级的数组大小 (combined_vol_list)
#define MAX_DIG_VOL             (100)

#ifdef CONFIG_CPU_BR22

#define MAX_ANA_VOL             (25) //模拟的最大音量为31, 这里限制为27级，防止差分推增益太大

#elif (defined CONFIG_CPU_BR23)

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_MONO_LR_DIFF || \
     TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_DUAL_LR_DIFF)
#define MAX_ANA_VOL             (21)
#else
#define MAX_ANA_VOL             (25)
#endif

#elif (defined CONFIG_CPU_BR25)

#if (TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_MONO_LR_DIFF || \
     TCFG_AUDIO_DAC_CONNECT_MODE == DAC_OUTPUT_DUAL_LR_DIFF)
#define MAX_ANA_VOL             (21)
#else
#define MAX_ANA_VOL             (25)
#endif

#else
#define MAX_ANA_VOL             (25)
#endif

#if (SYS_VOL_TYPE == 0)
#define SYS_MAX_VOL             MAX_DIG_VOL
#elif (SYS_VOL_TYPE == 1)
#define SYS_MAX_VOL             MAX_ANA_VOL
#elif (SYS_VOL_TYPE == 2)
#define SYS_MAX_VOL             MAX_COM_VOL
#endif

#define SYS_DEFAULT_VOL         0//(SYS_MAX_VOL/2)
#define SYS_DEFAULT_TONE_VOL    (18)//(SYS_MAX_VOL)

//#define AUDIO_MIC_TEST



extern u8 audio_dma_buffer[AUDIO_FIXED_SIZE];
extern const u16 dig_vol_table[];

#define APP_AUDIO_STATE_WTONE_BY_MUSIC 1//使能改宏，提示音音量使用music音量


#define APP_AUDIO_STATE_IDLE        0
#define APP_AUDIO_STATE_MUSIC       1
#define APP_AUDIO_STATE_CALL        2
#define APP_AUDIO_STATE_WTONE       3
#define APP_AUDIO_CURRENT_STATE     4




extern u8 get_max_sys_vol(void);

void app_audio_output_init(void);
void app_audio_output_sync_buff_init(void *sync_buff, int len);
int app_audio_output_channel_set(u8 output);
int app_audio_output_channel_get(void);
int app_audio_output_mode_set(u8 output);
int app_audio_output_mode_get(void);
int app_audio_output_write(void *buf, int len);
int app_audio_output_samplerate_select(u32 sample_rate, u8 high);
int app_audio_output_samplerate_set(int sample_rate);
int app_audio_output_samplerate_get(void);
int app_audio_output_start(void);
int app_audio_output_stop(void);
int app_audio_output_reset(u32 msecs);
int app_audio_output_state_get(void);
void app_audio_output_ch_mute(u8 ch, u8 mute);
int app_audio_output_ch_analog_gain_set(u8 ch, u8 again);
int app_audio_output_get_cur_buf_points(void);
s8 app_audio_get_volume(u8 state);
void app_audio_set_volume(u8 state, s8 volume, u8 fade);
void app_audio_volume_up(u8 value);
void app_audio_volume_down(u8 value);
void app_audio_state_switch(u8 state, s16 max_volume);
void app_audio_mute(u8 value);
s16 app_audio_get_max_volume(void);
void app_audio_state_switch(u8 state, s16 max_volume);
void app_audio_state_exit(u8 state);
u8 app_audio_get_state(void);
void volume_up_down_direct(s8 value);
void app_audio_set_mix_volume(u8 front_volume, u8 back_volume);
void app_audio_volume_init(void);
void app_audio_set_digital_volume(s16 volume);
void audio_fade_in_fade_out(u8 left_gain, u8 right_gain);
void dac_trim_hook(u8 pos);


void app_set_sys_vol(s16 vol_l, s16  vol_r);
void app_set_max_vol(s16 vol);

u32 phone_call_eq_open();
int eq_mode_sw();
int mic_test_start();
int mic_test_stop();

void dac_power_on(void);
void dac_power_off(void);

#endif
