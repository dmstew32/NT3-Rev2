/*
 * The light sensor code fills an array, readings[N_LIGHT_SAMPLES].  The
 * sample rate, LIGHT_SENSOR_HAL_SAMPLE_TIME_MS, is selected so that the
 * readings[] array will be filled in LIGHT_SENSOR_HAL_SAMPLE_TIME_MS.
 * Every LIGHT_SENSOR_HAL_SAMPLE_TIME_MS the execute_light_sensor_logic()
 * is called with new average.
 *
 */
#include <stdlib.h>
#include "af.h"
#include "zigbee_app_framework_event.h"
#include "sl_event_handler.h"
#include "em_i2c.h"
#include "sl_i2cspm.h"
#include "hal_utils.h"
#include "zap-id.h"
#include "hal_light_sensor.h"
#include "light_sensor.h"
#include "RehTOS.h"

#include "sl_zigbee_debug_print.h"

#ifndef HAL_LIGHT_SENSOR_PRINT
#define HAL_LIGHT_SENSOR_PRINT 0    /* Default to 0 */
#elif (HAL_LIGHT_SENSOR_PRINT != 0)
#warning "HAL_LIGHT_SENSOR_PRINT active"
#endif
//#define

static sl_zigbee_af_event_t light_sensor_event;
static light_sensor_info_t light_sensor_info = {0,};

// Fwd declarations
static bool hal_light_sensor_turn_on(void);
static bool hal_light_sensor_set_mode(uint8_t mode);
static uint16_t hal_light_sensor_read_data(void);


// Receives the latest light sensor reading.  Once buffer filled, N_LIGHT_SAMPLES,
// will generate a running average over the last N_LIGHT_SAMPLES readings
// and passes the result to the logic
static void update_light_sensor_task(taskParm_t parm0, taskParm_t parm1)
{
  uint32_t sum = 0UL;
  uint16_t new_reading = parm1.u16;

  removeTask(update_light_sensor_task);   // One shot

  light_sensor_info.readings[light_sensor_info.index] = new_reading;
  light_sensor_info.last_raw = new_reading;
  light_sensor_info.index++;
  light_sensor_info.index &= LIGHT_SAMPLE_MASK;

  if(0 == light_sensor_info.index)
  {
     for(uint8_t i = 0; i < N_LIGHT_SAMPLES; i++)
     {
        sum += (uint32_t)light_sensor_info.readings[i];
     }
     light_sensor_info.last_mean_lux = light_sensor_info.mean_lux;
     light_sensor_info.mean_lux = (uint16_t)(sum/(uint32_t)N_LIGHT_SAMPLES);

     // Start logic outside of ember event
     execute_light_sensor_logic(&light_sensor_info);
  }

  #if (0 != HAL_LIGHT_SENSOR_PRINT)
  sl_zigbee_app_debug_println("Light sensor mean:%d raw:%d   <%d>",
      light_sensor_info.mean_lux, new_reading, get_time_ms());
  for(i = 0; i < N_LIGHT_SAMPLES; i++)
  {
     sl_zigbee_app_debug_print("%d ",light_sensor_info.readings[i]);
  }
  sl_zigbee_app_debug_println("\n");
  #endif
}

// Ticks every LIGHT_SENSOR_HAL_SAMPLE_TIME_MS.  Does 1 reading from the sensor.
// Generates stats via update_light_sensor().  Once the buffer has been filled,
// N_LIGHT_SAMPLES,  call the logic.
static void light_sensor_handler(sl_zigbee_af_event_t *event)
{
  if(light_sensor_info.in_claibration)
  {
     sl_zigbee_af_event_set_delay_ms(&light_sensor_event, LIGHT_SENSOR_CAL_SAMPLE_TIME_MS);
  }
  else
  {
     // Normal
     sl_zigbee_af_event_set_delay_ms(&light_sensor_event, LIGHT_SENSOR_HAL_SAMPLE_TIME_MS);
  }

  // Start average/logic outside of ember event
  taskParm_t arg0, arg1;
  arg0.u32 = get_time_ms();
  arg1.u16 = hal_light_sensor_read_data();
  addTask(update_light_sensor_task, arg0, arg1 );

  #if (0 != HAL_LIGHT_SENSOR_PRINT)
  sl_zigbee_app_debug_println("[%d] %d   <%d>", light_sensor_info.index, reading, get_time_ms());
  #endif
}

