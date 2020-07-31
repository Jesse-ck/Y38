#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "gma_include.h"
#include "gma.h"
#include "sha256.h"
#include "aes.h"
#include "tm_frame_mg.h"
#include "generic/lbuf.h"
#include "tm_gma_hw_driver.h"
#include "init.h"
#include "tm_gma_deal.h"
#include "gma_license.h"


#if (GMA_EN)

#define GMA_DEBUG_LEVEL  9///10:ALL  9:CLOSE SEND BUF


#define ADPCM_CODE_EN
#ifdef ADPCM_CODE_EN
#include "adpcm.h"
#endif

typedef uint8_t (*command_callback)(void *buffer_info);

typedef enum {
    COMMAND_ERROR				= 0X0F,	 //device->app
    COMMAND_AUTH_START_REQ      = 0x10,  //app->device
    COMMAND_AUTH_START_RSP      = 0x11,  //device->app
    COMMAND_AUTH_RESULT_REQ     = 0x12,  //app->device
    COMMAND_AUTH_RESULT_RSP     = 0x13,  //device->app
#if (GMA_OTA_EN)
    COMMAND_OTA_VERSION_REQ		= 0X20,  //app->device
    COMMAND_OTA_VERSION_RSP		= 0X21,  //device->app
    COMMAND_OTA_UPDATE_REQ		= 0X22,  //app->device
    COMMAND_OTA_UPDATE_RSP		= 0X23,  //device->app
    COMMAND_OTA_RCV_DATA_RSP	= 0X24,  //device->app
    COMMAND_OTA_FM_SEND_END		= 0X25,  //app->device
    COMMAND_OTA_FM_CRC_RES		= 0x26,	 //device->app
    COMMAND_OTA_FM_RECV_DATA	= 0X2F,  //app->device
#endif
    COMMAND_MIC_VOICE_SEND      = 0x30,  //device->app
    COMMAND_DEV_INFO_REQ        = 0x32,  //app->device
    COMMAND_DEV_INFO_RSP        = 0x33,  //device->app
    COMMAND_ACTIVE_REQ          = 0x34,  //app->device
    COMMAND_ACTIVE_RSP          = 0x35,  //device->app
    COMMAND_STATUS_SET_REQ      = 0x36,  //app->device
    COMMAND_STATUS_SET_RSP      = 0x37,  //device->app
    COMMAND_A2DP_SET_REQ        = 0x38,  //app->device
    COMMAND_A2DP_SET_RSP        = 0x39,  //device->app
    COMMAND_HFP_SET_REQ         = 0x3a,  //app->device
    COMMAND_HFP_SET_RSP         = 0x3b,  //device->app
    COMMAND_MIC_READY_REQ       = 0x3e,  //device->app
    COMMAND_MIC_READY_RSP       = 0x3f,  //app->device
    COMMAND_DEV_CTRL_REQ		= 0X40,  //app->device
    COMMAND_DEV_CTRL_RSP		= 0x41,  //device->app
    COMMAND_EXCEPTION_NOTICE	= 0x42,  //app->device
    COMMAND_EXCEPTION_RSP		= 0x43,  //app->device
} GMA_CMD_E;

typedef enum {
    CONNCT_READY = 0,
    MIC_START,
    MIC_STOP,
    PLAY_TTS,
    STOP_TTS,
} MIC_STATUS_E;

typedef enum {
    A2DP_STATUS = 0X00,
    A2DP_VOICE  = 0X01,
    A2DP_PLAY   = 0X02,
    A2DP_PAUSE  = 0X03,
    A2DP_PREV   = 0X04,
    A2DP_NEXT   = 0X05,
    A2DP_STOP   = 0X06,
} A2DP_STATUS_E;

typedef enum {
    HFP_STATUS = 0,
    HFP_TEL_NUM,
    HFP_HANDLE,
    HFP_REFUSE,
} HFP_STATUS_E;

typedef enum {
    DEV_BATTERY_VALUE 	= 0x00,
    DEV_BATTERY_STATUS	= 0x01,
    DEV_FM_FRE_SET		= 0x02,
    DEV_FM_FRE_GET		= 0x03,
    DEV_FW_VER			= 0x04,
    DEV_BT_NAME			= 0x05,
    DEV_MIC_STATUS		= 0x06,
} DEV_INFO_E;

typedef enum {
    NOTICE_TCP_UNCONNECT 		= 0x00,
    NOTICE_APP_RECONNECT_SUCC	= 0x01,
} NOTICE_EXCEPTION_E;

#define GMA_PAYLOAD_POS     4
#define GMA_PAYLOAD_HEAD    4
#define GMA_VOICE_HEAD      5

#define SUCCESS     0
#define FAIL        1


typedef struct {
    uint8_t msg_id: 	4;
    uint8_t save_flag: 	1;
    uint8_t version: 	3;
    uint8_t cmd;
    uint8_t fn: 		4;
    uint8_t total_fn: 	4;
    uint8_t f_len;
} __attribute__((packed)) gma_frame_head_s;

typedef struct {
    gma_frame_head_s gma_frame_head;
    uint8_t *recv_data;
}  __attribute__((packed)) gma_recv_data_s;

typedef struct {
    uint8_t recv_cmd_type;
    command_callback cmd_cbk;
} gma_recv_cmd_list_s;

typedef struct {
    uint8_t mobile_type;
    uint16_t gma_ver;
} __attribute__((packed)) gma_mobile_info_s;

typedef struct {
    uint16_t ability;
    uint8_t audio_coding;
    uint16_t gma_ver;
#if GMA_TWS_SUPPORTED
    uint8_t classic_mac[12];
#else
    uint8_t classic_mac[6];
#endif
} __attribute__((packed)) gma_dev_info_s;

typedef struct {
    uint8_t type;
    uint8_t len;
    uint8_t *value;
} __attribute__((packed)) gma_tlv_s;

typedef struct {
    uint8_t random[16];
    uint8_t digest[16];
} __attribute__((packed)) gma_active_s;


static AES_CTX ali_aes_ctx;
static uint8_t iv_temp[16] = "123aqwed#*$!(4ju";

static gma_para_proc_s gma_para_proc;

extern ali_para_s ali_para;
extern volatile ali_para_s *active_ali_para;
#if (GMA_TWS_PAIR_USED_FIXED_MAC)
extern ali_para_s ali_para_remote;
#endif

/*
 *  init
 */
static volatile uint8_t gma_module_init_flg = 0;
#define GMA_MODULE_EN(en)   do{gma_module_init_flg = en;} while(0)
#define GMA_MODULE_IS_EN()  (gma_module_init_flg?  true:false)


/*
 *  hardware application
 */
static const struct __gma_hw_api *_gma_hw_api = &gma_hw_api;
static void gma_hw_api_register(const struct __gma_hw_api *gma_hw_api)
{
    _gma_hw_api = gma_hw_api;
}

/*
 *  common function
 */
///random number function register
void get_random_number(u8 *ptr, u8 len);
typedef void (*__get_random_number)(u8 *ptr, u8 len);
static const __get_random_number _get_random_number = get_random_number;

