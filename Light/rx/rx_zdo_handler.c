#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "byte_utils.h"
#include "sys_defines.h"
#include "time_convert.h"
#include "uptime.h"
#include "rx_zdo_handler.h"

#include "af.h"
#include "stack-info.h"
#include "zigbee-device-stack.h"

#define DEBUG_PRINT(...)     // sl_zigbee_af_core_println

typedef struct
{
    uint16_t dest_addr16;
    uint8_t  cluster_id_lsb;
    uint8_t  tsn;
    uint32_t expire_uptime_ms;
    zdo_response_cb cb;
} zdo_handler_t;

#define _ANY_SOURCE_ADDR16  0xFFFC
#define _ANY_TSN            0xFF
#define _MAX_ZDO_HANDLERS   10
#define _MAX_ZDO_PAYLOAD_SZ  95     // an upper limit on how large ZDO messages can be
#define _ZDO_TX_TIMEOUT_MS  SECONDS_TO_MS(5)

static zdo_handler_t m_zdo_handlers[_MAX_ZDO_HANDLERS];
// Bit i set = m_zdo_handlers[i] is a registered command callback that is never evicted.
// Kept outside zdo_handler_t to save RAM.
static uint32_t m_persistent_mask = 0;
#if (_MAX_ZDO_HANDLERS > 32)
#error "m_persistent_mask is too small for _MAX_ZDO_HANDLERS"
#endif
static bool m_initialized = false;

static bool _handle_address_response(uint8_t *p_msg, uint8_t length, zdo_response_t *p_response);
static bool _handle_device_annouce_response(uint8_t *p_msg, uint8_t length, zdo_response_t *p_response);
static bool _handle_match_descriptor_response(uint8_t *p_msg, uint8_t length, zdo_response_t *p_response);
static bool _handle_permit_join_request(uint8_t *p_msg, uint8_t length, zdo_response_t *p_response);
static bool _handle_simple_descriptor_response(uint8_t *p_msg, uint8_t length, zdo_response_t *p_response);

static void _init_index(uint8_t index);
static bool _is_expired(uint8_t index, uint32_t now_ms);
static bool _is_persistent(uint8_t index);
static void _set_persistent(uint8_t index, bool persistent);
static uint8_t _zdo_handler_get_index(void);
static sl_status_t _zdo_handler_set(uint16_t dest_addr16, uint16_t cluster_id, uint8_t tsn,
                                    zdo_response_cb cb, bool persistent);


void rx_zdo_on_startup(void)
{
    for(uint8_t i=0; i<ARRAY_SIZE(m_zdo_handlers); i++)
    {
        _init_index(i);
    }
    m_initialized = true;
}

