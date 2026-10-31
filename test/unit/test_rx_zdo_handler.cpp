#include <gtest/gtest.h>
#include <vector>
#include "fff.h"

extern "C" {
#include "nt3_test_stubs.h"
#include "rx_zdo_handler.h"
}

DEFINE_FFF_GLOBALS

extern "C" {
FAKE_VALUE_FUNC(sl_802154_short_addr_t, sl_zigbee_get_node_id);
FAKE_VALUE_FUNC(uint32_t, uptime_get_ms32);
}

#define MY_NODE_ID   0x0001

/* --------------------------------------------------------------- recorders */
struct Rsp { sl_zigbee_zdo_status_t status; uint16_t sender; zdo_response_t rsp; };
static std::vector<Rsp> m_rsp;
static void cb(sl_zigbee_zdo_status_t status, uint16_t sender, zdo_response_t *p) { m_rsp.push_back({ status, sender, *p }); }
static std::vector<Rsp> m_rsp2;
static void cb2(sl_zigbee_zdo_status_t status, uint16_t sender, zdo_response_t *p) { m_rsp2.push_back({ status, sender, *p }); }

static uint32_t m_now;
static uint32_t now_ms(void) { return m_now; }

class RxZdoHandler : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RESET_FAKE(sl_zigbee_get_node_id);
        RESET_FAKE(uptime_get_ms32);
        FFF_RESET_HISTORY();
        sl_zigbee_get_node_id_fake.return_val = MY_NODE_ID;
        uptime_get_ms32_fake.custom_fake = now_ms;
        m_now = 1000;
        m_rsp.clear();
        m_rsp2.clear();
        rx_zdo_on_startup();
    }

    static void rx(uint16_t sender, uint16_t cluster, std::vector<uint8_t> msg, uint16_t profile = SL_ZIGBEE_ZDO_PROFILE_ID)
    {
        sl_zigbee_aps_frame_t a = {};
        a.profileId = profile;
        a.clusterId = cluster;
        rx_zdo_rx_handler(sender, &a, msg.data(), (uint8_t)msg.size());
    }

    // [tsn][status][ieee x8][addr16]
    static std::vector<uint8_t> ieee_rsp(uint8_t tsn, uint16_t addr16)
    {
        return { tsn, SL_ZIGBEE_ZDP_SUCCESS, 1, 2, 3, 4, 5, 6, 7, 8,
                 (uint8_t)(addr16 & 0xFF), (uint8_t)(addr16 >> 8) };
    }
};

/* --------------------------------------------------------------- responses */
TEST_F(RxZdoHandler, AddressResponseMatchedOnSenderClusterAndTsn)
{
    ASSERT_EQ(SL_STATUS_OK, rx_zdo_register_zdo_callback(0x1234, IEEE_ADDRESS_REQUEST, 7, cb));

    rx(0x1234, IEEE_ADDRESS_RESPONSE, ieee_rsp(7, 0x1234));

    ASSERT_EQ(1u, m_rsp.size());
    EXPECT_EQ(SL_ZIGBEE_ZDP_SUCCESS, m_rsp[0].status);
    EXPECT_EQ(0x1234, m_rsp[0].sender);
    EXPECT_EQ(IEEE_ADDRESS_RESPONSE, m_rsp[0].rsp.cluster_id);
    EXPECT_EQ(0x1234, m_rsp[0].rsp.u.addr_rsp.addr16);
    EXPECT_EQ(8, m_rsp[0].rsp.u.addr_rsp.ieee_addr[7]);
}

TEST_F(RxZdoHandler, UnicastEntryIsClearedAfterItsResponse)
{
    rx_zdo_register_zdo_callback(0x1234, IEEE_ADDRESS_REQUEST, 7, cb);
    rx(0x1234, IEEE_ADDRESS_RESPONSE, ieee_rsp(7, 0x1234));
    rx(0x1234, IEEE_ADDRESS_RESPONSE, ieee_rsp(7, 0x1234));
    EXPECT_EQ(1u, m_rsp.size());
}

