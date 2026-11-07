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
 * - Occupancy while MID/OFF: send a WAKE keepalive after 0-OCC_GROUP_WAKE_BACKOFF_MS
 *   (0: at once, since the whole group waits for it; a light that is already
 *   on when it sees someone sends nothing).  This light does not move yet:
 *   it goes ON when its own wake comes back from the stack (multicast loopback),
 *   the same moment the rest of the group receives it, so the group comes on
 *   together.  If the stack accepted the wake but it does not come back within
 *   OCC_GROUP_OWN_WAKE_WAIT_MS, the light goes ON anyway.  If the wake cannot be
 *   sent at all, the light stays where it is, like the rest of the group.
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
 * Sending: a multicast the stack refuses (e.g. broadcast table full) is tried
 * again about once a second (OCC_GROUP_RETRY_MS +/- OCC_GROUP_RETRY_JITTER_MS),
 * up to OCC_GROUP_SEND_ATTEMPTS attempts.  While broadcast traffic is heavy the
 * first OCC_GROUP_HEAVY_HOLD_ATTEMPTS attempts are held (not sent); the last
 * ones are sent anyway, the stack refuses them if it has no room.  A held
 * level change does not move this light until it is really sent.  The retry
 * stays a pending message here, so another light's message arriving meanwhile
 * still cancels it, and the keepalive time is worked out at each attempt.
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

#define OCC_GROUP_WAKE_BACKOFF_MS     (0UL)       // wake goes out at once: the whole group waits for it
#define OCC_GROUP_REFRESH_WINDOW_MS   (60000UL)   // at most; timeout/4 if shorter
#define OCC_GROUP_REPLY_BACKOFF_MS    (5000UL)    // at most; window/3 if shorter
#define OCC_GROUP_EXPIRY_BACKOFF_MS   (5000UL)
#define OCC_GROUP_OCCUPIED_NOW_MS     (3000UL)    // "seeing occupancy right now"
#define OCC_GROUP_RETRY_MS            (1000UL)    // between send attempts ...
#define OCC_GROUP_RETRY_JITTER_MS     (250UL)     // ... +/- this
#define OCC_GROUP_SEND_ATTEMPTS       (5U)        // first attempt + 4 retries: about 4 s
#define OCC_GROUP_HEAVY_HOLD_ATTEMPTS (3U)        // heavy broadcast traffic: hold the first 3
#define OCC_GROUP_OWN_WAKE_WAIT_MS    (500UL)     // own wake sent: go ON if it has not come back by then

typedef struct
{
  void *ctx;
  uint32_t (*dim_timeout_ms)(void *ctx);      // ON -> MID
  uint32_t (*off_timeout_ms)(void *ctx);      // MID -> OFF
  void (*go_to_state)(void *ctx, occ_group_state_t state);   // move this light's level
  uint8_t (*level_for_state)(void *ctx, occ_group_state_t state);
  uint8_t (*current_level)(void *ctx);
  // Return false if the stack refused the multicast: it is tried again.
  bool (*send_keepalive)(void *ctx, uint16_t time_s, occ_group_state_t state,
                         occ_keepalive_reason_t reason, uint8_t level);
  bool (*send_level_change)(void *ctx, occ_group_state_t state, uint8_t level);
  uint32_t (*random_ms)(void *ctx, uint32_t max_ms);         // 0 .. max_ms
  bool (*broadcast_traffic_heavy)(void *ctx);                // hold sends while true
  void (*woke_from_off)(void *ctx);                          // OFF -> ON because of occupancy
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
  uint8_t attempts;                // send attempts made for the pending message
  occ_group_state_t pending_state; // OCC_SEND_LEVEL_CHANGE: MID or OFF
  bool own_wake_sent;              // our wake went out while not ON: waiting for it to come back
  uint32_t own_wake_wait_ms;       // ... go ON at this time if it has not
  uint32_t own_wake_expires_ms;    // ... with the timer the wake announced
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

// Our own keepalive, delivered back by the stack (multicast loopback).  Only a
// wake matters: it turns this light on together with the group.
void occ_group_rx_own_keepalive(occ_group_t *g, uint32_t now_ms, uint16_t time_s,
                                occ_group_state_t state, occ_keepalive_reason_t reason);

// The light was turned on/off by something else (switch, gateway).  No message.
void occ_group_external_on(occ_group_t *g, uint32_t now_ms);
void occ_group_external_off(occ_group_t *g);

// Restart the ON timer (the light was just commanded, e.g. after a switch command).
void occ_group_restart_timer(occ_group_t *g, uint32_t now_ms);

occ_group_state_t occ_group_state(const occ_group_t *g);
uint32_t occ_group_remaining_ms(const occ_group_t *g, uint32_t now_ms);

#endif /* OCC_GROUP_H_ */
