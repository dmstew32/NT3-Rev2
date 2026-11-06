#ifndef OTA_DIAG_H_
#define OTA_DIAG_H_

/*
 * OTA download timing, for a light with no serial port.  Eight presses of the
 * trim (T) button broadcast what was measured since the download started.
 * Debug only: set OTA_DIAG_ENABLED to 0 for releases.
 *
 * One block is one cycle, from one Image Block Request to the next:
 *
 *   request handed to stack  (tx)
 *     APS ack for the request  (ack)     tx_to_ack
 *     block reaches OTA client (rx)      tx_to_rx
 *     block written to flash             write
 *     10 ms timer fires                  timer_late (ms after it was due)
 *   next request handed to stack         rx_to_tx, total
 *
 * Packet: cluster 0xFCAD, mfg 0x1574, cmd 0xD5, server to client, broadcast.
 * Decode with scripts/ota_diag_decode.py.
 * All values little endian, u16 values stop at 0xFFFF, times in ms:
 *
 *   0  u8   layout version (OTA_DIAG_LAYOUT_VERSION)
 *   1  u8   bootload state
 *   2  u8   broadcast traffic (0 unknown, 1 light, 2 medium, 3 heavy)
 *   3  u8   last block length
 *   4  u32  last block offset
 *   8  u16  seconds since the download started
 *  10  u16  blocks accepted
 *  12  u16  slow cycles (total >= OTA_DIAG_SLOW_CYCLE_MS)
 *  14  u16  3 s response timeouts
 *  16  u16  responses dropped by the client
 *  18  u16  WAIT_FOR_DATA responses from the server
 *  20  u16  requests the stack refused
 *  22  u16  requests not delivered (no APS ack)
 *  24  u16  minimum block request period in use
 *  26  6*u16 latest slow cycle: total, tx_to_ack, tx_to_rx, write, rx_to_tx,
 *            timer_late (tx_to_ack is 0xFFFF if no ack came in that cycle)
 *  38  6*u16 average of all cycles, same order
 *  50  u16  longest cycle
 *  52  u16  main loop stalls (gap >= OTA_DIAG_LOOP_STALL_MS)
 *  54  u16  longest main loop gap
 *  56  u16  MAC unicast retries      (stack counters, since the download started)
 *  58  u16  MAC unicast failures
 *  60  u16  CCA failures
 *  62  u16  APS unicast retries
 *  64  u16  packet buffer failures
 *  66  u16  MAC broadcasts received
 *
 * Pure logic: no stack calls, the caller passes the time.  ota_diag_tx.c does
 * the sending and reads the stack counters.
 */

#include <stdbool.h>
#include <stdint.h>

#define OTA_DIAG_ENABLED         1

#define OTA_DIAG_LAYOUT_VERSION  1
#define OTA_DIAG_PACKET_LEN      68
#define OTA_DIAG_SLOW_CYCLE_MS   1000UL
#define OTA_DIAG_LOOP_STALL_MS   50UL
#define OTA_DIAG_NO_ACK          0xFFFFU

typedef struct
{
  uint16_t mac_retry;
  uint16_t mac_fail;
  uint16_t cca_fail;
  uint16_t aps_retry;
  uint16_t buffer_fail;
  uint16_t bcast_rx;
} ota_diag_counters_t;

// Clears everything: call when the download starts.
void ota_diag_download_start(uint32_t now_ms);

void ota_diag_request_sent(uint32_t now_ms, bool stack_accepted);
void ota_diag_request_acked(uint32_t now_ms, bool delivered);
// A block the client accepted, just before it is written.
void ota_diag_block_rx(uint32_t now_ms, uint32_t offset, uint8_t length, uint16_t min_block_period);
void ota_diag_block_written(uint32_t now_ms);
// The timer to the next request (not the response timeout).
void ota_diag_timer_set(uint32_t now_ms, uint32_t timer_ms);
void ota_diag_timer_fired(uint32_t now_ms);

void ota_diag_response_timeout(void);
void ota_diag_response_dropped(void);
void ota_diag_wait_for_data(void);

// Call on every pass of the main loop.
void ota_diag_main_loop(uint32_t now_ms);

// out must hold OTA_DIAG_PACKET_LEN bytes.  Returns the length.
uint8_t ota_diag_pack(uint8_t *out, uint32_t now_ms, uint8_t bootload_state, uint8_t traffic,
                      const ota_diag_counters_t *counters);

// ota_diag_tx.c, not part of the pure logic:
void ota_diag_tx_download_start(void);    // ota_diag_download_start + counter baseline
void ota_diag_tx_set_bootload_state(uint8_t state);
bool ota_diag_tx_send(void);              // false: the stack refused the broadcast

#endif /* OTA_DIAG_H_ */
