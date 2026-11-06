// OTA download timing: Light/nwk_monitor/ota_diag.c
#include <gtest/gtest.h>
#include <cstdint>

extern "C" {
#include "ota_diag.h"
}

namespace {

enum Offset
{
  OFF_VERSION = 0, OFF_STATE = 1, OFF_TRAFFIC = 2, OFF_LENGTH = 3, OFF_OFFSET = 4,
  OFF_ELAPSED = 8, OFF_BLOCKS = 10, OFF_SLOW_CYCLES = 12, OFF_TIMEOUTS = 14,
  OFF_DROPPED = 16, OFF_WAIT = 18, OFF_REFUSED = 20, OFF_NOT_DELIVERED = 22,
  OFF_MIN_PERIOD = 24, OFF_SLOW = 26, OFF_AVG = 38, OFF_LONGEST = 50,
  OFF_STALLS = 52, OFF_GAP = 54, OFF_COUNTERS = 56,
};
enum Cycle { TOTAL = 0, ACK = 2, RX = 4, WRITE = 6, RX_TO_TX = 8, LATE = 10 };

class OtaDiag : public ::testing::Test
{
protected:
  void SetUp() override { ota_diag_download_start(1000); }

  uint16_t u16(int offset)
  {
    pack();
    return (uint16_t)(m_out[offset] | (m_out[offset + 1] << 8));
  }

  uint8_t pack(uint32_t now = 1000)
  {
    return ota_diag_pack(m_out, now, 7, 2, &m_counters);
  }

  // One normal block, starting with the request.  The next request closes it.
  void block(uint32_t t, uint32_t ack, uint32_t rx, uint32_t write, uint32_t next_tx)
  {
    ota_diag_request_sent(t, true);
    ota_diag_request_acked(t + ack, true);
    ota_diag_block_rx(t + rx, 63, 63, 0);
    ota_diag_block_written(t + rx + write);
    ota_diag_timer_set(t + rx + write, 10);
    ota_diag_timer_fired(t + rx + write + 10);
    ota_diag_request_sent(t + next_tx, true);
  }

