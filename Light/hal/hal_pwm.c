#include <stdlib.h>
#include "af.h"
// SDK 2025.6.3: hal.h no longer pulls in the legacy token API, which
// provides the TOKEN_APP_* ids and types from sl_custom_token_header.h.
#include "sl_token_api.h"
#include "zigbee_app_framework_event.h"
#include "sl_pwm.h"
#include "sl_pwm_init_pwm_config.h"
#include "sl_token_manager.h"
#include "sl_gpio.h"
#include "sl_zigbee_debug_print.h"
#include "hal_pwm.h"
#include "em_cmu.h"
#include "em_timer.h"
#include "sl_event_handler.h"
#include "light_on_off.h"
#include "nwk_utils.h"
#include "adaptiv_config.h"

/*
 * Cut/paste from email 9/24/25
 *
 * Troffer have PWM1 and PWM2.
 * PWM1 is 5000K,
 * PWM2 is 3000K.
 * PWM1 and PWM2 sum is max 100%.
 * If PWM1 is 100%, PWM2 should be set to 0%.
 * If PWM2 is 100%, PWM1 should be set to 0%.
 * If PWM1 is 50%, PWM2 should be set under 50%.
 * Brightness control is reduce PWM1 and PWM2 at the same time.
 * If PWM1 is 50%, PWM2 is 50% (it is 4000K), Brightness 50% is that PWM1 is 25% and PWM2 is 25% (it is 4000K, 50% brightness)
 */

//#define PWM_PRINTF
//#define PWM_UNIT_TEST
#ifdef PWM_UNIT_TEST
#include "hal_leds.h"
#endif

static uint8_t dim_pwm_value = 0;
static uint32_t top_pwm_counter = 0UL;
static uint16_t present_color_K = DEFAULT_LIGHT_TEMPERATURE_K;
static sl_zigbee_af_event_t ramp_pwm1_event;
static sl_zigbee_af_event_t ramp_pwm2_event;
static uint8_t max_pwm_value = DEFAULT_MAX_LIGHT_LEVEL;
static uint8_t min_pwm_value = DEFAULT_MIN_LIGHT_LEVEL;

static pwm_params_t pwm_params =
{
  .present_pwm_value = 0,
  .present_light_temperature = DEFAULT_LIGHT_TEMPERATURE_K,
  .ratio_5000 = 0.5,
  .ratio_3000 = 0.5,
};

static pwmInfo_t pwm1 =
{
  .present_pwm_value = 0,
  .max_pwm_value = DEFAULT_MAX_LIGHT_LEVEL,
  .active = false,
  .brightness_ramp_step = 0.0F,
  .running_brightness_pwm_value = 0.0F,
  .final_brightness_ramp_value = 0,
};
static pwmInfo_t pwm2 =
{
  .present_pwm_value = 0,
  .max_pwm_value = DEFAULT_MAX_LIGHT_LEVEL,
  .active = false,
  .brightness_ramp_step = 0.0F,
  .running_brightness_pwm_value = 0.0F,
  .final_brightness_ramp_value = 0,
};

/*
 * PWM uses TIMER2 along with CC0 and CC1 for PWM_OUT1 & PWM_OUT2, (schematic).
 * Timer 2 is a 16 bit timer, Input rate is 38.39MHz. PWM_FREQUENCY = 1KHz
 * PWM_OUT1 -> J4-2        PWM_OUT2 -> J4-3          Same as Kimin's NT-2
 * Uses sl_pwm_init.c & sl_pwm.c
 */
