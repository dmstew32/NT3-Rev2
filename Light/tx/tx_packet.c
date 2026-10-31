#include <assert.h>
#include <string.h>

#include "byte_utils.h"
#include "sys_defines.h"
#include "tx_packet.h"
#include "tx_queue.h"

#include "sl_zigbee_types.h"

static uint8_t m_packet[LARGEST_PACKET_PAYLOAD_SZ];
static uint8_t m_packet_length = 0;

//static uint8_t m_aps_seq_number = 0;
//static uint8_t m_zcl_tsn = 0;
static uint8_t m_zdo_seq_number = 0;

//static EmberStatus _set_zcl_payload(zcl_header_t *p_zcl_header, uint8_t *p_data,
  //                                  uint8_t data_length, uint8_t *p_zcl_payload_sz_out);
//static EmberStatus _tx_send_multicast_buffer(uint16_t groupId, uint8_t *p_buffer, uint16_t length,
  //                                           uint8_t srcEndpoint, uint16_t clusterId, uint16_t profileId);

#if 0
EmberStatus tx_packet_send_binding_zcl(zcl_header_t *p_zcl_header, uint16_t clusterId,
                                       uint16_t profileId, uint8_t *p_data, uint8_t data_length,
                                       bool unicast_only)
{
    EmberStatus status = EMBER_NOT_FOUND;
    // send to all binding table entries that support this cluster
    for(uint8_t i=0; i<EMBER_BINDING_TABLE_SIZE; i++)
    {
        i = binding_table_get_next_matching_index(i, clusterId);
        if(i < EMBER_BINDING_TABLE_SIZE)
        {
            EmberBindingTableEntry entry = {EMBER_UNUSED_BINDING,0};
            EmberStatus status = binding_table_get_binding(&entry, i);
            if(status == EMBER_SUCCESS)
            {
                if(entry.type == EMBER_UNICAST_BINDING)
                {
                    uint16_t destAddr16 = binding_table_get_remote_addr16(i);
                    status = tx_packet_send_unicast_zcl(p_zcl_header, destAddr16,
                                               APP_ENDPOINT, entry.remote,
                                               clusterId, profileId,
                                               p_data, data_length);
                    emberAfCorePrintln("Tx Unicast[%d] to 0x%2x, cid=0x%2x, ep=%d",
                                        i, destAddr16, entry.clusterId, entry.remote);
                }
                else if((entry.type == EMBER_MULTICAST_BINDING) && (unicast_only == false))
                {
                    // 64-bit id is the group address
                    //tx_packet_send_binding_zcl(&zcl_header, i, NULL, 0);
                    uint16_t groupId = BYTES_LE_TO_U16(entry.identifier[0], entry.identifier[1]);

                    status = tx_packet_send_multicast_zcl(p_zcl_header, groupId, entry.local,
                                                 clusterId, profileId, p_data, data_length);
                    emberAfCorePrintln("Tx Binding[%d] to group 0x%2x, cid=0x%2x",
                                        i, groupId, clusterId);
                }
            }
        }
    }
    return status;
}

bool tx_packet_have_coord_binding(uint16_t clusterId)
{
    return binding_table_have_coord_binding(clusterId);
}
#endif

sl_status_t tx_packet_send_broadcast_zcl(zcl_header_t *p_zcl_header, uint16_t destAddr16,
                                        uint8_t srcEndpoint, uint8_t dstEndpoint,
                                        uint16_t clusterId, uint16_t profileId,
                                        uint8_t *p_data, uint8_t data_length)
{
    return tx_packet_send_unicast_zcl(p_zcl_header, destAddr16, srcEndpoint, dstEndpoint,
                                      clusterId, profileId, p_data, data_length);
}

sl_status_t tx_packet_send_unicast_zcl(zcl_header_t *p_zcl_header, uint16_t destAddr16,
                                       uint8_t srcEndpoint, uint8_t dstEndpoint,
                                       uint16_t clusterId, uint16_t profileId,
                                       uint8_t *p_data, uint8_t data_length)
{
    uint8_t zcl_header_len = zcl_header_to_data(m_packet, ARRAY_SIZE(m_packet), p_zcl_header);
    m_packet_length = zcl_header_len + data_length;
    sl_status_t status = SL_STATUS_MESSAGE_TOO_LONG;

    if((zcl_header_len != 0) && (m_packet_length <= ARRAY_SIZE(m_packet)))
    {
        memcpy(&m_packet[zcl_header_len], p_data, data_length);
        status = tx_queue_add_unicast(destAddr16, srcEndpoint, dstEndpoint,
                                      clusterId, profileId, m_packet,
                                      m_packet_length);
    }
    return status;
}

