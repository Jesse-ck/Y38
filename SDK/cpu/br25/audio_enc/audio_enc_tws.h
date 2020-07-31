
#ifndef _AUDIO_ENC_TWS_H_
#define _AUDIO_ENC_TWS_H_

#include "media/audio_encoder.h"

int pcm2tws_enc_output(void *priv, s16 *data, int len);
void pcm2tws_enc_set_resume_handler(void (*resume)(void));
void pcm2tws_enc_set_output_handler(int (*output)(struct audio_fmt *, s16 *, int));

int pcm2tws_sbc_sample_rate_select(int rate);

int pcm2tws_enc_open(struct audio_fmt *pfmt);
void pcm2tws_enc_close();
void pcm2tws_enc_resume(void);

int pcm2tws_enc_reset(void);

#endif





