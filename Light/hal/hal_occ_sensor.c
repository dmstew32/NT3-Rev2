#include "app/framework/include/af.h"
#include "sl_clock_manager.h"
#include "sl_token_manager.h"
#include "em_gpio.h"
#include "RehTOS.h"
#include "hal_utils.h"
#include "hal_leds.h"
#include "hal_occ_sensor.h"
#include "occ_sensor.h"
#include "adaptiv_config.h"
#include "nwk_utils.h"

#include "sl_zigbee_debug_print.h"

// Occ low level: 0 = nothing  1 = slow print  2 = all data  3 = LED
#define HAL_OCC_PRINT  (0)

/*
 * The low level occ sensor, when active, will use the silab event mechanism
 * for a periodic reading of the sensor every OCC_SENSOR_PERIOD.
 * Reading done in occ_sensor_handler().  Handler kicks off another timer
 * interrupt which bit bangs the single data line of the sensor.
 */

static occ_info_t occ_info;
static uint16_t _unoccupied_delay_secs;
static uint16_t _unoccupied_delay_to_off_secs;

static sl_zigbee_af_event_t occ_sensor_event;
static void occ_sensor_handler(sl_zigbee_af_event_t *event);
static void pir_delay(uint8_t count);


void hal_init_occ_sensor(void)
{
  uint8_t data[2] = {1};
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO); // Redundant?
  GPIO_PinModeSet (OCC_SENSOR_PORT, OCC_SENSOR_PIN, gpioModePushPull, 0UL);
  memset(&occ_info, 0, sizeof(occ_info_t));

  write_occupancy_sensor_attribute(ZCL_OCCUPANCY_ATTRIBUTE_ID,
                                            (uint8_t *)&data);

  // Set occupancy sensor threshold from nvm.  NOTE: from attribute?
  sl_token_get_data(TOKEN_APP_OCC_THRESHOLD, 0, &data, 2);
  occ_info.occ_threshold =  GET_16_FROM_ARRAY(data, 0);
  write_occupancy_sensor_attribute(ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_ATTRIBUTE_ID, (uint8_t *)&data[0]);

  sl_zigbee_af_event_init(&occ_sensor_event, occ_sensor_handler);
}

void hal_set_occ_sensor_info(occ_info_t *occ_information)
{
  occ_info = *occ_information;

#if (1 <= HAL_OCC_PRINT)
     sl_zigbee_app_debug_println("OCC threshold %d",occ_info.occ_threshold);
#endif
}
void hal_get_occ_sensor_info(occ_info_t *occ_information)
{
  *occ_information = occ_info;
}

void hal_set_long_delay_bit(uint8_t new_long_delay)
{
  occ_info.b_long_delay = new_long_delay;
}

// Bit bang
static void output_pir_line(uint8_t on)
{
  if(0 != on)
  {
     // Force line high with active push-pull
     BUS_RegMaskedWrite(&(GPIO->P[OCC_SENSOR_PORT].MODEL), 0xFu << (OCC_SENSOR_PIN * 4), (uint32_t)gpioModePushPull << (OCC_SENSOR_PIN * 4));
     GPIO_PinOutSet(OCC_SENSOR_PORT, OCC_SENSOR_PIN); // inline call

     pir_delay(7); // Set high time

     // Now put line into high-Z, for input
     BUS_RegMaskedWrite(&(GPIO->P[OCC_SENSOR_PORT].MODEL), 0xFu << (OCC_SENSOR_PIN * 4), (uint32_t)gpioModeInput << (OCC_SENSOR_PIN * 4));
  }
  else
  {
     // Force line low with active push-pull, leave in active push-pull
     GPIO_PinModeSet (OCC_SENSOR_PORT, OCC_SENSOR_PIN, gpioModePushPull, 0UL);
  }
}

static uint32_t read_pir_line(void)
{
  return(GPIO_PinInGet(OCC_SENSOR_PORT, OCC_SENSOR_PIN));
}

