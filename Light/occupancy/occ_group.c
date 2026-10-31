#include <stddef.h>
#include "occ_group.h"

/***********************  HELPERS  *********************/
static bool reached(uint32_t now_ms, uint32_t at_ms)
{
  return((int32_t)(now_ms - at_ms) >= 0);
}

uint32_t occ_group_remaining_ms(const occ_group_t *g, uint32_t now_ms)
{
  int32_t left;

  if(!g->timer_running)
  {
     return(0UL);
  }
  left = (int32_t)(g->expires_at_ms - now_ms);
  return((left > 0) ? (uint32_t)left : 0UL);
}

occ_group_state_t occ_group_state(const occ_group_t *g)
{
  return(g->state);
}

static uint32_t refresh_window_ms(const occ_group_t *g)
{
  uint32_t window = g->ops->dim_timeout_ms(g->ops->ctx) / 4UL;
  return((window < OCC_GROUP_REFRESH_WINDOW_MS) ? window : OCC_GROUP_REFRESH_WINDOW_MS);
}

// A light with this much more time left than a received keepalive replies
static uint32_t margin_ms(const occ_group_t *g)
{
  return(refresh_window_ms(g) / 4UL);
}

static uint32_t reply_backoff_ms(const occ_group_t *g)
{
  uint32_t backoff = refresh_window_ms(g) / 3UL;
  return((backoff < OCC_GROUP_REPLY_BACKOFF_MS) ? backoff : OCC_GROUP_REPLY_BACKOFF_MS);
}

static void start_timer(occ_group_t *g, uint32_t now_ms, uint32_t duration_ms)
{
  g->timer_running = true;
  g->expires_at_ms = now_ms + duration_ms;
  // Synced to a time already inside the refresh window: that was this period's refresh
  g->refresh_checked = (OCC_GROUP_ON == g->state) && (duration_ms <= refresh_window_ms(g));
}

static void schedule(occ_group_t *g, occ_group_send_t what, uint32_t now_ms, uint32_t max_backoff_ms)
{
  g->pending = what;
  g->send_at_ms = now_ms + g->ops->random_ms(g->ops->ctx, max_backoff_ms);
}

static void cancel(occ_group_t *g)
{
  g->pending = OCC_SEND_NONE;
}

// Change state and move the light.  ON/MID start their timer, OFF has none.
static void enter(occ_group_t *g, occ_group_state_t state, uint32_t now_ms)
{
  g->state = state;
  switch(state)
  {
     case OCC_GROUP_ON:
        start_timer(g, now_ms, g->ops->dim_timeout_ms(g->ops->ctx));
        break;
     case OCC_GROUP_MID:
        start_timer(g, now_ms, g->ops->off_timeout_ms(g->ops->ctx));
        break;
     default:
        g->timer_running = false;
        break;
  }
  g->ops->go_to_state(g->ops->ctx, state);
}

static uint16_t to_seconds(uint32_t ms)
{
  uint32_t s = (ms + 999UL) / 1000UL;
  return((s > 0xFFFFUL) ? 0xFFFFU : (uint16_t)s);
}

static bool occupied_now(const occ_group_t *g, uint32_t now_ms)
{
  return(g->occupied_seen && !reached(now_ms, g->last_occupied_ms + OCC_GROUP_OCCUPIED_NOW_MS));
}

static void send_pending(occ_group_t *g, uint32_t now_ms)
{
  const occ_group_ops_t *ops = g->ops;
  occ_group_send_t what = g->pending;
  uint32_t left = occ_group_remaining_ms(g, now_ms);

  g->pending = OCC_SEND_NONE;
  switch(what)
  {
     case OCC_SEND_WAKE:
        ops->send_keepalive(ops->ctx, to_seconds(left), OCC_GROUP_ON, OCC_KEEPALIVE_WAKE,
                            ops->level_for_state(ops->ctx, OCC_GROUP_ON));
        break;
     case OCC_SEND_REFRESH:
     case OCC_SEND_REPLY:
        if((OCC_GROUP_ON == g->state) && (0UL != left))
        {
           ops->send_keepalive(ops->ctx, to_seconds(left), OCC_GROUP_ON, OCC_KEEPALIVE_REFRESH,
                               ops->current_level(ops->ctx));
        }
        break;
     case OCC_SEND_LEVEL_CHANGE:
        enter(g, g->pending_state, now_ms);
        ops->send_level_change(ops->ctx, g->pending_state, ops->level_for_state(ops->ctx, g->pending_state));
        break;
     default:
        break;
  }
}

/***********************  API  *********************/
void occ_group_init(occ_group_t *g, const occ_group_ops_t *ops, uint32_t now_ms)
{
  g->ops = ops;
  g->state = OCC_GROUP_ON;
  g->pending = OCC_SEND_NONE;
  g->occupied_seen = false;
  g->last_occupied_ms = now_ms;
  start_timer(g, now_ms, ops->dim_timeout_ms(ops->ctx));
}

