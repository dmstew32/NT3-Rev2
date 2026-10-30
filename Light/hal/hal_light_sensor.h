#ifndef LIGHT_SENSOR_H_
#define LIGHT_SENSOR_H_

#ifdef NEW_DHS
// Set sensor timing.
#define LIGHT_SENSOR_UPDATE_AVERAGE_MS   (20000UL)   /* Update for pwm update from master light */
#define N_LIGHT_SAMPLES (8)     /* Must be 2^N */
#define LIGHT_SAMPLE_MASK (N_LIGHT_SAMPLES-1)
#define LIGHT_SENSOR_HAL_SAMPLE_TIME_MS (LIGHT_SENSOR_UPDATE_AVERAGE_MS/N_LIGHT_SAMPLES)  /* New buffer entry every 2.5 secs */

#define LIGHT_SENSOR_CAL_SAMPLE_TIME_MS (500UL)
#define LIGHT_SENSOR_CAL_UPDATE_AVERAGE_MS (N_LIGHT_SAMPLES*LIGHT_SENSOR_CAL_SAMPLE_TIME_MS)  /* 4 secs */

#else
// Set sensor timing.
#define N_LIGHT_SAMPLES (16)     /* Must be 2^N */
#define LIGHT_SENSOR_UPDATE_AVERAGE_MS   (30000UL)   /* Update lux average to light logic */

#define LIGHT_SENSOR_HAL_SAMPLE_TIME_MS (LIGHT_SENSOR_UPDATE_AVERAGE_MS/N_LIGHT_SAMPLES)
#define LIGHT_SENSOR_CAL_SAMPLE_TIME_MS (500UL)

#define LIGHT_SENSOR_BUFFER_FILL_TIME_MS (N_LIGHT_SAMPLES*LIGHT_SENSOR_HAL_SAMPLE_TIME_MS)
#define LIGHT_SENSOR_BUFFER_CAL_FILL_TIME_MS (N_LIGHT_SAMPLES*LIGHT_SENSOR_CAL_SAMPLE_TIME_MS)  /* 8 secs */
#define LIGHT_SAMPLE_MASK (N_LIGHT_SAMPLES-1)
#endif

typedef struct
{
  uint16_t last_raw;
  uint16_t last_mean_lux;
  uint16_t mean_lux;
  uint16_t readings[N_LIGHT_SAMPLES];
  uint8_t index;
  bool in_claibration;
} light_sensor_info_t;

uint32_t hal_light_sensor_init(void);

uint16_t hal_light_sensor_read_latest(void);

#endif /* LIGHT_SENSOR_H_ */
