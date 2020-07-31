
#define __BTSTACK_FILE__ "le_streamer_demo.c"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "system/app_core.h"
#include "system/includes.h"

#include "app_config.h"
#include "app_action.h"
#include "btstack/ble_data_types.h"
#include "btstack/ble_api.h"
#include "btstack/bluetooth.h"
#include "btstack/le_user.h"
#include "le_common.h"
#include "bt_common.h"

#include "le_streamer_demo.h"
#if (TCFG_BLE_DEMO_SELECT == DEF_BLE_DEMO_STREAMER)
/* #define LOG_TAG         "[APP]" */
/* #define LOG_INFO_ENABLE */
/* #define LOG_DUMP_ENABLE */
/* #define LOG_ERROR_ENABLE */
/* #include "common/debug.h" */
//------
#define ATT_LOCAL_PAYLOAD_SIZE    (200)                   //note: need >= 20
#define ATT_SEND_CBUF_SIZE        (512)                   //note: need >= 20
#define ATT_RAM_BUFSIZE           (ATT_CTRL_BLOCK_SIZE + ATT_LOCAL_PAYLOAD_SIZE + ATT_SEND_CBUF_SIZE)                   //note:
static u8 att_ram_buffer[ATT_RAM_BUFSIZE] __attribute__((aligned(4)));
//------



#define REPORT_INTERVAL_MS 3000
#define MAX_NR_CONNECTIONS 3

static void  packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void  cbk_sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static int   att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);
static void  streamer(void);
void bt_ble_adv_enable(u8 enable);
extern void le_l2cap_register_packet_handler(void (*handler)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size));

extern void le_device_db_init(void);


static hci_con_handle_t le_con_handle = 0;


const uint8_t ble_adv_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    // Name
    0x0c, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'L', 'E', ' ', 'S', 't', 'r', 'e', 'a', 'm', 'e', 'r',
    // Incomplete List of 16-bit Service Class UUIDs -- FF10 - only valid for testing!
    0x03, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x10, 0xff,
};
const uint8_t ble_rsp_data[] = {
    // Flags general discoverable, BR/EDR not supported
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, 0x06,
    // Name
    0x0c, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'L', 'E', ' ', 'S', 't', 'r', 'e', 'a', 'm', 'e', 'r',
    // Incomplete List of 16-bit Service Class UUIDs -- FF10 - only valid for testing!
    0x03, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x10, 0xff,
};
const uint8_t ble_adv_data_len = sizeof(ble_adv_data);
const uint8_t ble_rsp_data_len = sizeof(ble_rsp_data);

// support for multiple clients
typedef struct {
    char name;
    int le_notification_enabled;
    hci_con_handle_t connection_handle;
    int  counter;
    char test_data[ATT_DEFAULT_MTU - 3];
    int  test_data_len;
    uint32_t test_data_sent;
    uint32_t test_data_start;
} le_streamer_connection_t;
static le_streamer_connection_t le_streamer_connections[MAX_NR_CONNECTIONS];

// round robin sending
static int connection_index;

static void le_streamer_init_connections(void)
{
    // track connections
    int i;
    for (i = 0; i < MAX_NR_CONNECTIONS; i++) {
        le_streamer_connections[i].connection_handle = 0xffff;
        le_streamer_connections[i].name = 'A' + i;
    }
}

static le_streamer_connection_t *connection_for_conn_handle(hci_con_handle_t conn_handle)
{
    int i;
    for (i = 0; i < MAX_NR_CONNECTIONS; i++) {
        if (le_streamer_connections[i].connection_handle == conn_handle) {
            return &le_streamer_connections[i];
        }
    }
    return NULL;
}

static void next_connection_index(void)
{
    connection_index++;
    if (connection_index == MAX_NR_CONNECTIONS) {
        connection_index = 0;
    }
}

/* @section Main Application Setup
 *
 * @text Listing MainConfiguration shows main application code.
 * It initializes L2CAP, the Security Manager, and configures the ATT Server with the pre-compiled
 * ATT Database generated from $le_streamer.gatt$. Finally, it configures the advertisements
 * and boots the Bluetooth stack.
 */

/* LISTING_START(MainConfiguration): Init L2CAP, SM, ATT Server, and enable advertisements */

/* LISTING_END */

/*
 * @section Track throughput
 * @text We calculate the throughput by setting a start time and measuring the amount of
 * data sent. After a configurable REPORT_INTERVAL_MS, we print the throughput in kB/s
 * and reset the counter and start time.
 */

/* LISTING_START(tracking): Tracking throughput */
static void test_reset(le_streamer_connection_t *context)
{
    context->test_data_start = 0;   //  sys_timer_get_ms();
    context->test_data_sent = 0;
}