static void pir_delay(uint8_t count)
{
  while(count)
  {
     __NOP();
     count--;
  }
}
#if 1 // JGR
// This is in the TIMER1 isr.  Most other isrs are disabled, therefore
// interrupts are held off for ~175uS (70)    ~80uS (15)
// It is initiated in occ_sensor_handler(), occurs every OCC_SAMPLE_PERIOD_MS
static void hal_occ_sensor_read_isr(void)
{
  static uint8_t occ_isr_counter = 0;
  uint8_t bits = 14;        // Read occ data, no temp
  uint32_t raw_data = 0;    // Only the pir data is read, no temp

  if(OCC_READ_TEMPERATURE_N <= occ_isr_counter++)
  {
     occ_isr_counter = 0;
     bits = 28;
  }

  output_pir_line(0);       // Start bit

  // Start reading data
  do
  {
     output_pir_line(1);    // Force high then high-Z
     pir_delay(70);         // Empirically set, sets bit time
     raw_data <<= 1;
     raw_data |= read_pir_line();
     output_pir_line(0);    // Force low and stay low
     bits--;
  } while(bits);

  output_pir_line(0);

  if(occ_isr_counter)
  {
     // Pir only, 14 bits
     occ_info.pir_data[occ_info.index] = (uint16_t)(raw_data & 0x03FFFUL);
     occ_info.present_pir_data = occ_info.pir_data[occ_info.index];
  }
  else
    {
      // Temperature & pir, 28 bits
      occ_info.pir_temperature = (uint16_t)(raw_data & 0x03FFFUL);
      occ_info.pir_data[occ_info.index] = (uint16_t)((raw_data >> 14) & 0x03FFFUL);
      occ_info.present_pir_data = occ_info.pir_data[occ_info.index];
    }
}
#else
// This is in the TIMER1 isr.  Most other isrs are disabled, therefore
// interrupts are held off for ~175uS (70)    ~80uS (15)
static void hal_occ_sensor_read_isr(void)
{
  uint8_t bits = 14;        // Read only occ data, no temp
  uint16_t raw_data = 0;    // Only the pir data is read, no temp

  output_pir_line(0);       // Start bit

  // Start reading data
  do
  {
     output_pir_line(1);    // Force high then high-Z
     pir_delay(70);         // Empirically set, sets bit time
//     pir_delay(140);         // Empirically set
     raw_data <<= 1;
     raw_data |= read_pir_line();
     output_pir_line(0);    // Force low and stay low
     bits--;
  } while(bits);

  output_pir_line(0);

  occ_info.pir_data[occ_info.index] = raw_data & 0x3FFF;
  occ_info.present_pir_data = raw_data;
}
#endif

/*
 * This does some house keeping; generates average, delta reading, and index.
 * Then kicks off a timer isr that does the read, timing for occ sensor is
 * very tight and we have to bit bang, (not SPI or I2C).
 * Sample time is OCC_SAMPLE_PERIOD_MS
 */
static void occ_sensor_handler(sl_zigbee_af_event_t *event)
{
  static uint32_t occ_led_timeStamp = 0UL;
  uint8_t index = occ_info.index;
  uint32_t sum = 0;

  if((OCC_BAD_LEVEL_LOW < occ_info.pir_data[index]) &&
        (OCC_BAD_LEVEL_HIGH > occ_info.pir_data[index]))
  {
     occ_info.b_bad_occ_sensor = false;

     // Generate running average
     for(uint8_t i = 0; i < N_OCC_ARRAY; i++)
     {
        sum += (uint32_t)occ_info.pir_data[i];
     }
     occ_info.running_average = sum >> SH_OCC_ARRAY;

     occ_info.delta_pir_data = abs((int32_t)occ_info.pir_data[index]
                                 - (int32_t)occ_info.running_average);

     // Occupancy?
     occ_info.b_occ_detected = (occ_info.delta_pir_data >= occ_info.occ_threshold)
                                                              ? true : false;
     // Force event?  Resets timers and states
     occ_info.b_occ_detected |= occ_info.b_force_occ_event;

     #if 0
     sl_zigbee_app_debug_println("%d %d %d %d %d",occ_info.pir_data[index],
                                                 occ_info.running_average,
                                                 occ_info.delta_pir_data,
                                                 occ_info.b_occ_detected,
                                                 get_time_ms());
     #endif
     #if 0
     sl_zigbee_app_debug_println("%d %d %d",occ_info.pir_data[index],
                                                 occ_info.running_average,
                                                 occ_info.b_occ_detected);
     #endif
     #if 0
     sl_zigbee_app_debug_println("Temp %d",occ_info.pir_temperature);
     #endif

     occ_info.index++;
     occ_info.index &= M_OCC_ARRAY;

     // LED behavior
     if(occ_info.b_occ_detected)
     {
        hal_set_green_led(1);
        occ_led_timeStamp = get_time_ms();
     }
     else if(has_timed_out(occ_led_timeStamp, OCC_LED_DELAY))
     {
        hal_set_green_led(0);
     }
     else
     {
     }

  }
  else
  {
     // Bad sensor
     occ_info.b_occ_detected = false;
     occ_info.b_bad_occ_sensor = true;
     hal_toggle_green_led();
  }

  occ_sensor_logic((bool)occ_info.b_occ_detected);

  // Set next event
  sl_zigbee_af_event_set_delay_ms(&occ_sensor_event, OCC_SAMPLE_PERIOD_MS);
  occ_info.b_force_occ_event = false;

  // Start next reading, issue a start pulse
  output_pir_line(1);
  set_one_shot_us_timer_isr(hal_occ_sensor_read_isr, OCC_START_PULSE_US);

  #if (HAL_OCC_PRINT == 1)
  {
     static uint32_t counter = 0UL;

     if(2000UL/OCC_SAMPLE_PERIOD_MS <= counter++)  // Spew every 2 seconds
     {
        sl_zigbee_app_debug_println("occ %d   <%d>",  occ_info.pir_data[index], get_time_ms());
        counter = 0UL;
     }
  }
  #elif (HAL_OCC_PRINT == 2)

        sl_zigbee_app_debug_println("%d %d %d",occ_info.pir_data[index],
                                               occ_info.running_average,
                                               occ_info.delta_pir_data);
  #elif (HAL_OCC_PRINT == 3)
        //hal_set_green_led(occ_info.b_occ_detected);
  #endif  // HAL_OCC_PRINT
}