void rx_zdo_rx_handler(uint16_t sender, sl_zigbee_aps_frame_t *apsFrame,
                       uint8_t *p_msg, uint8_t msg_len)
{
    assert(apsFrame != NULL);
    if(sender == sl_zigbee_get_node_id())
    {
        // ignore any responses sent back by the stack internally
        return;
    }

    if((p_msg == NULL) || (msg_len < 2) || (apsFrame->profileId != SL_ZIGBEE_ZDO_PROFILE_ID))
    {
        //  invalid packet
        return;
    }

    uint8_t transaction_seq_num = p_msg[0];
    sl_zigbee_zdo_status_t zdo_status = SL_ZIGBEE_ZDP_SUCCESS;
    if(apsFrame->clusterId & 0x8000)
    {
        // response message with TSN and status before the payload
        zdo_status = (sl_zigbee_zdo_status_t)p_msg[1];
        msg_len -= 2;
        p_msg += 2;
    }
    else
    {
        // Not a response, only TSN.  We will set the TSN to ANY
        // to search for a match in the registered callbacks.
        transaction_seq_num = _ANY_TSN;
        zdo_status = SL_ZIGBEE_ZDP_SUCCESS;
        msg_len -= 1;
        p_msg += 1;
    }

    zdo_response_t response =
    {
        .cluster_id = apsFrame->clusterId
    };
    bool formatted = false;

    switch(apsFrame->clusterId)
    {
        case NETWORK_ADDRESS_RESPONSE:
        case IEEE_ADDRESS_RESPONSE:
            formatted = _handle_address_response(p_msg, msg_len, &response);
            break;

        case MATCH_DESCRIPTORS_RESPONSE:
            formatted = _handle_match_descriptor_response(p_msg, msg_len, &response);
            break;

        case SIMPLE_DESCRIPTOR_RESPONSE:
            formatted = _handle_simple_descriptor_response(p_msg, msg_len, &response);
            break;

        case END_DEVICE_ANNOUNCE:
            formatted = _handle_device_annouce_response(p_msg, msg_len, &response);
            break;

        case PERMIT_JOINING_REQUEST:
            formatted = _handle_permit_join_request(p_msg, msg_len, &response);
            break;

        default:
            break;
    }

    if((formatted == true) && (m_initialized == true))
    {
        // have a valid response structure - look for any consuming callbacks
        uint32_t now_ms = uptime_get_ms32();
        for(uint8_t i=0; i<ARRAY_SIZE(m_zdo_handlers); i++)
        {
            if(m_zdo_handlers[i].cb == NULL)
            {
                continue;
            }
            if(_is_expired(i, now_ms) == true)
            {
                // response window closed - once the 8-bit TSN wraps, a stale entry
                // could match a response to a newer request
                _init_index(i);
                continue;
            }
            if(((m_zdo_handlers[i].dest_addr16 == sender) || (m_zdo_handlers[i].dest_addr16 >= _ANY_SOURCE_ADDR16)) &&
                (m_zdo_handlers[i].cluster_id_lsb == (apsFrame->clusterId & 0xFF)) &&
                (m_zdo_handlers[i].tsn == transaction_seq_num))
            {
                // response received
                zdo_response_cb cb = m_zdo_handlers[i].cb;
                if(m_zdo_handlers[i].dest_addr16 == sender)
                {
                    // single response expected from unicast ZDO - clear entry
                    // otherwise, keep it to handle responses from ohter devices - broadcast, multicast
                    _init_index(i);
                }
                cb(zdo_status, sender, &response);
                if(m_zdo_handlers[i].dest_addr16 != _ANY_SOURCE_ADDR16)
                {
                    break;      // only expect one callback match
                }
            }
        }
    }
}

sl_status_t rx_zdo_register_zdo_cmd_callback(uint16_t cluster_id, zdo_response_cb cb)
{
    // Command callbacks (device announce, permit join, etc.) are registered once on startup
    // and must remain registered, so they are stored as persistent entries.
    return _zdo_handler_set(_ANY_SOURCE_ADDR16, cluster_id, _ANY_TSN, cb, true);
}

sl_status_t rx_zdo_register_zdo_callback(uint16_t sender, uint16_t cluster_id,
                                         uint8_t transaction_seq_num, zdo_response_cb cb)
{
    return _zdo_handler_set(sender, cluster_id, transaction_seq_num, cb, false);
}


static bool _handle_address_response(uint8_t *p_msg, uint8_t length, zdo_response_t *p_response)
{
    #define _MIN_ZDO_ADDRESS_RESP_LEN  10
    assert(p_msg != NULL);
    assert(p_response != NULL);
    bool handled = (length >= _MIN_ZDO_ADDRESS_RESP_LEN);
    if(handled == true)
    {
        memcpy(p_response->u.addr_rsp.ieee_addr, p_msg, 8);
        p_response->u.addr_rsp.addr16 = BYTES_LE_TO_U16(p_msg[8], p_msg[9]);
    }
    return handled;
}

static bool _handle_device_annouce_response(uint8_t *p_msg, uint8_t length, zdo_response_t *p_response)
{
    #define _MIN_ZDO_DEVICE_ANNOUNCE_LEN 11
    assert(p_msg != NULL);
    assert(p_response != NULL);
    bool handled = (length >= _MIN_ZDO_DEVICE_ANNOUNCE_LEN);
    if(handled == true)
    {
        p_response->u.dev_announce.addr16 = BYTES_LE_TO_U16(p_msg[0], p_msg[1]);
        memcpy(p_response->u.dev_announce.ieee_addr, &p_msg[2], 8);
        p_response->u.dev_announce.capability = p_msg[10];
    }
    return handled;
}

