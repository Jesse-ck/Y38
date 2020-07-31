#ifndef _FM_H_
#define _FM_H_

#include "system/event.h"

void fm_event_to_user(u8 event);

int fm_sys_event_handler(struct sys_event *event);

void app_fm_tone_play_start(u8 mix);


#endif

