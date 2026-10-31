#include <assert.h>
#include <string.h>
#include "adaptiv_zcl.h"
#include "byte_utils.h"
#include "network_defines.h"
#include "rx_zcl_handler.h"
#include "sys_defines.h"
#include "time_convert.h"
#include "tx_zcl.h"
#include "tx_packet.h"
#include "zap-id.h"
#include "zcl_header.h"

sl_status_t tx_zcl_send_cluster_command(uint16_t dest_addr16, uint8_t dstEndpoint,
                                        uint16_t clusterId, bool direction_to_server,
                                        uint16_t mfg_id, uint8_t cmd_id,
                                        uint8_t *p_data, uint8_t data_length)
{
    #define _MAX_CLUSTER_COMMAND_DATA_LENGTH  20
    bool mfg_specific = ((mfg_id != INVALID_MFG_CODE) && (mfg_id != 0));
    zcl_header_t zcl_header =
    {
        .cluster_specific = true,
        .disable_def_rsp = true,
        .direction_to_server = direction_to_server,
        .mfg_specific = mfg_specific,
        .cmd_id = cmd_id,
        .mfg_code = (mfg_specific == true) ? mfg_id : INVALID_MFG_CODE
    };
    uint8_t data[_MAX_CLUSTER_COMMAND_DATA_LENGTH] = {0};
    if(data_length > ARRAY_SIZE(data))
    {
        return SL_STATUS_MESSAGE_TOO_LONG;
    }
    memcpy(data, p_data, data_length);
    return tx_packet_send_unicast_zcl(&zcl_header, dest_addr16,
                                     APP_ENDPOINT, dstEndpoint,
                                     clusterId, HA_PROFILE_ID,
                                     data, data_length);
}

sl_status_t tx_zcl_default_response(uint16_t dest_addr16, uint8_t dstEndpoint,
                                    uint16_t clusterId, bool direction_to_server,
                                    uint8_t sequence_number, uint8_t cmd_id,
                                    sl_zigbee_af_status_t status)
{
    zcl_header_t zcl_header =
    {
        .cluster_specific = false,
        .disable_def_rsp = true,
        .direction_to_server = direction_to_server,
        .mfg_specific = false,
        .cmd_id = ZCL_DEFAULT_RESPONSE_COMMAND_ID,
        .mfg_code = INVALID_MFG_CODE
    };
    uint8_t data[2] = { cmd_id, status };
    zcl_header_force_sequence_number(sequence_number);
    
    return tx_packet_send_unicast_zcl(&zcl_header, dest_addr16, APP_ENDPOINT, dstEndpoint,
                                      clusterId, HA_PROFILE_ID, data, sizeof(data));
}

sl_status_t tx_zcl_default_response_to_sender(uint16_t dest_addr16, sl_zigbee_af_status_t cmd_status)
{
    uint16_t sender = 0;
    sl_zigbee_aps_frame_t apsFrame = {0};
    zcl_header_t zcl_header = {0};
    sl_status_t status = rx_zcl_get_sender_info(&sender, &apsFrame, &zcl_header);
    if(status == SL_STATUS_OK)
    {
        uint8_t direction = (zcl_header.direction_to_server == 0) ? 1 : 0;
        status = tx_zcl_default_response(dest_addr16, apsFrame.sourceEndpoint, apsFrame.clusterId,
                                         direction, zcl_header.sequence_number,
                                         zcl_header.cmd_id, cmd_status);
    }
    return status;
}

sl_status_t tx_zcl_read_attributes_req(uint16_t dest_addr16, uint8_t dstEndpoint,
                                       uint16_t clusterId, uint16_t mfg_id, bool direction_to_server,
                                       uint16_t *p_attribute_ids, uint8_t num_attributes)
{
    assert(p_attribute_ids != NULL);
    zcl_header_t zcl_header =
    {
        .cluster_specific = false,
        .disable_def_rsp = true,
        .direction_to_server = direction_to_server,
        .mfg_specific = (mfg_id != INVALID_MFG_CODE),
        .cmd_id = ZCL_READ_ATTRIBUTES_COMMAND_ID,
        .mfg_code = mfg_id
    };

    uint8_t read_attrib_data[48];
    bytes_packer_t packer = BYTES_PACKER(read_attrib_data, sizeof(read_attrib_data));
    for(uint8_t i=0; i<num_attributes; i++)
    {
        bytes_pack_u16_le(&packer, p_attribute_ids[i]);
    }

    return tx_packet_send_unicast_zcl(&zcl_header, dest_addr16,
                                     APP_ENDPOINT, dstEndpoint,
                                     clusterId, HA_PROFILE_ID,
                                     read_attrib_data, packer.index);
}

