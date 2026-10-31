#ifndef _TX_ZCL_H_
#define _TX_ZCL_H_

#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"
#include "zap-type.h"


typedef struct
{
    uint16_t attribute_id;
    uint8_t type;
    uint8_t *p_data;
    uint8_t data_length;
} write_attribute_record_t;

typedef struct
{
    uint16_t attribute_id;
    uint8_t  type;
    uint16_t min_interval_sec;
    uint16_t max_interval_sec;
    uint32_t reportable_change;
} attribute_record_t;


sl_status_t tx_zcl_send_cluster_command(uint16_t dest_addr16, uint8_t dstEndpoint,
                                        uint16_t clusterId, bool direction_to_server,
                                        uint16_t mfg_id, uint8_t cmd_id,
                                        uint8_t *p_data, uint8_t data_length);

sl_status_t tx_zcl_default_response(uint16_t dest_addr16, uint8_t dstEndpoint,
                                    uint16_t clusterId, bool direction_to_server,
                                    uint8_t sequence_number, uint8_t cmd_id,
                                    sl_zigbee_af_status_t status);

sl_status_t tx_zcl_default_response_to_sender(uint16_t dest_addr16, sl_zigbee_af_status_t cmd_status);

sl_status_t tx_zcl_read_attributes_req(uint16_t dest_addr16, uint8_t dstEndpoint,
                                       uint16_t clusterId, uint16_t mfg_id, bool direction_to_server,
                                       uint16_t *p_attribute_ids, uint8_t num_attributes);

//sl_status_t tx_zcl_read_attributes_req_mcast(uint16_t groupId, uint16_t clusterId, bool direction_to_server,
  //                                          uint16_t *p_attribute_ids, uint8_t num_attributes);

sl_status_t tx_zcl_write_attributes_req(uint16_t dest_addr16, uint8_t dstEndpoint,
                                        uint16_t clusterId, uint16_t mfg_id,
                                        bool direction_to_server, bool expect_response,
                                        write_attribute_record_t *p_records, uint8_t num_records);

sl_status_t tx_zcl_write_attributes_req_mcast(uint16_t group_id, uint16_t clusterId,
                                              uint16_t mfg_id, bool direction_to_server,
                                              write_attribute_record_t *p_records, uint8_t num_records);

sl_status_t tx_zcl_auto_commision_boost_enable(uint16_t dest_addr16, uint8_t dest_endpoint, bool enable);
sl_status_t tx_zcl_identify(uint16_t dest_addr16, uint8_t dstEndpoint, uint16_t identify_sec);
sl_status_t tx_zcl_identify_mcast(uint16_t group_id, uint16_t identify_sec);

sl_status_t tx_zcl_groups_get_membership(uint16_t dest_addr16, uint8_t dest_endpoint,
                                         uint8_t count, uint16_t *p_group_id_list);

sl_status_t tx_zcl_ota_end_response(uint16_t dest_addr16, uint8_t dest_endpoint,
                                    uint8_t sequence_number, uint16_t mfg_id,
                                    uint16_t image_type, uint32_t file_version);

sl_status_t tx_zcl_ota_get_mfg_image_type(uint16_t dest_addr16, uint8_t endpoint);
sl_status_t tx_zcl_ota_get_version(uint16_t dest_addr16, uint8_t endpoint);
//sl_status_t tx_zcl_ota_image_notify_cmd(uint16_t dest_addr16, uint8_t payload_type, uint8_t jitter,
  //                                      uint16_t mfg_id, uint16_t image_type, uint32_t file_version);
sl_status_t tx_zcl_ota_image_notify_cmd_bcast(uint8_t payload_type, uint8_t jitter,
                                              uint16_t mfg_id, uint16_t image_type,
                                              uint32_t file_version);
sl_status_t tx_zcl_ota_image_block_response(uint16_t dest_addr16, uint8_t dest_endpoint,
                                            uint8_t sequence_number, sl_zigbee_af_status_t status,
                                            uint16_t mfg_id, uint16_t image_type,
                                            uint32_t file_version, uint32_t offset,
                                            uint8_t *p_data, uint8_t data_length);

sl_status_t tx_zcl_ota_query_next_image_response(uint16_t dest_addr16, uint8_t dest_endpoint,
                                                 uint8_t sequence_number, sl_zigbee_af_status_t status,
                                                 uint16_t mfg_id, uint16_t image_type,
                                                 uint32_t file_version, uint32_t image_size);

sl_status_t tx_zcl_ota_version_blink_bcast(uint16_t seq_number, uint16_t mfg_id,
                                           uint16_t target_image_type, uint32_t target_version,
                                           bool match_criteria, uint16_t identify_blink_time_sec);

#endif  // #ifndef _TX_ZCL_H_
