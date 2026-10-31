#include "app/framework/include/af.h"
#include "hal_utils.h"
#include "hal_buttons.h"
#include "app_button.h"

#include "sl_zigbee_debug_print.h"

#define _NUM_SEQUENCE_CALLBACKS  2

#define _HOLD_DEFAULT_FIRST_MS   (4000UL)

static app_button_sequence_cb_t m_sequence_cbs[_NUM_SEQUENCE_CALLBACKS];
static app_button_hold_t m_holds[APP_BUTTON_COUNT];

// Hold tracking, per physical button.  The single-button and both-button
// gestures share this state, exactly like the original hal_buttons.c poll.
static struct
{
  bool active;
  uint32_t timestamp;
  uint32_t timeout;
} m_button[HAL_BUTTON_COUNT];

static sl_zigbee_af_event_t button_poll_event;

static void _on_sequence(const hal_buttons_sequence_t *p_sequence);
static void button_poll_handler(sl_zigbee_af_event_t *event);


void app_button_on_startup(void)
{
  for(uint8_t i = 0; i < _NUM_SEQUENCE_CALLBACKS; i++)
  {
     m_sequence_cbs[i] = NULL;
  }
  for(uint8_t i = 0; i < APP_BUTTON_COUNT; i++)
  {
     m_holds[i] = (app_button_hold_t){ .first_ms = _HOLD_DEFAULT_FIRST_MS };
  }
  for(uint8_t i = 0; i < HAL_BUTTON_COUNT; i++)
  {
     m_button[i].active = false;
     m_button[i].timestamp = 0UL;
     m_button[i].timeout = _HOLD_DEFAULT_FIRST_MS;
  }

  hal_buttons_register_sequence_cb(_on_sequence);

  sl_zigbee_af_event_init(&button_poll_event, button_poll_handler);
  sl_zigbee_af_event_set_active(&button_poll_event);
}

sl_status_t app_button_register_sequence_cb(app_button_sequence_cb_t cb)
{
  if(NULL == cb)
  {
     return SL_STATUS_INVALID_PARAMETER;
  }
  for(uint8_t i = 0; i < _NUM_SEQUENCE_CALLBACKS; i++)
  {
     if((NULL == m_sequence_cbs[i]) || (cb == m_sequence_cbs[i]))
     {
        m_sequence_cbs[i] = cb;
        return SL_STATUS_OK;
     }
  }
  return SL_STATUS_FULL;
}

sl_status_t app_button_register_hold(app_button_t gesture, const app_button_hold_t *p_hold)
{
  if((APP_BUTTON_COUNT <= gesture) || (NULL == p_hold) || (NULL == p_hold->cb))
  {
     return SL_STATUS_INVALID_PARAMETER;
  }
  m_holds[gesture] = *p_hold;
  if(APP_BUTTON_BOTH != gesture)
  {
     m_button[gesture].timeout = p_hold->first_ms;
  }
  return SL_STATUS_OK;
}

bool app_button_is_pressed(app_button_t gesture)
{
  bool t = hal_button_is_pressed(HAL_BUTTON_T);
  bool d = hal_button_is_pressed(HAL_BUTTON_D);

  switch(gesture)
  {
     case APP_BUTTON_T:    return(t && !d);
     case APP_BUTTON_D:    return(d && !t);
     case APP_BUTTON_BOTH: return(t && d);
     default:              return(false);
  }
}

// A press sequence finished (both buttons released for BUTTONS_DONE_US).
// If a press was really a hold (handled by the poll), ignore the sequence.
static void _on_sequence(const hal_buttons_sequence_t *p_sequence)
{
  uint32_t button_0_delta_time = get_time_ms() - p_sequence->last_press_ms[HAL_BUTTON_T];
  uint32_t button_1_delta_time = get_time_ms() - p_sequence->last_press_ms[HAL_BUTTON_D];

  if((p_sequence->presses[HAL_BUTTON_T] && (button_0_delta_time > APP_BUTTON_SEQUENCE_MAX_MS))
               || (p_sequence->presses[HAL_BUTTON_D] && (button_1_delta_time > APP_BUTTON_SEQUENCE_MAX_MS)))
  {
     sl_zigbee_core_debug_print("Time out [button_done_handler]");
     return;
  }

  for(uint8_t i = 0; i < _NUM_SEQUENCE_CALLBACKS; i++)
  {
     if(NULL != m_sequence_cbs[i])
     {
        m_sequence_cbs[i](p_sequence->presses[HAL_BUTTON_T], p_sequence->presses[HAL_BUTTON_D]);
     }
  }
}

