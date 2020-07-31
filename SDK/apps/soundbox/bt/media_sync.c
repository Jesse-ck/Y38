#include "app_config.h"
#include "media/includes.h"

extern u32 get_bt_slot_time(u8 type, u32 time);
extern int bt_send_audio_sync_data(void *, void *buf, u32 len);
extern void bt_media_sync_set_handler(void *, void *priv,
                                      void (*event_handler)(void *, int *, int));
extern int bt_media_sync_master(u8 type);
extern u8 bt_media_device_online(u8 dev);
extern void *bt_media_sync_open(void);
extern void bt_media_sync_role_lock(void *_sync, u8 lock);
extern void bt_media_sync_close(void *);

/*const static struct wireless_sync_ops wireless_sync_ops = {
    .open = bt_media_sync_open,
    .close = bt_media_sync_close,
    .time = get_bt_slot_time,
    .master = bt_media_sync_master,
    .online = bt_media_device_online,
    .set_handler = bt_media_sync_set_handler,
    .send = bt_send_audio_sync_data,
};*/

int sbc_play_sync_open(struct server *server, int buffer_size)
{
#if 0
    union audio_dec_req req = {0};

    if (!server) {
        return -EINVAL;
    }
    req.sync.cmd = AUDIO_DEC_SYNC_OPEN;
#if (TCFG_USER_TWS_ENABLE == 1)
    req.sync.ops = &wireless_sync_ops;
#endif
    req.sync.buffer_size = buffer_size;
#if (CONFIG_BD_NUM == 2)
    /*音频数据缓冲区间设置*/
    req.sync.top_percent = 70;
    req.sync.bottom_percent = 50;
    req.sync.start_percent = 60;
#elif (TCFG_USER_TWS_ENABLE == 1)
    /*音频数据缓冲区间设置*/
    req.sync.top_percent = 90;
    req.sync.bottom_percent = 50;
    req.sync.start_percent = 80;
#else
    /*音频数据缓冲区间设置*/
    req.sync.top_percent = 80;
    req.sync.bottom_percent = 60;
    req.sync.start_percent = 70;
#endif
    req.sync.protocol = WL_PROTOCOL_RTP;
    server_request(server, AUDIO_REQ_WIRELESS_SYNC, &req);
#endif

    return 0;
}

int phone_call_sync_open(struct server *server, int buffer_size)
{
#if 0
    union audio_dec_req req = {0};

    if (!server) {
        return -EINVAL;
    }
    req.sync.cmd = AUDIO_DEC_SYNC_OPEN;
#if (TCFG_USER_TWS_ENABLE == 1)
    req.sync.ops = &wireless_sync_ops;
#endif
    req.sync.buffer_size = buffer_size;
    /*音频数据缓冲区间设置*/
    req.sync.top_percent = 60;
    req.sync.bottom_percent = 6;
    req.sync.start_percent = 6;/*60*50*6/100 = 180(3 packet)*/
    req.sync.protocol = WL_PROTOCOL_SCO;
    server_request(server, AUDIO_REQ_WIRELESS_SYNC, &req);
#endif

    return 0;
}
