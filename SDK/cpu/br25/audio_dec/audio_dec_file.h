
#ifndef _AUDIO_DEC_FILE_H_
#define _AUDIO_DEC_FILE_H_

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "media/audio_decoder.h"


int file_eq_output2(void *priv, s16 *data, u32 len);
int file_eq_output_async(void *priv, s16 *data, u32 len);
bool file_dec_is_stop(void);
bool file_dec_is_play(void);
bool file_dec_is_pause(void);
int file_dec_pp(void);
int file_dec_FF(int step);
int file_dec_FR(int step);
int file_dec_get_breakpoint(struct audio_dec_breakpoint *bp);
int file_dec_get_total_time(void);
int file_dec_get_cur_time(void);
int file_dec_get_decoder_type(void);
int file_dec_create(void *priv, void (*handler)(void *, int argc, int *argv));
int file_dec_open(void *file, struct audio_dec_breakpoint *bp);
void file_dec_close();
int file_dec_restart(int magic);
int file_dec_push_restart(void);
void file_dec_resume(void);
int file_dec_no_out_sound(void);

#endif /*TCFG_APP_MUSIC_EN*/

