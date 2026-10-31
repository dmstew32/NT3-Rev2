#ifndef _TX_ZDO_H_
#define _TX_ZDO_H_

#include <stdint.h>
#include "rx_zdo_handler.h"

// tx_zdo_bind_request_unicast() is not available here: the Z3Switch version needs its
// address_table module to look up the target's EUI64.
sl_status_t tx_zdo_ieee_addr_request(uint16_t dest_addr16, uint16_t target_addr16, zdo_response_cb cb);
sl_status_t tx_zdo_match_desc_request(uint16_t dest_addr16, uint16_t match_addr16, uint16_t profile_id,
                                      const uint16_t *p_in_clusters, uint8_t num_in_clusters,
                                      const uint16_t *p_out_clusters, uint8_t num_out_clusters,
                                      zdo_response_cb cb);

sl_status_t tx_zdo_mgmt_leave_request(uint16_t dest_addr16);
sl_status_t tx_zdo_permit_join_request(uint16_t dest_addr16, uint8_t duration_sec);
sl_status_t tx_zdo_simple_desc_request(uint16_t dest_addr16, uint16_t match_addr16, uint8_t endpoint,
                                        zdo_response_cb cb);


#endif  // #ifndef _TX_ZDO_H_
