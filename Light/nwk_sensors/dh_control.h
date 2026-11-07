#ifndef DH_CONTROL_H_
#define DH_CONTROL_H_

/*
 * Daylight harvesting: the master's control logic.
 *
 * Sampling: the master reads the sensor every 2.5 s.  Every 2 readings (5 s) their
 * average goes through the algorithm.  If the level changes, the master multicasts
 * a DH Level Adjust.  The light itself ramps when that multicast comes back to it.
 *
 * Ramp timer: runs from the moment the stack accepts the multicast, for the ramp's
 * duration.  Readings while it runs are discarded, so the first 2 readings after it
 * ends are the next average.  If the multicast does not come back, the master did
 * not move and works out the level again at that point, so a multicast goes out
 * at most once per ramp time.
 *
 * Algorithm (dh_next_level):
 *
 *   calibrated_lux = sensor_average_lux * CALIBRATED_LUX_FACTOR
 *   desired_level  = current_level * target_lux / calibrated_lux
 *   level_step     = (desired_level - current_level) / 2, at most +/-30 % of current_level
 *   next_level     = current_level + level_step, kept between floor and ceiling
 *   If that changes the level by 5 % or less: current_level (nothing is sent).
 *
 * The more of the lux that is daylight, the smaller the correction, so the level
 * approaches the target without a measured gain.
 *
 * One master per group: a light selected as master announces it, retried about once
 * a second up to DH_SEND_ATTEMPTS attempts, and does so even while DH is not allowed
 * (the selection itself flashes the light).  light_sensor.c decides when a master
 * stands down.  A refused level is not retried: the next average, 5 s later, is.
 *
 * Pure logic: no stack calls.  The caller passes the readings and does the
 * actions in dh_ops_t.
 */

#include <stdbool.h>
#include <stdint.h>

#define CALIBRATED_LUX_FACTOR        (3.5F)    // sensor reading -> lux, to be measured
#define DEFAULT_DH_RAMP_DURATION_SEC (20U)
#define DH_MAX_STEP_PERCENT          (30U)     // of the current level, per adjustment
#define DH_MIN_STEP_PERCENT          (5U)      // this or less is not worth sending
#define DH_READINGS_PER_AVERAGE      (2U)
#define DH_RETRY_MS                  (1000UL)  // the claim: between send attempts ...
#define DH_RETRY_JITTER_MS           (250UL)   // ... +/- this
#define DH_SEND_ATTEMPTS             (5U)      // first attempt + 4 retries
#define DH_HEAVY_HOLD_ATTEMPTS       (3U)      // heavy broadcast traffic: hold the first 3

typedef struct
{
  void *ctx;
  // false = refused by the stack
  bool (*send_level)(void *ctx, uint8_t current_level, uint8_t target_level, uint8_t duration_sec);
  bool (*send_master_claim)(void *ctx);
  bool (*broadcast_traffic_heavy)(void *ctx);
  void (*schedule_retry)(void *ctx, uint32_t delay_ms);            // call dh_master_retry() then
  uint32_t (*random_ms)(void *ctx, uint32_t max_ms);               // 0..max_ms
} dh_ops_t;

typedef struct
{
  bool allowed;         // this light may control, and nothing changed the light since the last reading
  uint32_t now_ms;
  uint16_t reading;     // from the sensor
  uint8_t level;        // the level the correction starts from
  uint16_t target_lux;
  uint8_t floor;
  uint8_t ceiling;
} dh_reading_t;

typedef struct
{
  const dh_ops_t *ops;
  bool ramp_timer_active;
  uint32_t ramp_end_ms;
  bool have_first_reading;
  uint16_t first_reading;
  uint8_t last_level;     // last level sent, 0 = none yet
  bool claiming;
  uint8_t claim_attempts;
} dh_master_t;

uint32_t dh_calibrated_lux(uint32_t sensor_lux);

// The next level, floor..ceiling.  The same level = nothing to send.
uint8_t dh_next_level(uint8_t current_level, uint16_t target_lux, uint32_t sensor_average_lux,
                      uint8_t floor, uint8_t ceiling);

void dh_master_init(dh_master_t *m, const dh_ops_t *ops);
// Every reading from the sensor
void dh_master_on_reading(dh_master_t *m, const dh_reading_t *in);
// When schedule_retry() asked for it
void dh_master_retry(dh_master_t *m);
// This light was just selected as master: tell the group
void dh_master_claim(dh_master_t *m);
// This light is no longer the master
void dh_master_stop(dh_master_t *m);
uint8_t dh_master_last_level(const dh_master_t *m);

#endif /* DH_CONTROL_H_ */
