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
 *
 * Red priority:  factory reset (held on)  >  button feedback, firmware version
 *                >  network status.
 *   Button feedback replaces a network status blink (red off 500 ms first);
 *   feedback requested while other feedback runs is ignored; nothing else
 *   touches red during a factory reset.  The network status check only blinks
 *   when red is free.
 * Green priority: green flash  >  lux reading  >  occupancy.
 *   Every light sensor reading is taken with green off (at most ~185 ms);
 *   occupancy resumes when a flash or reading ends.  While green is lit
 *   that happens once per 20 s light sensor average (hal_light_sensor.c).
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

// Light sensor: hold green off for a reading.  false = a green flash is running,
// try again later.  app_leds_green_lux_done() restores green.
bool app_leds_green_off_for_lux(void);
void app_leds_green_lux_done(void);
// Green is showing something: occupancy or a green flash
bool app_leds_green_is_lit(void);

#endif /* APP_LEDS_H_ */
