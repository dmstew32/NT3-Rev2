#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "adaptiv_zcl.h"
#include "af.h"
#include "app_logger.h"
#include "byte_utils.h"
#include "network_defines.h"
#include "rx_zcl_handler.h"
#include "sys_defines.h"
#include "uptime.h"
#include "zap-id.h"
#include "zap-type.h"
#include "zcl_header.h"

// Parses incoming ZCL packets, extracts the ZCL header, and sends the packet to any
// registered callbacks that want to be notified of ZCL traffic.

// One example is the autocommissioning.c file that will need read attribute response
// packets, when reading the number of resets, and also when receiving power cycle
// notifications from the lights.


#define _NUM_ZCL_APP_CALLBACKS  15
#define _NUM_ZCL_REPORT_CALLBACKS 1
#define _ZCL_MAX_TIMEOUT_SEC  10
#define _ZCL_MIN_LENGTH_READ_ATTR_RESPONSE 3


static struct
{
    void *cb_u;
    uint16_t cluster_id;
    uint16_t mfg_id;
    uint8_t response_cmd_id;
    bool cluster_specific;
} m_zcl_rsp_callbacks[_NUM_ZCL_APP_CALLBACKS];

static struct
{
    bool valid;
    uint16_t addr16;
    sl_zigbee_aps_frame_t apsFrame;
    zcl_header_t zcl_header;
} m_sender_info = {0};

static struct
{
    zcl_occupancy_report_cb occ_cb;
} m_zcl_report_callbacks[_NUM_ZCL_REPORT_CALLBACKS];

static bool m_initialized = false;

static uint8_t _get_length_from_attribute_type(uint8_t type, uint8_t first_msg_byte);
static uint8_t _get_next_matching_callback_index(zcl_header_t *p_zcl_header, uint16_t cluster_id, uint8_t start_index);

static void _zcl_handle_cluster_specific(zcl_header_t *p_zcl_header, uint16_t sender,
                                        sl_zigbee_aps_frame_t *apsFrame, uint8_t *p_msg, uint16_t length);
static void _zcl_handle_global_rsp(zcl_header_t *p_zcl_header, uint16_t sender,
                                    sl_zigbee_aps_frame_t *apsFrame, uint8_t *p_msg, uint16_t length);

static void _zcl_handle_config_reporting_rsp_cb(uint16_t sender, sl_zigbee_aps_frame_t *apsFrame, zcl_header_t *p_zcl_header,
                                                uint8_t *p_msg, uint16_t length);
static void _zcl_handle_read_attrib_rsp_cb(uint16_t sender, sl_zigbee_aps_frame_t *apsFrame, zcl_header_t *p_zcl_header,
                                            uint8_t *p_msg, uint16_t length);
static void _zcl_handle_report_attributes(uint16_t sender, sl_zigbee_aps_frame_t *apsFrame, zcl_header_t *p_zcl_header,
                                            uint8_t *p_msg, uint16_t length);
static void _zcl_handle_write_attrib_rsp_cb(uint16_t sender, sl_zigbee_aps_frame_t *apsFrame, zcl_header_t * p_zcl_header,
                                            uint8_t *p_msg, uint16_t length);


void rx_zcl_handler_on_startup(void)
{
    for(uint8_t i=0; i<ARRAY_SIZE(m_zcl_rsp_callbacks); i++)
    {
        m_zcl_rsp_callbacks[i].cb_u = NULL;
        m_zcl_rsp_callbacks[i].cluster_id = INVALID_CLUSTER_ID;
        m_zcl_rsp_callbacks[i].mfg_id = INVALID_MFG_CODE;
    }

    for(uint8_t i=0; i<ARRAY_SIZE(m_zcl_report_callbacks); i++)
    {
        m_zcl_report_callbacks[i].occ_cb = NULL;
    }
    m_initialized = true;
}