/*sl_status_t tx_zcl_read_attributes_req_mcast(uint16_t groupId, uint16_t clusterId, bool direction_to_server,
                                            uint16_t *p_attribute_ids, uint8_t num_attributes)
{
    assert(p_attribute_ids != NULL);
    zcl_header_t zcl_header =
    {
        .cluster_specific = false,
        .disable_def_rsp = true,
        .direction_to_server = direction_to_server,
        .mfg_specific = false,
        .cmd_id = ZCL_READ_ATTRIBUTES_COMMAND_ID,
        .mfg_code = INVALID_MFG_CODE
    };

    uint8_t read_attrib_data[48];
    bytes_packer_t packer = BYTES_PACKER(read_attrib_data, sizeof(read_attrib_data));
    for(uint8_t i=0; i<num_attributes; i++)
    {
        bytes_pack_u16_le(&packer, p_attribute_ids[i]);
    }

    return tx_packet_send_binding_zcl(&zcl_header, clusterId, HA_PROFILE_ID,
                                      read_attrib_data, packer.index, false);
}*/

sl_status_t tx_zcl_write_attributes_req(uint16_t dest_addr16, uint8_t dstEndpoint,
                                        uint16_t clusterId, uint16_t mfg_id,
                                        bool direction_to_server, bool expect_response,
                                        write_attribute_record_t *p_records, uint8_t num_records)
{
    assert(p_records != NULL);
    if(mfg_id == 0x0000)
    {
        mfg_id = INVALID_MFG_CODE;
    }
    zcl_header_t zcl_header =
    {
        .cluster_specific = false,
        .disable_def_rsp = true,
        .direction_to_server = direction_to_server,
        .mfg_specific = (mfg_id != INVALID_MFG_CODE),
        .cmd_id = (expect_response == true) ? ZCL_WRITE_ATTRIBUTES_COMMAND_ID : ZCL_WRITE_ATTRIBUTES_NO_RESPONSE_COMMAND_ID,
        .mfg_code = mfg_id
    };
    uint8_t write_attrib_data[48];

    bytes_packer_t packer = BYTES_PACKER(write_attrib_data, sizeof(write_attrib_data));
    for(uint8_t i=0; i<num_records; i++)
    {
        bytes_pack_u16_le(&packer, p_records[i].attribute_id);
        bytes_pack_u8(&packer, p_records[i].type);
        bool packed = bytes_pack_array(&packer, p_records[i].p_data, p_records[i].data_length);
        if(packed == false)
        {
            sl_zigbee_af_core_println("ERROR:  Could not pack write attrib req, cid=0x%04x", clusterId);
            return SL_STATUS_MESSAGE_TOO_LONG;
        }
    }

    return tx_packet_send_unicast_zcl(&zcl_header, dest_addr16,
                                      APP_ENDPOINT, dstEndpoint,
                                      clusterId, HA_PROFILE_ID,
                                      write_attrib_data, packer.index);
}

sl_status_t tx_zcl_write_attributes_req_mcast(uint16_t group_id, uint16_t clusterId,
                                              uint16_t mfg_id, bool direction_to_server,
                                              write_attribute_record_t *p_records, uint8_t num_records)
{
    assert(p_records != NULL);
    if(mfg_id == 0x0000)
    {
        mfg_id = INVALID_MFG_CODE;
    }
    zcl_header_t zcl_header =
    {
        .cluster_specific = false,
        .disable_def_rsp = true,
        .direction_to_server = direction_to_server,
        .mfg_specific = (mfg_id != INVALID_MFG_CODE),
        .cmd_id = ZCL_WRITE_ATTRIBUTES_COMMAND_ID,
        .mfg_code = mfg_id
    };
    uint8_t write_attrib_data[48];

    bytes_packer_t packer = BYTES_PACKER(write_attrib_data, sizeof(write_attrib_data));
    for(uint8_t i=0; i<num_records; i++)
    {
        bytes_pack_u16_le(&packer, p_records[i].attribute_id);
        bytes_pack_u8(&packer, p_records[i].type);
        bool packed = bytes_pack_array(&packer, p_records[i].p_data, p_records[i].data_length);
        if(packed == false)
        {
            sl_zigbee_af_core_println("ERROR:  Could not pack write attrib req, cid=0x%04x", clusterId);
            return SL_STATUS_MESSAGE_TOO_LONG;
        }
    }
    return tx_packet_send_multicast_zcl(&zcl_header, group_id, APP_ENDPOINT,
                                         clusterId, HA_PROFILE_ID,
                                         write_attrib_data, packer.index);
}

