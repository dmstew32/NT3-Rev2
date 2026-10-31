#include <gtest/gtest.h>
#include <vector>
#include "fff.h"

extern "C" {
#include "nt3_test_stubs.h"
#include "adaptiv_zcl.h"
#include "network_defines.h"
#include "rx_zcl_handler.h"
#include "zap-id.h"
#include "zap-type.h"
}

DEFINE_FFF_GLOBALS

extern "C" {
FAKE_VOID_FUNC_VARARG(sl_zigbee_af_core_println, const char *, ...);
FAKE_VOID_FUNC_VARARG(app_logger, const char *, ...);
FAKE_VALUE_FUNC(uint8_t, sl_zigbee_af_get_data_size, uint8_t);
}

static uint8_t data_size(uint8_t type)
{
    switch(type)
    {
        case ZCL_BOOLEAN_ATTRIBUTE_TYPE:
        case ZCL_INT8U_ATTRIBUTE_TYPE:  return 1;
        case ZCL_INT16U_ATTRIBUTE_TYPE: return 2;
        case ZCL_INT32U_ATTRIBUTE_TYPE: return 4;
        default:                        return 0;
    }
}

/* --------------------------------------------------------------- recorders */
struct ReadRsp
{
    uint16_t sender; uint8_t ep; sl_status_t status; uint16_t cluster; uint16_t mfg;
    std::vector<read_attribute_response_record_t> records;
    std::vector<std::string> strings;
};
static std::vector<ReadRsp> m_read;
static void read_cb(uint16_t sender, uint8_t ep, sl_status_t status, uint16_t cluster, uint16_t mfg,
                    read_attribute_response_record_t *p, uint8_t n)
{
    ReadRsp r{ sender, ep, status, cluster, mfg, {}, {} };
    for(uint8_t i = 0; i < n; i++)
    {
        r.records.push_back(p[i]);
        bool is_str = (p[i].attribute_type == ZCL_CHAR_STRING_ATTRIBUTE_TYPE) ||
                      (p[i].attribute_type == ZCL_OCTET_STRING_ATTRIBUTE_TYPE);
        r.strings.push_back(is_str && p[i].u.s.p_str ? std::string((char *)p[i].u.s.p_str, p[i].u.s.str_length) : "");
    }
    m_read.push_back(r);
}
static int m_read2_calls;
static void read_cb2(uint16_t, uint8_t, sl_status_t, uint16_t, uint16_t, read_attribute_response_record_t *, uint8_t)
{ m_read2_calls++; }

struct WriteRsp { uint16_t sender; uint8_t ep; sl_zigbee_af_status_t status; };
static std::vector<WriteRsp> m_write;
static void write_cb(uint16_t sender, uint8_t ep, sl_zigbee_af_status_t status) { m_write.push_back({ sender, ep, status }); }

struct CfgRsp { uint16_t sender; uint8_t ep; sl_status_t status; uint16_t cluster; };
static std::vector<CfgRsp> m_cfg;
static void cfg_cb(uint16_t sender, uint8_t ep, sl_status_t status, uint16_t cluster) { m_cfg.push_back({ sender, ep, status, cluster }); }

struct OccRpt { uint16_t sender; uint8_t ep; bool occupied; };
static std::vector<OccRpt> m_occ;
static void occ_cb(uint16_t sender, uint8_t ep, bool occupied) { m_occ.push_back({ sender, ep, occupied }); }

struct CmdRsp { uint16_t sender; uint8_t ep; std::vector<uint8_t> payload; };
static std::vector<CmdRsp> m_cmd;
static void cmd_cb(uint16_t sender, uint8_t ep, uint8_t *p, uint8_t len)
{ m_cmd.push_back({ sender, ep, std::vector<uint8_t>(p, p + len) }); }

// sender info, as seen from inside a callback
static sl_status_t m_info_status;
static uint16_t m_info_sender;
static zcl_header_t m_info_header;
static void info_cb(uint16_t, uint8_t, sl_zigbee_af_status_t)
{
    sl_zigbee_aps_frame_t aps;
    m_info_status = rx_zcl_get_sender_info(&m_info_sender, &aps, &m_info_header);
}