extern u32 sys_timer_get_ms(void);
static void test_track_sent(le_streamer_connection_t *context, int bytes_sent)
{
    context->test_data_sent += bytes_sent;
    // evaluate
    uint32_t now = sys_timer_get_ms() * 1000;
    uint32_t time_passed = now - context->test_data_start;

    if (time_passed < REPORT_INTERVAL_MS) {
        return;
    }
    // print speed
    int bytes_per_second = context->test_data_sent * 1000 / time_passed;
    printf("%c: %u bytes sent-> %u.%03u kB/s\n", context->name, context->test_data_sent, bytes_per_second / 1000, bytes_per_second % 1000);

    // restart
    context->test_data_start = now;
    context->test_data_sent  = 0;
}

_WEAK_
u8 ble_update_get_ready_jump_flag(void)
{
    return 0;
}

/* LISTING_END(tracking): Tracking throughput */

/*
 * @section Packet Handler
 *
 * @text The packet handler is used to stop the notifications and reset the MTU on connect
 * It would also be a good place to request the connection parameter update as indicated
 * in the commented code block.
 */

/* LISTING_START(packetHandler): Packet Handler */
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    int mtu;
    uint16_t conn_interval;
    le_streamer_connection_t *context;
    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:

            printf("le demo DISCONNECT\n");

            le_con_handle = 0;
            ble_user_cmd_prepare(BLE_CMD_ATT_SEND_INIT, 4, le_con_handle, 0, 0, 0);

            context = connection_for_conn_handle(hci_event_disconnection_complete_get_connection_handle(packet));
            if (!context) {
                break;
            }
            // free connection
            /* log_info("%c: Disconnect, reason %02x\n", context->name, hci_event_disconnection_complete_get_reason(packet)); */
            context->le_notification_enabled = 0;
            context->connection_handle = HCI_CON_HANDLE_INVALID;

            if (!ble_update_get_ready_jump_flag()) {
                bt_ble_adv_enable(1);
            }

            break;
        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
            case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                // setup new
                context = connection_for_conn_handle(HCI_CON_HANDLE_INVALID);
                if (!context) {
                    break;
                }
                context->counter = 'A';
                context->test_data_len = ATT_DEFAULT_MTU - 3;
                context->connection_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);

                le_con_handle = context->connection_handle;


                ble_user_cmd_prepare(BLE_CMD_ATT_SEND_INIT, 4, le_con_handle, att_ram_buffer, ATT_RAM_BUFSIZE, ATT_LOCAL_PAYLOAD_SIZE);

                // print connection parameters (without using float operations)
                conn_interval = hci_subevent_le_connection_complete_get_conn_interval(packet);


                printf("context handle %x\n", context->connection_handle);


                /* log_info("%c: Connection Interval: %u.%02u ms\n", context->name, conn_interval * 125 / 100, 25 * (conn_interval & 3)); */
                /* log_info("%c: Connection Latency: %u\n", context->name, hci_subevent_le_connection_complete_get_conn_latency(packet)); */
                // min con interval 20 ms
                /* gap_request_connection_parameter_update(le_con_handle , 0x20, 0x28, 0, 300); */
                // log_info("Connected, requesting conn param update for handle 0x%04x\n", connection_handle);
                break;
            }
            break;
        case ATT_EVENT_MTU_EXCHANGE_COMPLETE:
            mtu = att_event_mtu_exchange_complete_get_MTU(packet) - 3;
            context = connection_for_conn_handle(att_event_mtu_exchange_complete_get_handle(packet));
            if (!context) {
                break;
            }

            printf("ATT MTU = %u\n", mtu);
            ble_user_cmd_prepare(BLE_CMD_ATT_MTU_SIZE, 1, mtu);

            /* context->test_data_len = ble_min(mtu - 3, sizeof(context->test_data)); */
            /* log_info("%c: ATT MTU = %u => use test data of len %u\n", context->name, mtu, context->test_data_len); */
            break;
        case ATT_EVENT_CAN_SEND_NOW:
            /* printf("att send\n"); */
            streamer();
            break;

        case L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_RESPONSE:
            break;
        }
    }
}

static void cbk_sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_JUST_WORKS_REQUEST: {
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
        }
            /* log_info("Just Works Confirmed.\n"); */
        break;
        }
    }
}
static int get_buffer_vaild_len(void *priv)
{
    u32 vaild_len = 0;
    ble_user_cmd_prepare(BLE_CMD_ATT_VAILD_LEN, 1, &vaild_len);
    return vaild_len;
}

static int app_send_user_data(u16 handle, u8 *data, u16 len, u8 handle_type)
{
    u32 ret = APP_BLE_NO_ERROR;

    if (!le_con_handle) {
        return APP_BLE_OPERATION_ERROR;
    }

    ret = ble_user_cmd_prepare(BLE_CMD_ATT_SEND_DATA, 4, handle, data, len, handle_type);
    if (ret == BLE_BUFFER_FULL) {
        ret = APP_BLE_BUFF_FULL;
    }

    if (ret) {
        printf("app_send_fail:%d !!!!!!\n", ret);
    }
    return ret;
}



/* LISTING_END */
/*
 * @section Streamer
 *
 * @text The streamer function checks if notifications are enabled and if a notification can be sent now.
 * It creates some test data - a single letter that gets increased every time - and tracks the data sent.
 */