// The gesture's hold time has elapsed: call it back, then set up the next callback.
static void _hold_fired(app_button_t gesture)
{
  const app_button_hold_t *p_hold = &m_holds[gesture];

  if(NULL != p_hold->cb)
  {
     p_hold->cb(gesture);
  }

  if(APP_BUTTON_BOTH == gesture)
  {
     if(p_hold->rearm)
     {
        // Prevent repeated calls: both buttons start over
        m_button[HAL_BUTTON_T].active = false;
        m_button[HAL_BUTTON_D].active = false;
     }
  }
  else if(p_hold->rearm)
  {
     m_button[gesture].active = false;
  }
  else if(APP_BUTTON_REPEAT_EVERY_POLL != p_hold->repeat_ms)
  {
     m_button[gesture].timestamp = get_time_ms();
     m_button[gesture].timeout = p_hold->repeat_ms;
  }
}

// Single button gesture: start timing on the first poll that sees it held.
static void _poll_single(app_button_t gesture, hal_button_t mine, hal_button_t other)
{
  if(hal_button_is_pressed(mine) && !m_button[mine].active && !hal_button_is_pressed(other))
  {
     // 1st time thru
     m_button[mine].timestamp = get_time_ms();
     m_button[mine].active = true;
     m_button[mine].timeout = m_holds[gesture].first_ms;
  }
  else if(hal_button_is_pressed(mine) && m_button[mine].active && !hal_button_is_pressed(other) &&
          has_timed_out(m_button[mine].timestamp, m_button[mine].timeout))
  {
     _hold_fired(gesture);
  }
  else
  {
  }
}

// Polls for button(s) being held.
//
// T held (D not held)  -> APP_BUTTON_T hold
// D held (T not held)  -> APP_BUTTON_D hold
// T and D held         -> APP_BUTTON_BOTH hold, only while its enabled() is true
static void button_poll_handler(sl_zigbee_af_event_t *event)
{
  (void)event;

  _poll_single(APP_BUTTON_T, HAL_BUTTON_T, HAL_BUTTON_D);
  _poll_single(APP_BUTTON_D, HAL_BUTTON_D, HAL_BUTTON_T);

  const app_button_hold_t *p_both = &m_holds[APP_BUTTON_BOTH];
  if((NULL != p_both->cb) && ((NULL == p_both->enabled) || p_both->enabled()))
  {
     bool t = hal_button_is_pressed(HAL_BUTTON_T);
     bool d = hal_button_is_pressed(HAL_BUTTON_D);

     if(t && !m_button[HAL_BUTTON_T].active && d && !m_button[HAL_BUTTON_D].active)
     {
        // 1st time thru
        for(uint8_t i = 0; i < HAL_BUTTON_COUNT; i++)
        {
           m_button[i].timestamp = get_time_ms();
           m_button[i].active = true;
           m_button[i].timeout = p_both->first_ms;
        }
     }
     else if(t && m_button[HAL_BUTTON_T].active && d && m_button[HAL_BUTTON_D].active &&
             has_timed_out(m_button[HAL_BUTTON_T].timestamp, m_button[HAL_BUTTON_T].timeout) &&
             has_timed_out(m_button[HAL_BUTTON_D].timestamp, m_button[HAL_BUTTON_D].timeout))
     {
        _hold_fired(APP_BUTTON_BOTH);
     }
     else
     {
     }
  }

  /*** Must always come through here ***/
  for(uint8_t i = 0; i < HAL_BUTTON_COUNT; i++)
  {
     if(!hal_button_is_pressed((hal_button_t)i))
     {
        m_button[i].active = false;
        m_button[i].timeout = m_holds[i].first_ms;
     }
  }

  if(!hal_button_is_pressed(HAL_BUTTON_T) && !hal_button_is_pressed(HAL_BUTTON_D))
  {
     sl_zigbee_af_event_set_delay_ms(&button_poll_event, BUTTONS_POLL_TIME_MS);
  }
  else
  {
     sl_zigbee_af_event_set_delay_ms(&button_poll_event, BUTTONS_ACTIVE_POLL_TIME_MS);
  }
}