void rx_zcl_handler_on_rx(uint16_t sender, sl_zigbee_aps_frame_t *apsFrame, uint8_t *p_data, uint8_t length)
{
    ASSERT(apsFrame != NULL);
    ASSERT(p_data != NULL);
    if((apsFrame->destinationEndpoint == 0) || (apsFrame->profileId != HA_PROFILE_ID) || (length < ZCL_HEADER_MIN_LENGTH))
    {
        return;
    }
    zcl_header_t zcl_header = {0};
    uint8_t zcl_length = zcl_header_from_data(p_data, length, &zcl_header);
    if((length < zcl_length) || (zcl_length == 0))
    {
        sl_zigbee_af_core_println("RX from 0x%02x, len err %d < %d", sender, length, zcl_length);
        return;
    }
    p_data += zcl_length;
    length -= zcl_length;

    // remember the sender so a handler can reply (see tx_zcl_default_response_to_sender())
    m_sender_info.addr16 = sender;
    memcpy(&m_sender_info.apsFrame, apsFrame, sizeof(sl_zigbee_aps_frame_t));
    memcpy(&m_sender_info.zcl_header, &zcl_header, sizeof(zcl_header_t));
    m_sender_info.valid = true;

    if(zcl_header.cluster_specific == false)
    {
        _zcl_handle_global_rsp(&zcl_header, sender, apsFrame, p_data, length);
    }
    else
    {
        _zcl_handle_cluster_specific(&zcl_header, sender, apsFrame, p_data, length);
    }
    m_sender_info.valid = false;
}

sl_status_t rx_zcl_get_sender_info(uint16_t *p_sender, sl_zigbee_aps_frame_t *p_apsFrame,
                                    zcl_header_t *p_zcl_header)
{
    assert(p_sender != NULL);
    assert(p_apsFrame != NULL);
    assert(p_zcl_header != NULL);

    if(m_sender_info.valid == true)
    {
        *p_sender = m_sender_info.addr16;
        memcpy(p_apsFrame, &m_sender_info.apsFrame, sizeof(sl_zigbee_aps_frame_t));
        memcpy(p_zcl_header, &m_sender_info.zcl_header, sizeof(zcl_header_t));
    }
    return (m_sender_info.valid == true) ? SL_STATUS_OK : SL_STATUS_NOT_FOUND;
}

void rx_zcl_handler_print(void)
{
    app_logger("====  ZCL CALLBACKS  [%d]", ARRAY_SIZE(m_zcl_rsp_callbacks));
    for(uint8_t i=0; i<ARRAY_SIZE(m_zcl_rsp_callbacks); i++)
    {
        if(m_zcl_rsp_callbacks[i].cb_u != NULL)
        {
            app_logger(" [%d]  cid=0x%04x, mfg=0x%04x, cmd=%d, csp=%d, cb=0x%08x",
                    i, m_zcl_rsp_callbacks[i].cluster_id, m_zcl_rsp_callbacks[i].mfg_id,
                    m_zcl_rsp_callbacks[i].response_cmd_id, m_zcl_rsp_callbacks[i].cluster_specific,
                    (uint32_t)m_zcl_rsp_callbacks[i].cb_u);
        }
    }
}

sl_status_t rx_zcl_handler_register_cfg_report_cmd_response_cb(uint16_t cluster_id, zcl_cfg_report_rsp_cb cb)
{
    sl_status_t status = SL_STATUS_INVALID_STATE;
    if(m_initialized == true)
    {
        status = SL_STATUS_FULL;
        for(uint8_t i=0; i<ARRAY_SIZE(m_zcl_rsp_callbacks); i++)
        {
            if((m_zcl_rsp_callbacks[i].cb_u == NULL) || (m_zcl_rsp_callbacks[i].cb_u == (void*)cb))
            {
                m_zcl_rsp_callbacks[i].cb_u = (void *)cb;
                m_zcl_rsp_callbacks[i].cluster_id = cluster_id;
                m_zcl_rsp_callbacks[i].mfg_id = INVALID_MFG_CODE;
                m_zcl_rsp_callbacks[i].response_cmd_id = ZCL_CONFIGURE_REPORTING_RESPONSE_COMMAND_ID;
                m_zcl_rsp_callbacks[i].cluster_specific = false;
                status = SL_STATUS_OK;
                break;
            }
        }
    }
    return status;
}