void occ_group_on_occupancy(occ_group_t *g, uint32_t now_ms)
{
  g->occupied_seen = true;
  g->last_occupied_ms = now_ms;

  if(OCC_GROUP_ON == g->state)
  {
     start_timer(g, now_ms, g->ops->dim_timeout_ms(g->ops->ctx));
     if((OCC_SEND_REFRESH == g->pending) || (OCC_SEND_LEVEL_CHANGE == g->pending))
     {
        cancel(g);    // no longer about to expire
     }
  }
  else
  {
     enter(g, OCC_GROUP_ON, now_ms);
     schedule(g, OCC_SEND_WAKE, now_ms, OCC_GROUP_WAKE_BACKOFF_MS);
  }
}

void occ_group_tick(occ_group_t *g, uint32_t now_ms)
{
  uint32_t left;

  if((OCC_SEND_NONE != g->pending) && reached(now_ms, g->send_at_ms))
  {
     send_pending(g, now_ms);
  }
  if(!g->timer_running)
  {
     return;
  }

  left = occ_group_remaining_ms(g, now_ms);

  // Refresh: sent somewhere in the first 2/3 of the window, leaving time for replies
  if((OCC_GROUP_ON == g->state) && !g->refresh_checked && (0UL != left) && (left <= refresh_window_ms(g)))
  {
     g->refresh_checked = true;
     if(OCC_SEND_NONE == g->pending)
     {
        schedule(g, OCC_SEND_REFRESH, now_ms, (2UL * refresh_window_ms(g)) / 3UL);
     }
  }

  // Expired: change level after a backoff unless another light's command comes first
  if(0UL == left)
  {
     g->timer_running = false;
     g->pending_state = (OCC_GROUP_ON == g->state) ? OCC_GROUP_MID : OCC_GROUP_OFF;
     schedule(g, OCC_SEND_LEVEL_CHANGE, now_ms, OCC_GROUP_EXPIRY_BACKOFF_MS);
  }
}

void occ_group_rx_keepalive(occ_group_t *g, uint32_t now_ms, uint16_t time_s,
                            occ_group_state_t state, occ_keepalive_reason_t reason)
{
  uint32_t received_ms = (uint32_t)time_s * 1000UL;
  uint32_t own_ms;

  (void)reason;
  if(OCC_GROUP_ON != state)
  {
     return;   // only ON keepalives are defined
  }

  if(OCC_GROUP_ON != g->state)
  {
     // A light in the group has occupancy (wake) or outlasted our timer (refresh)
     cancel(g);
     enter(g, OCC_GROUP_ON, now_ms);
     start_timer(g, now_ms, received_ms);
     return;
  }

  own_ms = occ_group_remaining_ms(g, now_ms);
  if(own_ms > received_ms + margin_ms(g))
  {
     // We saw occupancy later: tell the group (a pending wake already will)
     if((OCC_SEND_REPLY != g->pending) && (OCC_SEND_WAKE != g->pending))
     {
        schedule(g, OCC_SEND_REPLY, now_ms, reply_backoff_ms(g));
     }
  }
  else
  {
     // Sync to the sender, our own keepalive is not needed
     cancel(g);
     start_timer(g, now_ms, received_ms);
  }
}

void occ_group_rx_level_change(occ_group_t *g, uint32_t now_ms, occ_group_state_t state)
{
  if(OCC_GROUP_ON == state)
  {
     cancel(g);
     if(OCC_GROUP_ON != g->state)
     {
        enter(g, OCC_GROUP_ON, now_ms);
     }
     else
     {
        start_timer(g, now_ms, g->ops->dim_timeout_ms(g->ops->ctx));
     }
     return;
  }

  if((OCC_GROUP_ON == g->state) && occupied_now(g, now_ms))
  {
     // Occupied here: stay on and bring the group back
     if(OCC_SEND_WAKE != g->pending)
     {
        schedule(g, OCC_SEND_WAKE, now_ms, OCC_GROUP_WAKE_BACKOFF_MS);
     }
     return;
  }

  if(OCC_SEND_WAKE != g->pending)
  {
     cancel(g);
  }
  if(g->state == state)
  {
     // Already there (changed on our own timer): sync the timer
     if(OCC_GROUP_MID == state)
     {
        start_timer(g, now_ms, g->ops->off_timeout_ms(g->ops->ctx));
     }
     return;
  }
  if((OCC_GROUP_MID == state) && (OCC_GROUP_OFF == g->state))
  {
     return;   // late MID after we went off: stay off
  }
  enter(g, state, now_ms);
}

void occ_group_external_on(occ_group_t *g, uint32_t now_ms)
{
  cancel(g);
  g->state = OCC_GROUP_ON;
  start_timer(g, now_ms, g->ops->dim_timeout_ms(g->ops->ctx));
}

void occ_group_external_off(occ_group_t *g)
{
  cancel(g);
  g->state = OCC_GROUP_OFF;
  g->timer_running = false;
}

void occ_group_restart_timer(occ_group_t *g, uint32_t now_ms)
{
  if(OCC_GROUP_ON != g->state)
  {
     return;
  }
  start_timer(g, now_ms, g->ops->dim_timeout_ms(g->ops->ctx));
  if((OCC_SEND_REFRESH == g->pending) || (OCC_SEND_LEVEL_CHANGE == g->pending))
  {
     cancel(g);
  }
}
