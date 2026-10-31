#include <gtest/gtest.h>
#include <map>
#include <vector>
#include "fff.h"

extern "C" {
#include "nt3_test_stubs.h"
#include "fsm_event_scheduler.h"
#include "tx_queue.h"
}

DEFINE_FFF_GLOBALS

extern "C" {
FAKE_VOID_FUNC_VARARG(app_logger, const char *, ...);
FAKE_VOID_FUNC_VARARG(sl_zigbee_af_core_println, const char *, ...);
FAKE_VALUE_FUNC(uint32_t, uptime_get_ms32);
FAKE_VOID_FUNC(fsm_event_scheduler_init_event, sl_zigbee_af_event_t *, event_handler);
FAKE_VOID_FUNC(fsm_event_scheduler_schedule_event, sl_zigbee_af_event_t *, uint32_t);
FAKE_VALUE_FUNC(bool, fsm_event_scheduler_is_event_active, sl_zigbee_af_event_t *);
FAKE_VALUE_FUNC(sli_buffer_manager_buffer_t, sl_legacy_buffer_manager_fill_linked_buffers, const uint8_t *, uint8_t);
FAKE_VALUE_FUNC(uint16_t, sl_legacy_buffer_manager_message_buffer_length, sli_buffer_manager_buffer_t);
FAKE_VOID_FUNC(sli_legacy_buffer_manager_copy_from_linked_buffers, uint8_t *, sli_buffer_manager_buffer_t, uint8_t);
FAKE_VOID_FUNC(sli_legacy_buffer_manager_mark_buffer, sli_buffer_manager_buffer_t *);
FAKE_VALUE_FUNC(sl_status_t, sl_zigbee_send_unicast, sl_zigbee_outgoing_message_type_t, uint16_t,
                sl_zigbee_aps_frame_t *, uint16_t, uint8_t, const uint8_t *, uint8_t *);
FAKE_VALUE_FUNC(sl_status_t, sl_zigbee_send_broadcast, sl_802154_short_addr_t, sl_802154_short_addr_t, uint8_t,
                sl_zigbee_aps_frame_t *, uint8_t, uint16_t, uint8_t, const uint8_t *, uint8_t *);
FAKE_VALUE_FUNC(sl_status_t, sl_zigbee_send_multicast, sl_zigbee_aps_frame_t *, uint8_t, uint16_t,
                sl_802154_short_addr_t, uint8_t, uint16_t, uint8_t, const uint8_t *, uint8_t *);
FAKE_VALUE_FUNC(sl_status_t, sl_zigbee_af_send_unicast, sl_zigbee_outgoing_message_type_t, uint16_t,
                sl_zigbee_aps_frame_t *, uint16_t, uint8_t *);
}

/* --------------------------------------------------------------- stack buffer model */
static std::map<sli_buffer_manager_buffer_t, std::vector<uint8_t>> m_buffers;
static sli_buffer_manager_buffer_t m_next_buffer;
static bool m_buffers_exhausted;
static std::vector<sli_buffer_manager_buffer_t> m_marked;

static sli_buffer_manager_buffer_t fill(const uint8_t *p, uint8_t len)
{
    if(m_buffers_exhausted) { return SL_ZIGBEE_NULL_MESSAGE_BUFFER; }
    m_buffers[m_next_buffer] = std::vector<uint8_t>(p, p + len);
    return m_next_buffer++;
}
static uint16_t length(sli_buffer_manager_buffer_t b) { return (uint16_t)m_buffers.at(b).size(); }
static void copy(uint8_t *out, sli_buffer_manager_buffer_t b, uint8_t len) { memcpy(out, m_buffers.at(b).data(), len); }
static void mark(sli_buffer_manager_buffer_t *b) { m_marked.push_back(*b); }

/* --------------------------------------------------------------- scheduler model */
static event_handler m_handler;
static bool m_event_active;
static std::vector<uint32_t> m_schedules;
static void init_event(sl_zigbee_af_event_t *, event_handler h) { m_handler = h; }
static void schedule(sl_zigbee_af_event_t *, uint32_t ms) { m_event_active = true; m_schedules.push_back(ms); }
static bool is_active(sl_zigbee_af_event_t *) { return m_event_active; }