sl_status_t tx_zcl_auto_commision_boost_enable(uint16_t dest_addr16, uint8_t dest_endpoint, bool enable)
{
    write_attribute_record_t record =
    {
        .attribute_id = ZCL_MFG_AUTOCOMMISSION_BOOST_ENABLE_ATTRIBUTE_ID,
        .type = ZCL_BOOLEAN_ATTRIBUTE_TYPE,
        .p_data = (uint8_t*)&enable,
        .data_length = sizeof(enable)
    };

    return tx_zcl_write_attributes_req(dest_addr16, dest_endpoint, ZCL_MFG_AUTOCOMMISSIONING_CLUSTER_ID,
                                        ADAPTIV_MFG_ID, true, true, &record, 1);
}

sl_status_t tx_zcl_identify(uint16_t dest_addr16, uint8_t dstEndpoint, uint16_t identify_sec)
{
    zcl_header_t zcl_header =
    {
        .cluster_specific = true,
        .disable_def_rsp = true,
        .direction_to_server = true,
        .mfg_specific = false,
        .cmd_id = ZCL_IDENTIFY_COMMAND_ID,
        .mfg_code = INVALID_MFG_CODE
    };
    uint8_t data[] =
    {
        U16_LSB_TO_U8(identify_sec),
        U16_MSB_TO_U8(identify_sec),
        0
    };

    return tx_packet_send_unicast_zcl(&zcl_header, dest_addr16, APP_ENDPOINT, dstEndpoint,
                                    ZCL_IDENTIFY_CLUSTER_ID, HA_PROFILE_ID,
                                    data, ARRAY_SIZE(data));
}

sl_status_t tx_zcl_identify_mcast(uint16_t group_id, uint16_t identify_sec)
{
    zcl_header_t zcl_header =
    {
        .cluster_specific = true,
        .disable_def_rsp = true,
        .direction_to_server = true,
        .mfg_specific = false,
        .cmd_id = ZCL_IDENTIFY_COMMAND_ID,
        .mfg_code = INVALID_MFG_CODE
    };
    uint8_t data[] =
    {
        U16_LSB_TO_U8(identify_sec),
        U16_MSB_TO_U8(identify_sec),
        0
    };
    return tx_packet_send_multicast_zcl(&zcl_header, group_id, APP_ENDPOINT, ZCL_IDENTIFY_CLUSTER_ID,
                                        HA_PROFILE_ID, data, ARRAY_SIZE(data));
}

sl_status_t tx_zcl_groups_get_membership(uint16_t dest_addr16, uint8_t dest_endpoint,
                                         uint8_t count, uint16_t *p_group_id_list)
{
    zcl_header_t zcl_header =
    {
        .cluster_specific = true,
        .disable_def_rsp = true,
        .direction_to_server = true,
        .mfg_specific = false,
        .cmd_id = ZCL_GET_GROUP_MEMBERSHIP_COMMAND_ID
    };

    // construct the packet
    uint8_t data[17];
    bytes_packer_t packer = BYTES_PACKER(data, sizeof(data));
    bytes_pack_u8(&packer, count);
    count = MIN(count, (((ARRAY_SIZE(data))-1) / 2));
    if((count > 0) && (p_group_id_list != NULL))
    {
        for(uint8_t i=0; i<count; i++)
        {
            bytes_pack_u16_le(&packer, p_group_id_list[i]);
        }
    }
    return tx_packet_send_unicast_zcl(&zcl_header, dest_addr16, APP_ENDPOINT, dest_endpoint,
                                      ZCL_GROUPS_CLUSTER_ID, HA_PROFILE_ID, data, packer.index);
}

