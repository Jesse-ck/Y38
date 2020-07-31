#ifndef _TM_GMA_H
#define _TM_GMA_H
#include "tm_gma_type.h"
#include "typedef.h"

/**************************************************************
	moudle:			gma respond succeed or fail
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
**************************************************************/
typedef enum {
    _GMA_SUCCEED	= 0X00,
    _GMA_FAIL	= 0X01,
} _TM_GMA_SUCCEED_OR_FAIL;

/**************************************************************
	moudle:			gma head
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
**************************************************************/
/*
gma head
every packet has a head
*/
struct _gma_head {
    struct {
        /*
        1.msg_id increase 1 after a sending finish
        2.responded message have the same msg_id as requested message
        3.if msg_id larger than 15,set msg_id to 1
        */
        _uint8 msg_id  : 4;
        /*
        1.1:encrypt 0:do not encrypt
        2.default value is 0
        */
        _uint8 encrypt : 1;
        /*
        1.default value is 0
        */
        _uint8 version : 3;
    } base_info;

    /*
    1.cmd type
    */
    _uint8 cmd_type;

    struct {
        /*
        1.value range 0~15
        */
        _uint8 frame_seq  : 4;
        /*
        1.value range 0~15,at most have 16 packeys
        2.frame_seq & frame_total must be zero when payload length is zero
        */
        _uint8 frame_total : 4;
    } frame_info;

    /*
    current frame length,bluebooth 4.0 value range 0~16,bluetooth 4.2 and above version value range 0~240
    */
    _uint8 frame_length;
};

#define  _GMA_HEAD_INIT()  {{0/*msg_id*/,0/*encrypt*/,0/*version*/},\
							0/*cmd type*/,\
							{0/*frame_seq*/,0/*frame_total*/},\
							0/*frame_total*/}

/**************************************************************
	moudle:			type length value(TLV) format
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
**************************************************************/
/*
1.use TLV type to deal with the situation of one CMD include many functions
2.response use the same format as request
*/
struct _TLV_Type {
    /*
    data type
    */
    _uint8 type;
    /*
    data length
    */
    _uint8 length;
    /*
    data content
    */
    _uint8 *value;
};

/**************************************************************
	moudle:			exchange device information
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_APP_EXCHANGE_DEV_INFO  = 0X32,
					_GMA_CMD_FW_EXCHANGE_DEV_INFO	= 0X33,
**************************************************************/
/*
1.exchange device information
2.phone data structure
*/
struct _gma_app_exchange_dev_info {
    /*
    1.phone type, 0x00:IOS   0x01:Android
    */
    _uint8  phone_type;
    /*
    1.gma version:0x0001
    */
    _uint16 gma_version;
};

/*
1.exchange device information
2.firmware data structure
*/
struct _gma_fw_exchange_dev_info {
    struct {
        /*
        1.support wake up action
        */
        _uint16 support_wakeup		: 1;
        /*
        1.support voice activity detection
        */
        _uint16 support_VAD			: 1;
        /*
        1.support echo cancellation
        */
        _uint16	support_AEC			: 1;
        /*
        1.support denoice
        */
        _uint16 support_dnc			: 1;
        /*
        1.bt support a2dp
        */
        _uint16 support_a2dp		: 1;
        /*
        1.support fm tx
        */
        _uint16 support_fm_tx		: 1;
        /*
        1.support hfp
        */
        _uint16 support_HFP			: 1;
        /*
        1.reserved
        */
        _uint16 reserved			: 9;
    } _dev_bilities;

    /*
    1.audio format,0x00:PCM   0x01:ADPCM(must use alibaba algorithm)    0x02:Opus
    */
    _uint8 audio_format;
    /*
    1.gma version,default value:0x0001
    */
    _uint16 gma_version;
    /*
    1.class bluetooth mac address
    */
    _uint8 bt_mac[6];
};

typedef enum {
    _GMA_PHONE_TYPE_IOS		= 0X00,
    _GMA_PHONE_TYPE_ANDROID	= 0X01,
} _TM_GMA_PHONE_TYPE;