TEST_F(RxZdoHandler, WrongTsnSenderOrClusterIsNotDelivered)
{
    rx_zdo_register_zdo_callback(0x1234, IEEE_ADDRESS_REQUEST, 7, cb);
    rx(0x1234, IEEE_ADDRESS_RESPONSE, ieee_rsp(8, 0x1234));            // tsn
    rx(0x4321, IEEE_ADDRESS_RESPONSE, ieee_rsp(7, 0x1234));            // sender
    rx(0x1234, MATCH_DESCRIPTORS_RESPONSE, { 7, 0, 0x34, 0x12, 0 });   // cluster
    EXPECT_TRUE(m_rsp.empty());
}

TEST_F(RxZdoHandler, ErrorStatusIsPassedOn)
{
    rx_zdo_register_zdo_callback(0x1234, IEEE_ADDRESS_REQUEST, 7, cb);
    std::vector<uint8_t> msg = ieee_rsp(7, 0x1234);
    msg[1] = 0x81;   // device not found
    rx(0x1234, IEEE_ADDRESS_RESPONSE, msg);
    ASSERT_EQ(1u, m_rsp.size());
    EXPECT_EQ(0x81, m_rsp[0].status);
}

TEST_F(RxZdoHandler, BroadcastRequestCollectsResponsesFromEveryone)
{
    rx_zdo_register_zdo_callback(0xFFFD, MATCH_DESCRIPTORS_REQUEST, 3, cb);
    rx(0x0010, MATCH_DESCRIPTORS_RESPONSE, { 3, 0, 0x10, 0x00, 1, 0x01 });
    rx(0x0020, MATCH_DESCRIPTORS_RESPONSE, { 3, 0, 0x20, 0x00, 2, 0x01, 0x02 });
    ASSERT_EQ(2u, m_rsp.size());
    EXPECT_EQ(0x0020, m_rsp[1].rsp.u.match_desc_rsp.addr16);
    EXPECT_EQ(2, m_rsp[1].rsp.u.match_desc_rsp.num_endpoints);
    EXPECT_EQ(0x02, m_rsp[1].rsp.u.match_desc_rsp.endpoints[1]);
}

TEST_F(RxZdoHandler, MatchDescriptorEndpointsAreLimited)
{
    rx_zdo_register_zdo_callback(0x1234, MATCH_DESCRIPTORS_REQUEST, 3, cb);
    rx(0x1234, MATCH_DESCRIPTORS_RESPONSE, { 3, 0, 0x34, 0x12, 6, 1, 2, 3, 4, 5, 6 });
    ASSERT_EQ(1u, m_rsp.size());
    EXPECT_EQ(6, m_rsp[0].rsp.u.match_desc_rsp.num_endpoints);   // as reported
    EXPECT_EQ(4, m_rsp[0].rsp.u.match_desc_rsp.endpoints[3]);    // only ZDO_MATCH_DESC_RESP_MAX_ENDPOINTS stored
}

TEST_F(RxZdoHandler, SimpleDescriptorResponse)
{
    rx_zdo_register_zdo_callback(0x1234, SIMPLE_DESCRIPTOR_REQUEST, 9, cb);
    rx(0x1234, SIMPLE_DESCRIPTOR_RESPONSE, {
        9, 0,
        0x34, 0x12,               // addr16
        14,                       // descriptor length
        0x01,                     // endpoint
        0x04, 0x01,               // profile 0x0104
        0x00, 0x01,               // device 0x0100
        0x01,                     // version
        2, 0x00, 0x00, 0x06, 0x00, // in: basic, on/off
        1, 0x19, 0x00,            // out: ota
    });
    ASSERT_EQ(1u, m_rsp.size());
    const zdo_simple_desc_rsp_t &sd = m_rsp[0].rsp.u.simple_desc_rsp;
    EXPECT_EQ(0x1234, sd.addr16);
    EXPECT_EQ(1, sd.endpoint);
    EXPECT_EQ(0x0104, sd.profile_id);
    EXPECT_EQ(0x0100, sd.device_id);
    ASSERT_EQ(2, sd.num_in_clusters);
    EXPECT_EQ(0x0006, sd.in_clusters[1]);
    ASSERT_EQ(1, sd.num_out_clusters);
    EXPECT_EQ(0x0019, sd.out_clusters[0]);
}

