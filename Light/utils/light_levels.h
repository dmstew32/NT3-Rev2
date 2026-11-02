#ifndef LIGHT_LEVELS_H_
#define LIGHT_LEVELS_H_

/*
 * Light level arithmetic (no hardware, no stack).
 *
 * Daintree: the Zigbee level (CurrentLevel, Move to Level) 1..255 is mapped
 * onto the min..max levels Daintree wrote (configuration cluster 0xFC01):
 * level 1 = min, 255 = max, straight line between.  Level 0 = off.  The
 * Zigbee attributes keep the values as written / commanded.
 *
 * PWM: the light driver is unstable below LIGHT_PWM_MIN_STABLE, so every
 * channel that is on gets at least that.  0 stays 0 (off).
 */

#include <stdint.h>

#define LIGHT_LEVEL_ZIGBEE_MAX   (255)
#define LIGHT_PWM_MIN_STABLE     (10)

// Daintree: Zigbee level -> light output level
uint8_t light_level_map(uint8_t level, uint8_t min_level, uint8_t max_level);

// One colour channel's share of the output level (ratio 0..1).  A channel
// with a share never rounds down to 0 while the light is on.
uint8_t light_pwm_share(uint8_t output_level, float ratio);

// Value written to a PWM channel: 0 = off, else at least LIGHT_PWM_MIN_STABLE
uint8_t light_pwm_floor(uint8_t pwm_value);

#endif /* LIGHT_LEVELS_H_ */