sl_status_t tx_packet_send_multicast_zcl(zcl_header_t *p_zcl_header, uint16_t groupId, uint8_t srcEndpoint,
                                         uint16_t clusterId, uint16_t profileId,
                                         uint8_t *p_data, uint8_t data_length)
{
    uint8_t zcl_header_len = zcl_header_to_data(m_packet, ARRAY_SIZE(m_packet), p_zcl_header);
    m_packet_length = zcl_header_len + data_length;
    sl_status_t status = SL_STATUS_MESSAGE_TOO_LONG;

    if((zcl_header_len != 0) && (m_packet_length <= ARRAY_SIZE(m_packet)))
    {
        memcpy(&m_packet[zcl_header_len], p_data, data_length);
        status = tx_queue_add_multicast(groupId, srcEndpoint, clusterId, profileId,
                                        m_packet, m_packet_length);
    }
    return status;
}

sl_status_t tx_packet_send_zdo_unicast(uint16_t destAddr16, uint16_t clusterId,
                                       uint8_t *p_data, uint8_t length, uint8_t *p_assigned_tsn_out)
{
    // TSN is managed here, not by the application.
    // All other data must be provided by the caller.
    assert(p_data != NULL);

    sl_status_t status = SL_STATUS_MESSAGE_TOO_LONG;
    if(length < ARRAY_SIZE(m_packet)-1)
    {
        m_zdo_seq_number++;
        m_packet[0] = m_zdo_seq_number;
        memcpy(&m_packet[1], p_data, length);
        length++;
        if(p_assigned_tsn_out != NULL)
        {
            *p_assigned_tsn_out = m_zdo_seq_number;
        }
        status = tx_queue_add_unicast(destAddr16, SL_ZIGBEE_ZDO_ENDPOINT, SL_ZIGBEE_ZDO_ENDPOINT,
                                      clusterId, SL_ZIGBEE_ZDO_PROFILE_ID, m_packet,
                                      length);
    }
    return status;
}

/*
static struct
{
    uint8_t transaction_seq_number;
    bool valid;
} m_next_zcl_sequence_number = {0};

void tx_packet_force_response_sequence_number(uint8_t sequence_number)
{
    // this sets the transaction sequence number of the next packet
    // definitely a HACK, but will call this good for now.
    m_next_zcl_sequence_number.transaction_seq_number = sequence_number;
    m_next_zcl_sequence_number.valid = true;
}

static EmberStatus _set_zcl_payload(zcl_header_t *p_zcl_header, uint8_t *p_data,
                                    uint8_t data_length, uint8_t *p_zcl_payload_sz_out)
{
    assert(p_zcl_header != NULL);
    assert(p_zcl_payload_sz_out != NULL);

    uint8_t frctrl = (p_zcl_header->cluster_specific == true) ? (1 << 0) : 0x00;
    frctrl |= (p_zcl_header->mfg_specific == true) ? (1 << 2) : 0;
    frctrl |= (p_zcl_header->direction_to_server == true) ? 0 : (1 << 3);
    frctrl |= (p_zcl_header->disable_def_rsp == true) ? (1 << 4) : 0;

    bytes_packer_t packer = BYTES_PACKER(m_packet, ARRAY_SIZE(m_packet));
    bytes_pack_u8(&packer, frctrl);
    if(p_zcl_header->mfg_specific == true)
    {
        bytes_pack_u16_le(&packer, p_zcl_header->mfg_code);
    }
    uint8_t sequence_number = m_zcl_tsn;
    if(m_next_zcl_sequence_number.valid == true)
    {
        sequence_number = m_next_zcl_sequence_number.transaction_seq_number;
        m_next_zcl_sequence_number.valid = false;   // clear the valid flag
    }
    else
    {
        m_zcl_tsn++;
    }
    bytes_pack_u8(&packer, sequence_number);
    bool packed = bytes_pack_u8(&packer, p_zcl_header->cmd_id);

    if((p_data != NULL) && (data_length > 0))
    {
        packed = bytes_pack_array(&packer, p_data, data_length);
    }
    *p_zcl_payload_sz_out = (packed == true) ? packer.index : 0;
    return (packed == true) ? EMBER_SUCCESS : EMBER_MESSAGE_TOO_LONG;
}

static EmberStatus _tx_send_multicast_buffer(uint16_t groupId, uint8_t *p_buffer, uint16_t length,
                                             uint8_t srcEndpoint, uint16_t clusterId, uint16_t profileId)
{
    assert(p_buffer != NULL);
    m_aps_seq_number++;
    
    EmberMessageBuffer message = sli_legacy_buffer_manager_fill_buffer(p_buffer, length);
    EmberApsFrame apsFrame =
    {
        .groupId = groupId,
        .profileId = profileId,
        .clusterId = clusterId,
        .sourceEndpoint = srcEndpoint,
        .options = (EMBER_APS_OPTION_SOURCE_EUI64),
        .sequence = m_aps_seq_number,
        .radius = 0
    };
    EmberStatus status = emberSendMulticast(&apsFrame, 0, 0, message);
    return status;
}
*/
