#include "system/includes.h"
#include "app_config.h"
#include "btcontroller_config.h"
#include "btstack/bt_profile_config.h"
#include "bt_common.h"

#define LOG_TAG     "[BT-CFG]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#include "debug.h"


typedef struct {
    // linked list - assert: first field
    void *offset_item;

    // data is contained in same memory
    u32        service_record_handle;
    u8         *service_record;
} service_record_item_t;

extern const u8 sdp_pnp_service_data[];
extern const u8 sdp_a2dp_service_data[];
extern const u8 sdp_avctp_ct_service_data[];
extern const u8 sdp_avctp_ta_service_data[];
extern const u8 sdp_hfp_service_data[];
extern const u8 sdp_spp_service_data[];
extern const u8 sdp_dueros_spp_service_data[];
extern const u8 sdp_hid_service_data[];
extern const u8 sdp_gma_spp_service_data[];
extern service_record_item_t  sdp_record_item_begin[];
extern service_record_item_t  sdp_record_item_end[];

#define SDP_RECORD_HANDLER_REGISTER(handler) \
	const service_record_item_t  handler \
		sec(.sdp_record_item)

#if TCFG_USER_BLE_ENABLE

#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_ADV)
const int config_stack_modules = (BT_BTSTACK_CLASSIC | BT_BTSTACK_LE_ADV);
#else
const int config_stack_modules = BT_BTSTACK_CLASSIC | BT_BTSTACK_LE;
#endif

#else
const int config_stack_modules = BT_BTSTACK_CLASSIC;
#endif

#if (USER_SUPPORT_PROFILE_PNP==1)
SDP_RECORD_HANDLER_REGISTER(pnp_sdp_record_item) = {
    .service_record = (u8 *)sdp_pnp_service_data,
    .service_record_handle = 0x1000A,
};
#endif

#if (USER_SUPPORT_PROFILE_A2DP==1)
u8 a2dp_profile_support = 1;
SDP_RECORD_HANDLER_REGISTER(a2dp_sdp_record_item) = {
    .service_record = (u8 *)sdp_a2dp_service_data,
    .service_record_handle =  0x00010001,
};
#endif
#if (USER_SUPPORT_PROFILE_AVCTP==1)
u8 acp_profile_support = 1;
SDP_RECORD_HANDLER_REGISTER(arp_ct_sdp_record_item) = {
    .service_record = (u8 *)sdp_avctp_ct_service_data,
    .service_record_handle = 0x00010002,
};
#if BT_SUPPORT_MUSIC_VOL_SYNC
SDP_RECORD_HANDLER_REGISTER(arp_ta_sdp_record_item) = {
    .service_record = (u8 *)sdp_avctp_ta_service_data,
    .service_record_handle = 0x00010005,
};
#endif
#endif
#if (USER_SUPPORT_PROFILE_HFP==1)
u8 hfp_profile_support = 1;
SDP_RECORD_HANDLER_REGISTER(hfp_sdp_record_item) = {
    .service_record = (u8 *)sdp_hfp_service_data,
    .service_record_handle = 0x00010003,
};
#endif
#if (USER_SUPPORT_PROFILE_SPP==1)
u8 spp_profile_support = 1;
SDP_RECORD_HANDLER_REGISTER(spp_sdp_record_item) = {
#if DUEROS_DMA_EN
    .service_record = (u8 *)sdp_dueros_spp_service_data,
#elif GMA_EN
    .service_record = (u8 *)sdp_gma_spp_service_data,
#else
    .service_record = (u8 *)sdp_spp_service_data,
#endif
    .service_record_handle = 0x00010004,
};
#endif
#if (USER_SUPPORT_PROFILE_HID==1)
u8 hid_profile_support = 1;
SDP_RECORD_HANDLER_REGISTER(hid_sdp_record_item) = {
    .service_record = (u8 *)sdp_hid_service_data,
    .service_record_handle = 0x00010006,
};
#endif
#if (USER_SUPPORT_PROFILE_PBAP==1)
extern const u8 sdp_pbap_service_data[];
u8 pbap_profile_support = 1;
SDP_RECORD_HANDLER_REGISTER(pbap_sdp_record_item) = {
    .service_record = (u8 *)sdp_pbap_service_data,
    .service_record_handle = 0x00010007,
};
#endif

