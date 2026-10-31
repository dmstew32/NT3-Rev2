#include "light_levels.h"

uint8_t light_level_map(uint8_t level, uint8_t min_level, uint8_t max_level)
{
  uint8_t lo = min_level;
  uint8_t hi = max_level;

  if(0 == level)
  {
     return(0);   // off
  }
  if(1 > lo)
  {
     lo = 1;      // level 1 never turns the light off
  }
  if(hi < lo)
  {
     hi = lo;
  }
  if(LIGHT_LEVEL_ZIGBEE_MAX <= level)
  {
     return(hi);
  }
  // Rounded straight line: 1 -> lo, 254 -> hi
  return((uint8_t)(lo + (((uint32_t)(level - 1) * (uint32_t)(hi - lo)) + 126UL) / 253UL));
}

uint8_t light_pwm_share(uint8_t output_level, float ratio)
{
  uint8_t share = (uint8_t)(ratio * (float)output_level);

  if((0 == share) && (0 != output_level) && (0.0F < ratio))
  {
     share = 1;
  }
  return(share);
}

uint8_t light_pwm_floor(uint8_t pwm_value)
{
  if((0 != pwm_value) && (LIGHT_PWM_MIN_STABLE > pwm_value))
  {
     return(LIGHT_PWM_MIN_STABLE);
  }
  return(pwm_value);
}