sl_status_t hal_pwm_init( void )
{
  sl_pwm_config_t pwm_pwm_config =
  {
    .frequency = PWM_FREQUENCY,
    .polarity = PWM_ACTIVE_HIGH,
  };

  sl_pwm_instance_t sl_pwm_pwm_1 =
  {
    .timer = PWM_TIMER,
    .channel = PWM_1_CHAN,
    .port = PWM_1_PORT,
    .pin = PWM_1_PIN,
  };

  sl_pwm_instance_t sl_pwm_pwm_2 = {
    .timer = PWM_TIMER,
    .channel = PWM_2_CHAN,
    .port = PWM_2_PORT,
    .pin = PWM_2_PIN,
  };

#if 0 // Original

  sl_pwm_init(&sl_pwm_pwm_1, &pwm_pwm_config);
  sl_pwm_init(&sl_pwm_pwm_2, &pwm_pwm_config);

  sl_pwm_start(&sl_pwm_pwm_1);
  sl_pwm_start(&sl_pwm_pwm_2);

  uint16_t temperature = DEFAULT_LIGHT_TEMPERATURE_K;
  sl_token_get_data(TOKEN_APP_COLOR_TEMP_K, 0, &temperature, sizeof(temperature));
  save_light_color(temperature);

  sl_pwm_init(&sl_pwm_pwm_1, &pwm_pwm_config);
  sl_pwm_init(&sl_pwm_pwm_2, &pwm_pwm_config);

  sl_pwm_start(&sl_pwm_pwm_1);
  sl_pwm_start(&sl_pwm_pwm_2);

  sl_pwm_set_duty_cycle(&sl_pwm_pwm_1, DEFAULT_MAX_LIGHT_LEVEL);
  sl_pwm_set_duty_cycle(&sl_pwm_pwm_2, DEFAULT_MAX_LIGHT_LEVEL);

  top_pwm_counter = TIMER_TopGet(PWM_TIMER);

  sl_token_get_data(TOKEN_APP_LIGHT_LEVEL_MAX, 0, &max_pwm_value, sizeof(max_pwm_value));
  sl_token_get_data(TOKEN_APP_LIGHT_LEVEL_MIN, 0, &min_pwm_value, sizeof(min_pwm_value));

  pwm_params.max_pwm_value = max_pwm_value;
#else
  sl_token_get_data(TOKEN_APP_LIGHT_LEVEL_MAX, 0, &max_pwm_value, sizeof(max_pwm_value));
  sl_token_get_data(TOKEN_APP_LIGHT_LEVEL_MIN, 0, &min_pwm_value, sizeof(min_pwm_value));

  pwm_params.max_pwm_value = max_pwm_value;
  dim_pwm_value = max_pwm_value;

  uint16_t temperature = DEFAULT_LIGHT_TEMPERATURE_K;
  sl_token_get_data(TOKEN_APP_COLOR_TEMP_K, 0, &temperature, sizeof(temperature));

  sl_pwm_init(&sl_pwm_pwm_1, &pwm_pwm_config);
  sl_pwm_init(&sl_pwm_pwm_2, &pwm_pwm_config);

  sl_pwm_start(&sl_pwm_pwm_1);
  sl_pwm_start(&sl_pwm_pwm_2);

  top_pwm_counter = TIMER_TopGet(PWM_TIMER);

  //This will set color, level, level attrb and dim_pwm_value
  save_light_color(temperature);
#endif

  #ifdef PWM_PRINTF
  sl_zigbee_app_debug_println("PWM Init: Top 0x%X  Max %d  maxPWM1 %d  maxPWM2 %d",
      top_pwm_counter,pwm_params.max_pwm_value,pwm1.max_pwm_value,pwm2.max_pwm_value);
  #endif

  return SL_STATUS_OK;
}

// Input 0 to 254
static void hal_output_pwm1_255(uint8_t value)
{
  uint32_t compare_value;

  compare_value = (value*top_pwm_counter)/255UL;
  TIMER_CompareBufSet(PWM_TIMER, PWM_1_CHAN, compare_value);
  pwm1.present_pwm_value = value;
  #ifdef PWM_PRINTF
  sl_zigbee_app_debug_println("pwm1 %d", pwm1.present_pwm_value);
  #endif
}

static void hal_output_pwm2_255(uint8_t value)
{
  uint32_t compare_value;

  compare_value = (value*top_pwm_counter)/255UL;
  TIMER_CompareBufSet(PWM_TIMER, PWM_2_CHAN, compare_value);
  pwm2.present_pwm_value = value;
  #ifdef PWM_PRINTF
  sl_zigbee_app_debug_println("pwm2 %d", pwm2.present_pwm_value);
  #endif
}

uint8_t hal_get_dim_pwm_255(void)
{
  return(dim_pwm_value);
}

