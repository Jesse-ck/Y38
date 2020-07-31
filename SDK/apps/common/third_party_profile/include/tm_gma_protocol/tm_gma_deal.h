#ifndef _TM_GMA_DEAL_H
#define _TM_GMA_DEAL_H
#include "tm_gma_type.h"
#include "tm_gma.h"
#include "tm_frame_mg.h"

bool tm_gma_msg_deal(u32 param);
void gma_event_post(u32 type, u8 event);

#endif