sl_status_t rx_zcl_handler_register_read_attribute_response_cb(uint16_t cluster_id, uint16_t mfg_id,
                                                          zcl_read_attr_rsp_cb cb)
{
    sl_status_t status = SL_STATUS_INVALID_STATE;
    if(m_initialized == true)
    {
        status = SL_STATUS_FULL;
        if(mfg_id == 0)
        {
            mfg_id = INVALID_MFG_CODE;
        }
        for(uint8_t i=0; i<ARRAY_SIZE(m_zcl_rsp_callbacks); i++)
        {
            if((m_zcl_rsp_callbacks[i].cb_u == NULL) ||
                ((m_zcl_rsp_callbacks[i].cb_u == (void*)cb) && (m_zcl_rsp_callbacks[i].cluster_id == cluster_id) &&
                    (m_zcl_rsp_callbacks[i].mfg_id == mfg_id)))
            {
                m_zcl_rsp_callbacks[i].cb_u = (void *)cb;
                m_zcl_rsp_callbacks[i].cluster_id = cluster_id;
                m_zcl_rsp_callbacks[i].mfg_id = mfg_id;
                m_zcl_rsp_callbacks[i].response_cmd_id = ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID;
                m_zcl_rsp_callbacks[i].cluster_specific = false;
                status = SL_STATUS_OK;
                break;
            }
        }
    }
    return status;
}

sl_status_t rx_zcl_handler_register_write_attribute_response_cb(uint16_t cluster_id, uint16_t mfg_id,
                                                                zcl_write_attr_rsp_cb cb)
{
    sl_status_t status = SL_STATUS_INVALID_STATE;
    if(m_initialized == true)
    {
        status = SL_STATUS_FULL;
        if(mfg_id == 0)
        {
            mfg_id = INVALID_MFG_CODE;
        }
        for(uint8_t i=0; i<ARRAY_SIZE(m_zcl_rsp_callbacks); i++)
        {
            if((m_zcl_rsp_callbacks[i].cb_u == NULL) ||
                ((m_zcl_rsp_callbacks[i].cb_u == (void*)cb) && (m_zcl_rsp_callbacks[i].cluster_id == cluster_id) &&
                    (m_zcl_rsp_callbacks[i].mfg_id == mfg_id)))
            {
                m_zcl_rsp_callbacks[i].cb_u = (void *)cb;
                m_zcl_rsp_callbacks[i].cluster_id = cluster_id;
                m_zcl_rsp_callbacks[i].mfg_id = mfg_id;
                m_zcl_rsp_callbacks[i].response_cmd_id = ZCL_WRITE_ATTRIBUTES_RESPONSE_COMMAND_ID;
                m_zcl_rsp_callbacks[i].cluster_specific = false;
                status = SL_STATUS_OK;
                break;
            }
        }
    }
    return status;
}

sl_status_t rx_zcl_handler_register_cluster_cmd_response_cb(uint16_t cluster_id, uint16_t mfg_id, uint8_t cmd_id,
                                                            zcl_cluster_cmd_rsp_cb cb)
{
    sl_status_t status = SL_STATUS_INVALID_STATE;
    if(m_initialized == true)
    {
        status = SL_STATUS_FULL;
        for(uint8_t i=0; i<ARRAY_SIZE(m_zcl_rsp_callbacks); i++)
        {
            if((m_zcl_rsp_callbacks[i].cb_u == NULL) || (m_zcl_rsp_callbacks[i].cb_u == (void*)cb))
            {
                m_zcl_rsp_callbacks[i].cb_u = (void *)cb;
                m_zcl_rsp_callbacks[i].cluster_id = cluster_id;
                m_zcl_rsp_callbacks[i].mfg_id = mfg_id;
                m_zcl_rsp_callbacks[i].response_cmd_id = cmd_id;
                m_zcl_rsp_callbacks[i].cluster_specific = true;
                status = SL_STATUS_OK;
                break;
            }
        }
    }
    return status;
}