sl_status_t tx_zcl_ota_end_response(uint16_t dest_addr16, uint8_t dest_endpoint,
                                    uint8_t sequence_number, uint16_t mfg_id,
                                    uint16_t image_type, uint32_t file_version)
{
    zcl_header_t zcl_header =
    {
        .cluster_specific = true,
        .disable_def_rsp = true,
        .direction_to_server = false,
        .mfg_specific = false,
        .cmd_id = ZCL_UPGRADE_END_RESPONSE_COMMAND_ID
    };

    // construct the packet
    uint8_t data[16];
    bytes_packer_t packer = BYTES_PACKER(data, sizeof(data));
    bytes_pack_u16_le(&packer, mfg_id);
    bytes_pack_u16_le(&packer, image_type);
    bytes_pack_u32_le(&packer, file_version);
    bytes_pack_u32_le(&packer, 0);  // current time
    bytes_pack_u32_le(&packer, 0);  // upgrade time

    zcl_header_force_sequence_number(sequence_number);
    return tx_packet_send_unicast_zcl(&zcl_header, dest_addr16, APP_ENDPOINT, dest_endpoint,
                                      ZCL_OTA_BOOTLOAD_CLUSTER_ID, HA_PROFILE_ID, data, packer.index);
}

sl_status_t tx_zcl_ota_get_mfg_image_type(uint16_t dest_addr16, uint8_t endpoint)
{
    // Because the OTA cluster specifies the standard IMAGE TYPE attribute
    // shall return 0xFFFF unless an OTA is active, we define a MFG specific
    // version of that attribute to indicate the permanent Image Type value
    // that the device is seeking.
    // The OTA dongle can read this to determine if all devices have updated
    // to the current version or not.
    uint16_t attribute_ids[] =
    {
        ZCL_IMAGE_TYPE_ID_ATTRIBUTE_ID,
    };
    return tx_zcl_read_attributes_req(dest_addr16, endpoint,
                                      ZCL_OTA_BOOTLOAD_CLUSTER_ID, ADAPTIV_MFG_ID,
                                      false, attribute_ids, ARRAY_SIZE(attribute_ids));
}

sl_status_t tx_zcl_ota_get_version(uint16_t dest_addr16, uint8_t endpoint)
{
    uint16_t attribute_ids[] =
    {
        ZCL_CURRENT_FILE_VERSION_ATTRIBUTE_ID,
        ZCL_IMAGE_TYPE_ID_ATTRIBUTE_ID,
        ZCL_MANUFACTURER_ID_ATTRIBUTE_ID
    };
    return tx_zcl_read_attributes_req(dest_addr16, endpoint,
                                      ZCL_OTA_BOOTLOAD_CLUSTER_ID, INVALID_MFG_CODE,
                                      false, attribute_ids, ARRAY_SIZE(attribute_ids));
}

sl_status_t tx_zcl_ota_image_notify_cmd_bcast(uint8_t payload_type, uint8_t jitter,
                                              uint16_t mfg_id, uint16_t image_type,
                                              uint32_t file_version)
{
    zcl_header_t zcl_header =
    {
        .cluster_specific = true,
        .disable_def_rsp = true,
        .direction_to_server = false,
        .mfg_specific = false,
        .cmd_id = ZCL_IMAGE_NOTIFY_COMMAND_ID
    };

    static uint8_t data[20];
    bytes_packer_t packer = BYTES_PACKER(data, sizeof(data));
    bytes_pack_u8(&packer, payload_type);
    bytes_pack_u8(&packer, jitter);
    if(payload_type >= 1)
    {
        bytes_pack_u16_le(&packer, mfg_id);
    }
    if(payload_type >= 2)
    {
        bytes_pack_u16_le(&packer, image_type);
    }
    if(payload_type == 3)
    {
        bytes_pack_u32_le(&packer, file_version);
    }

    return tx_packet_send_broadcast_zcl(&zcl_header, 0xFFFF, APP_ENDPOINT, 0xFF,
                                      ZCL_OTA_BOOTLOAD_CLUSTER_ID, HA_PROFILE_ID, data, packer.index);
}

