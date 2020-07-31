#if 0
//#include <stdio.h>
#include <string.h>
#include "tm_gma.h"

/**************************************************************
	moudle:			gma head
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
**************************************************************/
void gma_head_init(struct _gma_head *param)
{
    static const struct _gma_head init_value = _GMA_HEAD_INIT();
    *param = init_value;
}

_uint32 gma_head_get_size(void)
{
    struct _gma_head gma_head = {{0}, 0, {0}, 0};
    return (sizeof(gma_head.base_info) + sizeof(gma_head.cmd_type) + sizeof(gma_head.frame_info) + sizeof(gma_head.frame_length));
}

int gma_head_2_packet(struct _gma_head gma_head, _uint8 *buf, _uint32 buf_len)
{
    _uint32 ptr = 0;

    if ((!buf) || (buf_len < gma_head_get_size())) {
        return -1;
    }

    memcpy(buf + ptr, &(gma_head.base_info), sizeof(gma_head.base_info));
    ptr += sizeof(gma_head.base_info);

    memcpy(buf + ptr, &(gma_head.cmd_type), sizeof(gma_head.cmd_type));
    ptr += sizeof(gma_head.cmd_type);

    memcpy(buf + ptr, &(gma_head.frame_info), sizeof(gma_head.frame_info));
    ptr += sizeof(gma_head.frame_info);

    memcpy(buf + ptr, &(gma_head.frame_length), sizeof(gma_head.frame_length));
    ptr += sizeof(gma_head.frame_length);

    return (int)gma_head_get_size();
}

int gma_head_unpacket(struct _gma_head *gma_head, _uint8 *buf)
{
    _uint32 ptr = 0;

    if ((!buf)) {
        return -1;
    }

    memcpy(&(gma_head->base_info), buf + ptr, sizeof(gma_head->base_info));
    ptr += sizeof(gma_head->base_info);

    memcpy(&(gma_head->cmd_type), buf + ptr, sizeof(gma_head->cmd_type));
    ptr += sizeof(gma_head->cmd_type);

    memcpy(&(gma_head->frame_info), buf + ptr, sizeof(gma_head->frame_info));
    ptr += sizeof(gma_head->frame_info);

    memcpy(&(gma_head->frame_length), buf + ptr, sizeof(gma_head->frame_length));
    ptr += sizeof(gma_head->frame_length);

    return 0;
}

/**************************************************************
	moudle:			exchange device information
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_APP_EXCHANGE_DEV_INFO  = 0X32,
					_GMA_CMD_FW_EXCHANGE_DEV_INFO	= 0X33,
**************************************************************/
_uint32 gma_app_exchange_dev_info_get_size(void)
{
    struct _gma_app_exchange_dev_info data = {0};
    return (sizeof(data.phone_type) + sizeof(data.gma_version));
}

int gma_app_exchange_dev_info_unpacket(struct _gma_app_exchange_dev_info *data, _uint8 *buf)
{
    _uint32 ptr = 0;

    if ((!buf)) {
        return -1;
    }

    memcpy(&(data->phone_type), buf + ptr, sizeof(data->phone_type));
    ptr += sizeof(data->phone_type);

    memcpy(&(data->gma_version), buf + ptr, sizeof(data->gma_version));
    ptr += sizeof(data->gma_version);

    return gma_app_exchange_dev_info_get_size();
}

void gma_fw_exchange_dev_info_init(struct _gma_fw_exchange_dev_info *param)
{
    static const struct _gma_fw_exchange_dev_info init_value =  _GMA_FW_EXCHANGE_DEV_INFO_INIT();
    *param = init_value;
}

_uint32 gma_fw_exchange_dev_info_get_size(void)
{
    struct _gma_fw_exchange_dev_info data = {{0}, 0};
    return (sizeof(data._dev_bilities) + sizeof(data.audio_format) + sizeof(data.gma_version) + sizeof(data.bt_mac));
}