sl_status_t rx_zcl_handler_register_report_occupancy_cb(zcl_occupancy_report_cb cb)
{
    sl_status_t status = SL_STATUS_INVALID_STATE;
    if(m_initialized == true)
    {
        status = SL_STATUS_FULL;
        uint8_t index = ARRAY_SIZE(m_zcl_report_callbacks);
        for(uint8_t i=0; i<ARRAY_SIZE(m_zcl_report_callbacks); i++)
        {
            if((m_zcl_report_callbacks[i].occ_cb == cb) ||
                (m_zcl_report_callbacks[i].occ_cb == NULL))
            {
                index = i;
                break;
            }
        }
        if(index < ARRAY_SIZE(m_zcl_report_callbacks))
        {
            m_zcl_report_callbacks[index].occ_cb = cb;
            status = SL_STATUS_OK;
        }
    }
    return status;
}


static uint8_t _get_length_from_attribute_type(uint8_t type, uint8_t first_msg_byte)
{
    uint8_t len = sl_zigbee_af_get_data_size(type);
    if((type == ZCL_CHAR_STRING_ATTRIBUTE_TYPE) || (type == ZCL_OCTET_STRING_ATTRIBUTE_TYPE))
    {
        len = first_msg_byte;
    }
    return len;
}

static uint8_t _get_next_matching_callback_index(zcl_header_t *p_zcl_header, uint16_t cluster_id, uint8_t start_index)
{
    assert(p_zcl_header != NULL);
    uint8_t index = ARRAY_SIZE(m_zcl_rsp_callbacks);
    for(uint8_t i=start_index; i<ARRAY_SIZE(m_zcl_rsp_callbacks); i++)
    {
        if(m_zcl_rsp_callbacks[i].cb_u != NULL)
        {
            bool valid_mfg_id = (p_zcl_header->mfg_specific == true) ? (p_zcl_header->mfg_code == m_zcl_rsp_callbacks[i].mfg_id) :
                                    (m_zcl_rsp_callbacks[i].mfg_id == INVALID_MFG_CODE);
            bool valid_cluster_id = ((m_zcl_rsp_callbacks[i].cluster_id == RX_ZCL_ANY_CLUSTER_ID) ||
                                    (m_zcl_rsp_callbacks[i].cluster_id == cluster_id));
            if((valid_cluster_id == true) && (valid_mfg_id == true) &&
                (m_zcl_rsp_callbacks[i].response_cmd_id == p_zcl_header->cmd_id) &&
                (m_zcl_rsp_callbacks[i].cluster_specific == p_zcl_header->cluster_specific))
            {
                index = i;
                break;
            }
        }
    }
    return index;
}

static void _zcl_handle_cluster_specific(zcl_header_t *p_zcl_header, uint16_t sender,
                                        sl_zigbee_aps_frame_t *apsFrame, uint8_t *p_msg, uint16_t length)
{
    assert(p_zcl_header != NULL);
    assert(apsFrame != NULL);

    bool command_of_interest = ((apsFrame->clusterId == ZCL_MFG_AUTOCOMMISSIONING_CLUSTER_ID) ||
                                (apsFrame->clusterId == ZCL_MFG_ADAPTIV_CONFIG_CLUSTER_ID) ||
                                ((apsFrame->clusterId == ZCL_GROUPS_CLUSTER_ID) && (p_zcl_header->cmd_id == ZCL_ADD_GROUP_RESPONSE_COMMAND_ID) &&
                                    (p_zcl_header->direction_to_server == false)));

    if(command_of_interest == true)
    {
        for(uint8_t next_i=0; next_i<ARRAY_SIZE(m_zcl_rsp_callbacks); next_i++)
        {
            next_i = _get_next_matching_callback_index(p_zcl_header, apsFrame->clusterId, next_i);
            if(next_i < ARRAY_SIZE(m_zcl_rsp_callbacks))
            {
                zcl_cluster_cmd_rsp_cb cb = (zcl_cluster_cmd_rsp_cb)m_zcl_rsp_callbacks[next_i].cb_u;
                cb(sender, apsFrame->sourceEndpoint, p_msg, length);
            }
        }
    }
}

