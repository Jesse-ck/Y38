#ifndef AUDIO_MIXER_H
#define AUDIO_MIXER_H

#include "generic/typedef.h"
#include "generic/list.h"

enum {
    MIXER_EVENT_CH_OPEN,
    MIXER_EVENT_CH_CLOSE,
};


struct audio_mixer;

struct audio_mix_handler {
    int (*mix_probe)(struct audio_mixer *);
    int (*mix_output)(struct audio_mixer *, s16 *, u16);
    int (*mix_post)(struct audio_mixer *);
};

struct audio_mixer {
    struct list_head head;
    s16 *output;
    u16 points;
    u16 remain_points;
    const struct audio_mix_handler *mix_handler;
    void (*evt_handler)(struct audio_mixer *, int);
};

struct audio_mixer_ch {
    u8 start;
    u8 pause;
    u16 offset;
    u16 sample_rate;
    struct list_head entry;
    struct audio_mixer *mixer;
};


int audio_mixer_open(struct audio_mixer *mixer);

void audio_mixer_set_handler(struct audio_mixer *, const struct audio_mix_handler *);

void audio_mixer_set_event_handler(struct audio_mixer *mixer,
                                   void (*handler)(struct audio_mixer *, int));

void audio_mixer_set_output_buf(struct audio_mixer *mixer, s16 *buf, u16 len);

int audio_mixer_get_sample_rate(struct audio_mixer *mixer);

int audio_mixer_get_ch_num(struct audio_mixer *mixer);

int audio_mixer_ch_open(struct audio_mixer_ch *ch, struct audio_mixer *mixer);

void audio_mixer_ch_set_sample_rate(struct audio_mixer_ch *ch, u16 sample_rate);

int audio_mixer_reset(struct audio_mixer_ch *ch, struct audio_mixer *mixer);

int audio_mixer_ch_reset(struct audio_mixer_ch *ch);

int audio_mixer_ch_write(struct audio_mixer_ch *ch, s16 *data, int len);

int audio_mixer_ch_close(struct audio_mixer_ch *ch);

void audio_mixer_ch_pause(struct audio_mixer_ch *ch, u8 pause);

int audio_mixer_get_active_ch_num(struct audio_mixer *mixer);
















#endif