typedef enum {
    _GMA_ENC_TYPE_PCM		= 0X00,
    _GMA_ENC_TYPE_ADPCM		= 0X01,/*must used adpcm algorithm*/
    _GMA_ENC_TYPE_OPUS		= 0X02,
} _TM_GMA_AUDIO_ENCODE_TYPE;

#define _GMA_FW_EXCHANGE_DEV_INFO_INIT()  {{0/*support wakeup*/,0/*support vad*/,0/*support aec*/,0/*support denoice*/,1/*supporta2dp*/,0/*support fm tx*/,1/*support HFP*/,0/*reserved*/},\
											0x02/*audio select opus*/,\
											0x0001/*gma version*/,\
											{0,0,0,0,0,0}/*bt mac*/}

/**************************************************************
	moudle:			alive device
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_APP_ALIVE_DEV			= 0X34,
					_GMA_CMD_FW_ALIVE_DEV			= 0X35,
**************************************************************/
/*
1.alive device
2.firmware
*/
struct _gma_fw_alive_dev {
    /*
    1.16 byte random strings
    */
    _uint8 random_data[16];
    /*
    1.digest_data = SHA256(Random,PID,MAC,Secret);
    2.PID,MAC,Secret save in firmware,do not need send to app
    */
    _uint8 digest_data[16];
};
#define _GMA_FW_ALIVE_DEV_INIT()  {{0}/*random data,16 byte*/,\
								{0}/*digest data,16 byte*/}

/**************************************************************
	moudle:			audio send
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_FW_AUDIO_SEND			= 0X30,
**************************************************************/
/*
1.audio send
2.firmware
*/
struct	_gma_fw_audio_send {
    struct {
        /*
        1.mic numbers
        */
        _uint8 mic : 4;
        /*
        1.reference mic numbers
        */
        _uint8 ref : 4;
    } nums;
    /*
    1.mic channel length
    */
    _uint16 mic_ch_len;
    /*
    1.ref channel length
    */
    _uint16 ref_ch_len;
    /*
    1.audio data,mic0 + mic1 + ~ + micn + ref0 + ref1 + ~ + refn
    */
    //_uint8 *audio_data;
};
#define _GMA_FW_AUDIO_SEND_INIT()  {{1/*mic numbers*/,0/*reference mic numbers*/},\
									0/*mic channel length*/,\
									0/*reference mic numbers*/}

/**************************************************************
	moudle:			app notify state
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_APP_NOTIFY_STATE		= 0X36,
					_GMA_CMD_FW_RESPOND_STATE		= 0x37,
**************************************************************/
/*
1.notify state,app start action
2.phone
*/
struct _gma_app_notify_state {
    /*
    1.type :
    	0x00: notify firmware enter connect process
    	0x01: notify firmware start record
    	0x02: notify firmware stop record
    	0x03: notify firmware start play tts
    	0x04: notify firmware end play tts
    */
    _uint8 type;
    /*
    1.length value 0
    */
    _uint8 length;
};

/*
1.respond state,app start action
2.firmware
*/
struct _gma_fw_respond_state {
    /*
    1.type :
    	0x00: respond firmware enter connect process
    	0x01: respond firmware start record
    	0x02: respond firmware stop record
    	0x03: respond firmware start play tts
    	0x04: respond firmware end play tts
    */
    _uint8 type;
    /*
    1.length value must be 1
    */
    _uint8 length;
    /*
    1.value: 0x00:succeed  0x01:fail
    */
    _uint8 succeed_or_fail;
};

typedef enum {
    _GMA_DEV_ENTER_CONNECT	= 0X00,
    _GMA_DEV_START_RECORD	= 0X01,
    _GMA_DEV_STOP_RECORD	= 0X02,
    _GMA_DEV_START_PLAY_TTS	= 0X03,
    _GMA_DEV_STOP_PLAY_TTS	= 0X04,

    _GMA_DEV_ERROR			= 0XFF,
} _TM_GMA_DEV_STATE;

#define _GMA_FW_RESPOND_STATE_INIT() {_GMA_DEV_ERROR/*state*/,\
									1/*length*/,\
									_GMA_SUCCEED}

