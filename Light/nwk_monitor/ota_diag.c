#include <string.h>

#include "ota_diag.h"

enum
{
  CYCLE_TOTAL,
  CYCLE_TX_TO_ACK,
  CYCLE_TX_TO_RX,
  CYCLE_WRITE,
  CYCLE_RX_TO_TX,
  CYCLE_TIMER_LATE,
  CYCLE_N_VALUES,
};

static struct
{
  uint32_t start_ms;
  bool started;

  // The cycle in progress
  bool have_tx;
  bool have_ack;
  bool have_rx;
  bool timer_armed;
  uint32_t tx_ms;
  uint32_t rx_ms;
  uint32_t timer_due_ms;
  uint32_t cycle[CYCLE_N_VALUES];

  // Finished cycles
  uint32_t sum[CYCLE_N_VALUES];
  uint32_t n_cycles;
  uint32_t n_acked_cycles;
  uint32_t slow[CYCLE_N_VALUES];
  uint32_t longest_cycle;

  uint32_t last_offset;
  uint8_t last_length;
  uint16_t min_block_period;
  uint32_t blocks;
  uint32_t slow_cycles;
  uint32_t timeouts;
  uint32_t dropped;
  uint32_t wait_for_data;
  uint32_t refused;
  uint32_t not_delivered;

  uint32_t last_loop_ms;
  bool have_loop;
  uint32_t loop_stalls;
  uint32_t longest_gap;
} m;

static uint8_t *put16(uint8_t *p, uint32_t value)
{
  uint16_t v = (value > 0xFFFFUL) ? 0xFFFFU : (uint16_t)value;
  *p++ = (uint8_t)(v & 0xFFU);
  *p++ = (uint8_t)(v >> 8);
  return(p);
}

static void finish_cycle(uint32_t now_ms)
{
  m.cycle[CYCLE_TOTAL] = now_ms - m.tx_ms;
  m.cycle[CYCLE_RX_TO_TX] = now_ms - m.rx_ms;

  for(uint8_t i = 0; i < CYCLE_N_VALUES; i++)
  {
     if((CYCLE_TX_TO_ACK != i) || m.have_ack)
     {
        m.sum[i] += m.cycle[i];
     }
  }
  m.n_cycles++;
  if(m.have_ack)
  {
     m.n_acked_cycles++;
  }

  if(m.cycle[CYCLE_TOTAL] > m.longest_cycle)
  {
     m.longest_cycle = m.cycle[CYCLE_TOTAL];
  }
  if(OTA_DIAG_SLOW_CYCLE_MS <= m.cycle[CYCLE_TOTAL])
  {
     m.slow_cycles++;
     memcpy(m.slow, m.cycle, sizeof(m.slow));
     if(!m.have_ack)
     {
        m.slow[CYCLE_TX_TO_ACK] = OTA_DIAG_NO_ACK;
     }
  }
}

void ota_diag_download_start(uint32_t now_ms)
{
  memset(&m, 0, sizeof(m));
  m.start_ms = now_ms;
  m.started = true;
}

void ota_diag_request_sent(uint32_t now_ms, bool stack_accepted)
{
  if(!stack_accepted)
  {
     m.refused++;
     return;
  }

  if(m.have_tx && m.have_rx)
  {
     finish_cycle(now_ms);
  }
  // A request with no block before it (a retry after a timeout) is not a cycle:
  // it is counted in timeouts and the next cycle starts from here.
  m.have_tx = true;
  m.have_ack = false;
  m.have_rx = false;
  m.tx_ms = now_ms;
  memset(m.cycle, 0, sizeof(m.cycle));
}

void ota_diag_request_acked(uint32_t now_ms, bool delivered)
{
  if(!delivered)
  {
     m.not_delivered++;
     return;
  }
  if(m.have_tx && !m.have_ack)
  {
     m.have_ack = true;
     m.cycle[CYCLE_TX_TO_ACK] = now_ms - m.tx_ms;
  }
}

