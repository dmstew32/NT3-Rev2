#ifndef _TX_QUEUE_H_
#define _TX_QUEUE_H_

#include <stdint.h>
#include "mac-types.h"
#include "sl_zigbee_types.h"

void tx_queue_on_startup(void);
void tx_queue_on_mark_buffers_callback(void);

sl_status_t tx_queue_add_unicast(uint16_t destAddr16, uint8_t srcEndpoint,
                          uint8_t dstEndpoint, uint16_t clusterId,
                          uint16_t profileId, uint8_t *p_data,
                          uint8_t data_length);

sl_status_t tx_queue_add_multicast(uint16_t groupId, uint8_t srcEndpoint,
                            uint16_t clusterId, uint16_t profileId,
                            uint8_t *p_data, uint8_t data_length);

#endif  // #ifndef _TX_QUEUE_H_