/**************************************************************
	moudle:			firmware notify state
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_FW_NOTIFY_STATE		= 0X3E,
					_GMA_CMD_APP_RESPOND_STATE		= 0X3F,
**************************************************************/
/*
1.firmware notify,firmware start action
2.firmware
*/
struct _gma_fw_notify_state {
    /*
    1.type: 0x00:start send record  0x01:stop send record
    */
    _uint8 type;
    /*
    1.length value must be 0
    */
    _uint8 length;
};

/*
1.app respond,firmware start action
2.phone
*/
struct _gma_app_respond_state {
    /*
    1.type: 0x00:respond start send record   0x01:respond stop send record
    */
    _uint8 type;
    /*
    1.length value must be 1
    */
    _uint8 length;
    /*
    1.value : 0x00:succeed  0x01:fail
    */
    _uint8 succeed_or_fail;
};

typedef enum {
    _GMA_NOTIFY_STATE_START_SNED_AUDIO	= 0X00,
    _GMA_NOTIFY_STATE_STOP_SNED_AUDIO	= 0X01,

    _GMA_NOTIFY_STATE_ERROR				= 0XFF,
} _TM_GMA_NOTIFY_STATE;
#define  _GMA_FW_NOTIFY_STATE_INIT()  {_GMA_NOTIFY_STATE_ERROR/*state*/,\
									0/*length*/}

/**************************************************************
	moudle:			audio control
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_APP_AUDIO_CTL			= 0X38,
					_GMA_CMD_FM_AUDIO_RES			= 0X39,
**************************************************************/
/*
1.audio control
2.phone
*/
struct _gma_app_audio_ctl {
    /*
    1.audio control type
    1.tpye:
    	0x00:get A2DP state
    	0x01:set vol
    	0x02:play
    	0x03:pause
    	0x04:next file
    	0x05:prev file
    	0x06:stop
    */
    _uint8 type;
    /*
    1.value length,0 or 1
    */
    _uint8 length;
    /*
    1.value
    */
    _uint8 value;
};

/*
1.audio respond
2.firmware
*/
struct _gma_fw_audio_res {
    /*
    1.audio control type
    1.tpye:
    	0x00:get A2DP state
    	0x01:set vol
    	0x02:play
    	0x03:pause
    	0x04:next file
    	0x05:prev file
    	0x06:stop
    */
    _uint8 type;
    /*
    1.length must be 1
    */
    _uint8 length;
    /*
    1.value: 0x00:succeed  0x01:fail
    */
    _uint8 succeed_or_fail;
};

typedef enum {
    _GMA_AUDIO_CTL_A2DP_STATE	= 0X00,
    _GMA_AUDIO_CTL_SEL_VOL		= 0X01,
    _GMA_AUDIO_CTL_PLAY			= 0X02,
    _GMA_AUDIO_CTL_PAUSE		= 0X03,
    _GMA_AUDIO_CTL_NEXT_FILE	= 0X04,
    _GMA_AUDIO_CTL_PREV_FILE	= 0X05,
    _GMA_AUDIO_CTL_STOP			= 0X06,

    _GMA_AUDIO_CTL_ERROR		= 0XFF,
} _GMA_AUDIO_CTL;

#define _GMA_FW_AUDIO_RES_INIT() {_GMA_AUDIO_CTL_ERROR/*type*/,\
								1/*length*/,\
								_GMA_SUCCEED}

/**************************************************************
	moudle:			HFP control
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_APP_HFP_STATE			= 0X3A,
					_GMA_CMD_FW_HFP_STATE			= 0X3B,
**************************************************************/
/*
1.HFP control
2.phone
*/
struct _gma_app_HFP_ctl {
    /*
    1.HFP control type
    2.type:
    	0x00:HFP connect status
    	0x01:phone numbers
    	0x02:answer a phone
    	0x03:hang up a phone
    	0x04:recall last phone
    	0x05:answer another phone
    	0x06:hang up holding phone
    	0x07:hold current phone answer another phone
    	0x08:add hold phone to current phone
    */
    _uint8 type;
    /*
    1.length:0~N
    */
    _uint8 length;
    /*
    1/phone numbers
    */
    _uint8 phone_nums[15];
};

