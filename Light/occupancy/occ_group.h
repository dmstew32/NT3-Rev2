#ifndef OCC_GROUP_H_
#define OCC_GROUP_H_

/*
 * Group occupancy control for grouped lights on an Adaptiv network.
 *
 * Every light keeps its own timer and they keep each other in sync with two
 * multicasts (Adaptiv manufacturer specific, occupancy sensing cluster):
 *
 *   KEEPALIVE     time_until_expiration_sec (2), state (1), reason (1), level (1)
 *   LEVEL_CHANGE  state (1), level (1)
 *
 * ON  --dim timeout-->  MID  --off timeout-->  OFF  (no timer)
 *
 * - Occupancy while ON: restart the timer, send nothing.
 * - Occupancy while MID/OFF: go ON at once and send a WAKE keepalive after
 *   0-OCC_GROUP_WAKE_BACKOFF_MS (cancelled if another light's wake arrives).
 * - Near the end of the ON timer (refresh window, OCC_GROUP_REFRESH_WINDOW_MS or
 *   timeout/4 if shorter) one light sends a REFRESH keepalive with its remaining
 *   time; every light that receives it cancels its own and syncs its timer.
 *   A light with more than "margin" (window/4) more time left replies, after
 *   0-OCC_GROUP_REPLY_BACKOFF_MS, with its own time: that light saw occupancy
 *   more recently and keeps the group on.
 * - When the timer expires: wait 0-OCC_GROUP_EXPIRY_BACKOFF_MS, then change
 *   level and send LEVEL_CHANGE, unless another light's arrives first (then
 *   follow it).  A light that missed the command still changes on its own.
 * - Receiving a keepalive while MID/OFF: go ON (someone has occupancy).
 *   Receiving a keepalive while ON only syncs the timer; the level is left
 *   alone (daylight harvesting).
 * - Receiving LEVEL_CHANGE while seeing occupancy right now: stay ON and wake
 *   the group instead.
 *
 * Pure logic: no stack calls.  The caller passes the time, the sensor result
 * and received messages, and does the actions in occ_group_ops_t.
 */

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  OCC_GROUP_ON  = 0,   // wake level (last level set from a switch)
  OCC_GROUP_MID = 1,   // mid level
  OCC_GROUP_OFF = 2,   // off (or stays at mid in EM mode)
} occ_group_state_t;

typedef enum
{
  OCC_KEEPALIVE_WAKE    = 0,   // occupancy turned this light on from MID/OFF
  OCC_KEEPALIVE_REFRESH = 1,   // timer sync near expiry, or a reply with a longer time
} occ_keepalive_reason_t;

#define OCC_GROUP_WAKE_BACKOFF_MS     (300UL)
#define OCC_GROUP_REFRESH_WINDOW_MS   (60000UL)   // at most; timeout/4 if shorter
#define OCC_GROUP_REPLY_BACKOFF_MS    (5000UL)    // at most; window/3 if shorter
#define OCC_GROUP_EXPIRY_BACKOFF_MS   (5000UL)
#define OCC_GROUP_OCCUPIED_NOW_MS     (3000UL)    // "seeing occupancy right now"

typedef struct
{
  void *ctx;
  uint32_t (*dim_timeout_ms)(void *ctx);      // ON -> MID
  uint32_t (*off_timeout_ms)(void *ctx);      // MID -> OFF
  void (*go_to_state)(void *ctx, occ_group_state_t state);   // move this light's level
  uint8_t (*level_for_state)(void *ctx, occ_group_state_t state);
  uint8_t (*current_level)(void *ctx);
  void (*send_keepalive)(void *ctx, uint16_t time_s, occ_group_state_t state,
                         occ_keepalive_reason_t reason, uint8_t level);
  void (*send_level_change)(void *ctx, occ_group_state_t state, uint8_t level);
  uint32_t (*random_ms)(void *ctx, uint32_t max_ms);         // 0 .. max_ms
} occ_group_ops_t;

typedef enum
{
  OCC_SEND_NONE,
  OCC_SEND_WAKE,
  OCC_SEND_REFRESH,
  OCC_SEND_REPLY,
  OCC_SEND_LEVEL_CHANGE,
} occ_group_send_t;

typedef struct
{
  const occ_group_ops_t *ops;
  occ_group_state_t state;
  bool timer_running;
  uint32_t expires_at_ms;
  bool refresh_checked;            // this timer's refresh has been scheduled or seen
  occ_group_send_t pending;        // message waiting for its backoff
  uint32_t send_at_ms;
  occ_group_state_t pending_state; // OCC_SEND_LEVEL_CHANGE: MID or OFF
  bool occupied_seen;
  uint32_t last_occupied_ms;
} occ_group_t;

// Starts ON with a full timer (power up / join): sends nothing, moves nothing.
void occ_group_init(occ_group_t *g, const occ_group_ops_t *ops, uint32_t now_ms);

// Call on every sensor sample that saw occupancy (and that may control the light).
void occ_group_on_occupancy(occ_group_t *g, uint32_t now_ms);

// Call regularly (every sensor sample).  Runs timers and sends.
void occ_group_tick(occ_group_t *g, uint32_t now_ms);

// Received from another light in the group.
void occ_group_rx_keepalive(occ_group_t *g, uint32_t now_ms, uint16_t time_s,
                            occ_group_state_t state, occ_keepalive_reason_t reason);
void occ_group_rx_level_change(occ_group_t *g, uint32_t now_ms, occ_group_state_t state);

// The light was turned on/off by something else (switch, gateway).  No message.
void occ_group_external_on(occ_group_t *g, uint32_t now_ms);
void occ_group_external_off(occ_group_t *g);

// Restart the ON timer (the light was just commanded, e.g. after a switch command).
void occ_group_restart_timer(occ_group_t *g, uint32_t now_ms);

occ_group_state_t occ_group_state(const occ_group_t *g);
uint32_t occ_group_remaining_ms(const occ_group_t *g, uint32_t now_ms);

#endif /* OCC_GROUP_H_ */
