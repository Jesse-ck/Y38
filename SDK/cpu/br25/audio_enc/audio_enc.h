#ifndef _AUDIO_ENC_H_
#define _AUDIO_ENC_H_

#include "generic/typedef.h"
#include "storage_dev/storage_dev.h"
#include "media/includes.h"

int esco_enc_open(u32 coding_type, u8 frame_len);
void esco_enc_close();

enum enc_source {
    ENCODE_SOURCE_MIX = 0x0,
    ENCODE_SOURCE_MIC,
    ENCODE_SOURCE_LINE0_LR,
    ENCODE_SOURCE_LINE1_LR,
    ENCODE_SOURCE_LINE2_LR,
};

u32 record_player_get_encoding_time();
int record_player_is_encoding(void);
void record_player_device_offline_check(char *logo);
int record_player_encode_start(struct audio_fmt *f, struct storage_dev *dev, enum enc_source source, u32 cut_head_time, u32 cut_tail_time, u32 limit_size);
void record_player_encode_stop(void);
void record_player_pcm2file_write_pcm_ex(s16 *data, int len);
int mixer_recorder_encoding(void);
int mixer_recorder_start(void);
void mixer_recorder_stop(void);

//////////////////////////////////////////////////////////////////
#include "audio_enc_file.h"
#include "audio_enc_tws.h"
#include "audio_enc_recoder.h"

#endif/*_AUDIO_ENC_H_*/