void gma_get_random_num(u8 *ptr, u8 len)
{
    const uint8_t string_member[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

    _get_random_number(ptr, len);

    for (int i = 0; i < len; i++) {
        ptr[i] = ptr[i]	% (sizeof(string_member) / sizeof(string_member[0]));
        ptr[i] = string_member[ptr[i]];
        putchar(ptr[i]);
    }
}

static void printf_buf(const u8 *buf, int len)
{
    put_buf(buf, len);
}

/********************************************************
 * 					tm memory moudle
 * *****************************************************/
///cmd applications register
typedef int (*__tm_cmd_send_data_to_queue)(_uint8 *in_buff, _uint32 len);
static const __tm_cmd_send_data_to_queue _tm_cmd_send_data_to_queue = tm_cmd_send_data_to_queue;
typedef void *(*__tm_cmd_malloc_lbuf)(void *buf, _uint32 sz);
static const __tm_cmd_malloc_lbuf _tm_cmd_malloc_lbuf = tm_cmd_malloc_lbuf;
///audio applications register
typedef void *(*__tm_audio_malloc_lbuf)(void *buf, _uint32 sz) ;
static const __tm_audio_malloc_lbuf _tm_audio_malloc_lbuf = tm_audio_malloc_lbuf;
typedef int (*__tm_audio_send_data_to_queue)(_uint8 *in_buff, _uint32 len);
static const __tm_audio_send_data_to_queue _tm_audio_send_data_to_queue = tm_audio_send_data_to_queue;

/*--------------- function -----------------*/
sint32_t gma_data_send(uint8_t *buf, uint8_t len)
{
    uint8_t i = 0;
#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("gma send %d: ", len);
    for (i = 0; i < len; i++) {
        GMA_DEBUG("%02x ", buf[i]);
    }
    GMA_DEBUG("\r\n");
#endif
    uint8_t *send_buf = _tm_cmd_malloc_lbuf(NULL, len);
    if (send_buf == NULL) {
        GMA_DEBUG(">>>>>>gma buffer alloc erro \n");
        return (-1);
    }
    memcpy(send_buf, buf, len);
    _tm_cmd_send_data_to_queue(send_buf, len);
    return 0;
}

sint32_t gma_audio_send(uint8_t *buf, uint8_t len)
{
    uint8_t i = 0;
#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("gma send %d: ", len);
    for (i = 0; i < len; i++) {
        GMA_DEBUG("%02x ", buf[i]);
    }
    GMA_DEBUG("\r\n");
#endif
    uint8_t *send_buf = _tm_audio_malloc_lbuf(NULL, len);
    if (send_buf == NULL) {
        GMA_DEBUG(">>>>>>gma buffer alloc erro \n");
        return (-1);
    }
    memcpy(send_buf, buf, len);
    _tm_audio_send_data_to_queue(send_buf, len);
    return 0;
}


/*
 *  define the function gma sdk
 */
static void aes_iv_set(uint8_t *iv_buf)
{
    if (iv_buf == NULL) {
        return;
    }
    memcpy(ali_aes_ctx.iv, iv_buf, 16);
}

static void ali_digest_cal(uint8_t *digest, uint8_t *random, ali_para_s t_ali_para)
{
    static uint8_t spc[80];
    static uint8_t s_addr[13];
    static uint8_t s_key[33];
    static uint8_t s_random[17];
    int i = 0;
    SHA256_CTX sha256_context;

    uint8_t digest_t[34];

    memset(spc, 0, sizeof(spc));
    /*
       memset(s_random, 0, sizeof(s_random));
       for(i = 0; i < 16; i++)
       {
           sprintf(s_random, "%s%02x", s_random,random[i]);
       }*/
    memset(s_random, 0, sizeof(s_random));
    memcpy(s_random, random, 16);

    memset(s_addr, 0, sizeof(s_addr));

    for (i = 0; i < 6; i++) {
        sprintf(s_addr, "%s%02x", s_addr, t_ali_para.mac[5 - i]);
    }

    memset(s_key, 0, sizeof(s_key));
    for (i = 0; i < 16; i++) {
        sprintf(s_key, "%s%02x", s_key, t_ali_para.secret[i]);
    }

    printf("s_addr:%s s_key:%s \n", s_addr, s_key);
//    sprintf(spc, "%s,%08x,%s,%s", s_random, t_ali_para.pid, s_addr, s_key);
    sprintf(&spc[16], ",%08x,%s,%s", t_ali_para.pid, s_addr, s_key);
    memcpy(spc, s_random, 16);

    sint32_t spc_len = 16 + strlen(&spc[16]);

    ali_sha256_init(&sha256_context);
    ali_sha256_update(&sha256_context, (const BYTE *)spc, spc_len/* strlen(spc)*/);
    ali_sha256_final(&sha256_context, digest_t);

    memcpy(digest, digest_t, 16);
#if 1
    GMA_DEBUG(">>>><<<<<key: [%s] %ld\r\n", spc, spc_len/* strlen(spc)*/);
    GMA_DEBUG(">>>><<<<<hash: ");
    for (i = 0; i < 16; i++) {
        GMA_DEBUG("%02X ", (uint8_t)digest[i]);
    }
    GMA_DEBUG("\r\n");
#endif
}

/*
 *  This example for adpcm coder and send it
 */
sint32_t gma_adpcm_voice_mic_send(short *voice_buf, uint16_t voice_len)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s gma_head;
    uint8_t *p_buf = NULL;
    //uint8_t send_buf[VOICE_SEND_MAX + GMA_PAYLOAD_HEAD] = {0};
    static uint8_t msg_id = 0;
    adpcm_state ali_adpcm_state;
    uint8_t payload_len;

    msg_id++;
    if (msg_id >= 15) {
        msg_id = 1;
    }
    gma_head.msg_id = 0;//msg_id;
    gma_head.save_flag = 0;
    gma_head.version = 0;
    gma_head.cmd = COMMAND_MIC_VOICE_SEND;
    gma_head.fn = 0;
    gma_head.total_fn = 0;
    gma_head.f_len = (voice_len / 4 + GMA_VOICE_HEAD); //example for pcm to adpcm
    p_buf = (uint8_t *)(&gma_head);

    payload_len = voice_len / 4;

    uint8_t *send_buf = _tm_audio_malloc_lbuf(NULL, 9 + payload_len);
    if (send_buf == NULL) {
        GMA_DEBUG(">>>>>>gma buffer alloc erro \n");
        return (-1);
    }

    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));

    send_buf[GMA_PAYLOAD_POS] = (REF_NUM << 4) | MIC_NUM;
    send_buf[GMA_PAYLOAD_POS + 1] = payload_len;
    send_buf[GMA_PAYLOAD_POS + 2] = 0x0;
    send_buf[GMA_PAYLOAD_POS + 3] = 0x0; //ref_len
    send_buf[GMA_PAYLOAD_POS + 4] = 0x0;

    adpcm_coder((short *)voice_buf, (char *)&send_buf[GMA_PAYLOAD_POS + GMA_VOICE_HEAD], voice_len / 2, &ali_adpcm_state);

    _tm_audio_send_data_to_queue(send_buf, payload_len + 9);
    return 0;
}

sint32_t gma_opus_voice_mic_send(uint8_t *voice_buf, uint16_t voice_len)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s gma_head;
    uint8_t *p_buf = NULL;
    static uint8_t msg_id = 0;
    adpcm_state ali_adpcm_state;
    uint8_t payload_len;

    msg_id++;
    if (msg_id >= 15) {
        msg_id = 0;
    }
    gma_head.msg_id = 0;//msg_id;
    gma_head.save_flag = 0;
    gma_head.version = 0;
    gma_head.cmd = COMMAND_MIC_VOICE_SEND;
    gma_head.fn = 0;
    gma_head.total_fn = 0;
    gma_head.f_len = (voice_len  + GMA_VOICE_HEAD); //example for pcm to adpcm
    p_buf = (uint8_t *)(&gma_head);

    payload_len = voice_len ;
    uint8_t *send_buf = _tm_audio_malloc_lbuf(NULL, 9 + payload_len);
    if (send_buf == NULL) {
        GMA_DEBUG(">>>>>>gma buffer alloc erro \n");
        return (-1);
    }
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));

    send_buf[GMA_PAYLOAD_POS] = (REF_NUM << 4) | MIC_NUM;
    send_buf[GMA_PAYLOAD_POS + 1] = payload_len;
    send_buf[GMA_PAYLOAD_POS + 2] = 0x0;
    send_buf[GMA_PAYLOAD_POS + 3] = 0x0; //ref_len
    send_buf[GMA_PAYLOAD_POS + 4] = 0x0;

    memcpy(&send_buf[9], voice_buf, payload_len);
    _tm_audio_send_data_to_queue(send_buf, payload_len + 9);

    return 0;
}

/*
 *  dev auth
 */
