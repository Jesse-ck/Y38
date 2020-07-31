
#ifndef _AUDIO_PITCH_API_H_
#define _AUDIO_PITCH_API_H_
#include "pitchshifter/pitchshifter_api.h"
#include "mono2stereo/reverb_mono2stero_api.h"
#include "audio_config.h"

typedef struct _s_pitch_hdl {

    PITCHSHIFT_FUNC_API *ops;
    RMONO2STEREO_FUNC_API *mono2stereo_ops;
    u8 *databuf;
    PITCH_SHIFT_PARM param;
    void *mono2stereo_buf;
    s16 signal_buf[512];

} s_pitch_hdl;

PITCH_SHIFT_PARM *get_pitch_parm(void);
s_pitch_hdl *open_pitch(PITCH_SHIFT_PARM *param);
void close_pitch(s_pitch_hdl *picth_hdl);
void update_pict_parm(s_pitch_hdl *picth_hdl);
void picth_run(s_pitch_hdl *picth_hdl, s16 *indata, s16 *outdata, int len, u8 ch_num);
#endif

