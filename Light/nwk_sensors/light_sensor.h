#ifndef NWK_MGMT_LIGHT_SENSOR_H_
#define NWK_MGMT_LIGHT_SENSOR_H_

#include "hal_light_sensor.h"

#define N_SENSOR_CAL_READINGS       (4+1)
//#define MAX_LUX_ERROR               (25)
#define MAX_LUX_ERROR               (100)   // John request 6/19/26


typedef struct
{
  uint16_t macID;
  uint16_t lux_high;
  uint16_t lux_low;
  uint8_t pwm;
  uint8_t mode;
} light_sensor_table_t;

typedef struct
{
  uint16_t lux_high_avg;
  uint16_t lux_low_avg;
  uint16_t lux_median;
  uint16_t lux_max;
  uint16_t lux_min;
} light_sensor_stats_t;

typedef struct
{
  uint16_t mean_lux;
  uint16_t last_mean_lux;
  uint8_t pwm_value;
  uint8_t pwm_saturated_count : 3;  // Counter
  uint8_t unused              : 5;
} light_sensor_data_t;

typedef struct
{
  uint16_t macID_mean;
  uint16_t macID_median;
  uint16_t lux_mean;
  uint16_t lux_median;
  int16_t lux_mean_error;
  int16_t lux_median_error;
  uint8_t vote;
} light_sensor_best_fit_t;

typedef struct
{
  uint32_t update_time_stamp;
  uint16_t dim_time_dsec;
  int16_t gain;
  uint8_t pwm_value;
  uint8_t last_pwm_value;
} master_light_sensor_t;

void light_sensor_init(void);
void start_reading_light_sensor(void);
void start_reading_light_sensor_for_calibration(void);
void stop_reading_light_sensor(void);
void execute_light_sensor_logic(light_sensor_info_t *light_sensor_info);

bool suspend_light_sensor_control(uint16_t Dsecs);
bool suspend_light_sensor_control_dimming(uint16_t Dsecs);

bool allow_light_sensor_control(void);

bool update_light_sensor_neighbor_table(sl_zigbee_af_cluster_command_t* cmd);

uint16_t read_light_sensor_attribute(sl_zigbee_af_attribute_id_t attributeID);
bool write_light_sensor_attribute( sl_zigbee_af_attribute_id_t attributeID, uint16_t data);

void save_light_lux_reference(uint16_t lux_ref);
uint16_t get_light_sensor_mean_lux(void);

uint16_t dhs_get_inhibit_bits(void);
void spew_on_dhs_changes(void);

#endif /* NWK_MGMT_LIGHT_SENSOR_H_ */