/*
1.HFP respond
2.firmware
*/
struct _gma_fw_HFP_res {
    /*
    1.HFP control type
    2.type:
    	0x00:HFP connect state
    	0x01:phone numbers
    	0x02:answer a phone
    	0x03:hang up a phone
    	0x04:recall last phone
    	0x05:answer another phone
    	0x06:hang up holding phone
    	0x07:hold current phone answer another phone
    	0x08:add hold phone to current phone
    */
    _uint8 type;
    /*
    1.length:value length must be 1
    */
    _uint8 length;
    /*
    1.value:
    	0x00:succedd
    	0x01:failed
    */
    _uint8 succeed_or_fail;
};

typedef enum {
    _GMA_HFP_CONNECT_STATE					= 0X00,
    _GMA_HFP_PHONE_NUMS						= 0X01,
    _GMA_HFP_ASW_A_PHONE					= 0X02,
    _GMA_HFP_HANG_UP_A_PHONE				= 0X03,
    _GMA_HFP_RECALL_LAST_PHONE				= 0X04,
    _GMA_HFP_ASW_ANOTHER_HPONE				= 0X05,
    _GMA_HFP_HANG_UP_HOLDING_PHONE			= 0X06,
    _GMA_HFP_HOLD_CUR_ASW_ANOTHER_HPONE		= 0X07,
    _GMA_HFP_ADD_HOLD2CUR_HPONE				= 0X08,

    _GMA_HFP_ERROR							= 0XFF,
} _GMA_HFP_CTL;

#define _GMA_FW_HFP_RES_INIT()  {_GMA_HFP_ERROR/*type*/,\
								1/*length*/,\
								_GMA_SUCCEED}

/**************************************************************
	moudle:			device base information
	author:			mqc
	description:
	data:			2019/06/12
	relative cmd:
					_GMA_CMD_APP_DEV_BASE_INFO		= 0X40,
					_GMA_CMD_FW_DEV_BASE_INFO		= 0X41,
**************************************************************/
/*
1.device base information
2.phone
*/
struct _gma_app_dev_base_info {
    /*
    1.type:
    	0x00:get battery value
    	0x01:get charge state
    */
    _uint8 type;
    /*
    1.length must be 0
    */
    _uint8 length;
};
/*
1.device base information
2.firmware
*/
struct _gma_fw_dev_base_info {
    /*
    1.type:
    	0x00:get battery value
    	0x01:get charge state
    */
    _uint8 type;
    /*
    1.length must be 2
    */
    _uint8 length;
    union {
        /*
        1.battery value(1byte;0x00 ~ 0x64) + low power or not(1byte;0x00:normal,0x01:low power)
        */
        _uint16 battery_value;
        /*
        1.charge state:
        	0x00:charging
        	0x01:using battery
        	0x02:using alternatin current
        */
        _uint8  charge_state;
        /*
        1.set fm fre res:
        	0x00:succeed
        	0x01:failed
         */
        _uint8  set_fm_fre_res;
        /*
        1.fm frequence string
         */
        _uint8 fm_fre_str[16];
        /*
        1.firmware verison :
        	0x00000002
         * */
        _uint8 fw_verison[16];
        /*
        1. bt name string:
         * */
        _uint8 bt_name_str[32];
        /*
        1.mic state:
        	0x00:useless
        	0x01:using
         * */
        _uint8 mic_state;
    } state;
};

typedef enum {
    _GMA_DEV_BASE_INFO_BATTERY_VALUE		= 0X00,
    _GMA_DEV_BASE_INFO_CHARGE_STATE			= 0X01,
    _GMA_DEV_BASE_INFO_FM_SET_RES			= 0X02,
    _GMA_DEV_BASE_INFO_CUR_FM_STR			= 0X03,
    _GMA_DEV_BASE_INFO_FW_VERSION			= 0X04,
    _GMA_DEV_BASE_INFO_BT_NAME_STR			= 0X05,
    _GMA_DEV_BASE_INFO_MIC_STATE			= 0X06,

    _GMA_DEV_BASE_INFO_ERROR				= 0XFF,
} _TM_GMA_DEV_BASE_INFO;

typedef enum {
    _GMA_DEV_CHARGE_STATE_CHARGING					= 0X00,
    _GMA_DEV_CHARGE_STATE_USING_BATTERY				= 0X01,
    _GMA_DEV_CHARGE_STATE_USING_ALTERNATIN_CURRENT	= 0X02,
} _TM_GMA_DEV_CHARGE_STATE;

