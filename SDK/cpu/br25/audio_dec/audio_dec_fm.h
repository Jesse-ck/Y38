
#ifndef _AUDIO_DEC_FM_H_
#define _AUDIO_DEC_FM_H_

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "media/audio_decoder.h"

void fm_sample_output_handler(s16 *data, int len);
void fm_dec_relaese();

int fm_dec_start();
int fm_dec_open(u8 source, u32 sample_rate);
void fm_dec_close(void);
int fm_dec_restart(int magic);
int fm_dec_push_restart(void);
void fm_dec_resume(void);
int fm_dec_no_out_sound(void);

/***********************inein pcm enc******************************/
void fm_pcm_enc_stop(void);
int fm_pcm_enc_start(void);
bool fm_pcm_enc_check();

#endif
