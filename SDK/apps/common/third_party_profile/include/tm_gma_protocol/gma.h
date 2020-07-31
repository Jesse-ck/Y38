
#ifndef __GMA_H__
#define __GMA_H__

#include "tm_frame_mg.h"
#include "gws_gma_hw.h"

typedef int 		  sint32_t;

typedef struct {
    uint8_t next_msgid: 4;
    uint8_t init_flag: 1;
    uint8_t : 3;
    uint8_t remote_random[16];
    uint8_t ble_key[16];
    uint8_t ble_mac[6];
    uint8_t dev_random[16];
} gma_para_proc_s;

typedef enum {
    START_MIC = 0,
    STOP_MIC,
} mic_status_e;

#define  VOICE_SEND_MAX     150
#define  VOICE_MIC_NUM      1
#define  MIC_NUM            1
#define  REF_NUM            0


sint32_t gma_mic_status_report(mic_status_e status);
sint32_t gma_recv_proc(uint8_t *buf, uint16_t len);
void gma_init(int (*should_send)(_uint16 len), int(*send_data)(_uint8 *buf, _uint16 len), int(*send_audio_data)(_uint8 *buf, _uint16 len));
void gma_exit(void);
sint32_t gma_adpcm_voice_mic_send(short *voice_buf, uint16_t voice_len);
sint32_t gma_opus_voice_mic_send(uint8_t *voice_buf, uint16_t voice_len);
bool gma_connect_success(void);
bool gma_ali_para_check(void);
void gma_send_secret_to_sibling(void);
void gma_sync_remote_addr(u8 *buf);
void gma_kick_license_to_sibling(void);
void gma_send_secret_sync(void);
void gma_slave_sync_remote_addr(u8 *buf);
void ble_mac_reset(void);
void gma_adv_restore(void);
sint32_t gma_data_send(uint8_t *buf, uint8_t len);
sint32_t gma_audio_send(uint8_t *buf, uint8_t len);

#endif /* __GMA_H__ */


