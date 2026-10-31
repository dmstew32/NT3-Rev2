#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_logger.h"
#include "fsm_event_scheduler.h"
#include "sys_defines.h"
#include "time_convert.h"
#include "tx_queue.h"
#include "uptime.h"

#include "buffer_manager/legacy-packet-buffer.h"
#include "message.h"

/**
 * Paces outgoing messages to the stack, one every _NEXT_TX_DELAY_MS.  A message is
 * complete once the stack accepts it - delivery is not tracked.  If the stack is
 * temporarily out of resources, the message stays at the head of the queue and is
 * retried until _TX_PACKET_LIFETIME_MS expires.
 **/

#define _APS_MULTICAST_OPTIONS  (SL_ZIGBEE_APS_OPTION_SOURCE_EUI64)
#define _APS_UNICAST_OPTIONS    (sl_zigbee_aps_option_t)(SL_ZIGBEE_APS_OPTION_RETRY | SL_ZIGBEE_APS_OPTION_ENABLE_ROUTE_DISCOVERY | \
                                 SL_ZIGBEE_APS_OPTION_SOURCE_EUI64)
#define _NEXT_TX_DELAY_MS       5
#define _TX_RETRY_DELAY_MS      50                  // retry delay when the stack is out of resources
#define _TX_PACKET_LIFETIME_MS  SECONDS_TO_MS(20)   // max time a packet can exist in tx queue

#ifdef TX_QUEUE_DEBUG
  #define txQueuePrintln(...) sl_zigbee_af_core_println(__VA_ARGS__)
#else
  #define txQueuePrintln(...)
#endif

typedef enum
{
    _MSG_TYPE_UNICAST,
    _MSG_TYPE_BCAST,
    _MSG_TYPE_MCAST
} msg_type_t;


typedef struct
{
    sl_zigbee_aps_frame_t apsFrame;
    uint16_t destAddr16;
    msg_type_t msg_type;
    sli_buffer_manager_buffer_t message;
    uint32_t allocated_ms;
    uint8_t data_length;
    uint16_t num_retries;
} tx_item_t;

typedef struct tx_queue_node_s
{
    struct tx_queue_node_s *next;
    tx_item_t item;
} tx_queue_node_t;

#define _NUM_TX_QUEUE_ENTRIES   10
#define _TX_QUEUE_TRANSMIT_DELAY_MS 1

static tx_queue_node_t m_tx_queue[_NUM_TX_QUEUE_ENTRIES];

static sl_zigbee_af_event_t m_schedule_tx_event;

static sl_status_t _fill_tx_queue_item(tx_queue_node_t *p_node, uint16_t destAddr16,
                                       uint16_t groupId, uint8_t srcEndpoint, uint8_t dstEndpoint,
                                       uint16_t clusterId, uint16_t profileId, msg_type_t msg_type,
                                       uint8_t *p_data, uint8_t data_length);

static bool _status_is_transient(sl_status_t status);
static sl_status_t _send_item(tx_item_t *p_item);
static void _schedule_tx_queue_event(void);
static void _schedule_tx_event_handler(sl_zigbee_af_event_t *p_event);

static tx_queue_node_t * _queue_pop(tx_queue_node_t **p_queue_head);
static void _queue_push(tx_queue_node_t **p_queue_head, tx_queue_node_t *p_node);

static void _clear_tx_queue_node_item(tx_queue_node_t *p_node);
static void _tx_free_queue_add(tx_queue_node_t *p_node);
static tx_queue_node_t * _tx_free_queue_pop(void);

static void _tx_queue_add(tx_queue_node_t *p_node);
static tx_queue_node_t * _tx_queue_pop(void);
static void _tx_queue_head_complete(void);

static tx_queue_node_t *pTxQueueHead = NULL;
static tx_queue_node_t *pTxFreeQueueHead = NULL;


void tx_queue_on_startup(void)
{
    pTxFreeQueueHead = NULL;
    for(uint8_t i=0; i<ARRAY_SIZE(m_tx_queue); i++)
    {
        _tx_free_queue_add(&m_tx_queue[i]);
    }
    pTxQueueHead = NULL;
    fsm_event_scheduler_init_event(&m_schedule_tx_event, _schedule_tx_event_handler);
}

sl_status_t tx_queue_add_unicast(uint16_t destAddr16, uint8_t srcEndpoint,
                          uint8_t dstEndpoint, uint16_t clusterId,
                          uint16_t profileId, uint8_t *p_data,
                          uint8_t data_length)
{
    tx_queue_node_t *p_node = _tx_free_queue_pop();
    if(p_node == NULL)
    {
        return SL_STATUS_NOT_FOUND;
    }

    msg_type_t msg_type = (sl_zigbee_is_zigbee_broadcast_address(destAddr16) == true) ?
                            _MSG_TYPE_BCAST : _MSG_TYPE_UNICAST;

    sl_status_t status = _fill_tx_queue_item(p_node, destAddr16, 0, srcEndpoint, dstEndpoint,
                                             clusterId, profileId, msg_type, p_data, data_length);

    if(status == SL_STATUS_OK)
    {
        _tx_queue_add(p_node);
        _schedule_tx_queue_event();
        txQueuePrintln("Scheduled TX Event");
    }
    else
    {
        // no stack buffer available - return the node or it is lost for good
        _tx_free_queue_add(p_node);
    }
    return status;
}

