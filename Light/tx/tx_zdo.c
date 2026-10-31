#include <assert.h>
#include <string.h>
#include "byte_utils.h"
#include "network_defines.h"
#include "rx_zdo_handler.h"
#include "sys_defines.h"
#include "tx_packet.h"
#include "tx_zdo.h"
#include "zigbee-device-stack.h"


sl_status_t tx_zdo_ieee_addr_request(uint16_t dest_addr16, uint16_t target_addr16, zdo_response_cb cb)
{
    uint8_t buffer[4] =
    {
        U16_LSB_TO_U8(target_addr16),
        U16_MSB_TO_U8(target_addr16),
        0,      // request type (0=single)
        0       // start index
    };
    uint8_t tsn = 0;
    sl_status_t status = tx_packet_send_zdo_unicast(dest_addr16, IEEE_ADDRESS_REQUEST,
                                      buffer, ARRAY_SIZE(buffer), &tsn);
    if((status == SL_STATUS_OK) && (cb != NULL))
    {
        status = rx_zdo_register_zdo_callback(dest_addr16, IEEE_ADDRESS_REQUEST, tsn, cb);
    }
    return status;
}

sl_status_t tx_zdo_match_desc_request(uint16_t dest_addr16, uint16_t match_addr16, uint16_t profile_id,
                                      const uint16_t *p_in_clusters, uint8_t num_in_clusters,
                                      const uint16_t *p_out_clusters, uint8_t num_out_clusters,
                                      zdo_response_cb cb)
{
    // [Nwk Addr (2)] + [Profile ID (2)] + [NumInClusters] + [InClusters[]] + [NumOutClusters] + [OutClusters[]]
    #define _MAX_MATCH_DESCR_REQ_SZ  (6 + (_MAX_CLUSTER_IDS * 2))
    #define _MAX_CLUSTER_IDS   12
    uint8_t buffer[_MAX_MATCH_DESCR_REQ_SZ];

    if((num_in_clusters + num_out_clusters) > _MAX_CLUSTER_IDS)
    {
        return SL_STATUS_FAIL;
    }

    bytes_packer_t packer = BYTES_PACKER(buffer, ARRAY_SIZE(buffer));
    bytes_pack_u16_le(&packer, match_addr16);
    bytes_pack_u16_le(&packer, profile_id);
    bytes_pack_u8(&packer, num_in_clusters);
    for(uint8_t i=0; i<num_in_clusters; i++)
    {
        if((p_in_clusters != NULL) && (p_in_clusters[i] != INVALID_CLUSTER_ID))
        {
            bytes_pack_u16_le(&packer, p_in_clusters[i]);
        }
    }
    bool packed = bytes_pack_u8(&packer, num_out_clusters);
    for(uint8_t i=0; i<num_out_clusters; i++)
    {
        if((p_out_clusters != NULL) && (p_out_clusters[i] != INVALID_CLUSTER_ID))
        {
            packed = bytes_pack_u16_le(&packer, p_out_clusters[i]);
        }
    }
    if(packed == false)
    {
        return SL_STATUS_FAIL;
    }
    uint8_t tsn = 0;
    sl_status_t status = tx_packet_send_zdo_unicast(dest_addr16, MATCH_DESCRIPTORS_REQUEST,
                                                    buffer, packer.index, &tsn);
    if((status == SL_STATUS_OK) && (cb != NULL))
    {
        status = rx_zdo_register_zdo_callback(dest_addr16, MATCH_DESCRIPTORS_REQUEST, tsn, cb);
    }
    return status;
}

sl_status_t tx_zdo_mgmt_leave_request(uint16_t dest_addr16)
{
    uint8_t data[9] = {0};
    return tx_packet_send_zdo_unicast(dest_addr16, LEAVE_REQUEST,
                                      data, ARRAY_SIZE(data), NULL);
}

sl_status_t tx_zdo_permit_join_request(uint16_t dest_addr16, uint8_t duration_sec)
{
    uint8_t buffer[] = { duration_sec, 0 };
    return tx_packet_send_zdo_unicast(dest_addr16, PERMIT_JOINING_REQUEST,
                                      buffer, ARRAY_SIZE(buffer), NULL);

}

sl_status_t tx_zdo_simple_desc_request(uint16_t dest_addr16, uint16_t match_addr16, uint8_t endpoint,
                                        zdo_response_cb cb)
{
    uint8_t buffer[3] =
    {
        U16_LSB_TO_U8(match_addr16),
        U16_MSB_TO_U8(match_addr16),
        endpoint
    };
    uint8_t tsn = 0;
    sl_status_t status = tx_packet_send_zdo_unicast(dest_addr16, SIMPLE_DESCRIPTOR_REQUEST,
                                                    buffer, ARRAY_SIZE(buffer), &tsn);
    if((status == SL_STATUS_OK) && (cb != NULL))
    {
        status = rx_zdo_register_zdo_callback(dest_addr16, SIMPLE_DESCRIPTOR_REQUEST, tsn, cb);
    }
    return status;
}
