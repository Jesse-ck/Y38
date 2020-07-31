#ifndef _TM_FRAME_MG_H
#define _TM_FRAME_MG_H
#include "typedef.h"
//#include "semlock.h"
#include "tm_gma_type.h"
#include "btstack/frame_queque.h"

typedef struct _send_frame {
    void *buffer;
    _uint32 len;
    struct _send_frame *next;
} _GNU_PACKED_  TM_SEND_FRAME;

typedef struct tm_dma_queque {
    TM_SEND_FRAME *head;
    TM_SEND_FRAME *tail;
    unsigned int depth;
    semlock_t mutex;
} _GNU_PACKED_  TM_QUEQUE;

typedef enum {
    TM_MSG_START_DATA_PROCESS = 1,
    TM_MSG_OTA_DATA_WRITE_TO_FLASH,
    TM_MGS_OTA_CRC16_CHECK,
} TM_MSG;


#if (GMA_OTA_EN)
void tm_ota_data_crc16_resume(void);
void tm_ota_data_writes_resume(void);
int tm_ota_process_register(int (*process)(int msg));
#endif
int tm_frame_push_queque(TM_QUEQUE *queque, TM_SEND_FRAME *frame);
int tm_frame_mg_init(int (*should_send)(_uint16 len), int (*send_data)(uint8_t *buf,  _uint16 len), \
                     int (*send_audio_data)(uint8_t *buf,  _uint16 len), int (*ots_process)(int msg));
void tm_frame_mg_close(void);
void *tm_cmd_malloc_lbuf(void *buf, _uint32 sz);
void *tm_audio_malloc_lbuf(void *buf, _uint32 sz);
int tm_cmd_send_data_to_queue(_uint8 *in_buff, _uint32 len);
int tm_audio_send_data_to_queue(_uint8 *in_buff, _uint32 len);
void tm_send_process_resume(void);
u8 tm_queque_is_busy(void);
#endif