static void _zcl_handle_global_rsp(zcl_header_t *p_zcl_header, uint16_t sender,
                                    sl_zigbee_aps_frame_t *apsFrame, uint8_t *p_msg, uint16_t length)
{

    switch(p_zcl_header->cmd_id)
    {
        case ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID:
            _zcl_handle_read_attrib_rsp_cb(sender, apsFrame, p_zcl_header, p_msg, length);
            break;

        case ZCL_WRITE_ATTRIBUTES_RESPONSE_COMMAND_ID:
            _zcl_handle_write_attrib_rsp_cb(sender, apsFrame, p_zcl_header, p_msg, length);
            break;

        case ZCL_CONFIGURE_REPORTING_RESPONSE_COMMAND_ID:
            _zcl_handle_config_reporting_rsp_cb(sender, apsFrame, p_zcl_header, p_msg, length);
            break;

        case ZCL_REPORT_ATTRIBUTES_COMMAND_ID:
            _zcl_handle_report_attributes(sender, apsFrame, p_zcl_header, p_msg, length);
            break;
    }
}

static void _zcl_handle_config_reporting_rsp_cb(uint16_t sender, sl_zigbee_aps_frame_t *apsFrame, zcl_header_t *p_zcl_header,
                                                uint8_t *p_msg, uint16_t length)
{
    assert(apsFrame != NULL);
    assert(p_zcl_header != NULL);
    assert(p_msg != NULL);
    (void)length;

    // for now, don't monitor status
    for(uint8_t next_i=0; next_i<ARRAY_SIZE(m_zcl_rsp_callbacks); next_i++)
    {
        next_i = _get_next_matching_callback_index(p_zcl_header, apsFrame->clusterId, next_i);
        if(next_i < ARRAY_SIZE(m_zcl_rsp_callbacks))
        {
            zcl_cfg_report_rsp_cb cb = (zcl_cfg_report_rsp_cb)m_zcl_rsp_callbacks[next_i].cb_u;
            cb(sender, apsFrame->sourceEndpoint, SL_STATUS_OK, apsFrame->clusterId);
        }
    }
}

static void _zcl_handle_read_attrib_rsp_cb(uint16_t sender, sl_zigbee_aps_frame_t *apsFrame, zcl_header_t *p_zcl_header,
                                            uint8_t *p_msg, uint16_t length)
{
    #define _ZCL_READ_ATT_RSP_RECORD_MIN_LENGTH  3
    #define _ZCL_MAX_READ_ATTRIBUTES 5

    assert(apsFrame != NULL);
    assert(p_zcl_header != NULL);
    assert(p_msg != NULL);

    read_attribute_response_record_t records[_ZCL_MAX_READ_ATTRIBUTES] = {0};
    // create read attribute response records
    uint8_t num_records = 0;
    bytes_parser_t parser = BYTES_PARSER(p_msg, (uint8_t)length);
    sl_status_t status = SL_STATUS_OK;

    for(uint8_t i=0; i<ARRAY_SIZE(records); i++)
    {
        bytes_parse_u16_le(&parser, &records[i].attribute_id);
        bool parsed = bytes_parse_u8(&parser, (uint8_t *)&records[i].status);

        if((records[i].status == SL_STATUS_OK) && (parsed == true))
        {
            bytes_parse_u8(&parser, &records[i].attribute_type);
            records[i].data_length = _get_length_from_attribute_type(records[i].attribute_type, p_msg[parser.index]);

            if((records[i].attribute_type == ZCL_OCTET_STRING_ATTRIBUTE_TYPE) ||
                (records[i].attribute_type == ZCL_CHAR_STRING_ATTRIBUTE_TYPE))
            {
                parsed = bytes_parse_u8(&parser, &records[i].u.s.str_length);
                if(records[i].u.s.str_length <= records[i].data_length)
                {
                    uint8_t str_length = records[i].u.s.str_length;
                    uint8_t remaining_len = parser.length - parser.index;
                    records[i].u.s.str_length = MIN(remaining_len, str_length);
                    records[i].u.s.p_str = &parser.p_bytes_in[parser.index];
                    parser.index += records[i].u.s.str_length;
                }
                else
                {
                    records[i].u.s.str_length = 0;
                    records[i].u.s.p_str = NULL;
                    parsed = false;
                }
            }
            else
            {
                switch(records[i].data_length)
                {
                    case 1:
                        parsed = bytes_parse_u8(&parser, (uint8_t*)&records[i].u.data8);
                        break;

                    case 2:
                        parsed = bytes_parse_u16_le(&parser, (uint16_t*)&records[i].u.data16);
                        break;

                    case 4:
                        parsed = bytes_parse_u32_le(&parser, &records[i].u.data32);
                        break;

                    default:
                        sl_zigbee_af_core_println("UNSUPP len[%d], type=%d, len=%d",
                                    i, records[i].attribute_type, records[i].data_length);
                        status = SL_STATUS_FAIL;
                        break;
                }
            }
        }
        else
        {
            records[i].data_length = 0;
        }

        if(parsed == true)
        {
            num_records++;
            if((parser.index > parser.length) || ((parser.length - parser.index) < _ZCL_READ_ATT_RSP_RECORD_MIN_LENGTH))
            {
                break;
            }
        }
        else
        {
            break;
        }
    }
    // extracted all attribute records
    for(uint8_t next_i=0; next_i<ARRAY_SIZE(m_zcl_rsp_callbacks); next_i++)
    {
        next_i = _get_next_matching_callback_index(p_zcl_header, apsFrame->clusterId, next_i);
        if(next_i < ARRAY_SIZE(m_zcl_rsp_callbacks))
        {
            zcl_read_attr_rsp_cb cb = (zcl_read_attr_rsp_cb)m_zcl_rsp_callbacks[next_i].cb_u;
            cb(sender, apsFrame->sourceEndpoint, status,
                apsFrame->clusterId, p_zcl_header->mfg_code,
                records, num_records);
        }
    }
}