int gma_fw_exchange_dev_info_2_packet(struct _gma_fw_exchange_dev_info data, _uint8 *buf, _uint32 buf_len)
{
    _uint32 ptr = 0;

    if ((!buf) || (buf_len < gma_fw_exchange_dev_info_get_size())) {
        return -1;
    }

    memcpy(buf + ptr, &(data._dev_bilities), sizeof(data._dev_bilities));
    ptr += sizeof(data._dev_bilities);

    memcpy(buf + ptr, &(data.audio_format), sizeof(data.audio_format));
    ptr += sizeof(data.audio_format);

    memcpy(buf + ptr, &(data.gma_version), sizeof(data.gma_version));
    ptr += sizeof(data.gma_version);

    memcpy(buf + ptr, (data.bt_mac), sizeof(data.bt_mac));
    ptr += sizeof(data.bt_mac);

    return gma_fw_exchange_dev_info_get_size();
}

/**************************************************************
	moudle:			alive device
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_APP_ALIVE_DEV			= 0X34,
					_GMA_CMD_FW_ALIVE_DEV			= 0X35,
**************************************************************/
void gma_fw_alive_dev_init(struct _gma_fw_alive_dev *param)
{
    static const struct _gma_fw_alive_dev init_value = _GMA_FW_ALIVE_DEV_INIT();
    *param = init_value;
}

_uint32 gma_fw_alive_dev_get_size(void)
{
    struct _gma_fw_alive_dev data = {{0}, {0}};
    return (sizeof(data.random_data) + sizeof(data.digest_data));
}

int gma_fw_alive_dev_2_packet(struct _gma_fw_alive_dev data, _uint8 *buf, _uint32 buf_len)
{
    _uint32 ptr = 0;

    if ((!buf) || (buf_len < gma_fw_alive_dev_get_size())) {
        return -1;
    }

    memcpy(buf + ptr, (data.random_data), sizeof(data.random_data));
    ptr += sizeof(data.random_data);

    memcpy(buf + ptr, (data.digest_data), sizeof(data.digest_data));
    ptr += sizeof(data.digest_data);

    return gma_fw_alive_dev_get_size();
}
/**************************************************************
	moudle:			audio send
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_FW_AUDIO_SEND			= 0X30,
**************************************************************/
void gma_fw_audio_send_init(struct _gma_fw_audio_send *param)
{
    static const struct _gma_fw_audio_send init_value = _GMA_FW_AUDIO_SEND_INIT();
    *param = init_value;
}

_uint32 gma_fw_audio_send_get_size(void)
{
    struct _gma_fw_audio_send data = {{0}, 0};
    return (sizeof(data.nums) + sizeof(data.mic_ch_len) + sizeof(data.ref_ch_len));
}

int gma_fw_audio_send_2_packet(struct _gma_fw_audio_send data, _uint8 *buf, _uint32 buf_len)
{
    _uint32 ptr = 0;

    if ((!buf) || (buf_len < gma_fw_audio_send_get_size())) {
        return -1;
    }

    memcpy(buf + ptr, &(data.nums), sizeof(data.nums));
    ptr += sizeof(data.nums);

    memcpy(buf + ptr, &(data.mic_ch_len), sizeof(data.mic_ch_len));
    ptr += sizeof(data.mic_ch_len);

    memcpy(buf + ptr, &(data.ref_ch_len), sizeof(data.ref_ch_len));
    ptr += sizeof(data.ref_ch_len);

    return gma_fw_audio_send_get_size();
}

/**************************************************************
	moudle:			app notify state
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_APP_NOTIFY_STATE		= 0X36,
					_GMA_CMD_FW_RESPOND_STATE		= 0x37,
**************************************************************/
_uint32 gma_app_notify_state_get_size(void)
{
    struct _gma_app_notify_state data = {0};
    return (sizeof(data.type) + sizeof(data.length));
}

int gma_app_notify_state_unpacket(struct _gma_app_notify_state *data, _uint8 *buf)
{
    _uint32 ptr = 0;

    if ((!buf)) {
        return -1;
    }

    memcpy(&(data->type), buf + ptr, sizeof(data->type));
    ptr += sizeof(data->type);

    memcpy(&(data->length), buf + ptr, sizeof(data->length));
    ptr += sizeof(data->length);

    return gma_app_notify_state_get_size();
}

void gma_fw_respond_state_init(struct _gma_fw_respond_state *param)
{
    static const struct _gma_fw_respond_state init_value = _GMA_FW_RESPOND_STATE_INIT();
    *param = init_value;
}

_uint32 gma_fw_respond_state_get_size(void)
{
    struct _gma_fw_respond_state data = {0};
    return (sizeof(data.type) + sizeof(data.length) + sizeof(data.succeed_or_fail));
}