static volatile uint8_t auth_state = 0;
#define GMA_AUTH_STATE_SET(en) do{auth_state = en;}while(0)
#define GMA_IS_AUTH_SUCCESS() (auth_state? true:false)
static sint32_t gma_reply_auth_start(uint8_t msg_id, uint8_t *payload, uint16_t len)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s reply_rsp_head;
    uint8_t *p_buf = NULL;
    static uint8_t send_buf[32 + GMA_PAYLOAD_HEAD] = {0};

    reply_rsp_head.msg_id = msg_id;
    reply_rsp_head.save_flag = 0;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_AUTH_START_RSP;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
    reply_rsp_head.f_len = len;

    p_buf = (uint8_t *)(&reply_rsp_head);
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));
    memcpy(&send_buf[GMA_PAYLOAD_POS], payload, len);
    return gma_data_send(send_buf, len + GMA_PAYLOAD_POS);
}

static sint32_t gma_recv_auth_start(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    static uint8_t digest_t[16];
    uint8_t random[17] = {0};
    static uint8_t en_output[32], i;
    int en_len;

    GMA_DEBUG("GMA SDK 1.0 auth_start->recv len %d\r\n", gma_recv_data->gma_frame_head.f_len);
    memcpy(gma_para_proc.remote_random, gma_recv_data->recv_data, gma_recv_data->gma_frame_head.f_len);
    ali_digest_cal(digest_t, gma_para_proc.remote_random, *active_ali_para);
    for (i = 0; i < 16; i++) {
        GMA_DEBUG("0x%02x ", digest_t[i]);
    }
    GMA_DEBUG("\r\n");
    memcpy(gma_para_proc.ble_key, digest_t, 16);
    aes_iv_set(iv_temp);
    aes_cbc_encrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, gma_para_proc.remote_random, 16, en_output, &en_len);

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("en_len %d: ", en_len);
    for (i = 0; i < en_len; i++) {
        GMA_DEBUG("%02x ", en_output[i]);
    }
    GMA_DEBUG("\r\n");
#endif

    en_len = 16;
    return gma_reply_auth_start(gma_recv_data->gma_frame_head.msg_id, en_output, en_len);
}

static sint32_t gma_reply_auth_result(uint8_t msg_id, uint8_t data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s reply_rsp_head;
    uint8_t *p_buf = NULL;
    uint8_t send_buf[1 + GMA_PAYLOAD_HEAD] = {0};

    reply_rsp_head.msg_id = msg_id;
    reply_rsp_head.save_flag = 0;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_AUTH_RESULT_RSP;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
    reply_rsp_head.f_len = 1;

    p_buf = (uint8_t *)(&reply_rsp_head);
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));
    send_buf[GMA_PAYLOAD_POS] = data;
    return gma_data_send(send_buf, 1 + GMA_PAYLOAD_POS);
}

static sint32_t gma_recv_auth_result(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    GMA_DEBUG("auth_result->recv len %d\r\n", gma_recv_data->gma_frame_head.f_len);
    if (SUCCESS == gma_recv_data->recv_data[0]) {
        //todo:success
        GMA_AUTH_STATE_SET(1);
        GMA_DEBUG("auth success\r\n");
    } else if (FAIL == gma_recv_data->recv_data[0]) {
        //todo:fail
        GMA_AUTH_STATE_SET(0);
        GMA_DEBUG("auth fail\r\n");
    } else {
        GMA_AUTH_STATE_SET(0);
        GMA_DEBUG("unidentify\r\n");
        return (-1);
    }
    return gma_reply_auth_result(gma_recv_data->gma_frame_head.msg_id, gma_recv_data->recv_data[0]);
}

#if (GMA_OTA_EN)
#include "tm_ota.h"
/*************************ota**************************/
///check version
static sint32_t gma_reply_ota_version(uint8_t msg_id)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s reply_rsp_head;
    gma_ota_fm_version_s ota_fm_version;
    uint8_t *p_buf;
    uint8_t payload[10], i;
    uint8_t send_buf[16 + GMA_PAYLOAD_HEAD] = {0};
    int en_len = 0;

    reply_rsp_head.msg_id = msg_id;
    reply_rsp_head.save_flag = 1;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_OTA_VERSION_RSP;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
    reply_rsp_head.f_len = 16;

    p_buf = (uint8_t *)(&reply_rsp_head);
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));
    gma_ota_get_fw_version(&ota_fm_version);
    printf(">>>>>>>fm version:%x \n", ota_fm_version.fm_version);
    p_buf = (uint8_t *)(&ota_fm_version);
    memcpy(payload, p_buf, sizeof(gma_ota_fm_version_s));
    aes_iv_set(iv_temp);
    aes_cbc_encrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, payload, sizeof(gma_ota_fm_version_s), &send_buf[GMA_PAYLOAD_POS], &en_len);

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("en_len %d: ", en_len);
    for (i = 0; i < en_len; i++) {
        GMA_DEBUG("%02x ", send_buf[GMA_PAYLOAD_POS + i]);
    }
    GMA_DEBUG("\r\n");
#endif

    return gma_audio_send(send_buf, en_len + GMA_PAYLOAD_POS);
    return gma_data_send(send_buf, en_len + GMA_PAYLOAD_POS);
}

static sint32_t gma_recv_ota_fm_version(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    uint8_t de_out[18], i;
    int de_len;

    GMA_DEBUG("ota fm version check recv packet len %d\r\n", gma_recv_data->gma_frame_head.f_len);
#if (GMA_DEBUG_LEVEL >= 10)
    for (i = 0; i < gma_recv_data->gma_frame_head.f_len; i++) {
        GMA_DEBUG("%02x ", gma_recv_data->recv_data[i]);
    }
    GMA_DEBUG("\r\n");
#endif
    memset(&de_out[16], 0xaa, 2);
    aes_iv_set(iv_temp);
    aes_cbc_decrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, gma_recv_data->recv_data, \
                          gma_recv_data->gma_frame_head.f_len, de_out, &de_len);


    if (de_out[16] != 0xaa && de_out[17] != 0xaa) {
        GMA_DEBUG("memory error func:%s line:%d \n", __func__, __LINE__);
        while (1);
    }

    GMA_DEBUG("len:%d \r\n", de_len);

    put_buf(de_out, de_len);

    return gma_reply_ota_version(gma_recv_data->gma_frame_head.msg_id);
}
///update request
static sint32_t gma_reply_ota_update_para(uint8_t msg_id)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s reply_rsp_head;
    gma_update_rply_para update_rply_para;
    uint8_t *p_buf;
    uint8_t payload[10], i;
    uint8_t send_buf[16 + GMA_PAYLOAD_HEAD] = {0};
    int en_len = 0;

    reply_rsp_head.msg_id = msg_id;
    reply_rsp_head.save_flag = 1;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_OTA_UPDATE_RSP;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
    reply_rsp_head.f_len = 16;

    p_buf = (uint8_t *)(&reply_rsp_head);
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));
    gma_ota_update_rply_para(&update_rply_para);
    p_buf = (uint8_t *)(&update_rply_para);
    memcpy(payload, p_buf, sizeof(gma_update_rply_para));
    aes_iv_set(iv_temp);
    aes_cbc_encrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, payload, sizeof(gma_update_rply_para), &send_buf[GMA_PAYLOAD_POS], &en_len);

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("en_len %d: ", en_len);
    for (i = 0; i < en_len; i++) {
        GMA_DEBUG("%02x ", send_buf[GMA_PAYLOAD_POS + i]);
    }
    GMA_DEBUG("\r\n");
#endif

    return gma_audio_send(send_buf, en_len + GMA_PAYLOAD_POS);
    return gma_data_send(send_buf, en_len + GMA_PAYLOAD_POS);
}

