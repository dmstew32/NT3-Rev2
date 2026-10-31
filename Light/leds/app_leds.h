#ifndef APP_LEDS_H_
#define APP_LEDS_H_

/*
 * What the NT-3 LEDs show.  Every LED indication of the application is here;
 * call these functions, not hal_leds.  Red = PD2, green = PD1.
 *
 *  Indication                          LED    Pattern
 *  ----------------------------------  -----  --------------------------------------
 *  Starting up                         both   on while app_init() runs, then off
 *  Not joined, first check after boot  red    1 long blink: 2 s on (first check 750 ms after init)
 *  Not joined                          red    1 blink 500/500 ms, every 6 s
 *  Joined, not in a group              red    2 blinks 500/500 ms, every 6 s
 *  Joined and in a group               red    off, checked every 12 s
 *  Button setting accepted             red    1 blink per press, 250/250 ms
 *  Button input rejected               red    1 long blink, 1.5 s on
 *  Manual dim step (D held)            red    1 blink 500/50 ms per step
 *  Button press sequence finished      both   off            (hal_buttons, interrupt)
 *  Factory reset running               red    on until done
 *  Firmware version                    r/g/r  major red, middle green, minor red, 500/500 ms
 *  Motion detected                     green  on, stays on 1.25 s after the last detection
 *  Bad occupancy sensor                green  toggles every sample (fast flicker)
 *  Occupancy sensor stopped            green  off
 *
 * The network status check only blinks red when no other red blink is running;
 * a red blink requested while one is running is ignored (hal_start_flashing_red_led).
 */

#include <stdbool.h>
#include <stdint.h>

#define APP_LEDS_VERSION_BLINK_MS   (500UL)   // firmware version digits: on and off time
#define APP_LEDS_OCCUPANCY_HOLD_MS  (1250UL)  // green stays on this long with no motion

typedef enum
{
  APP_LED_RED,
  APP_LED_GREEN,
} app_led_t;

// Startup
void app_leds_on_startup(void);           // init the LEDs, both on
void app_leds_on_startup_done(void);      // both off
void app_leds_network_status_start(void); // start the network status indication

// Buttons
void app_leds_show_presses(uint8_t presses);
void app_leds_show_fail(void);
void app_leds_show_dim_step(void);

// Factory reset
void app_leds_factory_reset(bool active);

// Firmware version
void app_leds_off(void);
void app_leds_show_version_digit(app_led_t led, uint32_t count);

// Occupancy sensor (called on every sensor sample)
void app_leds_on_occupancy_sample(bool detected);
void app_leds_on_bad_occ_sensor(void);
void app_leds_on_occ_sensor_stopped(void);

#endif /* APP_LEDS_H_ */
