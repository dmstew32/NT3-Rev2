#ifndef _RX_ZCL_HANDLER_H_
#define _RX_ZCL_HANDLER_H_

#include <stdint.h>
#include <stdbool.h>
#include "sl_status.h"
#include "sl_zigbee_types.h"
#include "zap-type.h"
#include "zcl_header.h"

typedef struct
{
    uint16_t attribute_id;
    sl_status_t status;
    uint8_t attribute_type;
    uint8_t data_length;
    union
    {
        uint8_t data8;
        uint16_t data16;
        uint32_t data32;          // convenience for u8, u16, u32 attributes
        struct st_
        {
            uint8_t str_length;
            uint8_t *p_str;        // convenience for array or string attributes
        } s;
    } u;
} read_attribute_response_record_t;

#define RX_ZCL_ANY_CLUSTER_ID  0xFFFF

typedef void (*zcl_cfg_report_rsp_cb)(uint16_t sender, uint8_t src_endpoint, sl_status_t status,
                                      uint16_t cluster_id);

typedef void (*zcl_read_attr_rsp_cb)(uint16_t sender, uint8_t src_endpoint, sl_status_t status,
                                     uint16_t cluster_id, uint16_t mfg_id,
                                     read_attribute_response_record_t *p_records,
                                     uint8_t num_records);

typedef void (*zcl_write_attr_rsp_cb)(uint16_t sender, uint8_t src_endpoint, sl_zigbee_af_status_t status);

typedef void (*zcl_reset_notify_ind_cb)(uint16_t sender, uint8_t src_endpoint, sl_status_t status);

typedef void (*zcl_cluster_cmd_rsp_cb)(uint16_t sender, uint8_t src_endpoint, uint8_t *p_data, uint8_t length);

typedef void (*zcl_occupancy_report_cb)(uint16_t sender, uint8_t src_endpoint, bool occupied);

void rx_zcl_handler_on_startup(void);
void rx_zcl_handler_on_rx(uint16_t sender, sl_zigbee_aps_frame_t *apsFrame, uint8_t *p_data, uint8_t length);
sl_status_t rx_zcl_get_sender_info(uint16_t *p_sender, sl_zigbee_aps_frame_t *p_apsFrame,
                                    zcl_header_t *p_zcl_header);

void rx_zcl_handler_print(void);
sl_status_t rx_zcl_handler_register_cfg_report_cmd_response_cb(uint16_t cluster_id, zcl_cfg_report_rsp_cb cb);
sl_status_t rx_zcl_handler_register_read_attribute_response_cb(uint16_t cluster_id, uint16_t mfg_id,
                                                          zcl_read_attr_rsp_cb cb);
sl_status_t rx_zcl_handler_register_write_attribute_response_cb(uint16_t cluster_id, uint16_t mfg_id,
                                                          zcl_write_attr_rsp_cb cb);
sl_status_t rx_zcl_handler_register_cluster_cmd_response_cb(uint16_t cluster_id, uint16_t mfg_id, uint8_t cmd_id,
                                                            zcl_cluster_cmd_rsp_cb cb);
sl_status_t rx_zcl_handler_register_report_occupancy_cb(zcl_occupancy_report_cb cb);

#endif  // #ifndef _RX_ZCL_HANDLER_H_