// Adjust each pwm for color and duty cycle
void hal_set_dim_pwm_255(uint8_t value)
{
  uint8_t max_level = get_max_light_level();

  dim_pwm_value = value;
  // Check for OV
  if(value > max_level)
  {
     dim_pwm_value = max_level;
     value = max_level;
  }

  // Set color ratio
  uint8_t pwm1_value = (uint8_t)(pwm_params.ratio_5000 * (float)value);
  uint8_t pwm2_value = (uint8_t)(pwm_params.ratio_3000 * (float)value);
  if(pwm1_value > pwm1.max_pwm_value)
  {
     pwm1_value = pwm1.max_pwm_value;
  }
  if(pwm2_value > pwm2.max_pwm_value)
  {
     pwm2_value = pwm2.max_pwm_value;
  }

  hal_output_pwm1_255(pwm1_value);
  hal_output_pwm2_255(pwm2_value);
  write_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID, &dim_pwm_value);

#ifdef PWM_PRINTF
  sl_zigbee_app_debug_println("pwm %d  %d  %d", pwm1_value, pwm2_value, pwm1_value+pwm2_value);
#endif
}

uint16_t get_light_color(void)
{
  return(present_color_K);
}

bool valid_light_temperature(uint16_t tempK)
{
  bool valid = true;

  switch(tempK)
  {
     case 3000:
     case 3500:
     case 4000:
     case 4500:
     case 5000:
       break;
     default:
        valid = false;
        break;
  }
  return(valid);
}

// Ratio's calculated from ratio_calc.m
void save_light_color(uint16_t temperature_K)
{
  if(!valid_light_temperature(temperature_K))
  {
     sl_zigbee_app_debug_println("WARNING: Not valid light temperature %d, defaulting to %d",
                                 DEFAULT_LIGHT_TEMPERATURE_K, temperature_K);
     temperature_K = DEFAULT_LIGHT_TEMPERATURE_K;
  }

  // Save local
  present_color_K = temperature_K;

  switch(temperature_K)
  {
     case 3000:
        pwm_params.ratio_5000 = 0.00F;
        pwm_params.ratio_3000 = 1.00F;
        break;
     case 3500:
        pwm_params.ratio_5000 = 0.25F;
        pwm_params.ratio_3000 = 0.75F;
        break;
     case 4000:
        pwm_params.ratio_5000 = 0.50F;
        pwm_params.ratio_3000 = 0.50F;
        break;
     case 4500:
       pwm_params.ratio_5000 = 0.75F;
       pwm_params.ratio_3000 = 0.25F;
       break;
     case 5000:
        pwm_params.ratio_5000 = 1.00F;
        pwm_params.ratio_3000 = 0.00F;
        break;
     default:
        pwm_params.ratio_5000 = 0.50;
        pwm_params.ratio_3000 = 0.50;
        break;
  }
  pwm1.max_pwm_value = (uint8_t)(pwm_params.ratio_5000 * 255.0F);
  pwm2.max_pwm_value = (uint8_t)(pwm_params.ratio_3000 * 255.0F);

  hal_set_dim_pwm_255(dim_pwm_value);

  sl_token_set_data(TOKEN_APP_COLOR_TEMP_K, 0, &present_color_K, sizeof(present_color_K));
  write_light_temperature_attribute(present_color_K);

  sl_zigbee_app_debug_println("New Color: %dK    (pwm1_max=%d pwm_2max=%d)",
                     present_color_K, pwm1.max_pwm_value, pwm2.max_pwm_value);

}

uint32_t hal_get_present_pwm1_value(void)
{
  return(pwm1.present_pwm_value);
}
uint32_t hal_get_present_pwm2_value(void)
{
  return(pwm2.present_pwm_value);
}

uint8_t hal_get_max_pwm_level(void)
{
  return(max_pwm_value);
}
uint8_t hal_get_min_pwm_level(void)
{
  return(min_pwm_value);
}

