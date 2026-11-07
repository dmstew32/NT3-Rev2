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
  g->attempts = 0U;
  g->send_at_ms = now_ms + g->ops->random_ms(g->ops->ctx, max_backoff_ms);
}

static void cancel(occ_group_t *g)
{
  g->pending = OCC_SEND_NONE;
  g->attempts = 0U;
}

// The pending message was not sent (held or refused): try again in about a
// second, or give up after the last attempt.
static void retry_later(occ_group_t *g, uint32_t now_ms)
{
  g->attempts++;
  if(g->attempts >= OCC_GROUP_SEND_ATTEMPTS)
  {
     cancel(g);
     return;
  }
  g->send_at_ms = now_ms + (OCC_GROUP_RETRY_MS - OCC_GROUP_RETRY_JITTER_MS)
                + g->ops->random_ms(g->ops->ctx, 2UL * OCC_GROUP_RETRY_JITTER_MS);
}

// Change state and move the light.  ON/MID start their timer, OFF has none.
static void enter(occ_group_t *g, occ_group_state_t state, uint32_t now_ms)
{
  g->state = state;
  if(OCC_GROUP_ON == state)
  {
     g->own_wake_sent = false;    // on: nothing left to wait for
  }
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

// Go ON with the group's timer (a wake was received, ours or another light's)
static void wake_on(occ_group_t *g, uint32_t now_ms, uint32_t duration_ms)
{
  if(OCC_GROUP_OFF == g->state)
  {
     g->ops->woke_from_off(g->ops->ctx);
  }
  cancel(g);
  enter(g, OCC_GROUP_ON, now_ms);
  start_timer(g, now_ms, duration_ms);
}

// Send the pending message.  It stays pending (for a retry) if it was held for
// heavy broadcast traffic or the stack refused it; any other outcome ends it.
static void send_pending(occ_group_t *g, uint32_t now_ms)
{
  const occ_group_ops_t *ops = g->ops;
  occ_group_send_t what = g->pending;
  uint32_t left = occ_group_remaining_ms(g, now_ms);
  bool sent = true;

  // Held: nothing happens yet (a level change does not move this light either,
  // so the group still moves together when the command goes out)
  if((g->attempts < OCC_GROUP_HEAVY_HOLD_ATTEMPTS) && ops->broadcast_traffic_heavy(ops->ctx))
  {
     retry_later(g, now_ms);
     return;
  }

  switch(what)
  {
     case OCC_SEND_WAKE:
     {
        // Not ON yet (the light waits for the group): announce a full ON timer
        uint32_t wake_ms = (OCC_GROUP_ON == g->state) ? left : ops->dim_timeout_ms(ops->ctx);
        sent = ops->send_keepalive(ops->ctx, to_seconds(wake_ms), OCC_GROUP_ON, OCC_KEEPALIVE_WAKE,
                                   ops->level_for_state(ops->ctx, OCC_GROUP_ON));
        if(sent && (OCC_GROUP_ON != g->state))
        {
           g->own_wake_sent = true;
           g->own_wake_wait_ms = now_ms + OCC_GROUP_OWN_WAKE_WAIT_MS;
           g->own_wake_expires_ms = now_ms + wake_ms;
        }
        break;
     }
     case OCC_SEND_REFRESH:
     case OCC_SEND_REPLY:
        if((OCC_GROUP_ON == g->state) && (0UL != left))
        {
           sent = ops->send_keepalive(ops->ctx, to_seconds(left), OCC_GROUP_ON, OCC_KEEPALIVE_REFRESH,
                                      ops->current_level(ops->ctx));
        }
        break;
     case OCC_SEND_LEVEL_CHANGE:
        // Move on the first real attempt, not again on a retry
        if(g->state != g->pending_state)
        {
           enter(g, g->pending_state, now_ms);
        }
        sent = ops->send_level_change(ops->ctx, g->pending_state,
                                      ops->level_for_state(ops->ctx, g->pending_state));
        break;
     default:
        break;
  }

  if(sent)
  {
     cancel(g);
  }
  else
  {
     retry_later(g, now_ms);
  }
}

/***********************  API  *********************/
void occ_group_init(occ_group_t *g, const occ_group_ops_t *ops, uint32_t now_ms)
{
  g->ops = ops;
  g->state = OCC_GROUP_ON;
  g->pending = OCC_SEND_NONE;
  g->attempts = 0U;
  g->own_wake_sent = false;
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
  else if((OCC_SEND_WAKE != g->pending) && !g->own_wake_sent)
  {
     // Wake the group; this light goes ON when the wake comes back (or another
     // light's wake arrives first).  Already waking: nothing more to do.
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
  if(g->own_wake_sent && reached(now_ms, g->own_wake_wait_ms))
  {
     // The group has our wake but it never came back to us: go ON with the group
     uint32_t left_ms = reached(now_ms, g->own_wake_expires_ms) ? 0UL : (g->own_wake_expires_ms - now_ms);
     g->own_wake_sent = false;
     if(OCC_GROUP_ON != g->state)
     {
        wake_on(g, now_ms, left_ms);
     }
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

  // Expired: change level after a backoff unless another light's command comes first.
  // Not while waking the group: the wake decides (if it fails, this runs next tick).
  if((0UL == left) && (OCC_SEND_WAKE != g->pending) && !g->own_wake_sent)
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
     wake_on(g, now_ms, received_ms);
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

void occ_group_rx_own_keepalive(occ_group_t *g, uint32_t now_ms, uint16_t time_s,
                                occ_group_state_t state, occ_keepalive_reason_t reason)
{
  // Our wake came back: the group has it now, go ON with it.  Anything else we
  // sent changes nothing here (we already acted on it).
  if((OCC_KEEPALIVE_WAKE == reason) && (OCC_GROUP_ON == state) && (OCC_GROUP_ON != g->state))
  {
     wake_on(g, now_ms, (uint32_t)time_s * 1000UL);
  }
}

void occ_group_external_on(occ_group_t *g, uint32_t now_ms)
{
  cancel(g);
  g->own_wake_sent = false;
  g->state = OCC_GROUP_ON;
  start_timer(g, now_ms, g->ops->dim_timeout_ms(g->ops->ctx));
}

void occ_group_external_off(occ_group_t *g)
{
  cancel(g);
  g->own_wake_sent = false;
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