static sint32_t gma_recv_ota_req_para(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    uint8_t de_out[18], i;
    int de_len;

    GMA_DEBUG("ota fm version check recv packet len %d\r\n", gma_recv_data->gma_frame_head.f_len);
#if (GMA_DEBUG_LEVEL >= 10)
    for (i = 0; i < gma_recv_data->gma_frame_head.f_len; i++) {
        GMA_DEBUG("%02x ", gma_recv_data->recv_data[i]);
    }
    GMA_DEBUG("\r\n");
#endif
    memset(&de_out[16], 0xaa, 2);
    aes_iv_set(iv_temp);
    aes_cbc_decrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, gma_recv_data->recv_data, \
                          gma_recv_data->gma_frame_head.f_len, de_out, &de_len);


    if (de_out[16] != 0xaa && de_out[17] != 0xaa) {
        GMA_DEBUG("memory error func:%s line:%d \n", __func__, __LINE__);
        while (1);
    }

    GMA_DEBUG("len:%d \r\n", de_len);

    put_buf(de_out, de_len);
    gma_ota_recv_update_request_para(de_out);

    return gma_reply_ota_update_para(gma_recv_data->gma_frame_head.msg_id);
}
///frame receive
static u8 ota_data_msg_id = 0;
static sint32_t gma_reply_ota_data(uint8_t msg_id)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    ota_data_msg_id = msg_id;
    return 0;
}

static sint32_t gma_ota_data_request(uint32_t last_frame_size)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    GMA_DEBUG(">>>>>>>>>>>>>last_frame_size:%x \n", last_frame_size);
    gma_frame_head_s reply_rsp_head;
    gma_ota_rply_get_fm_data fm_frame_data_rply;
    uint8_t *p_buf;
    static uint8_t payload[10], i;
    static uint8_t send_buf[16 + GMA_PAYLOAD_HEAD] = {0};
    int en_len = 0;

    reply_rsp_head.msg_id = ota_data_msg_id;
    reply_rsp_head.save_flag = 1;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_OTA_RCV_DATA_RSP;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
    reply_rsp_head.f_len = 16;

    p_buf = (uint8_t *)(&reply_rsp_head);
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));
    fm_frame_data_rply.totalFrame = 0;
    fm_frame_data_rply.frameSeq = 0;
    fm_frame_data_rply.last_frame_size = last_frame_size;
    p_buf = (uint8_t *)(&fm_frame_data_rply);
    memcpy(payload, p_buf, sizeof(gma_ota_rply_get_fm_data));
    aes_iv_set(iv_temp);
    aes_cbc_encrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, payload, sizeof(gma_ota_rply_get_fm_data), &send_buf[GMA_PAYLOAD_POS], &en_len);

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("en_len %d: ", en_len);
    for (i = 0; i < en_len; i++) {
        GMA_DEBUG("%02x ", send_buf[GMA_PAYLOAD_POS + i]);
    }
    GMA_DEBUG("\r\n");
#endif

    return gma_audio_send(send_buf, en_len + GMA_PAYLOAD_POS);
    return gma_data_send(send_buf, en_len + GMA_PAYLOAD_POS);
}

static sint32_t gma_recv_ota_data(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }

    if (gma_reply_ota_data(gma_recv_data->gma_frame_head.msg_id) == 0) {
        gma_ota_update_data_cb(gma_recv_data->recv_data, gma_recv_data->gma_frame_head.f_len, gma_ota_data_request);
    }

    return 0;
}
///firmware crc16 check
static sint32_t gma_reply_ota_crc16(uint8_t msg_id)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s reply_rsp_head;
    uint8_t crc16_res = gma_ota_is_crc16_ok();
    uint8_t *p_buf;
    uint8_t payload[10], i;
    uint8_t send_buf[16 + GMA_PAYLOAD_HEAD] = {0};
    int en_len = 0;

    reply_rsp_head.msg_id = msg_id;
    reply_rsp_head.save_flag = 1;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_OTA_FM_CRC_RES;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
    reply_rsp_head.f_len = 16;

    p_buf = (uint8_t *)(&reply_rsp_head);
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));
    p_buf = (uint8_t *)(&crc16_res);
    memcpy(payload, p_buf, sizeof(uint8_t));
    aes_iv_set(iv_temp);
    aes_cbc_encrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, payload, sizeof(uint8_t), &send_buf[GMA_PAYLOAD_POS], &en_len);

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("en_len %d: ", en_len);
    for (i = 0; i < en_len; i++) {
        GMA_DEBUG("%02x ", send_buf[GMA_PAYLOAD_POS + i]);
    }
    GMA_DEBUG("\r\n");
#endif

    return gma_data_send(send_buf, en_len + GMA_PAYLOAD_POS);
}

static sint32_t gma_recv_ota_fm_code_check(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    uint8_t de_out[18], i;
    int de_len;

    GMA_DEBUG("ota fm version check recv packet len %d\r\n", gma_recv_data->gma_frame_head.f_len);
#if (GMA_DEBUG_LEVEL >= 10)
    for (i = 0; i < gma_recv_data->gma_frame_head.f_len; i++) {
        GMA_DEBUG("%02x ", gma_recv_data->recv_data[i]);
    }
    GMA_DEBUG("\r\n");
#endif
    memset(&de_out[16], 0xaa, 2);
    aes_iv_set(iv_temp);
    aes_cbc_decrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, gma_recv_data->recv_data, \
                          gma_recv_data->gma_frame_head.f_len, de_out, &de_len);


    if (de_out[16] != 0xaa && de_out[17] != 0xaa) {
        GMA_DEBUG("memory error func:%s line:%d \n", __func__, __LINE__);
        while (1);
    }

    GMA_DEBUG("len:%d \r\n", de_len);

    put_buf(de_out, de_len);

    return gma_reply_ota_crc16(gma_recv_data->gma_frame_head.msg_id);
}

/*************************ota**************************/
#endif

static sint32_t gma_reply_dev_info(uint8_t msg_id, uint8_t *data, uint8_t len)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s reply_rsp_head;
    gma_dev_info_s dev_info;
    uint8_t *p_buf;
    uint8_t payload[20], i;
    uint8_t send_buf[32 + GMA_PAYLOAD_HEAD] = {0};
    int en_len = 0;

    reply_rsp_head.msg_id = msg_id;
    reply_rsp_head.save_flag = 1;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_DEV_INFO_RSP;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
#if GMA_TWS_SUPPORTED
    reply_rsp_head.f_len = 32;
#else
    reply_rsp_head.f_len = 16;
#endif

    p_buf = (uint8_t *)(&reply_rsp_head);
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));
    dev_info.ability = _gma_hw_api->get_fw_abilities();
    dev_info.audio_coding = _gma_hw_api->get_enc_type();
    dev_info.gma_ver = 0x0001;
#if GMA_TWS_SUPPORTED
    memcpy(&dev_info.classic_mac[0], gma_para_proc.ble_mac, 6);
    GMA_DEBUG(">>>>>>MAC HOST:");
    put_buf(&dev_info.classic_mac[0], 6);

//	memcpy(&dev_info.classic_mac[6], gma_para_proc.ble_mac, 6);
    gma_sibling_mac_get(&(dev_info.classic_mac[6]));
    GMA_DEBUG(">>>>>>MAC SIBLING:");
    put_buf(&dev_info.classic_mac[6], 6);
#else
    memcpy(&dev_info.classic_mac[0], gma_para_proc.ble_mac, 6);
    GMA_DEBUG(">>>>>>MAC HOST:");
    put_buf(&dev_info.classic_mac[0], 6);
#endif
    p_buf = (uint8_t *)(&dev_info);
    memcpy(payload, p_buf, sizeof(gma_dev_info_s));
    aes_iv_set(iv_temp);
    aes_cbc_encrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, payload, sizeof(gma_dev_info_s), &send_buf[GMA_PAYLOAD_POS], &en_len);

    GMA_DEBUG("en_len %d: ", en_len);
#if (GMA_DEBUG_LEVEL >= 10)
    for (i = 0; i < en_len; i++) {
        GMA_DEBUG("%02x ", send_buf[GMA_PAYLOAD_POS + i]);
    }
    GMA_DEBUG("\r\n");
#endif

    return gma_data_send(send_buf, en_len + GMA_PAYLOAD_POS);
}

/*
 *  dev information
 */