sl_status_t tx_queue_add_multicast(uint16_t groupId, uint8_t srcEndpoint,
                            uint16_t clusterId, uint16_t profileId,
                            uint8_t *p_data, uint8_t data_length)
{
    tx_queue_node_t *p_node = _tx_free_queue_pop();
    if(p_node == NULL)
    {
        return SL_STATUS_NOT_FOUND;
    }

    sl_status_t status = _fill_tx_queue_item(p_node, 0, groupId, srcEndpoint, 0,
                                             clusterId, profileId, _MSG_TYPE_MCAST, p_data, data_length);
    if(status == SL_STATUS_OK)
    {
        _tx_queue_add(p_node);
        _schedule_tx_queue_event();
        txQueuePrintln("Scheduled TX Event");
    }
    else
    {
        // no stack buffer available - return the node or it is lost for good
        _tx_free_queue_add(p_node);
    }
    return status;
}

void tx_queue_on_mark_buffers_callback(void)
{
    // This function is called during every emberTick() and must re-mark all
    // buffers that should not be reclaimed by the stack.  Every queued node holds
    // a valid buffer.  Only mark here - don't modify the queue during buffer cleanup.
    for(tx_queue_node_t *p_node = pTxQueueHead; p_node != NULL; p_node = p_node->next)
    {
        sli_legacy_buffer_manager_mark_buffer(&p_node->item.message);
    }
}


static void _clear_tx_queue_node_item(tx_queue_node_t *p_node)
{
    assert(p_node != NULL);
    p_node->item.allocated_ms = 0;
    p_node->item.num_retries = 0;
}

static sl_status_t _fill_tx_queue_item(tx_queue_node_t *p_node, uint16_t destAddr16,
                                       uint16_t groupId, uint8_t srcEndpoint, uint8_t dstEndpoint,
                                       uint16_t clusterId, uint16_t profileId, msg_type_t msg_type,
                                       uint8_t *p_data, uint8_t data_length)
{
    assert(p_node != NULL);

    tx_item_t *p_item = &p_node->item;
    p_item->apsFrame.profileId = profileId;
    p_item->apsFrame.clusterId = clusterId;
    p_item->apsFrame.sourceEndpoint = srcEndpoint;
    p_item->apsFrame.destinationEndpoint = dstEndpoint;
    p_item->apsFrame.options = (msg_type == _MSG_TYPE_UNICAST) ? _APS_UNICAST_OPTIONS : _APS_MULTICAST_OPTIONS;
    p_item->apsFrame.sequence = 0;      // assigned by the stack on send
    p_item->apsFrame.radius = 0;
    p_item->apsFrame.groupId = groupId;

    p_item->destAddr16 = destAddr16;
    p_item->msg_type = msg_type;
    p_item->message = sl_legacy_buffer_manager_fill_linked_buffers(p_data, data_length);
    p_item->data_length = data_length;

    p_item->allocated_ms = uptime_get_ms32();
    p_item->num_retries = 0;

    return (p_item->message != SL_ZIGBEE_NULL_MESSAGE_BUFFER) ? SL_STATUS_OK : SL_STATUS_FAIL;
}

static bool _status_is_transient(sl_status_t status)
{
    // stack resource limits that clear on their own - worth retrying
    return ((status == SL_STATUS_ZIGBEE_MAX_MESSAGE_LIMIT_REACHED) ||
            (status == SL_STATUS_ALLOCATION_FAILED) ||
            (status == SL_STATUS_BUSY));
}

#define _MSG_LENGTH_THRESHOLD_NO_ADDR_OPTION 72  // messages this size or longer are too large
                                                // for the packet to include a src/dst EUI64
static uint8_t m_tx_packet_data[100];