const u8 hid_conn_depend_on_dev_company = 1;
const u8 sdp_get_remote_pnp_info = 0;


#if ((TCFG_USER_BLE_ENABLE) && (TCFG_BLE_DEMO_SELECT != DEF_BLE_DEMO_ADV))
u8 app_le_pool[1850] sec(.btstack_pool)  ALIGNED(4);
#endif

u8 app_bredr_pool[1640] sec(.btstack_pool) ALIGNED(4);
u8 app_l2cap_pool[70] sec(.btstack_pool) ALIGNED(4);
u8 app_bredr_profile[1100] sec(.btstack_pool) ALIGNED(4);

u8 *get_bredr_pool_addr(void)
{
    u16 len = 0;

    if (STACK_MODULES_IS_SUPPORT(BT_BTSTACK_CLASSIC)) {
        len = get_bredr_pool_len();
        printf("bredr pool len %d\n", len);
        if (len > sizeof(app_bredr_pool)) {
            ASSERT(0, "bredr_pool is small\n");
        }

        return &app_bredr_pool;
    }

    return NULL;
}

u8 *get_le_pool_addr(void)
{
    u16 len = 0;

#if ((TCFG_USER_BLE_ENABLE) && (TCFG_BLE_DEMO_SELECT != DEF_BLE_DEMO_ADV))
    if (STACK_MODULES_IS_SUPPORT(BT_BTSTACK_LE)) {
        len = get_le_pool_len();
        printf("le pool len %d\n", len);
        if (len > sizeof(app_le_pool)) {
            ASSERT(0, "le_pool is small\n");
        }

        return &app_le_pool;
    }
#endif
    return NULL;
}

u8 *get_l2cap_stack_addr(void)
{
    u16 len = 0;

    len = get_l2cap_stack_len();
    printf("l2cap stack len %d\n", len);
    if (len > sizeof(app_l2cap_pool)) {
        ASSERT(0, "l2cap pool is small\n");
    }

    return &app_l2cap_pool;
}

u8 *get_profile_pool_addr(void)
{
    u16 len = 0;

    if (STACK_MODULES_IS_SUPPORT(BT_BTSTACK_CLASSIC)) {

        len = get_profile_pool_len();
        printf("bredr profile pool len %d\n", len);
        if (len > sizeof(app_bredr_profile)) {
            ASSERT(0, "bredr_profile is small\n");
        }

        return &app_bredr_profile;
    }
    return NULL;
}

#if (TCFG_BD_NUM == 2)
const u8 a2dp_mutual_support = 1;
const u8 more_addr_reconnect_support = 1;
const u8 more_hfp_cmd_support = 1;
const u8 more_avctp_cmd_support = 1;
#else
const u8 a2dp_mutual_support = 0;
const u8 more_addr_reconnect_support = 0;
const u8 more_hfp_cmd_support = 0;
const u8 more_avctp_cmd_support = 0;
#endif
#if TCFG_USER_EMITTER_ENABLE
const u8 hci_inquiry_support = 1;
const u8 btstack_emitter_support  = 1;  /*定义用于优化代码编译*/
extern const u8 sdp_a2dp_source_service_data[];
//u8 a2dp_profile_support = 1;
SDP_RECORD_HANDLER_REGISTER(a2dp_src_sdp_record_item) = {
    .service_record = (u8 *)sdp_a2dp_source_service_data,
    .service_record_handle =  0x0001000B,
};
#if (USER_SUPPORT_PROFILE_HFP_AG==1)
extern const u8 sdp_hfp_ag_service_data[];
u8 hfp_ag_profile_support = 1;
SDP_RECORD_HANDLER_REGISTER(hfp_ag_sdp_record_item) = {
    .service_record = (u8 *)sdp_hfp_ag_service_data,
    .service_record_handle = 0x00010008,
};
#endif
#else
const u8 hci_inquiry_support = 0;
const u8 btstack_emitter_support  = 0;  /*定义用于优化代码编译*/
#endif
/*u8 l2cap_debug_enable = 0xf0;
u8 rfcomm_debug_enable = 0xf;
u8 profile_debug_enable = 0xff;
u8 ble_debug_enable    = 0xff;
u8 btstack_tws_debug_enable = 0xf;*/