static sint32_t gma_recv_dev_info(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_mobile_info_s *mobile_info;
    uint8_t de_out[18], i;
    int de_len;

    GMA_DEBUG("dev_info->recv len %d\r\n", gma_recv_data->gma_frame_head.f_len);
#if (GMA_DEBUG_LEVEL >= 10)
    for (i = 0; i < gma_recv_data->gma_frame_head.f_len; i++) {
        GMA_DEBUG("%02x ", gma_recv_data->recv_data[i]);
    }
    GMA_DEBUG("\r\n");
#endif
    memset(&de_out[16], 0xaa, 2);
    aes_iv_set(iv_temp);
    aes_cbc_decrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, gma_recv_data->recv_data, \
                          gma_recv_data->gma_frame_head.f_len, de_out, &de_len);


    if (de_out[16] != 0xaa && de_out[17] != 0xaa) {
        GMA_DEBUG("memory error func:%s line:%d \n", __func__, __LINE__);
        while (1);
    }

    GMA_DEBUG("len:%d \r\n", de_len);

    mobile_info = (gma_mobile_info_s *)de_out;
    if (0 == mobile_info->mobile_type) {
        GMA_DEBUG("android\r\n");
    } else {
        GMA_DEBUG("ios\r\n");
    }
    return gma_reply_dev_info(gma_recv_data->gma_frame_head.msg_id, NULL, 0);
}

static sint32_t gma_reply_active(uint8_t msg_id)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s reply_rsp_head;
    gma_active_s active_value;
    uint8_t *p_buf, i;
    static uint8_t send_buf[48 + GMA_PAYLOAD_HEAD];
    int en_len;

    reply_rsp_head.msg_id = msg_id;
    reply_rsp_head.save_flag = 1;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_ACTIVE_RSP;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
    reply_rsp_head.f_len = 48;
    p_buf = (uint8_t *)&reply_rsp_head;
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));

    memcpy(active_value.random, gma_para_proc.dev_random, 16);
    ali_digest_cal(active_value.digest, gma_para_proc.dev_random, *active_ali_para);
    p_buf = (uint8_t *)&active_value;
    aes_iv_set(iv_temp);
    aes_cbc_encrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, p_buf, sizeof(gma_active_s), &send_buf[GMA_PAYLOAD_POS], &en_len);

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("en_len %d: ", en_len);
    for (i = 0; i < en_len; i++) {
        GMA_DEBUG("%02x ", send_buf[GMA_PAYLOAD_POS + i]);
    }
    GMA_DEBUG("\r\n");
#endif

    return gma_data_send(send_buf, en_len + GMA_PAYLOAD_POS);
}

/*
 *  active  moudule
 */
static sint32_t gma_recv_active(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    GMA_DEBUG("active->recv len %d\r\n", gma_recv_data->gma_frame_head.f_len);
    return gma_reply_active(gma_recv_data->gma_frame_head.msg_id);
}

static sint32_t gma_reply_status_set(uint8_t msg_id, gma_tlv_s para_tlv)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s reply_rsp_head;
    gma_active_s active_value;
    uint8_t *p_buf, i;
    uint8_t send_buf[16 + GMA_PAYLOAD_HEAD];
    int en_len;

    reply_rsp_head.msg_id = msg_id;
    reply_rsp_head.save_flag = 1;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_STATUS_SET_RSP;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
    reply_rsp_head.f_len = 16;
    p_buf = (uint8_t *)&reply_rsp_head;
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));

    p_buf = (uint8_t *)(&para_tlv);
    aes_iv_set(iv_temp);

    u8 send_buf_temp[3];
    send_buf_temp[0] = para_tlv.type;
    send_buf_temp[1] = para_tlv.len;
    send_buf_temp[2] = (para_tlv.value)[0];
    printf_buf(send_buf_temp, 3);
    aes_cbc_encrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, send_buf_temp, 3, &send_buf[GMA_PAYLOAD_POS], &en_len);

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("en_len %d: ", en_len);
    for (i = 0; i < en_len; i++) {
        GMA_DEBUG("%02x ", send_buf[GMA_PAYLOAD_POS + i]);
    }
    GMA_DEBUG("\r\n");
#endif

    return gma_data_send(send_buf, en_len + GMA_PAYLOAD_POS);
}

/*
 *  status notice
 */
static sint32_t gma_recv_status_set(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    uint8_t de_out[18], i;
    int de_len;
    uint8_t result_value;
    gma_tlv_s gma_tlv;

    GMA_DEBUG("status set->recv len %d\r\n", gma_recv_data->gma_frame_head.f_len);
#if (GMA_DEBUG_LEVEL >= 10)
    for (i = 0; i < gma_recv_data->gma_frame_head.f_len; i++) {
        GMA_DEBUG("%02x ", gma_recv_data->recv_data[i]);
    }
    GMA_DEBUG("\r\n");
#endif
    memset(&de_out[16], 0xaa, 2);
    aes_iv_set(iv_temp);
    aes_cbc_decrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, gma_recv_data->recv_data, \
                          gma_recv_data->gma_frame_head.f_len, de_out, &de_len);


    if (de_out[16] != 0xaa && de_out[17] != 0xaa) {
        GMA_DEBUG("memory error func:%s line:%d \n", __func__, __LINE__);
        while (1);
    }

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("len:%d ", de_len);
    for (i = 0; i < de_len; i++) {
        GMA_DEBUG("%02x ", de_out[i]);
    }
    GMA_DEBUG("\r\n");
#endif

    _gma_hw_api->app_notify_state(de_out[0]);
    switch (de_out[0]) {
    case CONNCT_READY:
        result_value = 0;
        gma_tlv.type = CONNCT_READY;
        gma_tlv.value = &result_value;
        GMA_DEBUG("ready\r\n");
        break;

    case MIC_START:
        GMA_DEBUG("mic start\r\n");
        _gma_hw_api->start_speech();
        result_value = 0;
        gma_tlv.type = MIC_START;
        gma_tlv.value = &result_value;
        break;

    case MIC_STOP:
        GMA_DEBUG("mic stop\r\n");
        _gma_hw_api->stop_speech();
        result_value = 0;
        gma_tlv.type = MIC_STOP;
        gma_tlv.value = &result_value;
        break;

    case PLAY_TTS:
        GMA_DEBUG("play tts\r\n");
        result_value = 0;
        gma_tlv.type = PLAY_TTS;
        gma_tlv.value = &result_value;
        break;

    case STOP_TTS:
        GMA_DEBUG("stop tts\r\n");
        result_value = 0;
        gma_tlv.type = STOP_TTS;
        gma_tlv.value = &result_value;
        break;

    default:
        break;
    }
    gma_tlv.len = 1;
    return gma_reply_status_set(gma_recv_data->gma_frame_head.msg_id, gma_tlv);
}

/*
 *  a2dp module
 */
static sint32_t gma_reply_a2dp_set(uint8_t msg_id, gma_tlv_s para_tlv)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s reply_rsp_head;
    gma_active_s active_value;
    uint8_t *p_buf, i;
    uint8_t send_buf[16 + GMA_PAYLOAD_HEAD];
    int en_len;

    reply_rsp_head.msg_id = msg_id;
    reply_rsp_head.save_flag = 1;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_A2DP_SET_RSP;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
    reply_rsp_head.f_len = 16;
    p_buf = (uint8_t *)&reply_rsp_head;
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));

    p_buf = (uint8_t *)(&para_tlv);
    aes_iv_set(iv_temp);

    u8 send_buf_temp[3];
    send_buf_temp[0] = para_tlv.type;
    send_buf_temp[1] = para_tlv.len;
    send_buf_temp[2] = (para_tlv.value)[0];
    printf_buf(send_buf_temp, 3);
    aes_cbc_encrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, send_buf_temp, 3, &send_buf[GMA_PAYLOAD_POS], &en_len);

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("en_len %d: ", en_len);
    for (i = 0; i < en_len; i++) {
        GMA_DEBUG("%02x ", send_buf[GMA_PAYLOAD_POS + i]);
    }
    GMA_DEBUG("\r\n");
#endif

    return gma_data_send(send_buf, en_len + GMA_PAYLOAD_POS);
}

