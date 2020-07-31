#ifndef _GMA_LICENSE_H
#define _GMA_LICENSE_H

typedef struct {
    uint8_t mac[6];
    uint32_t pid;
    uint8_t secret[16];
} ali_para_s;

void gma_bt_mac_addr_get(uint8_t *buf);
void gma_mac_addr_get(uint8_t *buf);
void gma_active_para_by_hci_para(void);
void gma_active_local_para(void);
void gma_slave_sync_remote_addr(u8 *buf);
bool gma_sibling_mac_get(uint8_t *buf);
void gma_send_secret_to_sibling(void);
#endif
