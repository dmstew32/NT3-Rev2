#include <gtest/gtest.h>

extern "C" {
#include "nt3_test_stubs.h"
#include "network_defines.h"
#include "zcl_header.h"
}

// frame control bits
#define FC_CLUSTER_SPECIFIC  0x01
#define FC_MFG_SPECIFIC      0x04
#define FC_TO_CLIENT         0x08
#define FC_DISABLE_DEF_RSP   0x10

TEST(ZclHeader, ParsesProfileWideHeader)
{
    uint8_t msg[] = { 0x00, 0x42, 0x01, 0xAA };   // fc, tsn, cmd, payload
    zcl_header_t h = {};

    EXPECT_EQ(3, zcl_header_from_data(msg, sizeof(msg), &h));
    EXPECT_FALSE(h.cluster_specific);
    EXPECT_FALSE(h.mfg_specific);
    EXPECT_TRUE(h.direction_to_server);
    EXPECT_FALSE(h.disable_def_rsp);
    EXPECT_EQ(INVALID_MFG_CODE, h.mfg_code);
    EXPECT_EQ(0x42, h.sequence_number);
    EXPECT_EQ(0x01, h.cmd_id);
}

TEST(ZclHeader, ParsesManufacturerSpecificHeader)
{
    uint8_t msg[] = { FC_CLUSTER_SPECIFIC | FC_MFG_SPECIFIC | FC_TO_CLIENT | FC_DISABLE_DEF_RSP,
                      0x74, 0x15, 0x07, 0x10 };   // mfg code 0x1574 (little endian)
    zcl_header_t h = {};

    EXPECT_EQ(5, zcl_header_from_data(msg, sizeof(msg), &h));
    EXPECT_TRUE(h.cluster_specific);
    EXPECT_TRUE(h.mfg_specific);
    EXPECT_FALSE(h.direction_to_server);
    EXPECT_TRUE(h.disable_def_rsp);
    EXPECT_EQ(0x1574, h.mfg_code);
    EXPECT_EQ(0x07, h.sequence_number);
    EXPECT_EQ(0x10, h.cmd_id);
}

TEST(ZclHeader, BoolFieldsAreZeroOrOne)
{
    // Z3Switch parser: the flags are converted to bool, not left as masked bits
    uint8_t msg[] = { FC_DISABLE_DEF_RSP | FC_MFG_SPECIFIC, 0x34, 0x12, 0x01, 0x02 };
    zcl_header_t h = {};

    zcl_header_from_data(msg, sizeof(msg), &h);
    EXPECT_EQ(1, (int)h.disable_def_rsp);
    EXPECT_EQ(1, (int)h.mfg_specific);
}

TEST(ZclHeader, TooShortReturnsZero)
{
    uint8_t msg[] = { 0x00, 0x01 };
    zcl_header_t h = {};
    EXPECT_EQ(0, zcl_header_from_data(msg, sizeof(msg), &h));
}

TEST(ZclHeader, TruncatedManufacturerHeaderReturnsZero)
{
    // mfg specific needs 5 bytes; only 4 present
    uint8_t msg[] = { FC_MFG_SPECIFIC, 0x74, 0x15, 0x07 };
    zcl_header_t h = {};
    EXPECT_EQ(0, zcl_header_from_data(msg, sizeof(msg), &h));
}

TEST(ZclHeader, EncodesHeaderAndIncrementsSequenceNumber)
{
    zcl_header_t h = {};
    h.cluster_specific = true;
    h.direction_to_server = false;
    h.disable_def_rsp = true;
    h.cmd_id = 0x05;

    uint8_t out1[8] = {}, out2[8] = {};
    ASSERT_EQ(3, zcl_header_to_data(out1, sizeof(out1), &h));
    ASSERT_EQ(3, zcl_header_to_data(out2, sizeof(out2), &h));

    EXPECT_EQ(FC_CLUSTER_SPECIFIC | FC_TO_CLIENT | FC_DISABLE_DEF_RSP, out1[0]);
    EXPECT_EQ(0x05, out1[2]);
    EXPECT_EQ((uint8_t)(out1[1] + 1), out2[1]);   // next sequence number
}

TEST(ZclHeader, EncodesManufacturerCode)
{
    zcl_header_t h = {};
    h.mfg_specific = true;
    h.direction_to_server = true;
    h.mfg_code = 0x1574;
    h.cmd_id = 0x10;

    uint8_t out[8] = {};
    ASSERT_EQ(5, zcl_header_to_data(out, sizeof(out), &h));
    EXPECT_EQ(FC_MFG_SPECIFIC, out[0]);
    EXPECT_EQ(0x74, out[1]);
    EXPECT_EQ(0x15, out[2]);
    EXPECT_EQ(0x10, out[4]);
}

TEST(ZclHeader, ForcedSequenceNumberIsUsedOnceAndDoesNotAdvanceCounter)
{
    zcl_header_t h = {};
    uint8_t a[8], b[8], c[8];

    zcl_header_to_data(a, sizeof(a), &h);            // counter = n, now n+1
    zcl_header_force_sequence_number(0xEE);
    zcl_header_to_data(b, sizeof(b), &h);            // forced
    zcl_header_to_data(c, sizeof(c), &h);            // counter resumes

    EXPECT_EQ(0xEE, b[1]);
    EXPECT_EQ((uint8_t)(a[1] + 1), c[1]);
}

TEST(ZclHeader, EncodeIntoTooSmallBufferReturnsZero)
{
    zcl_header_t h = {};
    uint8_t out[2];
    EXPECT_EQ(0, zcl_header_to_data(out, sizeof(out), &h));
}

TEST(ZclHeader, RoundTrip)
{
    zcl_header_t in = {};
    in.cluster_specific = true;
    in.mfg_specific = true;
    in.direction_to_server = true;
    in.mfg_code = 0x1234;
    in.cmd_id = 0x7F;

    uint8_t buf[8];
    uint8_t len = zcl_header_to_data(buf, sizeof(buf), &in);
    zcl_header_t out = {};
    ASSERT_EQ(len, zcl_header_from_data(buf, len, &out));
    EXPECT_EQ(in.cluster_specific, out.cluster_specific);
    EXPECT_EQ(in.mfg_specific, out.mfg_specific);
    EXPECT_EQ(in.direction_to_server, out.direction_to_server);
    EXPECT_EQ(in.mfg_code, out.mfg_code);
    EXPECT_EQ(in.cmd_id, out.cmd_id);
}