TEST_F(RxZdoHandler, TruncatedSimpleDescriptorIsNotDelivered)
{
    rx_zdo_register_zdo_callback(0x1234, SIMPLE_DESCRIPTOR_REQUEST, 9, cb);
    rx(0x1234, SIMPLE_DESCRIPTOR_RESPONSE, { 9, 0, 0x34, 0x12, 14, 0x01, 0x04, 0x01, 0x00, 0x01, 0x01,
                                             3, 0x00, 0x00 });   // claims 3 in-clusters, has 1
    EXPECT_TRUE(m_rsp.empty());
}

/* --------------------------------------------------------------- expiry */
TEST_F(RxZdoHandler, ExpiredEntryIsDroppedSoAWrappedTsnCannotMatchIt)
{
    rx_zdo_register_zdo_callback(0x1234, IEEE_ADDRESS_REQUEST, 7, cb);
    m_now += 5001;   // response window is 5 s
    rx(0x1234, IEEE_ADDRESS_RESPONSE, ieee_rsp(7, 0x1234));
    EXPECT_TRUE(m_rsp.empty());
}

TEST_F(RxZdoHandler, ResponseJustInsideTheWindowIsDelivered)
{
    rx_zdo_register_zdo_callback(0x1234, IEEE_ADDRESS_REQUEST, 7, cb);
    m_now += 5000;
    rx(0x1234, IEEE_ADDRESS_RESPONSE, ieee_rsp(7, 0x1234));
    EXPECT_EQ(1u, m_rsp.size());
}

TEST_F(RxZdoHandler, ExpiryWorksAcrossUptimeWrap)
{
    m_now = 0xFFFFF000u;
    rx_zdo_register_zdo_callback(0x1234, IEEE_ADDRESS_REQUEST, 7, cb);
    m_now = 0x00000100u;   // ~4.4 s later, after the wrap
    rx(0x1234, IEEE_ADDRESS_RESPONSE, ieee_rsp(7, 0x1234));
    EXPECT_EQ(1u, m_rsp.size());
}

/* --------------------------------------------------------------- incoming commands */
TEST_F(RxZdoHandler, DeviceAnnounceFromAnySenderEveryTime)
{
    ASSERT_EQ(SL_STATUS_OK, rx_zdo_register_zdo_cmd_callback(END_DEVICE_ANNOUNCE, cb));
    std::vector<uint8_t> ann = { 0x55, 0x78, 0x56, 1, 2, 3, 4, 5, 6, 7, 8, 0x8E };   // [tsn][addr][ieee][cap]
    rx(0x5678, END_DEVICE_ANNOUNCE, ann);
    m_now += 60000;   // command callbacks never expire
    rx(0x9ABC, END_DEVICE_ANNOUNCE, ann);

    ASSERT_EQ(2u, m_rsp.size());
    EXPECT_EQ(SL_ZIGBEE_ZDP_SUCCESS, m_rsp[0].status);
    EXPECT_EQ(0x5678, m_rsp[0].rsp.u.dev_announce.addr16);
    EXPECT_EQ(8, m_rsp[0].rsp.u.dev_announce.ieee_addr[7]);
    EXPECT_EQ(0x8E, m_rsp[0].rsp.u.dev_announce.capability);
    EXPECT_EQ(0x9ABC, m_rsp[1].sender);
}

TEST_F(RxZdoHandler, PermitJoinRequest)
{
    rx_zdo_register_zdo_cmd_callback(PERMIT_JOINING_REQUEST, cb);
    rx(0x0000, PERMIT_JOINING_REQUEST, { 0x10, 180, 1 });   // [tsn][duration][tc significance]
    ASSERT_EQ(1u, m_rsp.size());
    EXPECT_EQ(180, m_rsp[0].rsp.u.permit_join_req.duration_sec);
    EXPECT_EQ(1, m_rsp[0].rsp.u.permit_join_req.tc_significance);
}

TEST_F(RxZdoHandler, EveryCommandCallbackForTheClusterIsCalled)
{
    rx_zdo_register_zdo_cmd_callback(END_DEVICE_ANNOUNCE, cb);
    rx_zdo_register_zdo_cmd_callback(END_DEVICE_ANNOUNCE, cb2);
    rx(0x5678, END_DEVICE_ANNOUNCE, { 0x55, 0x78, 0x56, 1, 2, 3, 4, 5, 6, 7, 8, 0x8E });
    EXPECT_EQ(1u, m_rsp.size());
    EXPECT_EQ(1u, m_rsp2.size());
}

