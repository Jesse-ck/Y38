
#ifndef __GMA_INCLUDE__
#define __GMA_INCLUDE__

//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
//#include <stdint.h>

//#include <stddef.h>
#include "btstack/avctp_user.h"
#include "system/task.h"
#include "os/os_api.h"
#include "app_config.h"
#include "app_action.h"
#include "generic/lbuf.h"
#include "user_cfg.h"
#include "btstack/avctp_user.h"
#include "tone_player.h"
#include "key_event_deal.h"
#include "event.h"
#include "key_event_deal.h"
#include "dma_deal.h"
#include "classic/tws_local_media_sync.h"
#include "bt_tws.h"
#include "bt_common.h"
#include "3th_profile_api.h"
#include "system/timer.h"
#include "btstack/frame_queque.h"



typedef unsigned char BYTE, uint8_t;            // 8-bit byte
typedef char          sint8_t;
typedef unsigned int  WORD;             // 32-bit word, change to "long" for 16-bit machines
typedef unsigned int  u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef int 		  sint32_t;

#define GMA_DEBUG    printf
#define GMA_MALLOC()
#define GMA_FREE()

#endif /* __GMA_INCLUDE__ */