// Enable occ sensor for this light
void hal_start_occ_sensor(void)
{
  if(!sl_zigbee_af_event_is_scheduled(&occ_sensor_event))
  {
     sl_zigbee_af_event_set_active(&occ_sensor_event);
     sl_zigbee_app_debug_println("Occ Sensor ON");
  }
}

// Disable occ sensor for this light
void hal_stop_occ_sensor(void)
{
  uint8_t data[1] = {1};
  sl_zigbee_af_event_set_inactive(&occ_sensor_event);
  output_pir_line(0);
  occ_info.b_occ_detected = false;
  occ_info.b_last_occ_detected = false;
  occ_info.b_force_occ_event = false;
  hal_set_green_led(0);
  write_occupancy_sensor_attribute(ZCL_OCCUPANCY_ATTRIBUTE_ID,
                                              (uint8_t *)&data);
  sl_zigbee_app_debug_println("Occ Sensor OFF");
}

// If in long delay, restore back to original delay
void restore_unoccupied_delay(void)
{
  if(is_app_flags_nonvol_set(B_OCC_XXL_DELAY_ENABLED))
  {
     uint16_t delay;
     set_app_flags_nonvol(B_OCC_XXL_DELAY_ENABLED, 0);
     sl_token_get_data(TOKEN_APP_UNOCCUPIED_DELAY, 0, &delay, sizeof(delay));
     save_adaptiv_unoccupied_delay(delay);
  }
}

// Save to non-vol, local var, Adaptiv attr check if 0
void save_adaptiv_unoccupied_delay(uint16_t new_unoccupied_delay)
{
  // Save local variable and write novol
  _unoccupied_delay_secs = new_unoccupied_delay;

  if(DEFAULT_OCC_SENSOR_XXL_DELAY <= _unoccupied_delay_secs)
  {
      set_app_flags_nonvol(B_OCC_XXL_DELAY_ENABLED, 1);
  }
  else
  {
     sl_token_set_data(TOKEN_APP_UNOCCUPIED_DELAY, 0, &_unoccupied_delay_secs, sizeof(_unoccupied_delay_secs));
  }

  // Adaptive mfg attr
  if(write_adaptiv_occupancy_delay_attribute(ZCL_MFG_UNOCCUPIED_DELAY_ATTRIBUTE_ID, _unoccupied_delay_secs))
  {
     sl_zigbee_app_debug_println("Unoccupied delay nvm = %d", _unoccupied_delay_secs);
  }

  // Shut down occ sensor?
  if(0 == new_unoccupied_delay)
  {
     hal_stop_occ_sensor();
  }
  else
  {
     hal_start_occ_sensor();
  }
}

void save_adaptiv_unoccupied_delay_to_off(uint16_t new_unoccupied_delay_to_off)
{
  // Save local variable and write novol
  _unoccupied_delay_to_off_secs = new_unoccupied_delay_to_off;
  sl_token_set_data(TOKEN_APP_UNOCCUPIED_DELAY_TO_OFF, 0, &_unoccupied_delay_to_off_secs, sizeof(_unoccupied_delay_to_off_secs));

  if(write_adaptiv_occupancy_delay_attribute(ZCL_MFG_DELAY_TO_OFF_ATTRIBUTE_ID, _unoccupied_delay_to_off_secs))
  {
     sl_zigbee_app_debug_println("Unoccupied delay to off nvm = %d", _unoccupied_delay_to_off_secs);
  }
}

uint16_t get_unoccupied_delay(void)
{
  return(_unoccupied_delay_secs);
}
uint16_t get_unoccupied_delay_to_off(void)
{
  return(_unoccupied_delay_to_off_secs);
}

void force_an_occupied_event(void)
{
  occ_info.b_force_occ_event = 1;
  sl_zigbee_app_debug_println("OCC Forced occupied   <%d>", get_time_ms());
}

void set_occupancy_threshold(uint16_t new_threshold)
{
  uint8_t data[2] = {0};

  if(10 > new_threshold)
  {
      return;
  }
  data[0] = GET_LSB_FROM_16(new_threshold);
  data[1] = GET_MSB_FROM_16(new_threshold);
  sl_token_set_data(TOKEN_APP_OCC_THRESHOLD, 0, &data[0], 2);

  occ_info.occ_threshold = new_threshold;
  write_occupancy_sensor_attribute(ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_ATTRIBUTE_ID, (uint8_t *)&data[0]);

  sl_zigbee_app_debug_println("OCC New threshold %d", occ_info.occ_threshold);
}

uint16_t get_occupancy_threshold(void)
{
  return(occ_info.occ_threshold);
}