static sl_status_t _send_item(tx_item_t *p_item)
{
    assert(p_item != NULL);
    sl_status_t status = SL_STATUS_NOT_FOUND;
    uint8_t msg_length = sl_legacy_buffer_manager_message_buffer_length(p_item->message);
    if(msg_length >= _MSG_LENGTH_THRESHOLD_NO_ADDR_OPTION)
    {
        // message is too long for a src/dst EUI64 address in the nwk layer
        p_item->apsFrame.options &= ~SL_ZIGBEE_APS_OPTION_SOURCE_EUI64;
    }
    msg_length = MIN(msg_length, ARRAY_SIZE(m_tx_packet_data));
    sli_legacy_buffer_manager_copy_from_linked_buffers(m_tx_packet_data, p_item->message, msg_length);

    uint8_t aps_seq = 0;
    if(p_item->msg_type == _MSG_TYPE_UNICAST)
    {
        uint16_t tag = 0;
        status = sl_zigbee_send_unicast(SL_ZIGBEE_OUTGOING_DIRECT, p_item->destAddr16,
                                        &p_item->apsFrame, tag, msg_length,
                                        m_tx_packet_data, &aps_seq);

        if(status == SL_STATUS_MESSAGE_TOO_LONG)
        {
            p_item->apsFrame.options &= ~SL_ZIGBEE_APS_OPTION_SOURCE_EUI64;
            status = sl_zigbee_af_send_unicast(SL_ZIGBEE_OUTGOING_DIRECT, p_item->destAddr16,
                                  &p_item->apsFrame, msg_length, m_tx_packet_data);
        }
    }
    else if(p_item->msg_type == _MSG_TYPE_BCAST)
    {
        status = sl_zigbee_send_broadcast(SL_ZIGBEE_NULL_NODE_ID, p_item->destAddr16, 0,
                                &p_item->apsFrame, 0, 0, msg_length, m_tx_packet_data, &aps_seq);
    }
    else
    {
        status = sl_zigbee_send_multicast(&p_item->apsFrame, 0, SL_ZIGBEE_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS,
                                        SL_ZIGBEE_NULL_NODE_ID, 0, 0, msg_length, m_tx_packet_data, &aps_seq);
    }
    return status;
}

static void _schedule_tx_event_handler(sl_zigbee_af_event_t *p_event)
{
    (void)p_event;

    // drop any messages that have been waiting too long, e.g. behind a head that kept
    // failing with a transient error
    while(pTxQueueHead != NULL)
    {
        uint32_t age_ms = uptime_get_ms32() - pTxQueueHead->item.allocated_ms;
        if(age_ms <= _TX_PACKET_LIFETIME_MS)
        {
            break;
        }
        app_logger("TX DROP (expired), cid=0x%04x, retries=%d",
                   pTxQueueHead->item.apsFrame.clusterId, pTxQueueHead->item.num_retries);
        _tx_queue_head_complete();
    }

    if(pTxQueueHead == NULL)
    {
        return;
    }

    tx_item_t *p_item = &pTxQueueHead->item;
    sl_status_t status = _send_item(p_item);

    if(_status_is_transient(status) == true)
    {
        // stack is out of resources - keep the message at the head and retry.  The
        // lifetime check above drops it if the condition doesn't clear.
        if(p_item->num_retries < MAX_U16)
        {
            p_item->num_retries++;
        }
        txQueuePrintln("TX RETRY 0x%02x, cid=0x%04x", status, p_item->apsFrame.clusterId);
        fsm_event_scheduler_schedule_event(&m_schedule_tx_event, _TX_RETRY_DELAY_MS);
        return;
    }

    if(status != SL_STATUS_OK)
    {
        app_logger("TX ERR 0x%02x, cid=0x%04x, type=%d", status,
                        p_item->apsFrame.clusterId, p_item->msg_type);
    }
    // accepted by the stack, or a permanent error - done with this message
    _tx_queue_head_complete();
}

static void _schedule_tx_queue_event()
{
    if(fsm_event_scheduler_is_event_active(&m_schedule_tx_event) == false)
    {
        fsm_event_scheduler_schedule_event(&m_schedule_tx_event, _TX_QUEUE_TRANSMIT_DELAY_MS);
    }
}

static void _tx_queue_head_complete(void)
{
    tx_queue_node_t *p_head = _tx_queue_pop();
    if(p_head != NULL)
    {
        _tx_free_queue_add(p_head);
    }
    if(pTxQueueHead != NULL)
    {
        fsm_event_scheduler_schedule_event(&m_schedule_tx_event, _NEXT_TX_DELAY_MS);
    }
}

static tx_queue_node_t * _queue_pop(tx_queue_node_t **p_queue_head)
{
    tx_queue_node_t *phead = NULL;
    if(*p_queue_head != NULL)
    {
        phead = *p_queue_head;
        *p_queue_head = (*p_queue_head)->next;
        phead->next = NULL;
    }
    return phead;
}

static void _queue_push(tx_queue_node_t **p_queue_head, tx_queue_node_t *p_node)
{
    assert(p_node != NULL);
    if(*p_queue_head == NULL)
    {
        *p_queue_head = p_node;
        p_node->next = NULL;
    }
    else
    {
        tx_queue_node_t *plist = *p_queue_head;
        while(plist->next != NULL)
        {
            plist = plist->next;
        }
        // plist->next is now NULL
        plist->next = p_node;
        p_node->next = NULL;
    }
}

static void _tx_free_queue_add(tx_queue_node_t *p_node)
{
    _clear_tx_queue_node_item(p_node);
    _queue_push(&pTxFreeQueueHead, p_node);
}

static tx_queue_node_t * _tx_free_queue_pop()
{
    return _queue_pop(&pTxFreeQueueHead);
}

static void _tx_queue_add(tx_queue_node_t *p_node)
{
    _queue_push(&pTxQueueHead, p_node);
}

static tx_queue_node_t * _tx_queue_pop()
{
    return _queue_pop(&pTxQueueHead);
}