/* --------------------------------------------------------------- sends */
struct Sent
{
    char kind;                       // 'u' unicast, 'a' af unicast, 'b' broadcast, 'm' multicast
    uint16_t dest;
    sl_zigbee_aps_frame_t aps;
    std::vector<uint8_t> data;
    uint16_t bcast_addr;
};
static std::vector<Sent> m_sent;
static std::vector<sl_status_t> m_send_results;   // returned in order, then SL_STATUS_OK
static sl_status_t next_result(void)
{
    if(m_send_results.empty()) { return SL_STATUS_OK; }
    sl_status_t s = m_send_results.front();
    m_send_results.erase(m_send_results.begin());
    return s;
}
static sl_status_t send_unicast(sl_zigbee_outgoing_message_type_t, uint16_t dest, sl_zigbee_aps_frame_t *a,
                                uint16_t, uint8_t len, const uint8_t *p, uint8_t *)
{ m_sent.push_back({ 'u', dest, *a, std::vector<uint8_t>(p, p + len), 0 }); return next_result(); }
static sl_status_t af_send_unicast(sl_zigbee_outgoing_message_type_t, uint16_t dest, sl_zigbee_aps_frame_t *a,
                                   uint16_t len, uint8_t *p)
{ m_sent.push_back({ 'a', dest, *a, std::vector<uint8_t>(p, p + len), 0 }); return next_result(); }
static sl_status_t send_broadcast(sl_802154_short_addr_t, sl_802154_short_addr_t dest, uint8_t, sl_zigbee_aps_frame_t *a,
                                  uint8_t, uint16_t, uint8_t len, const uint8_t *p, uint8_t *)
{ m_sent.push_back({ 'b', dest, *a, std::vector<uint8_t>(p, p + len), 0 }); return next_result(); }
static sl_status_t send_multicast(sl_zigbee_aps_frame_t *a, uint8_t, uint16_t bcast, sl_802154_short_addr_t, uint8_t,
                                  uint16_t, uint8_t len, const uint8_t *p, uint8_t *)
{ m_sent.push_back({ 'm', 0, *a, std::vector<uint8_t>(p, p + len), bcast }); return next_result(); }

static uint32_t m_now;
static uint32_t now_ms(void) { return m_now; }

static uint8_t m_data[] = { 0x11, 0x22, 0x33 };

class TxQueue : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RESET_FAKE(app_logger); RESET_FAKE(sl_zigbee_af_core_println); RESET_FAKE(uptime_get_ms32);
        RESET_FAKE(fsm_event_scheduler_init_event); RESET_FAKE(fsm_event_scheduler_schedule_event);
        RESET_FAKE(fsm_event_scheduler_is_event_active);
        RESET_FAKE(sl_legacy_buffer_manager_fill_linked_buffers); RESET_FAKE(sl_legacy_buffer_manager_message_buffer_length);
        RESET_FAKE(sli_legacy_buffer_manager_copy_from_linked_buffers); RESET_FAKE(sli_legacy_buffer_manager_mark_buffer);
        RESET_FAKE(sl_zigbee_send_unicast); RESET_FAKE(sl_zigbee_send_broadcast);
        RESET_FAKE(sl_zigbee_send_multicast); RESET_FAKE(sl_zigbee_af_send_unicast);
        FFF_RESET_HISTORY();

        uptime_get_ms32_fake.custom_fake = now_ms;
        fsm_event_scheduler_init_event_fake.custom_fake = init_event;
        fsm_event_scheduler_schedule_event_fake.custom_fake = schedule;
        fsm_event_scheduler_is_event_active_fake.custom_fake = is_active;
        sl_legacy_buffer_manager_fill_linked_buffers_fake.custom_fake = fill;
        sl_legacy_buffer_manager_message_buffer_length_fake.custom_fake = length;
        sli_legacy_buffer_manager_copy_from_linked_buffers_fake.custom_fake = copy;
        sli_legacy_buffer_manager_mark_buffer_fake.custom_fake = mark;
        sl_zigbee_send_unicast_fake.custom_fake = send_unicast;
        sl_zigbee_af_send_unicast_fake.custom_fake = af_send_unicast;
        sl_zigbee_send_broadcast_fake.custom_fake = send_broadcast;
        sl_zigbee_send_multicast_fake.custom_fake = send_multicast;

        m_buffers.clear(); m_next_buffer = 1; m_buffers_exhausted = false; m_marked.clear();
        m_handler = NULL; m_event_active = false; m_schedules.clear();
        m_sent.clear(); m_send_results.clear();
        m_now = 1000;

        tx_queue_on_startup();
        ASSERT_NE(nullptr, m_handler);
    }

    // run the tx event, as the stack would when it fires
    static void run_event(void)
    {
        m_event_active = false;
        m_handler(NULL);
    }
};