/* --------------------------------------------------------------- fixture */
class RxZclHandler : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RESET_FAKE(sl_zigbee_af_core_println);
        RESET_FAKE(app_logger);
        RESET_FAKE(sl_zigbee_af_get_data_size);
        FFF_RESET_HISTORY();
        sl_zigbee_af_get_data_size_fake.custom_fake = data_size;
        m_read.clear(); m_write.clear(); m_cfg.clear(); m_occ.clear(); m_cmd.clear();
        m_read2_calls = 0;
        rx_zcl_handler_on_startup();
    }

    static sl_zigbee_aps_frame_t aps(uint16_t cluster, uint8_t src_ep = 5, uint8_t dst_ep = 1,
                                     uint16_t profile = HA_PROFILE_ID)
    {
        sl_zigbee_aps_frame_t a = {};
        a.profileId = profile;
        a.clusterId = cluster;
        a.sourceEndpoint = src_ep;
        a.destinationEndpoint = dst_ep;
        return a;
    }

    static void rx(uint16_t sender, sl_zigbee_aps_frame_t a, std::vector<uint8_t> msg)
    {
        rx_zcl_handler_on_rx(sender, &a, msg.data(), (uint8_t)msg.size());
    }
};

/* --------------------------------------------------------------- filtering */
TEST_F(RxZclHandler, IgnoresNonHaProfileEndpointZeroAndShortFrames)
{
    rx_zcl_handler_register_write_attribute_response_cb(0x0006, 0, write_cb);
    rx(0x1111, aps(0x0006, 5, 1, 0x0000), { 0x18, 0x01, ZCL_WRITE_ATTRIBUTES_RESPONSE_COMMAND_ID, 0x00 });
    rx(0x1111, aps(0x0006, 5, 0),         { 0x18, 0x01, ZCL_WRITE_ATTRIBUTES_RESPONSE_COMMAND_ID, 0x00 });
    rx(0x1111, aps(0x0006),               { 0x18, 0x01 });
    EXPECT_TRUE(m_write.empty());
}

/* --------------------------------------------------------------- read attributes response */
TEST_F(RxZclHandler, ReadAttributesResponseParsesIntegerAndStringRecords)
{
    rx_zcl_handler_register_read_attribute_response_cb(0x0000, 0, read_cb);
    rx(0x2222, aps(0x0000, 7), {
        0x18, 0x10, ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID,
        0x01, 0x00, 0x00, ZCL_INT8U_ATTRIBUTE_TYPE, 0x2A,                         // u8 = 42
        0x02, 0x00, 0x00, ZCL_INT16U_ATTRIBUTE_TYPE, 0x34, 0x12,                  // u16 = 0x1234
        0x04, 0x00, 0x00, ZCL_CHAR_STRING_ATTRIBUTE_TYPE, 0x03, 'A', 'd', 'a',    // "Ada"
        0x06, 0x00, 0x86,                                                         // unsupported attribute
    });

    ASSERT_EQ(1u, m_read.size());
    const ReadRsp &r = m_read[0];
    EXPECT_EQ(0x2222, r.sender);
    EXPECT_EQ(7, r.ep);
    EXPECT_EQ(SL_STATUS_OK, r.status);
    EXPECT_EQ(0x0000, r.cluster);
    ASSERT_EQ(4u, r.records.size());
    EXPECT_EQ(0x0001, r.records[0].attribute_id);
    EXPECT_EQ(42, r.records[0].u.data8);
    EXPECT_EQ(0x1234, r.records[1].u.data16);
    EXPECT_EQ("Ada", r.strings[2]);
    EXPECT_EQ(0x0006, r.records[3].attribute_id);
    EXPECT_EQ(0x86u, r.records[3].status & 0xFF);
    EXPECT_EQ(0, r.records[3].data_length);
}

TEST_F(RxZclHandler, ReadAttributesResponseParsesU32)
{
    rx_zcl_handler_register_read_attribute_response_cb(0x0019, 0, read_cb);
    rx(0x2222, aps(0x0019), { 0x18, 0x10, ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID,
                              0x02, 0x00, 0x00, ZCL_INT32U_ATTRIBUTE_TYPE, 0x04, 0x01, 0x09, 0x00 });
    ASSERT_EQ(1u, m_read.size());
    EXPECT_EQ(0x00090104u, m_read[0].records[0].u.data32);
}

