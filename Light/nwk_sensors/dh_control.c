#include "dh_control.h"

uint8_t dh_next_level(uint8_t level, uint16_t lux, uint16_t target, uint8_t floor, uint8_t ceiling)
{
  int32_t present = (int32_t)level;
  int32_t desired = (present * (int32_t)target) / (int32_t)((0U == lux) ? 1U : lux);
  int32_t step = (desired - present) / 2L;
  int32_t limit = (present * (int32_t)DH_MAX_STEP_PERCENT) / 100L;

  if(1L > limit)
  {
     limit = 1L;
  }
  if(limit < step)
  {
     step = limit;
  }
  else if(-limit > step)
  {
     step = -limit;
  }
  else if((0L == step) && (desired != present))
  {
     // Under half a level from the target: still move, or the level never gets there
     step = (desired > present) ? 1L : -1L;
  }

  int32_t next = present + step;

  if((int32_t)floor > next)
  {
     next = (int32_t)floor;
  }
  if((int32_t)ceiling < next)
  {
     next = (int32_t)ceiling;
  }
  return((uint8_t)next);
}

// False = no attempts left
static bool retry_later(dh_master_t *m, uint8_t *attempts)
{
  const dh_ops_t *ops = m->ops;

  (*attempts)++;
  if(DH_SEND_ATTEMPTS <= *attempts)
  {
     return(false);
  }
  ops->schedule_retry(ops->ctx, (DH_RETRY_MS - DH_RETRY_JITTER_MS)
                                + ops->random_ms(ops->ctx, 2UL * DH_RETRY_JITTER_MS));
  return(true);
}

static void level_retry_later(dh_master_t *m)
{
  if(!retry_later(m, &m->attempts))
  {
     // Give up.  A new level is worked out again at the next average.
     m->sending = false;
  }
}

static void attempt_claim(dh_master_t *m)
{
  const dh_ops_t *ops = m->ops;
  bool held = (DH_HEAVY_HOLD_ATTEMPTS > m->claim_attempts) && ops->broadcast_traffic_heavy(ops->ctx);

  if(!held && ops->send_master_claim(ops->ctx))
  {
     m->claiming = false;
  }
  else if(!retry_later(m, &m->claim_attempts))
  {
     // Give up.  A second master still stands down on hearing a level from the other.
     m->claiming = false;
  }
}

static void attempt_send(dh_master_t *m)
{
  const dh_ops_t *ops = m->ops;

  if((DH_HEAVY_HOLD_ATTEMPTS > m->attempts) && ops->broadcast_traffic_heavy(ops->ctx))
  {
     level_retry_later(m);
     return;
  }
  if(!ops->send_level(ops->ctx, m->send_level, (uint8_t)DH_RAMP_DS))
  {
     level_retry_later(m);
     return;
  }

  m->sending = false;
  m->last_level = m->send_level;
  m->repeat_pending = !m->sending_repeat;
  m->skip_averages = DH_SKIP_AFTER_SEND;
  m->quiet_averages = 0U;
}

static void start_send(dh_master_t *m, uint8_t level, bool repeat)
{
  m->sending = true;
  m->sending_repeat = repeat;
  m->send_level = level;
  m->attempts = 0U;
  m->repeat_pending = false;
  attempt_send(m);
}

void dh_master_init(dh_master_t *m, const dh_ops_t *ops)
{
  m->ops = ops;
  m->skip_averages = DH_SKIP_AFTER_CHANGE;
  m->repeat_pending = false;
  m->last_level = 0U;
  m->quiet_averages = 0U;
  m->sending = false;
  m->sending_repeat = false;
  m->send_level = 0U;
  m->attempts = 0U;
  m->claiming = false;
  m->claim_attempts = 0U;
}

void dh_master_on_average(dh_master_t *m, const dh_average_t *in)
{
  if(!in->allowed)
  {
     m->sending = false;
     m->repeat_pending = false;
     if(DH_SKIP_AFTER_CHANGE > m->skip_averages)
     {
        m->skip_averages = DH_SKIP_AFTER_CHANGE;
     }
     return;
  }
  if(m->sending)
  {
     return;     // Retries still running
  }
  if(0U != m->skip_averages)
  {
     m->skip_averages--;
     return;
  }

  uint16_t error = (in->lux > in->target) ? (in->lux - in->target) : (in->target - in->lux);
  uint8_t next = in->level;

  if(DH_LUX_DEADBAND < error)
  {
     next = dh_next_level(in->level, in->lux, in->target, in->floor, in->ceiling);
  }

  if(next != in->level)
  {
     start_send(m, next, false);
  }
  else if(m->repeat_pending)
  {
     start_send(m, m->last_level, true);
  }
  else
  {
     m->quiet_averages++;
     if(DH_REFRESH_AVERAGES <= m->quiet_averages)
     {
        // The present level, not a change.  It doesn't need a repeat.
        start_send(m, in->level, true);
     }
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
  m->sending = false;
  m->repeat_pending = false;
  m->claiming = false;
}

void dh_master_retry(dh_master_t *m, bool allowed)
{
  if(m->claiming)
  {
     attempt_claim(m);
  }
  if(!m->sending)
  {
     return;
  }
  if(!allowed)
  {
     m->sending = false;     // Whatever stopped DH, the level is out of date
     return;
  }
  attempt_send(m);
}

uint8_t dh_master_last_level(const dh_master_t *m)
{
  return(m->last_level);
}
