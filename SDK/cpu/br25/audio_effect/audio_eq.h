#ifndef _EQ_API_H_
#define _EQ_API_H_

#include "typedef.h"
// #include "asm/audio_platform.h"
#include "asm/hw_eq.h"

#define EQ_CHANNEL_MAX      2
#define EQ_SECTION_MAX     10
// #define EQ_SR_IDX_MAX       9

/*eq type*/
typedef enum {
    EQ_MODE_NORMAL = 0,
    EQ_MODE_ROCK,
    EQ_MODE_POP,
    EQ_MODE_CLASSIC,
    EQ_MODE_JAZZ,
    EQ_MODE_COUNTRY,
    EQ_MODE_CUSTOM,//自定义
    EQ_MODE_MAX,
} EQ_MODE;

/*eq type*/
typedef enum {
    EQ_TYPE_FILE = 0x01,
    EQ_TYPE_ONLINE,
    EQ_TYPE_MODE_TAB,
} EQ_TYPE;

#define audio_eq_filter_info 	eq_coeff_info

typedef int (*audio_eq_filter_cb)(int sr, struct audio_eq_filter_info *info);

struct audio_eq_param {
    u32 channels : 2;
    u32 online_en : 1;
    u32 mode_en : 1;
    u32 remain_en : 1;
    u32 no_wait : 1;
    u32 max_nsection : 6;
    u32 reserved : 20;
    u32 eq_switch : 1;
    audio_eq_filter_cb cb;

    u32 nsection : 6;
    u32 drc_en: 1;
    u32 eq_name;
};

#ifdef CONFIG_EQ_SUPPORT_ASYNC
struct audio_eq_async {
    u16 ptr;
    u16 len;
    u16 buf_len;
    u16 clear : 1;
    u16 reserved : 15;

    char *buf;
    char *buf_bk;
};
#endif

struct audio_eq {
    // struct hw_eq_ch hw_eq;
    void *eq_ch;
    u32 sr : 16;
    u32 remain_flag : 1;
    u32 updata : 1;
    u32 online_en : 1;
    u32 mode_en : 1;
    u32 remain_en : 1;
    u32 start : 1;
    u32 max_nsection : 6;
    u32 eq_name;

#ifdef CONFIG_EQ_SUPPORT_ASYNC
    u32 async_toggle : 1;
    struct audio_eq_async async[2];
#endif

    audio_eq_filter_cb cb;
    void *output_priv;
    int (*output)(void *priv, void *data, u32 len);
    int (*output_source)(void *priv, void *data, u32 len);
    void *src_data_priv;
};


void audio_eq_init(void);

int audio_eq_open(struct audio_eq *eq, struct audio_eq_param *param);

void audio_eq_set_output_handle(struct audio_eq *eq, int (*output)(void *priv, void *data, u32 len), void *output_priv);

void audio_eq_set_samplerate(struct audio_eq *eq, int sr);
void audio_eq_set_channel(struct audio_eq *eq, u8 channel);

int audio_eq_set_info(struct audio_eq *eq, u8 channels, u8 out_32bit);

int audio_eq_start(struct audio_eq *eq);
int audio_eq_run(struct audio_eq *eq, s16 *data, u32 len);
int audio_eq_close(struct audio_eq *eq);

void audio_eq_flush_out(void);

extern int eq_get_filter_info(int sr, struct audio_eq_filter_info *info);
extern int eq_get_filter_info2(int sr, struct audio_eq_filter_info *info);
extern void eq_app_run_check(struct audio_eq *eq);

int audio_eq_change(struct audio_eq *eq, u8 sw);
int audio_eq_run_inOut(struct audio_eq *eq, s16 *indata, s16 *data, u32 len);

#ifdef CONFIG_EQ_SUPPORT_ASYNC
void audio_eq_async_data_clear(struct audio_eq *eq);
#endif
void audio_eq_set_output_source_handle(struct audio_eq *eq, int (*output)(void *priv, void *data, u32 len), void *output_priv);

#endif

