/*
 * hal_leds.h
 *
 *  Created on: Aug 2, 2025
 *      Author: jeff
 */

#ifndef HAL_HAL_LEDS_H_
#define HAL_HAL_LEDS_H_


#define LED_FLASH_TIME    (250UL)   /* Used for 1 time flash nwk status etc */

typedef struct
{
  uint32_t n_toggles;
  uint32_t on_ms;
  uint32_t off_ms;
} led_info_t;

void hal_leds_init(void);

void hal_set_red_led(uint8_t on);
void hal_set_green_led(uint8_t on);

void hal_toggle_red_led(void);
void hal_toggle_green_led(void);

bool hal_start_flashing_red_led(uint32_t n_flashes, uint32_t led_on_time_ms, uint32_t led_off_time_ms);
void hal_stop_flashing_red_led(void);
bool is_red_led_free(void);


bool hal_start_flashing_green_led(uint32_t n_flashes, uint32_t led_on_time_ms, uint32_t led_off_time_ms);
void hal_stop_flashing_green_led(void);

#endif /* HAL_HAL_LEDS_H_ */