static void _zcl_handle_report_attributes(uint16_t sender, sl_zigbee_aps_frame_t *apsFrame, zcl_header_t *p_zcl_header,
                                            uint8_t *p_msg, uint16_t length)
{
    #define _MIN_REPORT_LENGTH  4
    assert(apsFrame != NULL);
    assert(p_zcl_header != NULL);
    assert(p_msg != NULL);

    if((apsFrame->clusterId == ZCL_OCCUPANCY_SENSING_CLUSTER_ID) &&
        (length >= _MIN_REPORT_LENGTH))
    {
        uint16_t attribute_id = 0xFFFF;
        uint8_t attribute_type = 0;
        uint8_t occupied = 0;
        bytes_parser_t parser = BYTES_PARSER(p_msg, (uint8_t)length);
        bytes_parse_u16_le(&parser, &attribute_id);
        bytes_parse_u8(&parser, &attribute_type);
        bool parsed = bytes_parse_u8(&parser, &occupied);
        if((parsed == true) && (attribute_id == ZCL_OCCUPANCY_ATTRIBUTE_ID))
        {
            // occupancy update
            for(uint8_t i=0; i<ARRAY_SIZE(m_zcl_report_callbacks); i++)
            {
                if(m_zcl_report_callbacks[i].occ_cb != NULL)
                {
                    m_zcl_report_callbacks[i].occ_cb(sender, apsFrame->sourceEndpoint, occupied);
                }
            }
        }
    }
}

static void _zcl_handle_write_attrib_rsp_cb(uint16_t sender, sl_zigbee_aps_frame_t *apsFrame, zcl_header_t * p_zcl_header,
                                            uint8_t *p_msg, uint16_t length)
{
    if(length < 1)
    {
        return;
    }
    sl_zigbee_af_status_t status = (sl_zigbee_af_status_t)p_msg[0];
    for(uint8_t next_i=0; next_i<ARRAY_SIZE(m_zcl_rsp_callbacks); next_i++)
    {
        next_i = _get_next_matching_callback_index(p_zcl_header, apsFrame->clusterId, next_i);
        if(next_i < ARRAY_SIZE(m_zcl_rsp_callbacks))
        {
            zcl_write_attr_rsp_cb cb = (zcl_write_attr_rsp_cb)m_zcl_rsp_callbacks[next_i].cb_u;
            cb(sender, apsFrame->sourceEndpoint, status);
        }
    }
}