/* LISTING_START(streamer): Streaming code */
static void streamer(void)
{

    // find next active streaming connection
    int old_connection_index = connection_index;
    while (1) {
        // active found?
        if ((le_streamer_connections[connection_index].connection_handle != HCI_CON_HANDLE_INVALID) &&
            (le_streamer_connections[connection_index].le_notification_enabled)) {
            break;
        }

        // check next
        next_connection_index();

        // none found
        if (connection_index == old_connection_index) {
            return;
        }
    }

    le_streamer_connection_t *context = &le_streamer_connections[connection_index];

    // create test data
    context->counter++;
    if (context->counter > 'Z') {
        context->counter = 'A';
    }
    /* printf("conunt %d\n",context->counter); */
    memset(context->test_data, context->counter, sizeof(context->test_data));

    // send
//att_server_notify(context->connection_handle, ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t *) context->test_data, context->test_data_len);

    u16 vaild_len = get_buffer_vaild_len(0);

    if (vaild_len) {
        app_send_user_data(ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t *) context->test_data, vaild_len, ATT_OP_AUTO_READ_CCC);
    } else {
        return;
    }
    // track
    test_track_sent(context, vaild_len);

    // request next send event
//    att_server_request_can_send_now_event(context->connection_handle);

    // check next
    next_connection_index();
}
/* LISTING_END */

/*
 * @section ATT Write
 *
 * @text The only valid ATT write in this example is to the Client Characteristic Configuration, which configures notification
 * and indication. If the ATT handle matches the client configuration handle, the new configuration value is stored.
 * If notifications get enabled, an ATT_EVENT_CAN_SEND_NOW is requested. See Listing attWrite.
 */

/* LISTING_START(attWrite): ATT Write */
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size)
{

    // log_info("att_write_callback att_handle %04x, transaction mode %u\n", att_handle, transaction_mode);
    if (transaction_mode != ATT_TRANSACTION_MODE_NONE) {
        return 0;
    }
    le_streamer_connection_t *context = connection_for_conn_handle(con_handle);
    switch (att_handle) {
    case ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_CLIENT_CONFIGURATION_HANDLE:
        att_set_ccc_config(att_handle, buffer[0]);
        printf("att client cof handle\n");

        context->le_notification_enabled = little_endian_read_16(buffer, 0) == GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION;
        /* log_info("%c: Notifications enabled %u\n", context->name, context->le_notification_enabled); */
        /* log_info_hexdump(buffer, buffer_size); */
        if (context->le_notification_enabled) {
            att_server_request_can_send_now_event(context->connection_handle);
        }
        test_reset(context);


        break;
    case ATT_CHARACTERISTIC_0000FF12_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE:
        /* log_info("%c: Write to ...FF12... : ", context->name); */
        /* log_info_hexdump(buffer, buffer_size); */
        break;
    }
    return 0;
}

static void ble_adv_setup_init(void)
{
    u8  adv_type = 0;
    u8  adv_channel  = 0x07;
    u16 adv_interval = 0x30;

    ble_user_cmd_prepare(BLE_CMD_ADV_PARAM, 3, adv_interval, adv_type, adv_channel);
    ble_user_cmd_prepare(BLE_CMD_ADV_DATA, 2, ble_adv_data_len, ble_adv_data);
    ble_user_cmd_prepare(BLE_CMD_RSP_DATA, 2, ble_rsp_data_len, ble_rsp_data);
}

void bt_ble_adv_enable(u8 enable)
{
    if (enable) {
        ble_adv_setup_init();
    }
    ble_user_cmd_prepare(BLE_CMD_ADV_ENABLE, 1, enable);
}

void ble_sm_setup_init(io_capability_t io_type, u8 auth_req, uint8_t min_key_size, u8 security_en)
{
    //setup SM: Display only
    sm_init();
    sm_set_io_capabilities(io_type);
    sm_set_authentication_requirements(auth_req);
    sm_set_encryption_key_size_range(min_key_size, 16);
    sm_set_request_security(security_en);
    sm_event_callback_set(&cbk_sm_packet_handler);
}


void ble_profile_init(void)
{
    printf("ble profile init\n");


    le_device_db_init();


    ble_sm_setup_init(IO_CAPABILITY_NO_INPUT_NO_OUTPUT, SM_AUTHREQ_BONDING, 7, TCFG_BLE_SECURITY_EN);

    /* setup ATT server */
    att_server_init(profile_data, NULL, att_write_callback);
    att_server_register_packet_handler(packet_handler);

    // setup advertisements
    le_streamer_init_connections();

    // register for HCI events
    hci_event_callback_set(&packet_handler);


    le_l2cap_register_packet_handler(&packet_handler);


}

void bt_ble_init(void)
{
    printf("***** ble_init******\n");
    bt_ble_adv_enable(1);

}

void bt_ble_exit(void)
{
    printf("***** ble_exit******\n");
    bt_ble_adv_enable(0);
}

void input_key_handler(u8 key_status, u8 key_number)
{
}

#endif