int gma_fw_respond_state_2_packet(struct _gma_fw_respond_state data, _uint8 *buf, _uint32 buf_len)
{
    _uint32 ptr = 0;

    if ((!buf) || (buf_len < gma_fw_respond_state_get_size())) {
        return -1;
    }

    memcpy(buf + ptr, &(data.type), sizeof(data.type));
    ptr += sizeof(data.type);

    memcpy(buf + ptr, &(data.length), sizeof(data.length));
    ptr += sizeof(data.length);

    memcpy(buf + ptr, &(data.succeed_or_fail), sizeof(data.succeed_or_fail));
    ptr += sizeof(data.succeed_or_fail);

    return gma_fw_respond_state_get_size();
}

/**************************************************************
	moudle:			firmware notify state
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_FW_NOTIFY_STATE		= 0X3E,
					_GMA_CMD_APP_RESPOND_STATE		= 0X3F,
**************************************************************/
void gma_fw_notify_state_init(struct _gma_fw_notify_state *param)
{
    static const struct _gma_fw_notify_state init_value = _GMA_FW_NOTIFY_STATE_INIT();
    *param = init_value;
}

_uint32 gma_fw_notify_state_get_size(void)
{
    struct _gma_fw_notify_state data = {0};
    return (sizeof(data.type) + sizeof(data.length));
}

int gma_fw_notify_state_2_packet(struct _gma_fw_notify_state data, _uint8 *buf, _uint32 buf_len)
{
    _uint32 ptr = 0;

    if ((!buf) || (buf_len < gma_fw_notify_state_get_size())) {
        return -1;
    }

    memcpy(buf + ptr, &(data.type), sizeof(data.type));
    ptr += sizeof(data.type);

    memcpy(buf + ptr, &(data.length), sizeof(data.length));
    ptr += sizeof(data.length);

    return gma_fw_notify_state_get_size();
}

_uint32 gma_app_respond_state_get_size(void)
{
    struct _gma_app_respond_state data = {0};
    return (sizeof(data.type) + sizeof(data.length) + sizeof(data.succeed_or_fail));
}

int gma_app_respond_state_unpacket(struct _gma_app_respond_state *data, _uint8 *buf)
{
    _uint32 ptr = 0;

    if ((!buf)) {
        return -1;
    }

    memcpy(&(data->type), buf + ptr, sizeof(data->type));
    ptr += sizeof(data->type);

    memcpy(&(data->length), buf + ptr, sizeof(data->length));
    ptr += sizeof(data->length);

    memcpy(&(data->succeed_or_fail), buf + ptr, sizeof(data->succeed_or_fail));
    ptr += sizeof(data->succeed_or_fail);

    return gma_app_respond_state_get_size();
}
/**************************************************************
	moudle:			audio control
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_APP_AUDIO_CTL			= 0X38,
					_GMA_CMD_FM_AUDIO_RES			= 0X39,
**************************************************************/
_uint32 gma_app_audio_ctl_get_size(void)
{
    struct _gma_app_audio_ctl data = {0};
    return (sizeof(data.type) + sizeof(data.length) + sizeof(data.value));
}

int gma_app_audio_ctl_unpacket(struct _gma_app_audio_ctl *data, _uint8 *buf)
{
    _uint32 ptr = 0;

    if ((!buf)) {
        return -1;
    }

    memcpy(&(data->type), buf + ptr, sizeof(data->type));
    ptr += sizeof(data->type);

    memcpy(&(data->length), buf + ptr, sizeof(data->length));
    ptr += sizeof(data->length);

    memcpy(&(data->value), buf + ptr, sizeof(data->value));
    ptr += sizeof(data->value);

    return gma_app_audio_ctl_get_size();
}

void gma_fw_audio_res_init(struct _gma_fw_audio_res *param)
{
    static const struct _gma_fw_audio_res init_value = _GMA_FW_AUDIO_RES_INIT();
    *param = init_value;
}

_uint32 gma_fw_audio_res_get_size(void)
{
    struct _gma_fw_audio_res data = {0};
    return (sizeof(data.type) + sizeof(data.length) + sizeof(data.succeed_or_fail));
}

