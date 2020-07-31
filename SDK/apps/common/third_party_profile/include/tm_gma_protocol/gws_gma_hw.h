#ifndef _GWS_GMA_H
#define _GWS_GMA_H
typedef enum {
    SET_OFF = 0,
    SET_ON,
} set_onoff_e;

typedef enum {
    TICK_TYPE_10MS = 0,
    TICK_TYPE_50MS,
    TICK_TYPE_MAX,
} tick_type_e;

typedef enum {
    BLE_CONNECT = 0,
    BLE_DISCONNECT,
    SPP_CONNECT,
    SPP_DISCONNECT,
    BT_DEFAULT,
    BT_STATE_MAX,
} bt_connect_e;

typedef enum {
    A2DP_CONNECT = 0,
    A2DP_DISCONNECT,
} a2dp_connect_e;

typedef struct {
    unsigned char dav_data[37];
} gws_ble_adv_para_t;

typedef struct {
    unsigned short srv_uuid;
    unsigned short write_att;  //phone to accessory for data(prop write with resp)
    unsigned short read_att;
    unsigned short indicate_att;//accessory to phone for except mic and ota data
    unsigned short notfy_att; //accessory to phone for ota/mic data
    unsigned short ota_write_att; //phone to accessory for ota(prop write no resp)
} gws_ble_para_t;

typedef struct {
    unsigned char srv_uuid[16];  //uuid
} gws_spp_para_t;

typedef struct {
    gws_ble_para_t gma_ble_para; //ble svr
    gws_spp_para_t spp_para; //spp uuid svr
} hal_gws_base_t;

typedef struct {
    unsigned char mac[6];
    unsigned int pid;
    unsigned char secret[16];
} gws_triple_para_t;

typedef struct {
    unsigned char value;
    unsigned char status;
} gws_power_value_t;

typedef struct {
    hal_gws_base_t gws_base;
} gws_model_cb;

void gws_init(void);
void gws_gma_recv_proc_cb(unsigned char *buf, unsigned short len);
void gws_ota_recv_proc_cb(unsigned char *buf, unsigned short len);
int gws_mic_send_data_cb(unsigned char *buf, unsigned short len);
void gws_time_tick_cb(tick_type_e tick_type);  //0->10ms tick 1->50ms tick
void gws_bt_connect_cb(bt_connect_e bt_state);

void sal_gma_ble_adv_data_set(void *para, unsigned char len);
void sal_gma_ble_adv_start(void);
void sal_gma_ble_adv_stop(void);
int sal_gma_ble_send_notify(unsigned char *buf, unsigned short len);
int sal_gma_ble_send_indicate(unsigned char *buf, unsigned short len);
int sal_gma_spp_send_data(unsigned char *buf, unsigned short len);

unsigned char sal_gma_a2dp_status_get(void);
void sal_gma_play_start(void);
void sal_gma_play_pause(void);
void sal_gma_play_stop(void);
void sal_gma_play_next(void);
void sal_gma_play_last(void);
void sal_gma_volume_set(unsigned short type, unsigned char vol_num);

unsigned char sal_gma_hfp_status_get(void);
void sal_gma_hfp_phone_num(unsigned char *buf, unsigned short len);
void sal_gma_hfp_telephone_call(void);
void sal_gma_hfp_telephone_refuse(void);

void sal_time_start(void);
void sal_gma_mic_set(set_onoff_e onoff);
void sal_gma_led_set(unsigned char channel, set_onoff_e onoff);
void sal_gma_hfp_bvra_set(void);
unsigned char sal_triple_para_get(gws_triple_para_t *gws_triple);
void sal_power_value_get(gws_power_value_t *gws_power);
void sal_random_value_get(unsigned char *gws_random, unsigned char len);

void sal_ota_init(void);
void sal_ota_flash_write(unsigned char *write_buf, unsigned short len);
void sal_ota_boot(void);
#endif