static sint32_t gma_recv_a2dp_set(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    uint8_t de_out[34], i;
    int de_len;
    uint8_t result_value;
    gma_tlv_s gma_tlv;

    GMA_DEBUG("status set->recv len %d\r\n", gma_recv_data->gma_frame_head.f_len);
#if (GMA_DEBUG_LEVEL >= 10)
    for (i = 0; i < gma_recv_data->gma_frame_head.f_len; i++) {
        GMA_DEBUG("%02x ", gma_recv_data->recv_data[i]);
    }
    GMA_DEBUG("\r\n");
#endif

    memset(&de_out[32], 0xaa, 2);
    aes_iv_set(iv_temp);
    aes_cbc_decrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, gma_recv_data->recv_data, \
                          gma_recv_data->gma_frame_head.f_len, de_out, &de_len);


    if (de_out[32] != 0xaa && de_out[33] != 0xaa) {
        GMA_DEBUG("memory error func:%s line:%d \n", __func__, __LINE__);
        while (1);
    }

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("len:%d ", de_len);
    for (i = 0; i < de_len; i++) {
        GMA_DEBUG("%02x ", de_out[i]);
    }
    GMA_DEBUG("\r\n");
#endif
    struct _gma_app_audio_ctl info;
    info.type = de_out[0];
    result_value = _gma_hw_api->audio_state(info);
    gma_tlv.type = de_out[0];
    gma_tlv.value = &result_value;
    gma_tlv.len = 1;
    return gma_reply_a2dp_set(gma_recv_data->gma_frame_head.msg_id, gma_tlv);
}

static sint32_t gma_reply_hfp_set(uint8_t msg_id, gma_tlv_s para_tlv)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s reply_rsp_head;
    gma_active_s active_value;
    uint8_t *p_buf, i;
    uint8_t send_buf[16 + GMA_PAYLOAD_HEAD];
    int en_len;

    reply_rsp_head.msg_id = msg_id;
    reply_rsp_head.save_flag = 1;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_HFP_SET_RSP;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
    reply_rsp_head.f_len = 16;
    p_buf = (uint8_t *)&reply_rsp_head;
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));

    p_buf = (uint8_t *)(&para_tlv);
    aes_iv_set(iv_temp);

    u8 send_buf_temp[3];
    send_buf_temp[0] = para_tlv.type;
    send_buf_temp[1] = para_tlv.len;
    send_buf_temp[2] = (para_tlv.value)[0];
    printf_buf(send_buf_temp, 3);
    aes_cbc_encrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, send_buf_temp, 3, &send_buf[GMA_PAYLOAD_POS], &en_len);

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("en_len %d: ", en_len);
    for (i = 0; i < en_len; i++) {
        GMA_DEBUG("%02x ", send_buf[GMA_PAYLOAD_POS + i]);
    }
    GMA_DEBUG("\r\n");
#endif

    return gma_data_send(send_buf, en_len + GMA_PAYLOAD_POS);
}

/*
 *  hfp module
 */
static sint32_t gma_recv_hfp_set(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    uint8_t de_out[34], i;
    int de_len;
    uint8_t result_value;
    gma_tlv_s gma_tlv;

    GMA_DEBUG("status set->recv len %d\r\n", gma_recv_data->gma_frame_head.f_len);
#if (GMA_DEBUG_LEVEL >= 10)
    for (i = 0; i < gma_recv_data->gma_frame_head.f_len; i++) {
        GMA_DEBUG("%02x ", gma_recv_data->recv_data[i]);
    }
    GMA_DEBUG("\r\n");
#endif
    memset(&de_out[32], 0xaa, 2);
    aes_iv_set(iv_temp);
    aes_cbc_decrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, gma_recv_data->recv_data, \
                          gma_recv_data->gma_frame_head.f_len, de_out, &de_len);

    if (de_out[32] != 0xaa && de_out[33] != 0xaa) {
        GMA_DEBUG("memory error func:%s line:%d \n", __func__, __LINE__);
        while (1);
    }

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("len:%d ", de_len);
    for (i = 0; i < de_len; i++) {
        GMA_DEBUG("%02x ", de_out[i]);
    }
    GMA_DEBUG("\r\n");
#endif

    struct _gma_app_HFP_ctl info;
    info.type = de_out[0];
    info.length = de_out[1];
    if (info.length && (info.length < sizeof(info.phone_nums))) {
        memcpy(info.phone_nums, &de_out[2], info.length);
    }

    result_value = _gma_hw_api->get_HFP_state(info);
    gma_tlv.type = de_out[0];
    gma_tlv.value = &result_value;
    gma_tlv.len = 1;
    return gma_reply_hfp_set(gma_recv_data->gma_frame_head.msg_id, gma_tlv);
}

sint32_t gma_mic_status_report(mic_status_e status)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s reply_rsp_head;
    uint8_t *p_buf, i, data[2];
    uint8_t send_buf[16 + GMA_PAYLOAD_HEAD];
    int en_len;

    reply_rsp_head.msg_id = gma_para_proc.next_msgid;
    reply_rsp_head.save_flag = 1;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_MIC_READY_REQ;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
    reply_rsp_head.f_len = 16;
    p_buf = (uint8_t *)&reply_rsp_head;
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));

    data[0] = status;
    data[1] = 0;
    aes_iv_set(iv_temp);
    aes_cbc_encrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, data, 2, &send_buf[GMA_PAYLOAD_POS], &en_len);

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("en_len %d: ", en_len);
    for (i = 0; i < en_len; i++) {
        GMA_DEBUG("%02x ", send_buf[GMA_PAYLOAD_POS + i]);
    }
    GMA_DEBUG("\r\n");
#endif

    return gma_data_send(send_buf, en_len + GMA_PAYLOAD_POS);
}

/*
 *  prase mic ready status
 */
static sint32_t gma_recv_mic_ready(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    uint8_t de_out[18], i;
    int de_len;

    GMA_DEBUG("status set->recv len %d\r\n", gma_recv_data->gma_frame_head.f_len);
#if (GMA_DEBUG_LEVEL >= 10)
    for (i = 0; i < gma_recv_data->gma_frame_head.f_len; i++) {
        GMA_DEBUG("%02x ", gma_recv_data->recv_data[i]);
    }
    GMA_DEBUG("\r\n");
#endif

    memset(&de_out[16], 0xaa, 2);
    aes_iv_set(iv_temp);
    aes_cbc_decrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, gma_recv_data->recv_data, \
                          gma_recv_data->gma_frame_head.f_len, de_out, &de_len);

    if (de_out[16] != 0xaa && de_out[17] != 0xaa) {
        GMA_DEBUG("memory error func:%s line:%d \n", __func__, __LINE__);
        while (1);
    }

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("len:%d ", de_len);
    for (i = 0; i < de_len; i++) {
        GMA_DEBUG("%02x ", de_out[i]);
    }
    GMA_DEBUG("\r\n");
#endif
    if (0 == de_out[0]) {
        if ((1 == de_out[1]) && (SUCCESS == de_out[2])) {
            GMA_DEBUG("start sucess\r\n");
        } else if ((1 == de_out[1]) && (FAIL == de_out[2])) {
            GMA_DEBUG("start fail\r\n");
        }
    } else if (1 == de_out[0]) {
        if ((1 == de_out[1]) && (SUCCESS == de_out[2])) {
            GMA_DEBUG("stop sucess\r\n");
        } else if ((1 == de_out[1]) && (FAIL == de_out[2])) {
            GMA_DEBUG("stop fail\r\n");
        }
    }

    return 0;
}

/*
 *  dev ctrl
 */
//static sint32_t gma_reply_dev_ctrl(uint8_t msg_id, gma_tlv_s para_tlv)
static sint32_t gma_reply_dev_ctrl(uint8_t msg_id, u8 *payload, int len)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s reply_rsp_head;
    gma_active_s active_value;
    uint8_t *p_buf, i;
    uint8_t send_buf[48 + GMA_PAYLOAD_HEAD];
    int en_len;
#if (GMA_OTA_EN)
    gma_ota_fm_version_s ota_fm_version;
    gma_ota_get_fw_version(&ota_fm_version);
    int fireware_ver = ota_fm_version.fm_version;
#else
    int fireware_ver = 0x00000000;
#endif

    reply_rsp_head.msg_id = msg_id;
    reply_rsp_head.save_flag = 1;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_DEV_CTRL_RSP;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
