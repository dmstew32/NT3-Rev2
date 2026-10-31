#ifndef _RX_ZDO_HANDLER_H_
#define _RX_ZDO_HANDLER_H_

#include <stdint.h>
#include "sl_status.h"
#include "sl_zigbee_types.h"

#define ZDO_MATCH_DESC_RESP_MAX_ENDPOINTS  4
#define ZDO_SIMPLE_DESC_RESP_MAX_CLUSTERS 10

typedef struct
{
    uint8_t ieee_addr[8];
    uint16_t addr16;
} zdo_addr_rsp_t;

typedef struct
{
    uint16_t addr16;
    uint8_t ieee_addr[8];
    uint8_t capability;
            // 0x02 - FFD
            // 0x04 - Mains Power
            // 0x08 - Rx On When Idle
            // 0x80 - Allocate Addr
} zdo_dev_announce_t;

typedef struct
{
    uint16_t addr16;
    uint8_t num_endpoints;
    uint8_t endpoints[ZDO_MATCH_DESC_RESP_MAX_ENDPOINTS];
} zdo_match_desc_rsp_t;


typedef struct
{
    uint8_t duration_sec;
    uint8_t tc_significance;
} zdo_permit_join_req_t;

typedef struct
{
    uint16_t addr16;
    uint16_t device_id;
    uint16_t profile_id;
    uint8_t endpoint;
    uint8_t num_in_clusters;
    uint8_t num_out_clusters;
    uint16_t in_clusters[ZDO_SIMPLE_DESC_RESP_MAX_CLUSTERS];
    uint16_t out_clusters[ZDO_SIMPLE_DESC_RESP_MAX_CLUSTERS];
} zdo_simple_desc_rsp_t;

typedef struct
{
    uint16_t cluster_id;
    union
    {
        zdo_addr_rsp_t addr_rsp;        // ieee addr rsp and nwk addr rsp have the same payload
        zdo_dev_announce_t dev_announce;
        zdo_match_desc_rsp_t match_desc_rsp;
        zdo_permit_join_req_t permit_join_req;
        zdo_simple_desc_rsp_t simple_desc_rsp;
    } u;
} zdo_response_t;

typedef void (*zdo_response_cb)(sl_zigbee_zdo_status_t status, uint16_t sender_addr16, zdo_response_t *p_rsp);

void rx_zdo_on_startup(void);
void rx_zdo_rx_handler(uint16_t sender, sl_zigbee_aps_frame_t *apsFrame,
                       uint8_t *p_msg, uint8_t msg_len);

// Incoming ZDO commands (device announce, permit join request, ...) from any sender.
// Registered once at startup; these entries are never evicted.
sl_status_t rx_zdo_register_zdo_cmd_callback(uint16_t cluster_id, zdo_response_cb cb);

// Response to a request this device sent (matched on sender, cluster and TSN).
sl_status_t rx_zdo_register_zdo_callback(uint16_t sender, uint16_t cluster_id,
                                         uint8_t transaction_seq_num, zdo_response_cb cb);

#endif // #ifndef _RX_ZDO_HANDLER_H_
