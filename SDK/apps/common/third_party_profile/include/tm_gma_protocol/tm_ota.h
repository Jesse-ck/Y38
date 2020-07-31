#ifndef _TM_OTA_H
#define _TM_OTA_H
#include "gma_include.h"

typedef struct {
    u8 fm_type;
    u32 fm_version;
} __attribute__((packed)) gma_ota_fm_version_s;

typedef struct {
    u8 fm_type;///default:0
    u32 fm_version;
    u32 fm_size;
    u16 fm_crc16;
    u8 update_notation;///0:update all  1:update partical code
} __attribute__((packed)) gma_update_req_para;

typedef struct {
    u8 fm_update_en;///1:allow   0:disallow
    u32 prev_tran_data_size;
    u8 circle_packet_size;///range:0~15
} __attribute__((packed)) gma_update_rply_para;

typedef struct {
    u8 frameSeq		: 4;
    u8 totalFrame 	: 4;
    u32 last_frame_size;
} __attribute__((packed)) gma_ota_rply_get_fm_data;

int gma_ota_update_rply_para(gma_update_rply_para *para);
int gma_ota_recv_update_request_para(void *buf);
int gma_ota_get_fw_version(gma_ota_fm_version_s *version);
void gma_ota_process(int msg);
void CRC16_CCITT_FALSE_INIT(void);
void gma_ota_update_data_cb(uint8_t *buf, u16 len, sint32_t (*ota_data_request)(uint32_t offset));
uint8_t gma_ota_is_crc16_ok(void);
u32 gma_ota_code_reveive_size_require(void);
#endif
