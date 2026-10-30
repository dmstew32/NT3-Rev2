/*
 * pwm.h
 *
 *  Created on: May 29, 2025
 *      Author: jeff
 */

#ifndef PWMS_PWM_H_
#define PWMS_PWM_H_

#define PWM_TIMER     TIMER2
#define PWM_FREQUENCY (1000UL)
//#define PWM_ACTIVE_
//#define PWM_TIME_TOP (0xFFFE) // This determines PWM frequency

#define PWM_1_PORT  gpioPortA
#define PWM_1_PIN   0
#define PWM_1_CHAN  0

#define PWM_2_PORT  gpioPortA
#define PWM_2_PIN   4
#define PWM_2_CHAN  1

#define RAMP_UPDATE_MS  (100UL)

typedef struct
{
  uint8_t present_pwm_value;    // Commanded value
  uint8_t max_pwm_value;        // Attribute
  uint16_t present_light_temperature;
  float ratio_5000;
  float ratio_3000;
} pwm_params_t;

typedef struct
{
  uint8_t present_pwm_value;
  uint8_t max_pwm_value;
  bool ramp_up;
  bool active;
  float brightness_ramp_step;   // Float, amount to increse/decrease per event
  float running_brightness_pwm_value; // Float, running sum
  int16_t final_brightness_ramp_value;
} pwmInfo_t;

sl_status_t hal_pwm_init( void );
void hal_set_dim_pwm_255(uint8_t value);
uint8_t hal_get_dim_pwm_255(void);

uint32_t hal_get_present_pwm1_value(void);
uint32_t hal_get_present_pwm2_value(void);

uint8_t hal_get_max_pwm_level(void);
uint8_t hal_get_min_pwm_level(void);

void save_light_color(uint16_t temperature_K);
uint16_t get_light_color(void);
bool valid_light_temperature(uint16_t tempK);

void hal_start_pwm1_ramp(uint8_t start_level, uint8_t end_level, uint32_t duration_ms);
void hal_stop_pwm1_ramp(void);
void hal_start_pwm2_ramp(uint8_t start_level, uint8_t end_level, uint32_t duration_ms);
void hal_stop_pwm2_ramp(void);

void init_app_timers(void);

void start_PWM_unit_test(uint32_t mSecs);

void pwm_self_test(void);

#endif /* PWMS_PWM_H_ */
