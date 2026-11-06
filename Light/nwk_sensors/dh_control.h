#ifndef DH_CONTROL_H_
#define DH_CONTROL_H_

/*
 * Daylight harvesting: the master's control logic.
 *
 * Measured lux = daylight + light from the group, and the light from the group
 * follows the level.  Scaling the level by target / lux needs no measured gain:
 *
 *   desired = level * target / lux
 *   step    = (desired - level) / 2, at most +/-30 % of the level
 *
 * The more of the lux that is daylight, the smaller the correction, so the
 * level approaches the target without overshoot.  The factor 1/2 keeps it stable
 * when light output is not linear with the level.
 *
 * Timing: called with every 20 s lux average.  After a send, two averages are
 * skipped: the first includes the 18.5 s ramp, and the third (60 s) is free of
 * it.  After anything else that changed the light, one average is skipped.
 *
 * Within DH_LUX_DEADBAND of the target nothing changes.  The last new level is
 * sent once more at the next update if there is no newer level, for a light that
 * missed it.  Then the group only hears the present level again about every
 * 10 minutes, for a light that restarted or missed both.
 *
 * Sending: a multicast the stack refuses is tried again about once a second, up
 * to DH_SEND_ATTEMPTS attempts.  While broadcast traffic is heavy the first
 * DH_HEAVY_HOLD_ATTEMPTS attempts are held (not sent).
 *
 * One master per group: a light selected as master announces it, with the same
 * retries, and does so even while DH is not allowed (the selection itself flashes
 * the light).  light_sensor.c decides when a master stands down.
 *
 * Pure logic: no stack calls.  The caller passes the readings and does the
 * actions in dh_ops_t.
 */

#include <stdbool.h>
#include <stdint.h>

#define DH_LUX_DEADBAND         (50U)     // lux, either side of the target
#define DH_MAX_STEP_PERCENT     (30U)     // of the present level, per update
#define DH_RAMP_DS              (185U)    // 18.5 s
#define DH_SKIP_AFTER_SEND      (2U)      // third 20 s average: one update per 60 s
#define DH_SKIP_AFTER_CHANGE    (1U)      // next average may still include the change
#define DH_REFRESH_AVERAGES     (28U)     // quiet averages + the 2 after a send: 30 x 20 s = 10 minutes
#define DH_RETRY_MS             (1000UL)  // between send attempts ...
#define DH_RETRY_JITTER_MS      (250UL)   // ... +/- this
#define DH_SEND_ATTEMPTS        (5U)      // first attempt + 4 retries
#define DH_HEAVY_HOLD_ATTEMPTS  (3U)      // heavy broadcast traffic: hold the first 3

typedef struct
{
  void *ctx;
  bool (*send_level)(void *ctx, uint8_t level, uint8_t ramp_ds);   // false = refused by the stack
  bool (*broadcast_traffic_heavy)(void *ctx);
  void (*schedule_retry)(void *ctx, uint32_t delay_ms);            // call dh_master_retry() then
  uint32_t (*random_ms)(void *ctx, uint32_t max_ms);               // 0..max_ms
  bool (*send_master_claim)(void *ctx);                            // false = refused by the stack
} dh_ops_t;

typedef struct
{
  bool allowed;       // this light may control, and nothing changed the light during the average
  uint8_t level;      // the level the correction starts from
  uint16_t lux;
  uint16_t target;
  uint8_t floor;
  uint8_t ceiling;
} dh_average_t;

typedef struct
{
  const dh_ops_t *ops;
  uint8_t skip_averages;
  bool repeat_pending;
  uint8_t last_level;     // last level sent, 0 = none yet
  uint8_t quiet_averages; // since the last send, while allowed
  bool sending;
  bool sending_repeat;
  uint8_t send_level;
  uint8_t attempts;
  bool claiming;
  uint8_t claim_attempts;
} dh_master_t;

// The next level, floor..ceiling.  The same level = nothing to do.
uint8_t dh_next_level(uint8_t level, uint16_t lux, uint16_t target, uint8_t floor, uint8_t ceiling);

void dh_master_init(dh_master_t *m, const dh_ops_t *ops);
// Every 20 s lux average
void dh_master_on_average(dh_master_t *m, const dh_average_t *in);
// When schedule_retry() asked for it.  allowed = this light may still control.
void dh_master_retry(dh_master_t *m, bool allowed);
// This light was just selected as master: tell the group
void dh_master_claim(dh_master_t *m);
// This light is no longer the master: nothing it was going to send goes out
void dh_master_stop(dh_master_t *m);
uint8_t dh_master_last_level(const dh_master_t *m);

#endif /* DH_CONTROL_H_ */