/* --------------------------------------------------------------- send paths */
TEST_F(TxQueue, UnicastIsQueuedThenSentWithRetryRouteAndEui64Options)
{
    EXPECT_EQ(SL_STATUS_OK, tx_queue_add_unicast(0x1234, 1, 2, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data)));
    EXPECT_TRUE(m_sent.empty());                      // nothing sent until the event runs
    ASSERT_EQ(1u, m_schedules.size());
    EXPECT_EQ(1u, m_schedules[0]);

    run_event();
    ASSERT_EQ(1u, m_sent.size());
    const Sent &s = m_sent[0];
    EXPECT_EQ('u', s.kind);
    EXPECT_EQ(0x1234, s.dest);
    EXPECT_EQ(0x0006, s.aps.clusterId);
    EXPECT_EQ(HA_PROFILE_ID, s.aps.profileId);
    EXPECT_EQ(1, s.aps.sourceEndpoint);
    EXPECT_EQ(2, s.aps.destinationEndpoint);
    EXPECT_EQ(SL_ZIGBEE_APS_OPTION_RETRY | SL_ZIGBEE_APS_OPTION_ENABLE_ROUTE_DISCOVERY | SL_ZIGBEE_APS_OPTION_SOURCE_EUI64,
              s.aps.options);
    EXPECT_EQ(std::vector<uint8_t>(m_data, m_data + sizeof(m_data)), s.data);
    EXPECT_EQ(1u, m_schedules.size());                // queue empty: no further event
}

TEST_F(TxQueue, BroadcastAddressIsSentAsBroadcast)
{
    tx_queue_add_unicast(SL_ZIGBEE_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS, 1, 0xFF, 0x0019, HA_PROFILE_ID, m_data, sizeof(m_data));
    run_event();
    ASSERT_EQ(1u, m_sent.size());
    EXPECT_EQ('b', m_sent[0].kind);
    EXPECT_EQ(SL_ZIGBEE_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS, m_sent[0].dest);
    EXPECT_EQ(SL_ZIGBEE_APS_OPTION_SOURCE_EUI64, m_sent[0].aps.options);
}

TEST_F(TxQueue, MulticastGoesToTheGroup)
{
    EXPECT_EQ(SL_STATUS_OK, tx_queue_add_multicast(0x4321, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data)));
    run_event();
    ASSERT_EQ(1u, m_sent.size());
    EXPECT_EQ('m', m_sent[0].kind);
    EXPECT_EQ(0x4321, m_sent[0].aps.groupId);
    EXPECT_EQ(SL_ZIGBEE_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS, m_sent[0].bcast_addr);
    EXPECT_EQ(SL_ZIGBEE_APS_OPTION_SOURCE_EUI64, m_sent[0].aps.options);
}

TEST_F(TxQueue, MessagesAreSentInOrderOnePerEvent)
{
    for(uint16_t i = 0; i < 3; i++)
    {
        tx_queue_add_unicast(0x1000 + i, 1, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data));
    }
    EXPECT_EQ(1u, m_schedules.size());   // event already pending: not re-scheduled

    run_event();
    EXPECT_EQ(1u, m_sent.size());
    EXPECT_EQ(5u, m_schedules.back());   // next one 5 ms later
    run_event();
    run_event();
    ASSERT_EQ(3u, m_sent.size());
    EXPECT_EQ(0x1000, m_sent[0].dest);
    EXPECT_EQ(0x1001, m_sent[1].dest);
    EXPECT_EQ(0x1002, m_sent[2].dest);
}

TEST_F(TxQueue, LongMessageIsSentWithoutSourceEui64)
{
    uint8_t big[72] = {};
    tx_queue_add_unicast(0x1234, 1, 1, 0x0006, HA_PROFILE_ID, big, sizeof(big));
    run_event();
    ASSERT_EQ(1u, m_sent.size());
    EXPECT_EQ(0, m_sent[0].aps.options & SL_ZIGBEE_APS_OPTION_SOURCE_EUI64);
    EXPECT_EQ(72u, m_sent[0].data.size());
}

TEST_F(TxQueue, UnicastTooLongIsRetriedWithoutEui64)
{
    m_send_results = { SL_STATUS_MESSAGE_TOO_LONG };
    tx_queue_add_unicast(0x1234, 1, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data));
    run_event();
    ASSERT_EQ(2u, m_sent.size());
    EXPECT_EQ('a', m_sent[1].kind);
    EXPECT_EQ(0, m_sent[1].aps.options & SL_ZIGBEE_APS_OPTION_SOURCE_EUI64);
}

