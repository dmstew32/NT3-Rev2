#include "dh_control.h"

uint32_t dh_calibrated_lux(uint32_t sensor_lux)
{
  return((uint32_t)((float)sensor_lux * CALIBRATED_LUX_FACTOR));
}

uint8_t dh_next_level(uint8_t current_level, uint16_t target_lux, uint32_t sensor_average_lux,
                      uint8_t floor, uint8_t ceiling)
{
  int32_t current = (int32_t)current_level;
  uint32_t calibrated_lux = dh_calibrated_lux(sensor_average_lux);
  int32_t desired = (current * (int32_t)target_lux) / (int32_t)((0UL == calibrated_lux) ? 1UL : calibrated_lux);
  int32_t step = (desired - current) / 2L;
  int32_t limit = (current * (int32_t)DH_MAX_STEP_PERCENT) / 100L;

  if(limit < step)
  {
     step = limit;
  }
  else if(-limit > step)
  {
     step = -limit;
  }

  int32_t next = current + step;

  if((int32_t)floor > next)
  {
     next = (int32_t)floor;
  }
  if((int32_t)ceiling < next)
  {
     next = (int32_t)ceiling;
  }

  int32_t change = (next > current) ? (next - current) : (current - next);

  if((change * 100L) <= (current * (int32_t)DH_MIN_STEP_PERCENT))
  {
     return(current_level);
  }
  return((uint8_t)next);
}

static void attempt_claim(dh_master_t *m)
{
  const dh_ops_t *ops = m->ops;
  bool held = (DH_HEAVY_HOLD_ATTEMPTS > m->claim_attempts) && ops->broadcast_traffic_heavy(ops->ctx);

  if(!held && ops->send_master_claim(ops->ctx))
  {
     m->claiming = false;
     return;
  }

  m->claim_attempts++;
  if(DH_SEND_ATTEMPTS <= m->claim_attempts)
  {
     // Give up.  A second master still stands down on hearing a level from the other.
     m->claiming = false;
     return;
  }
  ops->schedule_retry(ops->ctx, (DH_RETRY_MS - DH_RETRY_JITTER_MS)
                                + ops->random_ms(ops->ctx, 2UL * DH_RETRY_JITTER_MS));
}

void dh_master_init(dh_master_t *m, const dh_ops_t *ops)
{
  m->ops = ops;
  m->ramp_timer_active = false;
  m->ramp_end_ms = 0UL;
  m->have_first_reading = false;
  m->first_reading = 0U;
  m->last_level = 0U;
  m->claiming = false;
  m->claim_attempts = 0U;
}

void dh_master_on_reading(dh_master_t *m, const dh_reading_t *in)
{
  if(!in->allowed)
  {
     m->have_first_reading = false;
     return;
  }
  if(m->ramp_timer_active)
  {
     if(0L > (int32_t)(in->now_ms - m->ramp_end_ms))
     {
        return;     // Ramping: discard
     }
     m->ramp_timer_active = false;
  }
  if(!m->have_first_reading)
  {
     m->first_reading = in->reading;
     m->have_first_reading = true;
     return;
  }
  m->have_first_reading = false;

  uint32_t average = ((uint32_t)m->first_reading + (uint32_t)in->reading) / DH_READINGS_PER_AVERAGE;

  uint8_t next = dh_next_level(in->level, in->target_lux, average, in->floor, in->ceiling);

  if(next == in->level)
  {
     return;
  }
  if(m->ops->send_level(m->ops->ctx, in->level, next, (uint8_t)DEFAULT_DH_RAMP_DURATION_SEC))
  {
     m->last_level = next;
     m->ramp_timer_active = true;
     m->ramp_end_ms = in->now_ms + ((uint32_t)DEFAULT_DH_RAMP_DURATION_SEC * 1000UL);
  }
}

void dh_master_claim(dh_master_t *m)
{
  m->claiming = true;
  m->claim_attempts = 0U;
  attempt_claim(m);
}

void dh_master_stop(dh_master_t *m)
{
  m->have_first_reading = false;
  m->claiming = false;
}

void dh_master_retry(dh_master_t *m)
{
  if(m->claiming)
  {
     attempt_claim(m);
  }
}

uint8_t dh_master_last_level(const dh_master_t *m)
{
  return(m->last_level);
}
