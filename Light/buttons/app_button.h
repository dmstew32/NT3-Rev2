#ifndef APP_BUTTON_H_
#define APP_BUTTON_H_

/*
 * Button gestures, built on hal_buttons.
 *
 * Two kinds of gestures are reported through registered callbacks:
 *
 *  - Press sequence: T and/or D pressed and released a number of times.  When
 *    both buttons have been released for 1.5 seconds the callback gets the
 *    press counts.  A sequence whose last press was held longer than
 *    APP_BUTTON_SEQUENCE_MAX_MS (i.e. a hold, not a press) is ignored.
 *
 *  - Hold: T alone, D alone, or both buttons held.  Holds are sampled by a
 *    poll (BUTTONS_POLL_TIME_MS idle, BUTTONS_ACTIVE_POLL_TIME_MS while a
 *    button is down).  The callback runs once the gesture has been held for
 *    first_ms, then according to repeat_ms / rearm.
 *
 * Call app_button_on_startup() after hal_buttons_init(), then register.
 */

#include <stdbool.h>
#include <stdint.h>
#include "sl_status.h"

#define BUTTONS_POLL_TIME_MS          (1005UL)
#define BUTTONS_ACTIVE_POLL_TIME_MS   (200UL)

#define APP_BUTTON_SEQUENCE_MAX_MS    (2000UL)  // last press older than this = it was a hold

#define APP_BUTTON_REPEAT_EVERY_POLL  (0UL)     // repeat_ms: call back on every poll

typedef enum
{
  APP_BUTTON_T = 0,     // BUTTON 0 alone
  APP_BUTTON_D,         // BUTTON 1 alone
  APP_BUTTON_BOTH,      // both buttons
  APP_BUTTON_COUNT
} app_button_t;

typedef void (*app_button_sequence_cb_t)(uint8_t presses_t, uint8_t presses_d);
typedef void (*app_button_held_cb_t)(app_button_t gesture);
typedef bool (*app_button_enabled_cb_t)(void);

typedef struct
{
  uint32_t first_ms;                 // held this long before the first callback
  uint32_t repeat_ms;                // then every repeat_ms, or APP_BUTTON_REPEAT_EVERY_POLL
  bool rearm;                        // after a callback, start over: first_ms again
  app_button_enabled_cb_t enabled;   // APP_BUTTON_BOTH only: gesture off when this returns false (NULL = on)
  app_button_held_cb_t cb;
} app_button_hold_t;

void app_button_on_startup(void);

sl_status_t app_button_register_sequence_cb(app_button_sequence_cb_t cb);
sl_status_t app_button_register_hold(app_button_t gesture, const app_button_hold_t *p_hold);

bool app_button_is_pressed(app_button_t gesture);

#endif /* APP_BUTTON_H_ */