void ota_diag_block_rx(uint32_t now_ms, uint32_t offset, uint8_t length, uint16_t min_block_period)
{
  m.blocks++;
  m.last_offset = offset;
  m.last_length = length;
  m.min_block_period = min_block_period;
  if(m.have_tx && !m.have_rx)
  {
     m.have_rx = true;
     m.rx_ms = now_ms;
     m.cycle[CYCLE_TX_TO_RX] = now_ms - m.tx_ms;
  }
}

void ota_diag_block_written(uint32_t now_ms)
{
  if(m.have_rx)
  {
     m.cycle[CYCLE_WRITE] = now_ms - m.rx_ms;
  }
}

void ota_diag_timer_set(uint32_t now_ms, uint32_t timer_ms)
{
  m.timer_armed = true;
  m.timer_due_ms = now_ms + timer_ms;
}

void ota_diag_timer_fired(uint32_t now_ms)
{
  if(m.timer_armed)
  {
     int32_t late = (int32_t)(now_ms - m.timer_due_ms);
     m.cycle[CYCLE_TIMER_LATE] = (late > 0) ? (uint32_t)late : 0UL;
     m.timer_armed = false;
  }
}

void ota_diag_response_timeout(void)
{
  m.timeouts++;
}

void ota_diag_response_dropped(void)
{
  m.dropped++;
}

void ota_diag_wait_for_data(void)
{
  m.wait_for_data++;
}

void ota_diag_main_loop(uint32_t now_ms)
{
  if(m.have_loop)
  {
     uint32_t gap = now_ms - m.last_loop_ms;
     if(OTA_DIAG_LOOP_STALL_MS <= gap)
     {
        m.loop_stalls++;
     }
     if(gap > m.longest_gap)
     {
        m.longest_gap = gap;
     }
  }
  m.have_loop = true;
  m.last_loop_ms = now_ms;
}

uint8_t ota_diag_pack(uint8_t *out, uint32_t now_ms, uint8_t bootload_state, uint8_t traffic,
                      const ota_diag_counters_t *counters)
{
  uint8_t *p = out;

  *p++ = OTA_DIAG_LAYOUT_VERSION;
  *p++ = bootload_state;
  *p++ = traffic;
  *p++ = m.last_length;
  *p++ = (uint8_t)(m.last_offset & 0xFFUL);
  *p++ = (uint8_t)((m.last_offset >> 8) & 0xFFUL);
  *p++ = (uint8_t)((m.last_offset >> 16) & 0xFFUL);
  *p++ = (uint8_t)((m.last_offset >> 24) & 0xFFUL);
  p = put16(p, m.started ? ((now_ms - m.start_ms) / 1000UL) : 0UL);
  p = put16(p, m.blocks);
  p = put16(p, m.slow_cycles);
  p = put16(p, m.timeouts);
  p = put16(p, m.dropped);
  p = put16(p, m.wait_for_data);
  p = put16(p, m.refused);
  p = put16(p, m.not_delivered);
  p = put16(p, m.min_block_period);

  for(uint8_t i = 0; i < CYCLE_N_VALUES; i++)
  {
     p = put16(p, m.slow[i]);
  }
  for(uint8_t i = 0; i < CYCLE_N_VALUES; i++)
  {
     uint32_t n = (CYCLE_TX_TO_ACK == i) ? m.n_acked_cycles : m.n_cycles;
     p = put16(p, (n > 0UL) ? (m.sum[i] / n) : 0UL);
  }

  p = put16(p, m.longest_cycle);
  p = put16(p, m.loop_stalls);
  p = put16(p, m.longest_gap);
  p = put16(p, counters->mac_retry);
  p = put16(p, counters->mac_fail);
  p = put16(p, counters->cca_fail);
  p = put16(p, counters->aps_retry);
  p = put16(p, counters->buffer_fail);
  p = put16(p, counters->bcast_rx);

  return((uint8_t)(p - out));
}