static bool _handle_match_descriptor_response(uint8_t *p_msg, uint8_t length, zdo_response_t *p_response)
{
    #define _MIN_ZDO_MATCH_DESCR_RESP_LEN  3
    assert(p_msg != NULL);
    assert(p_response != NULL);
    bool handled = (length >= _MIN_ZDO_MATCH_DESCR_RESP_LEN);
    if(handled == true)
    {
        p_response->u.match_desc_rsp.addr16 = BYTES_LE_TO_U16(p_msg[0], p_msg[1]);
        p_response->u.match_desc_rsp.num_endpoints = p_msg[2];
        uint8_t match_length = MIN(p_msg[2], ARRAY_SIZE(p_response->u.match_desc_rsp.endpoints));
        for(uint8_t i=0; i<match_length; i++)
        {
            p_response->u.match_desc_rsp.endpoints[i] = p_msg[3+i];
        }
    }
    return handled;
}

static bool _handle_permit_join_request(uint8_t *p_msg, uint8_t length, zdo_response_t *p_response)
{
    #define _MIN_PERMIT_JOIN_REQ_LEN  2
    // [Duration (sec)] + [TC Significance]
    assert(p_msg != NULL);
    assert(p_response != NULL);
    bool handled = (length >= _MIN_PERMIT_JOIN_REQ_LEN);
    if(handled == true)
    {
        p_response->u.permit_join_req.duration_sec = p_msg[0];
        p_response->u.permit_join_req.tc_significance = p_msg[1];
    }
    return handled;
}

static bool _handle_simple_descriptor_response(uint8_t *p_msg, uint8_t length, zdo_response_t *p_response)
{
    #define _MIN_ZDO_SIMPLE_DESCR_RESP_LEN  11
    // [NwkAddr (2)] + [length] + [Simple Desc]
    // SD:  [endpoint] + [profileId (2)] + [deviceId (2)] + [version] + [inClusterCount] +
    //      [inClusters (2*n)] + [outClusterCount] + [outClusters (2*n)]
    assert(p_msg != NULL);
    assert(p_response != NULL);
    bool handled = (length >= _MIN_ZDO_SIMPLE_DESCR_RESP_LEN);
    if(handled == true)
    {
        // skip addr16 and descriptor length fields
        bytes_parser_t parser = BYTES_PARSER(p_msg, length);
        bytes_parse_u16_le(&parser, &p_response->u.simple_desc_rsp.addr16);
        // skip descriptor length
        uint8_t dummy = 0;
        bytes_parse_u8(&parser, &dummy);

        bytes_parse_u8(&parser, &p_response->u.simple_desc_rsp.endpoint);
        bytes_parse_u16_le(&parser, &p_response->u.simple_desc_rsp.profile_id);
        bytes_parse_u16_le(&parser, &p_response->u.simple_desc_rsp.device_id);
        // skip version
        bytes_parse_u8(&parser, &dummy);
        uint8_t num_clusters = 0;
        bool parsed = bytes_parse_u8(&parser, &num_clusters);
        p_response->u.simple_desc_rsp.num_in_clusters = MIN(num_clusters, \
                                                        ARRAY_SIZE(p_response->u.simple_desc_rsp.in_clusters));

        for(uint8_t i=0; i<num_clusters; i++)
        {
            uint16_t cid = 0;
            parsed = bytes_parse_u16_le(&parser, &cid);
            if(i < p_response->u.simple_desc_rsp.num_in_clusters)
            {
                p_response->u.simple_desc_rsp.in_clusters[i] = cid;
            }
            if(parsed == false)
            {
                break;
            }
        }
        parsed = bytes_parse_u8(&parser, &num_clusters);
        p_response->u.simple_desc_rsp.num_out_clusters = MIN(num_clusters, \
                                                        ARRAY_SIZE(p_response->u.simple_desc_rsp.out_clusters));

        for(uint8_t i=0; i<p_response->u.simple_desc_rsp.num_out_clusters; i++)
        {
            uint16_t cid = 0;
            parsed = bytes_parse_u16_le(&parser, &cid);
            if(i < p_response->u.simple_desc_rsp.num_out_clusters)
            {
                p_response->u.simple_desc_rsp.out_clusters[i] = cid;
            }
            if(parsed == false)
            {
                break;
            }
        }
        handled = (parsed == true);
    }
    return handled;
}