TEST_F(RxZclHandler, StringLongerThanFrameIsClipped)
{
    rx_zcl_handler_register_read_attribute_response_cb(0x0000, 0, read_cb);
    // string claims 10 characters, only 3 present
    rx(0x2222, aps(0x0000), { 0x18, 0x10, ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID,
                              0x05, 0x00, 0x00, ZCL_CHAR_STRING_ATTRIBUTE_TYPE, 0x0A, 'a', 'b', 'c' });
    ASSERT_EQ(1u, m_read.size());
    EXPECT_EQ("abc", m_read[0].strings[0]);
}

TEST_F(RxZclHandler, ReadResponseOnlyGoesToMatchingClusterAndManufacturer)
{
    rx_zcl_handler_register_read_attribute_response_cb(0x0006, 0, read_cb);                 // standard
    rx_zcl_handler_register_read_attribute_response_cb(0x0006, ADAPTIV_MFG_ID, read_cb2);   // mfg specific

    // standard response on another cluster: nobody
    rx(0x1, aps(0x0008), { 0x18, 0x01, ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID, 0x00, 0x00, 0x00, ZCL_INT8U_ATTRIBUTE_TYPE, 1 });
    EXPECT_TRUE(m_read.empty());
    EXPECT_EQ(0, m_read2_calls);

    // standard response on 0x0006: only the standard callback
    rx(0x1, aps(0x0006), { 0x18, 0x01, ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID, 0x00, 0x00, 0x00, ZCL_INT8U_ATTRIBUTE_TYPE, 1 });
    EXPECT_EQ(1u, m_read.size());
    EXPECT_EQ(0, m_read2_calls);

    // manufacturer specific response (0x1574): only the mfg callback
    rx(0x1, aps(0x0006), { 0x1C, 0x74, 0x15, 0x01, ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID,
                           0x00, 0x00, 0x00, ZCL_INT8U_ATTRIBUTE_TYPE, 1 });
    EXPECT_EQ(1u, m_read.size());
    EXPECT_EQ(1, m_read2_calls);
}

TEST_F(RxZclHandler, AnyClusterCallbackSeesEveryCluster)
{
    rx_zcl_handler_register_read_attribute_response_cb(RX_ZCL_ANY_CLUSTER_ID, 0, read_cb);
    rx(0x1, aps(0x0006), { 0x18, 0x01, ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID, 0x00, 0x00, 0x00, ZCL_INT8U_ATTRIBUTE_TYPE, 1 });
    rx(0x1, aps(0x0300), { 0x18, 0x01, ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID, 0x00, 0x00, 0x00, ZCL_INT8U_ATTRIBUTE_TYPE, 1 });
    ASSERT_EQ(2u, m_read.size());
    EXPECT_EQ(0x0300, m_read[1].cluster);
}

/* --------------------------------------------------------------- other global responses */
TEST_F(RxZclHandler, WriteAttributesResponsePassesStatus)
{
    rx_zcl_handler_register_write_attribute_response_cb(0x0006, 0, write_cb);
    rx(0x3333, aps(0x0006, 2), { 0x18, 0x05, ZCL_WRITE_ATTRIBUTES_RESPONSE_COMMAND_ID, 0x87 });
    ASSERT_EQ(1u, m_write.size());
    EXPECT_EQ(0x3333, m_write[0].sender);
    EXPECT_EQ(2, m_write[0].ep);
    EXPECT_EQ(0x87, m_write[0].status);
}

TEST_F(RxZclHandler, WriteAttributesResponseWithoutPayloadIsIgnored)
{
    rx_zcl_handler_register_write_attribute_response_cb(0x0006, 0, write_cb);
    rx(0x3333, aps(0x0006), { 0x18, 0x05, ZCL_WRITE_ATTRIBUTES_RESPONSE_COMMAND_ID });
    EXPECT_TRUE(m_write.empty());
}