/* --------------------------------------------------------------- errors */
TEST_F(TxQueue, TransientErrorKeepsTheMessageAndRetries)
{
    for(sl_status_t transient : { SL_STATUS_ZIGBEE_MAX_MESSAGE_LIMIT_REACHED, SL_STATUS_ALLOCATION_FAILED, SL_STATUS_BUSY })
    {
        m_sent.clear();
        m_send_results = { transient };
        tx_queue_add_unicast(0x1234, 1, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data));
        run_event();
        EXPECT_EQ(50u, m_schedules.back()) << "status " << transient;
        run_event();
        ASSERT_EQ(2u, m_sent.size());
        EXPECT_EQ(m_sent[0].data, m_sent[1].data);   // same message again
    }
}

TEST_F(TxQueue, MessageStillFailingAfterLifetimeIsDropped)
{
    tx_queue_add_unicast(0x1111, 1, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data));
    tx_queue_add_unicast(0x2222, 1, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data));
    m_send_results = { SL_STATUS_BUSY, SL_STATUS_BUSY };
    run_event();
    run_event();
    EXPECT_EQ(0u, app_logger_fake.call_count);

    m_now += 20001;          // lifetime is 20 s
    run_event();
    EXPECT_EQ(2u, app_logger_fake.call_count);           // TX DROP, once per expired message
    EXPECT_EQ(2u, m_sent.size());                        // nothing more sent
}

TEST_F(TxQueue, PermanentErrorDropsTheMessageAndMovesOn)
{
    m_send_results = { SL_STATUS_FAIL };
    tx_queue_add_unicast(0x1111, 1, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data));
    tx_queue_add_unicast(0x2222, 1, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data));
    run_event();
    EXPECT_EQ(1u, app_logger_fake.call_count);           // TX ERR
    run_event();
    ASSERT_EQ(2u, m_sent.size());
    EXPECT_EQ(0x2222, m_sent[1].dest);
}

/* --------------------------------------------------------------- capacity */
TEST_F(TxQueue, HoldsTenMessages)
{
    for(int i = 0; i < 10; i++)
    {
        EXPECT_EQ(SL_STATUS_OK, tx_queue_add_unicast(0x1234, 1, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data)));
    }
    EXPECT_EQ(SL_STATUS_NOT_FOUND, tx_queue_add_unicast(0x1234, 1, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data)));
    EXPECT_EQ(SL_STATUS_NOT_FOUND, tx_queue_add_multicast(0x0001, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data)));
}

TEST_F(TxQueue, NoStackBufferFailsWithoutLosingAQueueEntry)
{
    m_buffers_exhausted = true;
    for(int i = 0; i < 15; i++)
    {
        EXPECT_EQ(SL_STATUS_FAIL, tx_queue_add_unicast(0x1234, 1, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data)));
        EXPECT_EQ(SL_STATUS_FAIL, tx_queue_add_multicast(0x0001, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data)));
    }
    m_buffers_exhausted = false;
    for(int i = 0; i < 10; i++)
    {
        EXPECT_EQ(SL_STATUS_OK, tx_queue_add_unicast(0x1234, 1, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data)));
    }
}

TEST_F(TxQueue, SentMessageFreesItsEntry)
{
    for(int round = 0; round < 3; round++)
    {
        for(int i = 0; i < 10; i++)
        {
            ASSERT_EQ(SL_STATUS_OK, tx_queue_add_unicast(0x1234, 1, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data)));
        }
        for(int i = 0; i < 10; i++)
        {
            run_event();
        }
    }
    EXPECT_EQ(30u, m_sent.size());
}

/* --------------------------------------------------------------- buffer marking */
TEST_F(TxQueue, MarksTheBufferOfEveryQueuedMessage)
{
    tx_queue_add_unicast(0x1111, 1, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data));
    tx_queue_add_multicast(0x0001, 1, 0x0006, HA_PROFILE_ID, m_data, sizeof(m_data));
    tx_queue_on_mark_buffers_callback();
    ASSERT_EQ(2u, m_marked.size());
    EXPECT_EQ(1, m_marked[0]);
    EXPECT_EQ(2, m_marked[1]);

    m_marked.clear();
    run_event();                       // first one sent
    tx_queue_on_mark_buffers_callback();
    ASSERT_EQ(1u, m_marked.size());
    EXPECT_EQ(2, m_marked[0]);
}