int gma_fw_audio_res_2_packet(struct _gma_fw_audio_res data, _uint8 *buf, _uint32 buf_len)
{
    _uint32 ptr = 0;

    if ((!buf) || (buf_len < gma_fw_audio_res_get_size())) {
        return -1;
    }

    memcpy(buf + ptr, &(data.type), sizeof(data.type));
    ptr += sizeof(data.type);

    memcpy(buf + ptr, &(data.length), sizeof(data.length));
    ptr += sizeof(data.length);

    memcpy(buf + ptr, &(data.succeed_or_fail), sizeof(data.succeed_or_fail));
    ptr += sizeof(data.succeed_or_fail);

    return gma_fw_audio_res_get_size();
}
/**************************************************************
	moudle:			HFP control
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_APP_HFP_STATE			= 0X3A,
					_GMA_CMD_FW_HFP_STATE			= 0X3B,
**************************************************************/
int gma_app_HFP_ctl_unpacket(struct _gma_app_HFP_ctl *data, _uint8 *buf)
{
    _uint32 ptr = 0;

    if ((!buf) || (data->length > sizeof(data->phone_nums))) {
        return -1;
    }

    memcpy(&(data->type), buf + ptr, sizeof(data->type));
    ptr += sizeof(data->type);

    memcpy(&(data->length), buf + ptr, sizeof(data->length));
    ptr += sizeof(data->length);

    int len  = 0;
    if (data->type == _GMA_HFP_PHONE_NUMS && (data->length < sizeof(data->phone_nums))) {
        memset((data->phone_nums), '\0', sizeof(data->phone_nums));
        memcpy((data->phone_nums), buf + ptr, data->length);
        ptr += data->length;
    }

    len = sizeof(data->phone_nums) + sizeof(data->length) + data->length;

    return len;
}

void gma_fw_HFP_res_init(struct _gma_fw_HFP_res *param)
{
    static const struct _gma_fw_HFP_res init_value = _GMA_FW_HFP_RES_INIT();
    *param = init_value;
}

_uint32 gma_fw_HFP_res_get_size(void)
{
    struct _gma_fw_HFP_res data = {0};
    return (sizeof(data.type) + sizeof(data.length) + sizeof(data.succeed_or_fail));
}

int gma_fw_HFP_res_2_packet(struct _gma_fw_HFP_res data, _uint8 *buf, _uint32 buf_len)
{
    _uint32 ptr = 0;

    if ((!buf) || (buf_len < gma_fw_HFP_res_get_size())) {
        return -1;
    }

    memcpy(buf + ptr, &(data.type), sizeof(data.type));
    ptr += sizeof(data.type);

    memcpy(buf + ptr, &(data.length), sizeof(data.length));
    ptr += sizeof(data.length);

    memcpy(buf + ptr, &(data.succeed_or_fail), sizeof(data.succeed_or_fail));
    ptr += sizeof(data.succeed_or_fail);

    return gma_fw_HFP_res_get_size();
}
/**************************************************************
	moudle:			device base information
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_APP_DEV_BASE_INFO		= 0X40,
					_GMA_CMD_FW_DEV_BASE_INFO		= 0X41,
**************************************************************/
_uint32 gma_app_dev_base_info_get_size(void)
{
    struct _gma_app_dev_base_info data = {0};
    return (sizeof(data.type) + sizeof(data.length));
}

int gma_app_dev_base_info_unpacket(struct _gma_app_dev_base_info *data, _uint8 *buf)
{
    _uint32 ptr = 0;

    if ((!buf)) {
        return -1;
    }

    memcpy(&(data->type), buf + ptr, sizeof(data->type));
    ptr += sizeof(data->type);

    memcpy(&(data->length), buf + ptr, sizeof(data->length));
    ptr += sizeof(data->length);

    return gma_app_dev_base_info_get_size();
}

void gma_fw_dev_base_info_init(struct _gma_fw_dev_base_info *param)
{
    static const struct _gma_fw_dev_base_info init_value = _GMA_FW_DEV_BASE_INFO_INIT();
    *param = init_value;
}

