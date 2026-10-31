#ifndef _TX_PACKET_H_
#define _TX_PACKET_H_

#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"
#include "zcl_header.h"


//sl_status_t tx_packet_send_binding_zcl(zcl_header_t *p_zcl_header, uint16_t clusterId,
  //                                     uint16_t profileId, uint8_t *p_data, uint8_t data_length,
    //                                   bool unicast_only);

sl_status_t tx_packet_send_broadcast_zcl(zcl_header_t *p_zcl_header, uint16_t destAddr16,
                                        uint8_t srcEndpoint, uint8_t dstEndpoint,
                                        uint16_t clusterId, uint16_t profileId,
                                        uint8_t *p_data, uint8_t data_length);

sl_status_t tx_packet_send_unicast_zcl(zcl_header_t *p_zcl_header, uint16_t destAddr16,
                                       uint8_t srcEndpoint, uint8_t dstEndpoint,
                                       uint16_t clusterId, uint16_t profileId,
                                       uint8_t *p_data, uint8_t data_length);

sl_status_t tx_packet_send_multicast_zcl(zcl_header_t *p_zcl_header, uint16_t groupId, uint8_t srcEndpoint,
                                         uint16_t clusterId, uint16_t profileId,
                                         uint8_t *p_data, uint8_t data_length);


sl_status_t tx_packet_send_zdo_unicast(uint16_t destAddr16, uint16_t clusterId,
                                       uint8_t *p_data, uint8_t length, uint8_t *p_assigned_tsn_out);

//bool tx_packet_have_coord_binding(uint16_t clusterId);
//void tx_packet_force_response_sequence_number(uint8_t sequence_number);

#endif  // #ifndef _TX_PACKET_H_
