
#ifndef _AUDIO_DEC_LOCAL_TWS_H_
#define _AUDIO_DEC_LOCAL_TWS_H_

#include "asm/includes.h"
#include "media/includes.h"
#include "system/includes.h"
#include "media/audio_decoder.h"

u8 is_local_tws_dec_open();

void dec2tws_media_enable(void);
void dec2tws_media_disable(void);

void set_wait_a2dp_start(u8 flag);
int dec2tws_media_set(u8 *data, struct audio_fmt *pfmt);
int dec2tws_media_get(u8 *data, struct audio_fmt *pfmt);
int local_tws_dec_create(void);
void local_tws_drop_frame_start();
void local_tws_drop_frame_stop();

int local_tws_dec_open(u32 value);
int local_tws_dec_close(u8 drop_frame_start);
int local_tws_output(struct audio_fmt *pfmt, s16 *data, int len);
int local_tws_send_end(int tmrout);
int local_tws_send_pause(int tmrout);
void local_tws_decoder_pause(void);
void local_tws_start(struct audio_fmt *pfmt);
void local_tws_stop(void);
//////////////////////////////////////////////////////////////////////////////
void encfile_tws_dec_resume(void);
int encfile_tws_wfile(struct audio_fmt *pfmt, s16 *data, int len);
int encfile_tws_dec_open(struct audio_fmt *pfmt);
void encfile_tws_dec_close();

//////////////////////////////////////////////////////////////////////////////
void dec2tws_master_dec_resume(void);
void dec2tws_dec_restart(void);
int dec2tws_check_enable(void);
int dec2tws_tws_event_deal(struct bt_event *evt);

#endif