//    reply_rsp_head.f_len = 16;
//    p_buf = (uint8_t *)&reply_rsp_head;
//    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));

    aes_iv_set(iv_temp);

    static u8 send_buf_temp_object[48];
    u8 *send_buf_temp = send_buf_temp_object;
    int tlv_send_len = 0;
    for (gma_tlv_s *para_tlv = (gma_tlv_s *)payload; \
         len >= 0; \
         payload += (2 + para_tlv->len), para_tlv = (gma_tlv_s *)payload, len -= 2, len -= para_tlv->len) {
//	gma_tlv_s *para_tlv = (gma_tlv_s *)payload;

        send_buf_temp[0] = para_tlv->type;
        switch (para_tlv->type) {
        case DEV_BATTERY_VALUE:
            GMA_DEBUG("request battery value \n");
#if  GMA_TWS_SUPPORTED
            //len
            send_buf_temp[1] = 4;
            ///value
            send_buf_temp[2] = _gma_hw_api->get_battery_value();//battery range:0x00 ~ 0x64
            send_buf_temp[3] = _gma_hw_api->low_power_state();//0x00:normal  0x01:low power
            ///value
            send_buf_temp[4] = _gma_hw_api->get_battery_value();//battery range:0x00 ~ 0x64
            send_buf_temp[5] = _gma_hw_api->low_power_state();//0x00:normal  0x01:low power
#else
            //len
            send_buf_temp[1] = 2;
            ///value
            send_buf_temp[2] = _gma_hw_api->get_battery_value();//battery range:0x00 ~ 0x64
            send_buf_temp[3] = _gma_hw_api->low_power_state();//0x00:normal  0x01:low power
#endif
            break;
        case DEV_BATTERY_STATUS:
            GMA_DEBUG("request battery status \n");
#if  GMA_TWS_SUPPORTED
            //len
            send_buf_temp[1] = 2;
            ///value
            send_buf_temp[2] = _gma_hw_api->battery_state();//0x00:charging  0x01:using battery  0x02:alternating current
            ///value
            send_buf_temp[3] = _gma_hw_api->battery_state();//0x00:charging  0x01:using battery  0x02:alternating current
#else
            //len
            send_buf_temp[1] = 1;
            ///value
            send_buf_temp[2] = _gma_hw_api->battery_state();//0x00:charging  0x01:using battery  0x02:alternating current
#endif

            break;
        case DEV_FM_FRE_SET:
            GMA_DEBUG("set fm fre \n");
            _gma_hw_api->set_fm_fre(payload + 2/*fm frequence string*/, para_tlv->len);
            break;
        case DEV_FM_FRE_GET:
            GMA_DEBUG("request fm fre \n");
            //len
            send_buf_temp[1] = strlen(_gma_hw_api->get_fm_fre());
            //value
            memcpy(&send_buf_temp[2], _gma_hw_api->get_fm_fre(), strlen(_gma_hw_api->get_fm_fre()));
            break;
        case DEV_FW_VER:
            GMA_DEBUG("request firmware version \n");
            //len
            send_buf_temp[1] = 4;
            //value
            memcpy(&send_buf_temp[2], &fireware_ver/*_gma_hw_api->get_fw_version()*/, 4);
            break;
        case DEV_BT_NAME:
            GMA_DEBUG("request bluetooth local connect name \n");
            //len
            send_buf_temp[1] = strlen(_gma_hw_api->get_bt_name());
            GMA_DEBUG("local name:%s string len:%d  \n", _gma_hw_api->get_bt_name(), strlen(_gma_hw_api->get_bt_name()));
            //value
            memcpy(&send_buf_temp[2], _gma_hw_api->get_bt_name(), strlen(_gma_hw_api->get_bt_name()));
            break;
        case DEV_MIC_STATUS:
            GMA_DEBUG("request mic status \n");
            //len
            send_buf_temp[1] = 1;
            //value
            send_buf_temp[2] = _gma_hw_api->get_mic_state();//0x00:unused   0x01:using
            break;

        default:
            break;
        }

        tlv_send_len += (2 + send_buf_temp[1]);
        send_buf_temp += (2 + send_buf_temp[1]);

        GMA_DEBUG("---len:%d \n", tlv_send_len);

        if (tlv_send_len >= sizeof(send_buf_temp_object)) {
            GMA_DEBUG("error !!! func:%s line:%d \n", __func__, __LINE__);
            while (1);
        }
    }

    aes_cbc_encrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, send_buf_temp_object, tlv_send_len, &send_buf[GMA_PAYLOAD_POS], &en_len);

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("en_len %d: ", en_len);
    for (i = 0; i < en_len; i++) {
        GMA_DEBUG("%02x ", send_buf[GMA_PAYLOAD_POS + i]);
    }
    GMA_DEBUG("\r\n");
#endif

    ///load head length
    reply_rsp_head.f_len = en_len;
    p_buf = (uint8_t *)&reply_rsp_head;
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));

    return gma_data_send(send_buf, en_len + GMA_PAYLOAD_POS);
}

static sint32_t gma_recv_dev_ctrl(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    uint8_t de_out[18], i;
    int de_len;
    uint8_t result_value;
    gma_tlv_s gma_tlv;

    GMA_DEBUG("status set->recv len %d\r\n", gma_recv_data->gma_frame_head.f_len);
#if (GMA_DEBUG_LEVEL >= 10)
    for (i = 0; i < gma_recv_data->gma_frame_head.f_len; i++) {
        GMA_DEBUG("%02x ", gma_recv_data->recv_data[i]);
    }
    GMA_DEBUG("\r\n");
#endif
    memset(&de_out[16], 0xaa, 2);
    aes_iv_set(iv_temp);
    aes_cbc_decrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, gma_recv_data->recv_data, \
                          gma_recv_data->gma_frame_head.f_len, de_out, &de_len);


    if (de_out[16] != 0xaa && de_out[17] != 0xaa) {
        GMA_DEBUG("memory error func:%s line:%d \n", __func__, __LINE__);
        while (1);
    }

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("len:%d ", de_len);
    for (i = 0; i < de_len; i++) {
        GMA_DEBUG("%02x ", de_out[i]);
    }
    GMA_DEBUG("\r\n");
#endif

    memcpy(&gma_tlv, de_out, 2);
    // have value
    if (de_len > 2) {
        gma_tlv.value = &(de_out[2]);
    }

    return gma_reply_dev_ctrl(gma_recv_data->gma_frame_head.msg_id, de_out, de_len);
}

/*
 *  exception notice
 */
static sint32_t gma_reply_exception_notice(uint8_t msg_id, gma_tlv_s para_tlv)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s reply_rsp_head;
    gma_active_s active_value;
    uint8_t *p_buf, i;
    static uint8_t send_buf[48 + GMA_PAYLOAD_HEAD];
    int en_len;

    reply_rsp_head.msg_id = msg_id;
    reply_rsp_head.save_flag = 1;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_EXCEPTION_RSP;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
//    reply_rsp_head.f_len = 16;
//    p_buf = (uint8_t *)&reply_rsp_head;
//    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));

    p_buf = (uint8_t *)(&para_tlv);
    aes_iv_set(iv_temp);

    static u8 send_buf_temp[48];
    send_buf_temp[0] = para_tlv.type;
    switch (para_tlv.type) {
    case NOTICE_TCP_UNCONNECT:
        GMA_DEBUG("NOTICE_TCP_UNCONNECT \n");
        //len
        send_buf_temp[1] = 1;
        //value
        send_buf_temp[2] = 0x00;//0x00:succ   0x01:failed
        break;
    case NOTICE_APP_RECONNECT_SUCC:
        GMA_DEBUG("NOTICE_APP_RECONNECT_SUCC \n");
        //len
        send_buf_temp[1] = 1;
        //value
        send_buf_temp[2] = 0x00;//0x00:succ   0x01:failed
        break;

    default:
        break;
    }

    aes_cbc_encrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, send_buf_temp, send_buf_temp[1] + 2, &send_buf[GMA_PAYLOAD_POS], &en_len);

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("en_len %d: ", en_len);
    for (i = 0; i < en_len; i++) {
        GMA_DEBUG("%02x ", send_buf[GMA_PAYLOAD_POS + i]);
    }
    GMA_DEBUG("\r\n");