  uint8_t m_out[OTA_DIAG_PACKET_LEN + 8] = {0};
  ota_diag_counters_t m_counters = {1, 2, 3, 4, 5, 6};
};

TEST_F(OtaDiag, PacketIsExactlyTheDocumentedLength)
{
  m_out[OTA_DIAG_PACKET_LEN] = 0xA5;
  EXPECT_EQ(OTA_DIAG_PACKET_LEN, pack());
  EXPECT_EQ(0xA5, m_out[OTA_DIAG_PACKET_LEN]);  // nothing written past the end
}

TEST_F(OtaDiag, HeaderFields)
{
  ota_diag_block_rx(1500, 0x01020304, 63, 250);
  pack(6000);
  EXPECT_EQ(OTA_DIAG_LAYOUT_VERSION, m_out[OFF_VERSION]);
  EXPECT_EQ(7, m_out[OFF_STATE]);
  EXPECT_EQ(2, m_out[OFF_TRAFFIC]);
  EXPECT_EQ(63, m_out[OFF_LENGTH]);
  EXPECT_EQ(0x04, m_out[OFF_OFFSET]);        // little endian
  EXPECT_EQ(0x01, m_out[OFF_OFFSET + 3]);
  EXPECT_EQ(5, m_out[OFF_ELAPSED]);          // 5 s since download start
  EXPECT_EQ(250, u16(OFF_MIN_PERIOD));
}

TEST_F(OtaDiag, StackCountersAreInLayoutOrder)
{
  for(int i = 0; i < 6; i++)
  {
    EXPECT_EQ(i + 1, u16(OFF_COUNTERS + (2 * i)));
  }
}

TEST_F(OtaDiag, NormalCycleIsAveragedAndNotSlow)
{
  block(2000, 20, 40, 2, 55);
  EXPECT_EQ(55, u16(OFF_AVG + TOTAL));
  EXPECT_EQ(20, u16(OFF_AVG + ACK));
  EXPECT_EQ(40, u16(OFF_AVG + RX));
  EXPECT_EQ(2, u16(OFF_AVG + WRITE));
  EXPECT_EQ(15, u16(OFF_AVG + RX_TO_TX));
  EXPECT_EQ(0, u16(OFF_SLOW_CYCLES));
  EXPECT_EQ(0, u16(OFF_SLOW + TOTAL));
}

TEST_F(OtaDiag, SlowCycleKeepsItsBreakdown)
{
  block(2000, 20, 40, 2, 55);
  // 3 s lost between the block and the next request
  block(3000, 25, 45, 3, 3050);
  EXPECT_EQ(1, u16(OFF_SLOW_CYCLES));
  EXPECT_EQ(3050, u16(OFF_SLOW + TOTAL));
  EXPECT_EQ(25, u16(OFF_SLOW + ACK));
  EXPECT_EQ(45, u16(OFF_SLOW + RX));
  EXPECT_EQ(3, u16(OFF_SLOW + WRITE));
  EXPECT_EQ(3005, u16(OFF_SLOW + RX_TO_TX));
  EXPECT_EQ(3050, u16(OFF_LONGEST));
}

TEST_F(OtaDiag, SlowResponseShowsInTxToRxNotRxToTx)
{
  block(3000, 25, 2800, 3, 2815);
  EXPECT_EQ(2800, u16(OFF_SLOW + RX));
  EXPECT_EQ(15, u16(OFF_SLOW + RX_TO_TX));
}

TEST_F(OtaDiag, LateTimerIsMeasuredFromItsDueTime)
{
  ota_diag_request_sent(2000, true);
  ota_diag_block_rx(2040, 0, 63, 0);
  ota_diag_timer_set(2042, 10);
  ota_diag_timer_fired(4052);        // due at 2052
  ota_diag_request_sent(4053, true);
  EXPECT_EQ(2000, u16(OFF_SLOW + LATE));
}

TEST_F(OtaDiag, TimerFiringEarlyIsNotLate)
{
  ota_diag_request_sent(2000, true);
  ota_diag_block_rx(2040, 0, 63, 0);
  ota_diag_timer_set(2042, 10);
  ota_diag_timer_fired(2050);
  ota_diag_request_sent(3100, true);
  EXPECT_EQ(0, u16(OFF_SLOW + LATE));
}

TEST_F(OtaDiag, MissingAckIsMarkedAndLeftOutOfTheAverage)
{
  block(2000, 20, 40, 2, 55);
  // No ack in this slow cycle
  ota_diag_request_sent(3000, true);
  ota_diag_block_rx(3040, 0, 63, 0);
  ota_diag_request_sent(4100, true);
  EXPECT_EQ(OTA_DIAG_NO_ACK, u16(OFF_SLOW + ACK));
  EXPECT_EQ(20, u16(OFF_AVG + ACK));
}

TEST_F(OtaDiag, RetryWithNoBlockIsNotACycle)
{
  ota_diag_request_sent(2000, true);
  ota_diag_response_timeout();
  ota_diag_request_sent(7000, true);   // retry, 5 s later
  ota_diag_block_rx(7040, 0, 63, 0);
  ota_diag_request_sent(7055, true);
  EXPECT_EQ(1, u16(OFF_TIMEOUTS));
  EXPECT_EQ(55, u16(OFF_LONGEST));
  EXPECT_EQ(0, u16(OFF_SLOW_CYCLES));
}

TEST_F(OtaDiag, RefusedRequestDoesNotCloseTheCycle)
{
  ota_diag_request_sent(2000, true);
  ota_diag_block_rx(2040, 0, 63, 0);
  ota_diag_request_sent(2055, false);
  ota_diag_request_sent(2500, true);
  EXPECT_EQ(1, u16(OFF_REFUSED));
  EXPECT_EQ(500, u16(OFF_AVG + TOTAL));
}

TEST_F(OtaDiag, FailureCounters)
{
  ota_diag_request_sent(2000, true);
  ota_diag_request_acked(2100, false);
  ota_diag_response_dropped();
  ota_diag_wait_for_data();
  EXPECT_EQ(1, u16(OFF_NOT_DELIVERED));
  EXPECT_EQ(1, u16(OFF_DROPPED));
  EXPECT_EQ(1, u16(OFF_WAIT));
}

TEST_F(OtaDiag, DuplicateBlockCountsButKeepsTheFirstTime)
{
  ota_diag_request_sent(2000, true);
  ota_diag_block_rx(2040, 0, 63, 0);
  ota_diag_block_rx(2900, 0, 63, 0);
  ota_diag_request_sent(2950, true);
  EXPECT_EQ(2, u16(OFF_BLOCKS));
  EXPECT_EQ(40, u16(OFF_AVG + RX));
}

TEST_F(OtaDiag, MainLoopStalls)
{
  ota_diag_main_loop(2000);
  ota_diag_main_loop(2005);
  ota_diag_main_loop(2054);   // 49 ms: not a stall
  ota_diag_main_loop(2104);   // 50 ms: a stall
  ota_diag_main_loop(5104);   // 3 s
  EXPECT_EQ(2, u16(OFF_STALLS));
  EXPECT_EQ(3000, u16(OFF_GAP));
}

TEST_F(OtaDiag, FirstLoopPassIsNotAStall)
{
  ota_diag_main_loop(900000);
  EXPECT_EQ(0, u16(OFF_STALLS));
  EXPECT_EQ(0, u16(OFF_GAP));
}

TEST_F(OtaDiag, ValuesStopAt16BitMax)
{
  ota_diag_request_sent(2000, true);
  ota_diag_block_rx(2040, 0, 63, 0);
  ota_diag_request_sent(100000, true);
  EXPECT_EQ(0xFFFF, u16(OFF_LONGEST));
  EXPECT_EQ(0xFFFF, u16(OFF_SLOW + TOTAL));
}

TEST_F(OtaDiag, DownloadStartClearsEverything)
{
  block(2000, 20, 40, 2, 2000);
  ota_diag_main_loop(9000);
  ota_diag_main_loop(9500);
  ota_diag_download_start(20000);
  EXPECT_EQ(0, u16(OFF_BLOCKS));
  EXPECT_EQ(0, u16(OFF_SLOW_CYCLES));
  EXPECT_EQ(0, u16(OFF_AVG + TOTAL));
  EXPECT_EQ(0, u16(OFF_STALLS));
}

}  // namespace