static void ramp_pwm1_handler(sl_zigbee_af_event_t *event)
{
  int16_t next_pwm_value;

  sl_zigbee_af_event_set_delay_ms(&ramp_pwm1_event, RAMP_UPDATE_MS);

  if(pwm1.ramp_up)
  {
     pwm1.running_brightness_pwm_value += pwm1.brightness_ramp_step;  // Float
     next_pwm_value = (int16_t)pwm1.running_brightness_pwm_value;
     if(next_pwm_value >= pwm1.final_brightness_ramp_value)
     {
        next_pwm_value = pwm1.final_brightness_ramp_value;
        hal_stop_pwm1_ramp();
     }
  }
  else
  {
     pwm1.running_brightness_pwm_value -= pwm1.brightness_ramp_step;  // Float
     next_pwm_value = (int16_t)pwm1.running_brightness_pwm_value;
     if(next_pwm_value <= pwm1.final_brightness_ramp_value)
     {
        next_pwm_value = pwm1.final_brightness_ramp_value;
        hal_stop_pwm1_ramp();
     }
  }
  hal_output_pwm1_255((uint8_t)next_pwm_value);
  #ifdef PWM_PRINTF
  sl_zigbee_app_debug_println("PWM1 ramp %d  %3.3f", next_pwm_value,
                                 pwm1.running_brightness_pwm_value);
  #endif
}

// Ramps up or down, updates at a constant RAMP_UPDATE_MS
// Input levels are 0 - 254
void hal_start_pwm1_ramp(uint8_t start_level, uint8_t end_level, uint32_t duration_ms)
{
  uint8_t delta;
  float n_events;

  duration_ms = (RAMP_UPDATE_MS <= duration_ms) ? duration_ms : RAMP_UPDATE_MS;

  pwm1.final_brightness_ramp_value = (int16_t)end_level;
  pwm1.active = true;

  if(start_level > end_level)
  {
     pwm1.ramp_up = false;
     delta = start_level - end_level;
  }
  else if(start_level < end_level)
  {
     pwm1.ramp_up = true;
     delta = end_level - start_level;
  }
  else  // ==
  {
     hal_output_pwm1_255((uint8_t)pwm1.final_brightness_ramp_value);
     pwm1.active = false;
     return;
  }

  n_events = (float)duration_ms/(float)RAMP_UPDATE_MS;
  pwm1.brightness_ramp_step = (float)delta/n_events;
  pwm1.running_brightness_pwm_value = (float)start_level;

  sl_zigbee_af_event_init(&ramp_pwm1_event, ramp_pwm1_handler);
  sl_zigbee_af_event_set_active(&ramp_pwm1_event);
  #ifdef PWM_PRINTF
  sl_zigbee_app_debug_println("Start pwm1 ramp: %d->%d duration %dmS  events:%3.2f step:%3.2f",
                  start_level, end_level, duration_ms, n_events, pwm1.brightness_ramp_step);
  #endif
}
void hal_stop_pwm1_ramp(void)
{
  sl_zigbee_af_event_set_inactive(&ramp_pwm1_event);
  pwm1.running_brightness_pwm_value = 0.0;
  pwm1.active = false;
}



static void ramp_pwm2_handler(sl_zigbee_af_event_t *event)
{
  int16_t next_pwm_value;

  sl_zigbee_af_event_set_delay_ms(&ramp_pwm2_event, RAMP_UPDATE_MS);

  if(pwm2.ramp_up)
  {
     pwm2.running_brightness_pwm_value += pwm2.brightness_ramp_step;  // Float
     next_pwm_value = (int16_t)pwm2.running_brightness_pwm_value;
     if(next_pwm_value >= pwm2.final_brightness_ramp_value)
     {
        next_pwm_value = pwm2.final_brightness_ramp_value;
        hal_stop_pwm2_ramp();
     }
  }
  else
  {
     pwm2.running_brightness_pwm_value -= pwm2.brightness_ramp_step;  // Float
     next_pwm_value = (int16_t)pwm2.running_brightness_pwm_value;
     if(next_pwm_value <= pwm2.final_brightness_ramp_value)
     {
        next_pwm_value = pwm2.final_brightness_ramp_value;
        hal_stop_pwm2_ramp();
     }
  }
  hal_output_pwm2_255((uint8_t)next_pwm_value);
  #ifdef PWM_PRINTF
  sl_zigbee_app_debug_println("pwm2 ramp %d  %3.3f", next_pwm_value,
                                 pwm2.running_brightness_pwm_value);
  #endif
}