#endif

    ///load head length
    reply_rsp_head.f_len = en_len;
    p_buf = (uint8_t *)&reply_rsp_head;
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));

    return gma_data_send(send_buf, en_len + GMA_PAYLOAD_POS);
}
static sint32_t gma_recv_exception_notice(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    uint8_t de_out[18], i;
    int de_len;
    uint8_t result_value;
    gma_tlv_s gma_tlv;

    GMA_DEBUG("status set->recv len %d\r\n", gma_recv_data->gma_frame_head.f_len);
#if (GMA_DEBUG_LEVEL >= 10)
    for (i = 0; i < gma_recv_data->gma_frame_head.f_len; i++) {
        GMA_DEBUG("%02x ", gma_recv_data->recv_data[i]);
    }
    GMA_DEBUG("\r\n");
#endif
    memset(&de_out[16], 0xaa, 2);
    aes_iv_set(iv_temp);
    aes_cbc_decrypt_pkcs7(&ali_aes_ctx, gma_para_proc.ble_key, gma_recv_data->recv_data, \
                          gma_recv_data->gma_frame_head.f_len, de_out, &de_len);


    if (de_out[16] != 0xaa && de_out[17] != 0xaa) {
        GMA_DEBUG("memory error func:%s line:%d \n", __func__, __LINE__);
        while (1);
    }

#if (GMA_DEBUG_LEVEL >= 10)
    GMA_DEBUG("len:%d ", de_len);
    for (i = 0; i < de_len; i++) {
        GMA_DEBUG("%02x ", de_out[i]);
    }
    GMA_DEBUG("\r\n");
#endif

    memcpy(&gma_tlv, de_out, 2);
    // have value
    if (de_len > 2) {
        gma_tlv.value = &(de_out[2]);
    }

    return gma_reply_exception_notice(gma_recv_data->gma_frame_head.msg_id, gma_tlv);
}

/*
 *  error report
 */
static sint32_t gma_error_report(gma_recv_data_s *gma_recv_data)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
    gma_frame_head_s reply_rsp_head;
    uint8_t *p_buf, i, data[2];
    uint8_t send_buf[16 + GMA_PAYLOAD_HEAD];
    int en_len;

    reply_rsp_head.msg_id = gma_recv_data->gma_frame_head.msg_id;
    reply_rsp_head.save_flag = 0;
    reply_rsp_head.version = 0;
    reply_rsp_head.cmd = COMMAND_ERROR;
    reply_rsp_head.fn = 0;
    reply_rsp_head.total_fn = 0;
    reply_rsp_head.f_len = 0;
    p_buf = (uint8_t *)&reply_rsp_head;
    memcpy(send_buf, p_buf, sizeof(gma_frame_head_s));

    return gma_data_send(send_buf, GMA_PAYLOAD_POS);
}

/*
 *  receive process
 */
const gma_recv_cmd_list_s gma_recv_cmd_list[] = {
    {COMMAND_AUTH_START_REQ,	(command_callback)gma_recv_auth_start},
    {COMMAND_AUTH_RESULT_REQ,	(command_callback)gma_recv_auth_result},
#if (GMA_OTA_EN)
    {COMMAND_OTA_VERSION_REQ,	(command_callback)gma_recv_ota_fm_version},
    {COMMAND_OTA_UPDATE_REQ,	(command_callback)gma_recv_ota_req_para},
    {COMMAND_OTA_FM_RECV_DATA,	(command_callback)gma_recv_ota_data},
    {COMMAND_OTA_FM_SEND_END,	(command_callback)gma_recv_ota_fm_code_check},
#endif
    {COMMAND_DEV_INFO_REQ,	(command_callback)gma_recv_dev_info},
    {COMMAND_ACTIVE_REQ,	(command_callback)gma_recv_active},
    {COMMAND_STATUS_SET_REQ,	(command_callback)gma_recv_status_set},
    {COMMAND_A2DP_SET_REQ,	(command_callback)gma_recv_a2dp_set},
    {COMMAND_HFP_SET_REQ,	(command_callback)gma_recv_hfp_set},
    {COMMAND_MIC_READY_RSP,	(command_callback)gma_recv_mic_ready},
    {COMMAND_DEV_CTRL_REQ,	(command_callback)gma_recv_dev_ctrl},
    {COMMAND_EXCEPTION_NOTICE,	(command_callback)gma_recv_exception_notice},
    {0, NULL},
};

sint32_t gma_recv_proc(uint8_t *buf, uint16_t len)
{
    if (!GMA_MODULE_IS_EN()) {
        GMA_DEBUG("gma module uninit !!! \n");
        return (-1);
    }
#if (GMA_DEBUG_LEVEL >= 10)
    printf(">>>>>>>>>>>gma_recv_proc \n");
    printf_buf(buf, len);
#endif
    gma_recv_data_s gma_recv_data;
    gma_frame_head_s *gma_frame_head;
    uint8_t i = 0;

    {
        i = 0;
        gma_frame_head = (gma_frame_head_s *)buf;
        gma_recv_data.gma_frame_head = *gma_frame_head;
        gma_recv_data.recv_data = &buf[GMA_PAYLOAD_POS];

        if (gma_para_proc.init_flag) {
            gma_para_proc.init_flag = 0;
            gma_para_proc.next_msgid = gma_recv_data.gma_frame_head.msg_id;
            goto proc_msg;
        }

        if (gma_para_proc.next_msgid != gma_recv_data.gma_frame_head.msg_id) {
            gma_para_proc.next_msgid = gma_recv_data.gma_frame_head.msg_id;
            gma_para_proc.next_msgid++;
            GMA_DEBUG("msg id error\r\n");
            //        gma_error_report(&gma_recv_data);
            //        return (-1);
        }
        printf(">>>>gma_recv_data.gma_frame_head.cmd:%x \n", gma_recv_data.gma_frame_head.cmd);
proc_msg:
        gma_para_proc.next_msgid++;
        do {
            if (gma_recv_data.gma_frame_head.cmd && (gma_recv_cmd_list[i].recv_cmd_type  == gma_recv_data.gma_frame_head.cmd)) {
                GMA_DEBUG("start msg proc%d\r\n", i);
                gma_recv_cmd_list[i].cmd_cbk(&gma_recv_data);
                GMA_DEBUG("msg proc%d\r\n", i);
                break;
            }
            i++;
        } while (i <= (sizeof(gma_recv_cmd_list) / sizeof(gma_recv_cmd_list_s)));
        GMA_DEBUG("msg proc end %d\r\n", i);

        if (i > (sizeof(gma_recv_cmd_list) / sizeof(gma_recv_cmd_list_s))) {
            GMA_DEBUG("unknow gma command !!! \n");
            gma_error_report(&gma_recv_data);
        }
    }

    return 0;
}

/*
 *  init
 */
void gma_init(int (*should_send)(_uint16 len), int(*send_cmd_data)(_uint8 *buf, _uint16 len), int(*send_audio_data)(_uint8 *buf, _uint16 len))
{
    gma_para_proc.next_msgid = 0;
    gma_para_proc.init_flag = 1;

    gma_mac_addr_get(gma_para_proc.ble_mac);
    gma_get_random_num(gma_para_proc.dev_random, 16);
#if (GMA_OTA_EN)
    tm_frame_mg_init(should_send, send_cmd_data, send_audio_data, NULL);
#else
    tm_frame_mg_init(should_send, send_cmd_data, send_audio_data, NULL);
#endif
    GMA_AUTH_STATE_SET(0);
    GMA_MODULE_EN(1);
}

void gma_exit(void)
{
    GMA_MODULE_EN(0);
    GMA_AUTH_STATE_SET(0);
    _gma_hw_api->stop_speech();
    tm_frame_mg_close();
}

bool gma_connect_success(void)
{
    if (!GMA_IS_AUTH_SUCCESS()) {
        return false;
    }

    return true;
}

#endif
