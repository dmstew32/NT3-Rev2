#ifndef LIGHT_SENSOR_H_
#define LIGHT_SENSOR_H_

// Sensor timing
#define LIGHT_SENSOR_UPDATE_AVERAGE_MS   (20000UL)   /* 20 s average: illuminance attribute */
#define N_LIGHT_SAMPLES (8)     /* Must be 2^N */
#define LIGHT_SAMPLE_MASK (N_LIGHT_SAMPLES-1)
#define LIGHT_SENSOR_HAL_SAMPLE_TIME_MS (LIGHT_SENSOR_UPDATE_AVERAGE_MS/N_LIGHT_SAMPLES)  /* A reading every 2.5 secs */

typedef struct
{
  uint16_t last_raw;
  uint16_t mean_lux;
  uint16_t readings[N_LIGHT_SAMPLES];
  uint8_t index;
} light_sensor_info_t;

uint32_t hal_light_sensor_init(void);

uint16_t hal_light_sensor_read_latest(void);    // Only the unit tests use it

#endif /* LIGHT_SENSOR_H_ */
