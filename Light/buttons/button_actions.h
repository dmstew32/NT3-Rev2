#ifndef BUTTON_ACTIONS_H_
#define BUTTON_ACTIONS_H_

/*
 * What the NT-3 buttons do.  Registers the gestures with app_button.
 *
 * BUTTON 0 also known as T
 * BUTTON 1 also known as D, closest to sensor
 *
 * PRESS SEQUENCES  (press and release, then wait 1.5 seconds)
 *   T x10 + D x10 (+/-1)  Factory reset
 *   T x2  + D x2          Toggle 50% mode
 *   T x4  + D x4          Toggle EM mode
 *   T x6  + D x6          Local auto commission (Adaptiv network, not in a group)
 *   T x8  + D x8          Show firmware version on the LEDs (ALLOW_DISPLAY_VERSION)
 *
 *   The rest are only allowed when are_buttons_allowed() (see button_actions.c):
 *   D x1-12               Occupancy sensor delays / threshold, see m_occ_delays[]
 *   T x1-6                Max-mid-min light levels, see m_light_levels[]
 *
 * HOLDS
 *   T  held 4 seconds     Cycle color temperature, +500K every 2.5 seconds
 *   D  held 4 seconds     Manual dim: steps down, then back to max
 *   T+D held 4 seconds    Manual add to group (Adaptiv network, not in a group)
 */

void button_actions_on_startup(void);

#endif /* BUTTON_ACTIONS_H_ */