sl_status_t tx_zcl_ota_version_blink_bcast(uint16_t seq_number, uint16_t mfg_id,
                                           uint16_t target_image_type, uint32_t target_version,
                                           bool match_criteria, uint16_t identify_blink_time_sec)
{
    zcl_header_t zcl_header =
    {
        .cluster_specific = true,
        .disable_def_rsp = true,
        .direction_to_server = true,
        .mfg_specific = true,
        .mfg_code = ADAPTIV_MFG_ID,
        .cmd_id = ZCL_MFG_AUTOCOMMISSION_VERSION_IDENTIFY_COMMAND_ID
    };

    static uint8_t data[16];
    bytes_packer_t packer = BYTES_PACKER(data, sizeof(data));
    bytes_pack_u16_le(&packer, seq_number);
    bytes_pack_u16_le(&packer, mfg_id);
    bytes_pack_u16_le(&packer, target_image_type);
    bytes_pack_u32_le(&packer, target_version);
    bytes_pack_u8(&packer, match_criteria);      // identify version match (T=identify if match, F=identify if no match)
    bytes_pack_u16_le(&packer, identify_blink_time_sec);

    return tx_packet_send_broadcast_zcl(&zcl_header, 0xFFFF, APP_ENDPOINT, 0xFF,
                                      ZCL_MFG_AUTOCOMMISSIONING_CLUSTER_ID, HA_PROFILE_ID, data, packer.index);
}

sl_status_t tx_zcl_ota_image_block_response(uint16_t dest_addr16, uint8_t dest_endpoint,
                                            uint8_t sequence_number, sl_zigbee_af_status_t status,
                                            uint16_t mfg_id, uint16_t image_type,
                                            uint32_t file_version, uint32_t offset,
                                            uint8_t *p_data, uint8_t data_length)
{
    zcl_header_t zcl_header =
    {
        .cluster_specific = true,
        .disable_def_rsp = true,
        .direction_to_server = false,
        .mfg_specific = false,
        .cmd_id = ZCL_IMAGE_BLOCK_RESPONSE_COMMAND_ID
    };

    static uint8_t data[100];
    bytes_packer_t packer = BYTES_PACKER(data, sizeof(data));
    bytes_pack_u8(&packer, status);

    if(status == SL_ZIGBEE_ZCL_STATUS_SUCCESS)
    {
        bytes_pack_u16_le(&packer, mfg_id);
        bytes_pack_u16_le(&packer, image_type);
        bytes_pack_u32_le(&packer, file_version);
        bytes_pack_u32_le(&packer, offset);
        uint8_t max_size = ARRAY_SIZE(data) - packer.index;
        data_length = MIN(data_length, max_size);
        bytes_pack_u8(&packer, data_length);
        bool packed = bytes_pack_array(&packer, p_data, data_length);
        if(packed == false)
        {
            return SL_STATUS_MESSAGE_TOO_LONG;
        }
    }

    zcl_header_force_sequence_number(sequence_number);
    return tx_packet_send_unicast_zcl(&zcl_header, dest_addr16, APP_ENDPOINT, dest_endpoint,
                                      ZCL_OTA_BOOTLOAD_CLUSTER_ID, HA_PROFILE_ID, data, packer.index);
}

sl_status_t tx_zcl_ota_query_next_image_response(uint16_t dest_addr16, uint8_t dest_endpoint,
                                                 uint8_t sequence_number, sl_zigbee_af_status_t status,
                                                 uint16_t mfg_id, uint16_t image_type,
                                                 uint32_t file_version, uint32_t image_size)
{
    zcl_header_t zcl_header =
    {
        .cluster_specific = true,
        .disable_def_rsp = true,
        .direction_to_server = false,
        .mfg_specific = false,
        .cmd_id = ZCL_QUERY_NEXT_IMAGE_RESPONSE_COMMAND_ID
    };

    // construct the packet
    uint8_t data[14];
    bytes_packer_t packer = BYTES_PACKER(data, sizeof(data));
    bytes_pack_u8(&packer, status);

    if(status == SL_ZIGBEE_ZCL_STATUS_SUCCESS)
    {
        bytes_pack_u16_le(&packer, mfg_id);
        bytes_pack_u16_le(&packer, image_type);
        bytes_pack_u32_le(&packer, file_version);
        bytes_pack_u32_le(&packer, image_size);
    }

    zcl_header_force_sequence_number(sequence_number);
    return tx_packet_send_unicast_zcl(&zcl_header, dest_addr16, APP_ENDPOINT, dest_endpoint,
                                      ZCL_OTA_BOOTLOAD_CLUSTER_ID, HA_PROFILE_ID, data, packer.index);
}