_uint8 gma_fw_dev_base_info_get_state_len(struct _gma_fw_dev_base_info res_info)
{

    _uint8 state_len = 0;

    switch (res_info.type) {
    case _GMA_DEV_BASE_INFO_BATTERY_VALUE:
        state_len = sizeof(res_info.state.battery_value);
        break;
    case _GMA_DEV_BASE_INFO_CHARGE_STATE:
        state_len = sizeof(res_info.state.charge_state);
        break;
    case _GMA_DEV_BASE_INFO_FM_SET_RES:
        state_len = sizeof(res_info.state.set_fm_fre_res);
        break;
    case _GMA_DEV_BASE_INFO_CUR_FM_STR:
        state_len = strlen((const char *)res_info.state.fm_fre_str);
        break;
    case _GMA_DEV_BASE_INFO_FW_VERSION:
        state_len = strlen((const char *)res_info.state.fw_verison);
        break;
    case _GMA_DEV_BASE_INFO_BT_NAME_STR:
        state_len = strlen((const char *)res_info.state.bt_name_str);
        break;
    case _GMA_DEV_BASE_INFO_MIC_STATE:
        state_len = sizeof(res_info.state.mic_state);
        break;

    default:
        break;

    }

    return state_len;
}

_uint32 gma_fw_dev_base_info_get_size(struct _gma_fw_dev_base_info res_info)
{
    struct _gma_fw_dev_base_info data = {0};

    return (sizeof(data.type) + sizeof(data.length) + gma_fw_dev_base_info_get_state_len(res_info));
}

int gma_fw_dev_base_info_2_packet(struct _gma_fw_dev_base_info data, _uint8 *buf, _uint32 buf_len)
{
    _uint32 ptr = 0;

    if ((!buf) || (buf_len < gma_fw_dev_base_info_get_size(data))) {
        return -1;
    }

    memcpy(buf + ptr, &(data.type), sizeof(data.type));
    ptr += sizeof(data.type);

    data.length = gma_fw_dev_base_info_get_state_len(data);
    memcpy(buf + ptr, &(data.length), sizeof(data.length));
    ptr += sizeof(data.length);

    memcpy(buf + ptr, &(data.state), gma_fw_dev_base_info_get_state_len(data));
    ptr += gma_fw_dev_base_info_get_state_len(data);

    return gma_fw_dev_base_info_get_size(data);
}

/**************************************************************
	moudle:			phone exception notify
	author:			mqc
	description:
	data:			2019/07/19
	relative cmd:
					_GMA_CMD_APP_EXCEPTION_NOTIFY		= 0X42,
					_GMA_CMD_FW_EXCEPTION_RES			= 0X43,
**************************************************************/
/*
1.exception information
2.phone
*/
_uint32 gma_app_exception_notify_get_size(void)
{
    struct _gma_app_exception_notify data  = {0};
    return (sizeof(data.type) + sizeof(data.length));
}

int gma_app_exception_notify_unpacket(struct _gma_app_exception_notify *data, _uint8 *buf)
{
    _uint32 ptr = 0;

    if ((!buf)) {
        return -1;
    }

    memcpy(&(data->type), buf + ptr, sizeof(data->type));
    ptr += sizeof(data->type);

    memcpy(&(data->length), buf + ptr, sizeof(data->length));
    ptr += sizeof(data->length);

    return gma_app_dev_base_info_get_size();
}
/*
firmware
 * */
void gma_fw_exception_rsp_init(struct _gma_fw_exception_rsp *param)
{
    static const struct _gma_fw_exception_rsp init_value = _GMA_FW_EXCEPTION_RSP_INIT();
    *param = init_value;
}

_uint32 gma_fw_exception_rsp_get_size(void)
{
    struct _gma_fw_exception_rsp data = {0};

    return (sizeof(data.type) + sizeof(data.length) + sizeof(data.succeed_or_fail));
}

int gma_fw_exception_rsp_2_packet(struct _gma_fw_exception_rsp data, _uint8 *buf, _uint32 buf_len)
{
    _uint32 ptr = 0;

    if ((!buf) || (buf_len < gma_fw_exception_rsp_get_size())) {
        return -1;
    }

    memcpy(buf + ptr, &(data.type), sizeof(data.type));
    ptr += sizeof(data.type);

    data.length = data.succeed_or_fail;
    memcpy(buf + ptr, &(data.length), sizeof(data.length));
    ptr += sizeof(data.length);

    memcpy(buf + ptr, &(data.succeed_or_fail), sizeof(data.succeed_or_fail));
    ptr += sizeof(data.succeed_or_fail);

    return gma_fw_exception_rsp_get_size();
}
#endif
