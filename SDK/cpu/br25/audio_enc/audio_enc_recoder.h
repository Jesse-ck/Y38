
#ifndef _AUDIO_ENC_RECODER_H_
#define _AUDIO_ENC_RECODER_H_

#include "media/audio_encoder.h"


int audio_adc_mic_init(u16 sr);
void audio_adc_mic_exit(void);

void linein_sample_set_resume_handler(void *priv, void (*resume)(void));
void fm_inside_output_handler(void *priv, s16 *data, int len);
int linein_sample_read(void *hdl, void *data, int len);
int linein_sample_size(void *hdl);
int linein_sample_total(void *hdl);
void *linein_sample_open(u8 source, u16 sample_rate);
void linein_sample_close(void *hdl);
void *fm_sample_open(u8 source, u16 sample_rate);
void fm_sample_close(void *hdl, u8 source);

////>>>>>>>>>>>>>>record_player api录音接口<<<<<<<<<<<<<<<<<<<<<///
void record_player_pcm2file_write_pcm_ex(s16 *data, int len);
void record_player_encode_stop(void);
void record_cut_head_timeout(void *priv);
///cut_head_time:单位 1ms
///cut_tail_time:单位 1ms
int record_player_encode_start(struct audio_fmt *f, struct storage_dev *dev, enum enc_source source, u32 cut_head_time, u32 cut_tail_time, u32 limit_size);
u32 record_player_get_encoding_time();
///检查录音是否正在进行
int record_player_is_encoding(void);
void record_player_device_offline_check(char *logo);
int mixer_recorder_encoding(void);
int mixer_recorder_start(void);
void mixer_recorder_stop(void);

#endif