void start_reading_light_sensor(void)
{
  //stop_light_sensor();
  light_sensor_info.in_claibration = false;
  sl_zigbee_af_event_set_active(&light_sensor_event);
  sl_zigbee_app_debug_println("Light sensor started normal   <%d>", get_time_ms());
}
void start_reading_light_sensor_for_calibration(void)
{
  light_sensor_info.in_claibration = true;
  sl_zigbee_af_event_set_active(&light_sensor_event);
  sl_zigbee_app_debug_println("Light sensor fast sample   <%d>", get_time_ms());
}
void stop_reading_light_sensor(void)
{
  sl_zigbee_af_event_set_inactive(&light_sensor_event);
  sl_zigbee_app_debug_println("DHS stopped   <%d>", get_time_ms());
}

/*
 * Below functions are the i2c calls
 * This call will initialize and turns on the sensor, set it for high resolution,
 * and start continuous readings at LIGHT_SENSOR_HAL_SAMPLE_TIME_MS.
 */
uint32_t hal_light_sensor_init(void)
{
  memset(&light_sensor_info, 0, sizeof(light_sensor_info_t));

  if(hal_light_sensor_turn_on())
  {
     if(hal_light_sensor_set_mode(0x12))   // Hi res
     {
        // Set up an event
        sl_zigbee_af_event_init(&light_sensor_event, light_sensor_handler);

        // Start continuously reading light sensor
        start_reading_light_sensor();
     }
  }
  return(0);
}

static bool hal_light_sensor_turn_on(void)
{
  uint8_t i2c_buf_0[2] = {0x01, 0x00};  // Set lsb to power on chip
  uint8_t i2c_buf_1[2] = {0};
  I2C_TransferReturn_TypeDef status;
  I2C_TransferSeq_TypeDef write_pwr_on =
  {
     .addr = 0x46,    /* LSB = 0 for R/W bit */
     .flags = I2C_FLAG_WRITE,
     .buf[0].data = i2c_buf_0,
     .buf[0].len = 1,
     .buf[1].data = i2c_buf_1,
     .buf[1].len = 0,
  };

  status = I2CSPM_Transfer(I2C0, &write_pwr_on);

  if(status)
  {
     sl_zigbee_app_debug_println("ERR: i2c turn on %d", status);
     return(false);
  }
  return(true);
}

static bool hal_light_sensor_set_mode(uint8_t mode)
{
  uint8_t i2c_buf_0[2] = {0};
  uint8_t i2c_buf_1[2] = {0};
  I2C_TransferReturn_TypeDef status;
  I2C_TransferSeq_TypeDef write_pwr_on =
  {
     .addr = 0x46,    /* LSB = 0 for R/W bit */
     .flags = I2C_FLAG_WRITE,
     .buf[0].data = i2c_buf_0,
     .buf[0].len = 1,
     .buf[1].data = i2c_buf_1,
     .buf[1].len = 0,
  };
  i2c_buf_0[0] = mode;

  status = I2CSPM_Transfer(I2C0, &write_pwr_on);

  if(status)
  {
     sl_zigbee_app_debug_println("ERR: i2c turn on %d", status);
     return(false);
  }
  return(true);
}

static uint16_t hal_light_sensor_read_data(void)
{
  uint8_t i2c_buf_0[2] = {0};
  uint8_t i2c_buf_1[2] = {0};
  I2C_TransferReturn_TypeDef status;
  I2C_TransferSeq_TypeDef read_data =
  {
     .addr = 0x46,    /* LSB = 0 for R/W bit */
     .flags = I2C_FLAG_READ,
     .buf[0].data = i2c_buf_0,
     .buf[0].len = 2,
     .buf[1].data = i2c_buf_1,
     .buf[1].len = 0,
  };

  status = I2CSPM_Transfer(I2C0, &read_data);
  if(status)
  {
     sl_zigbee_app_debug_println("ERR: i2c turn on %d", status);
     return(0);
  }
  return(((uint16_t)i2c_buf_0[0] << 8) + (uint16_t)i2c_buf_0[1]);
}

uint16_t hal_light_sensor_read_latest(void)
{
  return(light_sensor_info.last_raw);
}