static void _init_index(uint8_t index)
{
    if(index < ARRAY_SIZE(m_zdo_handlers))
    {
        m_zdo_handlers[index].dest_addr16 = SL_ZIGBEE_NULL_NODE_ID;
        m_zdo_handlers[index].cluster_id_lsb = 0xFF;
        m_zdo_handlers[index].tsn = 0;
        m_zdo_handlers[index].expire_uptime_ms = 0;
        _set_persistent(index, false);
        m_zdo_handlers[index].cb = NULL;
    }
}

static bool _is_expired(uint8_t index, uint32_t now_ms)
{
    // Persistent (command) entries never expire.  Compared relative to now so the
    // check survives uptime_get_ms32() wrapping.
    return ((_is_persistent(index) == false) &&
            ((int32_t)(now_ms - m_zdo_handlers[index].expire_uptime_ms) > 0));
}

static bool _is_persistent(uint8_t index)
{
    return ((index < ARRAY_SIZE(m_zdo_handlers)) && ((m_persistent_mask & (1UL << index)) != 0));
}

static void _set_persistent(uint8_t index, bool persistent)
{
    if(index < ARRAY_SIZE(m_zdo_handlers))
    {
        if(persistent == true)
        {
            m_persistent_mask |= (1UL << index);
        }
        else
        {
            m_persistent_mask &= ~(1UL << index);
        }
    }
}

static uint8_t _zdo_handler_get_index(void)
{
    // Returns an unused index if available.  Otherwise, returns the non-persistent entry that
    // expires first (or expired the longest time ago).  Persistent entries are never returned.
    // Returns ARRAY_SIZE(m_zdo_handlers) if all entries are persistent.
    // Expiry is compared relative to now so the comparison survives uptime_get_ms32() wrapping.
    uint8_t index = ARRAY_SIZE(m_zdo_handlers);
    uint32_t now_ms = uptime_get_ms32();
    int32_t min_ms_until_expire = INT32_MAX;
    for(uint8_t i=0; i<ARRAY_SIZE(m_zdo_handlers); i++)
    {
        if(m_zdo_handlers[i].cb == NULL)
        {
            // unused index - use this
            index = i;
            break;
        }
        else if(_is_persistent(i) == false)
        {
            // use this if an unused entry cannot be found
            int32_t ms_until_expire = (int32_t)(m_zdo_handlers[i].expire_uptime_ms - now_ms);
            if((index == ARRAY_SIZE(m_zdo_handlers)) || (ms_until_expire < min_ms_until_expire))
            {
                index = i;
                min_ms_until_expire = ms_until_expire;
            }
        }
    }
    return index;
}

static sl_status_t _zdo_handler_set(uint16_t dest_addr16, uint16_t cluster_id, uint8_t tsn,
                                    zdo_response_cb cb, bool persistent)
{
    sl_status_t status = SL_STATUS_FAIL;
    if(m_initialized == false)
    {
        return SL_STATUS_NOT_INITIALIZED;
    }
    if(cb == NULL)
    {
        // no need to store anything
        return SL_STATUS_OK;
    }
    uint8_t i = ARRAY_SIZE(m_zdo_handlers);
    if(persistent == true)
    {
        // don't add a duplicate entry if this callback is already registered
        for(uint8_t j=0; j<ARRAY_SIZE(m_zdo_handlers); j++)
        {
            if((_is_persistent(j) == true) &&
                (m_zdo_handlers[j].cluster_id_lsb == (uint8_t)(cluster_id & 0xFF)) &&
                (m_zdo_handlers[j].cb == cb))
            {
                i = j;
                break;
            }
        }
    }
    if(i >= ARRAY_SIZE(m_zdo_handlers))
    {
        i = _zdo_handler_get_index();
    }
    if(i < ARRAY_SIZE(m_zdo_handlers))
    {
        m_zdo_handlers[i].dest_addr16 = dest_addr16;
        m_zdo_handlers[i].cluster_id_lsb = (uint8_t)(cluster_id & 0xFF);
        m_zdo_handlers[i].tsn = tsn;
        m_zdo_handlers[i].expire_uptime_ms = uptime_get_ms32() + _ZDO_TX_TIMEOUT_MS;
        _set_persistent(i, persistent);
        m_zdo_handlers[i].cb = cb;
        status = SL_STATUS_OK;
    }
    else
    {
        status = SL_STATUS_FULL;
    }
    return status;
}