TEST_F(RxZclHandler, ConfigureReportingResponse)
{
    rx_zcl_handler_register_cfg_report_cmd_response_cb(0x0406, cfg_cb);
    rx(0x4444, aps(0x0406, 3), { 0x18, 0x09, ZCL_CONFIGURE_REPORTING_RESPONSE_COMMAND_ID, 0x00 });
    ASSERT_EQ(1u, m_cfg.size());
    EXPECT_EQ(0x4444, m_cfg[0].sender);
    EXPECT_EQ(SL_STATUS_OK, m_cfg[0].status);
    EXPECT_EQ(0x0406, m_cfg[0].cluster);
}

TEST_F(RxZclHandler, OccupancyReport)
{
    rx_zcl_handler_register_report_occupancy_cb(occ_cb);
    rx(0x5555, aps(ZCL_OCCUPANCY_SENSING_CLUSTER_ID, 4),
       { 0x18, 0x01, ZCL_REPORT_ATTRIBUTES_COMMAND_ID, 0x00, 0x00, 0x18, 0x01 });
    rx(0x5555, aps(ZCL_OCCUPANCY_SENSING_CLUSTER_ID, 4),
       { 0x18, 0x02, ZCL_REPORT_ATTRIBUTES_COMMAND_ID, 0x00, 0x00, 0x18, 0x00 });
    ASSERT_EQ(2u, m_occ.size());
    EXPECT_TRUE(m_occ[0].occupied);
    EXPECT_FALSE(m_occ[1].occupied);
    EXPECT_EQ(4, m_occ[0].ep);
}

TEST_F(RxZclHandler, OtherReportsDoNotCallOccupancyCallback)
{
    rx_zcl_handler_register_report_occupancy_cb(occ_cb);
    rx(0x5555, aps(0x0400), { 0x18, 0x01, ZCL_REPORT_ATTRIBUTES_COMMAND_ID, 0x00, 0x00, 0x21, 0x10, 0x00 });   // illuminance
    rx(0x5555, aps(ZCL_OCCUPANCY_SENSING_CLUSTER_ID), { 0x18, 0x01, ZCL_REPORT_ATTRIBUTES_COMMAND_ID, 0x10, 0x00, 0x21, 0x10 });
    EXPECT_TRUE(m_occ.empty());
}

/* --------------------------------------------------------------- cluster specific */
TEST_F(RxZclHandler, AutoCommissioningCommandReachesCallbackWithPayload)
{
    rx_zcl_handler_register_cluster_cmd_response_cb(ZCL_MFG_AUTOCOMMISSIONING_CLUSTER_ID, ADAPTIV_MFG_ID,
                                                    ZCL_MFG_AUTOCOMMISSION_ANNOUNCE_COMMAND_ID, cmd_cb);
    rx(0x6666, aps(ZCL_MFG_AUTOCOMMISSIONING_CLUSTER_ID, 9),
       { 0x05, 0x74, 0x15, 0x22, ZCL_MFG_AUTOCOMMISSION_ANNOUNCE_COMMAND_ID, 0xAB, 0xCD });
    ASSERT_EQ(1u, m_cmd.size());
    EXPECT_EQ(0x6666, m_cmd[0].sender);
    EXPECT_EQ(9, m_cmd[0].ep);
    EXPECT_EQ((std::vector<uint8_t>{ 0xAB, 0xCD }), m_cmd[0].payload);
}

TEST_F(RxZclHandler, ClusterCommandNeedsMatchingCommandAndManufacturer)
{
    rx_zcl_handler_register_cluster_cmd_response_cb(ZCL_MFG_AUTOCOMMISSIONING_CLUSTER_ID, ADAPTIV_MFG_ID,
                                                    ZCL_MFG_AUTOCOMMISSION_ANNOUNCE_COMMAND_ID, cmd_cb);
    // other command
    rx(0x6666, aps(ZCL_MFG_AUTOCOMMISSIONING_CLUSTER_ID), { 0x05, 0x74, 0x15, 0x22, 0x02, 0xAB });
    // other manufacturer
    rx(0x6666, aps(ZCL_MFG_AUTOCOMMISSIONING_CLUSTER_ID), { 0x05, 0x34, 0x12, 0x22, ZCL_MFG_AUTOCOMMISSION_ANNOUNCE_COMMAND_ID });
    // global (not cluster specific) command with the same id
    rx(0x6666, aps(ZCL_MFG_AUTOCOMMISSIONING_CLUSTER_ID), { 0x04, 0x74, 0x15, 0x22, ZCL_MFG_AUTOCOMMISSION_ANNOUNCE_COMMAND_ID });
    EXPECT_TRUE(m_cmd.empty());
}

