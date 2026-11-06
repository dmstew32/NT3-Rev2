#include <gtest/gtest.h>
#include <vector>
#include "fff.h"

/*
 * dh_debug.c: the debug unicast from the daylight harvesting master.
 */

extern "C" {
#include "nt3_test_stubs.h"
#include "dh_debug.h"
}

DEFINE_FFF_GLOBALS

extern "C" {
FAKE_VALUE_FUNC(uint32_t, get_time_ms);
FAKE_VOID_FUNC(sl_zigbee_af_get_eui64, uint8_t *);
FAKE_VALUE_FUNC(uint8_t, sl_zigbee_af_next_sequence);
FAKE_VALUE_FUNC(sl_status_t, sl_zigbee_af_send_unicast, sl_zigbee_outgoing_message_type_t, uint16_t,
                sl_zigbee_aps_frame_t *, uint16_t, uint8_t *);
FAKE_VOID_FUNC_VARARG(sl_zigbee_app_debug_println, const char *, ...);
FAKE_VOID_FUNC_VARARG(sl_zigbee_app_debug_print, const char *, ...);
}

static std::vector<uint8_t> m_bytes;
static sl_zigbee_aps_frame_t m_aps;
static uint16_t m_to;

static void eui(uint8_t *e)
{
    const uint8_t bytes[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
    memcpy(e, bytes, sizeof(bytes));
}

static sl_status_t unicast(sl_zigbee_outgoing_message_type_t, uint16_t to, sl_zigbee_aps_frame_t *aps,
                           uint16_t length, uint8_t *message)
{
    m_to = to;
    m_aps = *aps;
    m_bytes.assign(message, message + length);
    return SL_STATUS_OK;
}

class DhDebug : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RESET_FAKE(get_time_ms); RESET_FAKE(sl_zigbee_af_get_eui64); RESET_FAKE(sl_zigbee_af_next_sequence);
        RESET_FAKE(sl_zigbee_af_send_unicast); RESET_FAKE(sl_zigbee_app_debug_println); RESET_FAKE(sl_zigbee_app_debug_print);
        FFF_RESET_HISTORY();
        sl_zigbee_af_get_eui64_fake.custom_fake = eui;
        sl_zigbee_af_send_unicast_fake.custom_fake = unicast;
        get_time_ms_fake.return_val = 70000000UL;     // 70000 s = 0x00011170
        sl_zigbee_af_next_sequence_fake.return_val = 0x5A;
        m_bytes.clear();
        dh_debug_set_target(DH_DEBUG_TARGET_NONE);
    }

    static bool send(void) { return dh_debug_send(0x0123, 500, 97, DH_DEBUG_MODE_MASTER, 112); }

    static void rx(std::vector<uint8_t> payload)
    {
        std::vector<uint8_t> bytes(5, 0);
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        sl_zigbee_af_cluster_command_t cmd = {};
        cmd.buffer = bytes.data();
        cmd.bufLen = (uint16_t)bytes.size();
        cmd.payloadStartIndex = 5;
        EXPECT_TRUE(dh_debug_rx(&cmd));
    }
};

/* --------------------------------------------------------------- the address */
TEST_F(DhDebug, NoTargetByDefaultNothingIsSent)
{
    EXPECT_EQ(0xFFFE, dh_debug_get_target());
    EXPECT_FALSE(send());
    EXPECT_EQ(0u, sl_zigbee_af_send_unicast_fake.call_count);
}

TEST_F(DhDebug, TheBroadcastAddressesAreNotSentTo)
{
    for(uint16_t address : { 0xFFFCu, 0xFFFDu, 0xFFFEu, 0xFFFFu })
    {
        dh_debug_set_target(address);
        EXPECT_FALSE(send());
    }
    EXPECT_EQ(0u, sl_zigbee_af_send_unicast_fake.call_count);
}

TEST_F(DhDebug, AnyAddressBelowThoseIsAUnicastToThatAddress)
{
    for(uint16_t address : { 0x0000u, 0x1234u, 0xFFFBu })
    {
        dh_debug_set_target(address);
        EXPECT_TRUE(send());
        EXPECT_EQ(address, m_to);
    }
}

/* --------------------------------------------------------------- bytes on the air */
TEST_F(DhDebug, TheMessageBytes)
{
    dh_debug_set_target(0x1234);
    ASSERT_TRUE(send());

    EXPECT_EQ(ZCL_ADAPTIV_DEBUG_CLUSTER_ID, m_aps.clusterId);
    EXPECT_EQ(LIGHT_ENDPOINT, m_aps.sourceEndpoint);
    EXPECT_EQ(LIGHT_ENDPOINT, m_aps.destinationEndpoint);

    const std::vector<uint8_t> expected = {
        (ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER |
         ZCL_MANUFACTURER_SPECIFIC_MASK | ZCL_DISABLE_DEFAULT_RESPONSE_MASK),
        0x74, 0x15, 0x5A, 0xDB,            // manufacturer 0x1574, sequence, command
        0x11, 0x22, 0x33, 0x44,            // EUI64, lower 4 bytes
        0x70, 0x11, 0x01, 0x00,            // uptime 70000 s
        0x23, 0x01,                        // lux 0x0123
        0xF4, 0x01,                        // target 500
        97,                                // level
        0x01,                              // master
        112,                               // new level
    };
    EXPECT_EQ(expected, m_bytes);
}

TEST_F(DhDebug, ARefusedSendIsReported)
{
    dh_debug_set_target(0x1234);
    sl_zigbee_af_send_unicast_fake.custom_fake = nullptr;
    sl_zigbee_af_send_unicast_fake.return_val = (sl_status_t)0x0C;
    EXPECT_FALSE(send());
}

/* --------------------------------------------------------------- receiving */
TEST_F(DhDebug, TheTargetIsSetByTheShortCommand)
{
    rx({ 0xAA, 0x34, 0x12 });
    EXPECT_EQ(0x1234, dh_debug_get_target());
    rx({ 0xAA, 0xFE, 0xFF });
    EXPECT_EQ(0xFFFE, dh_debug_get_target());
}

TEST_F(DhDebug, ADataMessageNeverChangesTheTargetEvenIfItStartsLikeTheCommand)
{
    dh_debug_set_target(0x1234);
    rx({ 0xAA, 0x22, 0x33, 0x44, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
    EXPECT_EQ(0x1234, dh_debug_get_target());
    EXPECT_EQ(1u, sl_zigbee_app_debug_print_fake.call_count);     // printed instead
}

TEST_F(DhDebug, OtherPayloadsAreIgnored)
{
    dh_debug_set_target(0x1234);
    rx({});
    rx({ 0xAA });
    rx({ 0xAB, 0x00, 0x00 });
    rx({ 0xAA, 0x00, 0x00, 0x00 });
    EXPECT_EQ(0x1234, dh_debug_get_target());
}