// Ramps up or down, updates at a constant RAMP_UPDATE_MS
void hal_start_pwm2_ramp(uint8_t start_level, uint8_t end_level, uint32_t duration_ms)
{
  uint8_t delta;
  float n_events;

  duration_ms = (RAMP_UPDATE_MS <= duration_ms) ? duration_ms : RAMP_UPDATE_MS;

  pwm2.final_brightness_ramp_value = (int16_t)end_level;
  pwm2.active = true;

  if(start_level > end_level)
  {
     pwm2.ramp_up = false;
     delta = start_level - end_level;
  }
  else if(start_level < end_level)
  {
     pwm2.ramp_up = true;
     delta = end_level - start_level;
  }
  else  // ==
  {
     hal_output_pwm2_255((uint8_t)pwm2.final_brightness_ramp_value);
     pwm2.active = false;
     return;
  }

  n_events = (float)duration_ms/(float)RAMP_UPDATE_MS;
  pwm2.brightness_ramp_step = (float)delta/n_events;
  pwm2.running_brightness_pwm_value = (float)start_level;

  sl_zigbee_af_event_init(&ramp_pwm2_event, ramp_pwm2_handler);
  sl_zigbee_af_event_set_active(&ramp_pwm2_event);
  #ifdef PWM_PRINTF
  sl_zigbee_app_debug_println("Start pwm2 ramp: %d->%d duration %dmS  events:%3.2f step:%3.2f",
                  start_level, end_level, duration_ms, n_events, pwm2.brightness_ramp_step);
  #endif
}
void hal_stop_pwm2_ramp(void)
{
  sl_zigbee_af_event_set_inactive(&ramp_pwm2_event);
  pwm2.running_brightness_pwm_value = 0.0;
  pwm2.active = false;
}

#ifdef PWM_UNIT_TEST
#define DURATION (20000UL)
#if 1
void pwm_self_test(void)
{
  static bool ramp_up = true;

  if(pwm1.active || pwm2.active)
  {
    return;
  }

  if(ramp_up)
  {
     hal_start_pwm1_ramp(0, 128, DURATION);
     hal_start_pwm2_ramp(0, 128, DURATION);
     ramp_up = false;
  }
  else
  {
     hal_start_pwm1_ramp(128, 0, DURATION);
     hal_start_pwm2_ramp(128, 0, DURATION);
     ramp_up = true;
  }

}


#else
void pwm_self_test(void)
{
  static uint8_t active_pwm = 1;
  uint8_t level[2] = {0, 254};

  // Wait till open.  Only do 1 pwm at a time
  if(pwm1.active || pwm2.active)
  {
    return;
  }
  hal_stop_flashing_green_led();
  hal_stop_flashing_red_led();

  // Select next pwm to test
  //active_pwm = (2 == active_pwm) ? 1 : 2;

  // Both pwm's done
  if(1 == active_pwm)
  {
     if(128 < hal_get_present_pwm1_value())
     {
        hal_start_pwm1_ramp(level[1], level[0], DURATION);
        hal_start_flashing_red_led2UL, 250UL, 250UL);
     }
     else
     {
        hal_start_pwm1_ramp(level[0], level[1], DURATION);
        hal_start_flashing_green_led(DURATION+250UL, 250UL, 0);
     }
  }
  else if(2 == active_pwm)
  {
     if(128 < hal_get_present_pwm2_value())
     {
        hal_start_pwm2_ramp(level[1], level[0], DURATION);
        hal_start_flashing_red_led(DURATION+250UL, 250UL, 0);
     }
     else
     {
        hal_start_pwm2_ramp(level[0], level[1], DURATION);
        hal_start_flashing_green_led(DURATION+250UL, 250UL, 0);
     }
  }
  else
  {
     // ????
  }
}
#endif
#else // PWM_UNIT_TEST
void pwm_self_test(void) {}

#endif

/*** TIMERS ***/
void init_app_timers(void)
{
  sl_zigbee_app_debug_println("init_app_timers %d", CMU->EM01GRPACLKCTRL);
  //CMU->EM01GRPACLKCTRL = CMU_EM01GRPACLKCTRL_CLKSEL_HFXO;
}