TEST_F(RxZclHandler, OnlyClustersOfInterestAreDispatched)
{
    // registered on On/Off: not one of the clusters this handler passes on
    rx_zcl_handler_register_cluster_cmd_response_cb(ZCL_ON_OFF_CLUSTER_ID, INVALID_MFG_CODE, 0x01, cmd_cb);
    rx(0x7777, aps(ZCL_ON_OFF_CLUSTER_ID), { 0x01, 0x22, 0x01 });
    EXPECT_TRUE(m_cmd.empty());
}

TEST_F(RxZclHandler, GroupsOnlyAddGroupResponseFromServer)
{
    rx_zcl_handler_register_cluster_cmd_response_cb(ZCL_GROUPS_CLUSTER_ID, INVALID_MFG_CODE,
                                                    ZCL_ADD_GROUP_RESPONSE_COMMAND_ID, cmd_cb);
    // add group REQUEST (to server, same command id 0x00): ignored
    rx(0x8888, aps(ZCL_GROUPS_CLUSTER_ID), { 0x01, 0x22, ZCL_ADD_GROUP_RESPONSE_COMMAND_ID, 0x34, 0x12, 0x00 });
    EXPECT_TRUE(m_cmd.empty());
    // add group RESPONSE (to client)
    rx(0x8888, aps(ZCL_GROUPS_CLUSTER_ID), { 0x09, 0x22, ZCL_ADD_GROUP_RESPONSE_COMMAND_ID, 0x00, 0x34, 0x12 });
    ASSERT_EQ(1u, m_cmd.size());
    EXPECT_EQ((std::vector<uint8_t>{ 0x00, 0x34, 0x12 }), m_cmd[0].payload);
}

/* --------------------------------------------------------------- sender info */
TEST_F(RxZclHandler, SenderInfoIsAvailableOnlyWhileHandlingAMessage)
{
    rx_zcl_handler_register_write_attribute_response_cb(0x0006, 0, info_cb);
    rx(0x9999, aps(0x0006, 4), { 0x18, 0x5A, ZCL_WRITE_ATTRIBUTES_RESPONSE_COMMAND_ID, 0x00 });

    EXPECT_EQ(SL_STATUS_OK, m_info_status);
    EXPECT_EQ(0x9999, m_info_sender);
    EXPECT_EQ(0x5A, m_info_header.sequence_number);
    EXPECT_EQ(ZCL_WRITE_ATTRIBUTES_RESPONSE_COMMAND_ID, m_info_header.cmd_id);

    uint16_t sender; sl_zigbee_aps_frame_t a; zcl_header_t h;
    EXPECT_EQ(SL_STATUS_NOT_FOUND, rx_zcl_get_sender_info(&sender, &a, &h));
}

/* --------------------------------------------------------------- registration */
TEST_F(RxZclHandler, TableHoldsFifteenCallbacks)
{
    for(uint16_t cid = 0; cid < 15; cid++)
    {
        EXPECT_EQ(SL_STATUS_OK, rx_zcl_handler_register_read_attribute_response_cb(cid + 1, 0, read_cb));
    }
    EXPECT_EQ(SL_STATUS_FULL, rx_zcl_handler_register_read_attribute_response_cb(0x0100, 0, read_cb));
}

TEST_F(RxZclHandler, SameReadRegistrationDoesNotUseAnotherSlot)
{
    for(int i = 0; i < 20; i++)
    {
        EXPECT_EQ(SL_STATUS_OK, rx_zcl_handler_register_read_attribute_response_cb(0x0006, 0, read_cb));
    }
    rx(0x1, aps(0x0006), { 0x18, 0x01, ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID, 0x00, 0x00, 0x00, ZCL_INT8U_ATTRIBUTE_TYPE, 1 });
    EXPECT_EQ(1u, m_read.size());   // one entry, one call
}