typedef enum {
    _TM_GMA_DEV_BATTERY_VALUE_NORMAL		= 0X00,
    _TM_GMA_DEV_BATTERY_VALUE_LOW_POWER		= 0X01,
} _TM_GMA_DEV_LOW_POWER_OR_NOT;

#define _GMA_FW_DEV_BASE_INFO_INIT()  {_GMA_DEV_BASE_INFO_ERROR/*type*/,\
									2/*length*/,\
									{0X0000}/*value*/}

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
struct _gma_app_exception_notify {
    /*
    1.type:
    	0x00:phone do not connect to internet
    	0x01:phone reconnect internet succeed
    */
    _uint8 type;
    /*
    1.length must be 0
    */
    _uint8 length;
};
/*
1.exception information
2.phone
*/
struct _gma_fw_exception_rsp {
    /*
    1.type:
    	0x00:phone do not connect to internet
    	0x01:phone reconnect internet succeed
    */
    _uint8 type;
    /*
    1.length must be 1
    */
    _uint8 length;
    /*
    1.respond app action ok or not
    	0x00: succeed
    	0x01: fail
     * */
    _uint8 succeed_or_fail;
};

typedef enum {
    _GMA_PHONE_EXCEPTION_UNCONNECT_INTERNET		= 0x00,
    _GMA_PHONE_EXCEPTION_RECONNECT_INTERNET_OK	= 0x01,
} _GMA_PHONE_EXCEPTION_TYPE;
#define _GMA_FW_EXCEPTION_RSP_INIT()  {_GMA_PHONE_EXCEPTION_UNCONNECT_INTERNET/*type*/,1,_GMA_SUCCEED}

/*
	APP:CMD FROM PHONE
	FW: CMD FROM FIRMWARE
*/
typedef enum {
    /*exchange device information*/
    _GMA_CMD_APP_EXCHANGE_DEV_INFO  = 0X32,
    _GMA_CMD_FW_EXCHANGE_DEV_INFO	= 0X33,

    /*alive device*/
    _GMA_CMD_APP_ALIVE_DEV			= 0X34,
    _GMA_CMD_FW_ALIVE_DEV			= 0X35,

    /*firmware audio send*/
    _GMA_CMD_FW_AUDIO_SEND			= 0X30,

    /*firmware set state,reserved command*/
    _GMA_CMD_SET_STATE				= 0X31,

    /*app notify state when state haved changed*/
    _GMA_CMD_APP_NOTIFY_STATE		= 0X36,
    _GMA_CMD_FW_RESPOND_STATE		= 0x37,

    /*firmware notiry state when state haved changed*/
    _GMA_CMD_FW_NOTIFY_STATE		= 0X3E,
    _GMA_CMD_APP_RESPOND_STATE		= 0X3F,

    /*audio control*/
    _GMA_CMD_APP_AUDIO_CTL			= 0X38,
    _GMA_CMD_FM_AUDIO_RES			= 0X39,

    /*HFP function*/
    _GMA_CMD_APP_HFP_STATE			= 0X3A,
    _GMA_CMD_FW_HFP_STATE			= 0X3B,

    /*device base information*/
    _GMA_CMD_APP_DEV_BASE_INFO		= 0X40,
    _GMA_CMD_FW_DEV_BASE_INFO		= 0X41,

    /*phone exception notify*/
    _GMA_CMD_APP_EXCEPTION_NOTIFY	= 0x42,
    _GMA_CMD_FW_EXCEPTION_RES		= 0X43,

    /*authentication*/
    _GMA_CMD_APP_START_AUTH			= 0X10,
    _GMA_CMD_FW_RSP_AUTH			= 0X11,
    _GMA_CMD_AUTH_RESULT			= 0X12,
    _GMA_CMD_COUNT_BLE_KEY_RESULE	= 0X13,

    /*common command*/
    _GMA_CMD_COMMON_1				= 0X01,
    _GMA_CMD_COMMON_2				= 0X02,
    _GMA_CMD_COMMON_3				= 0X03,

    /*exception cmd*/
    _GMA_CMD_EXCEPTION_UPDATE		= 0X0F,
} _TM_GMA_CMD;
#endif