TEST_F(RxZdoHandler, CommandCallbacksAreNeverEvicted)
{
    rx_zdo_register_zdo_cmd_callback(END_DEVICE_ANNOUNCE, cb);
    for(uint8_t tsn = 0; tsn < 30; tsn++)
    {
        EXPECT_EQ(SL_STATUS_OK, rx_zdo_register_zdo_callback(0x1000 + tsn, IEEE_ADDRESS_REQUEST, tsn, cb2));
    }
    rx(0x5678, END_DEVICE_ANNOUNCE, { 0x55, 0x78, 0x56, 1, 2, 3, 4, 5, 6, 7, 8, 0x8E });
    EXPECT_EQ(1u, m_rsp.size());
}

TEST_F(RxZdoHandler, RegisteringTheSameCommandCallbackTwiceUsesOneEntry)
{
    for(int i = 0; i < 20; i++)
    {
        EXPECT_EQ(SL_STATUS_OK, rx_zdo_register_zdo_cmd_callback(END_DEVICE_ANNOUNCE, cb));
    }
    rx(0x5678, END_DEVICE_ANNOUNCE, { 0x55, 0x78, 0x56, 1, 2, 3, 4, 5, 6, 7, 8, 0x8E });
    EXPECT_EQ(1u, m_rsp.size());
}

TEST_F(RxZdoHandler, TableFullOfCommandCallbacks)
{
    static const uint16_t clusters[10] = { 0x0013, 0x0036, 0x0010, 0x0011, 0x0012, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018 };
    for(uint16_t c : clusters)
    {
        EXPECT_EQ(SL_STATUS_OK, rx_zdo_register_zdo_cmd_callback(c, cb));
    }
    EXPECT_EQ(SL_STATUS_FULL, rx_zdo_register_zdo_callback(0x1234, IEEE_ADDRESS_REQUEST, 1, cb2));
}

TEST_F(RxZdoHandler, OldestResponseEntryIsReusedWhenFull)
{
    for(uint8_t tsn = 0; tsn < 10; tsn++)
    {
        rx_zdo_register_zdo_callback(0x1000 + tsn, IEEE_ADDRESS_REQUEST, tsn, cb);
        m_now += 10;
    }
    rx_zdo_register_zdo_callback(0x2000, IEEE_ADDRESS_REQUEST, 99, cb);   // replaces tsn 0

    rx(0x1000, IEEE_ADDRESS_RESPONSE, ieee_rsp(0, 0x1000));
    rx(0x1001, IEEE_ADDRESS_RESPONSE, ieee_rsp(1, 0x1001));
    rx(0x2000, IEEE_ADDRESS_RESPONSE, ieee_rsp(99, 0x2000));
    ASSERT_EQ(2u, m_rsp.size());
    EXPECT_EQ(0x1001, m_rsp[0].sender);
    EXPECT_EQ(0x2000, m_rsp[1].sender);
}

TEST_F(RxZdoHandler, NullCallbackIsAcceptedAndNotStored)
{
    EXPECT_EQ(SL_STATUS_OK, rx_zdo_register_zdo_callback(0x1234, IEEE_ADDRESS_REQUEST, 7, NULL));
}

/* --------------------------------------------------------------- filtering */
TEST_F(RxZdoHandler, IgnoresOwnMessagesOtherProfilesAndShortFrames)
{
    rx_zdo_register_zdo_cmd_callback(END_DEVICE_ANNOUNCE, cb);
    std::vector<uint8_t> ann = { 0x55, 0x78, 0x56, 1, 2, 3, 4, 5, 6, 7, 8, 0x8E };
    rx(MY_NODE_ID, END_DEVICE_ANNOUNCE, ann);
    rx(0x5678, END_DEVICE_ANNOUNCE, ann, HA_PROFILE_ID);
    rx(0x5678, END_DEVICE_ANNOUNCE, { 0x55 });
    rx(0x5678, END_DEVICE_ANNOUNCE, { 0x55, 0x78, 0x56 });   // too short to be an announce
    EXPECT_TRUE(m_rsp.empty());
}
