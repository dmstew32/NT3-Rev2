#ifndef NWK_MGMT_LIGHT_SENSOR_H_
#define NWK_MGMT_LIGHT_SENSOR_H_

#include "hal_light_sensor.h"

typedef struct
{
  uint16_t mean_lux;
  uint16_t last_mean_lux;
  uint8_t pwm_value;
  uint8_t pwm_saturated_count : 3;  // Counter
  uint8_t unused              : 5;
} light_sensor_data_t;

void light_sensor_init(void);
void start_reading_light_sensor(void);
void stop_reading_light_sensor(void);
void execute_light_sensor_logic(light_sensor_info_t *light_sensor_info);

// A network level command: someone chose this level
#define DH_SUSPEND_AFTER_LEVEL_CMD_MS (120000UL)

bool suspend_light_sensor_control(uint32_t delay_ms);
bool suspend_light_sensor_control_dimming(uint32_t delay_ms);

bool allow_light_sensor_control(void);

bool dh_rx_level_command(sl_zigbee_af_cluster_command_t* cmd);
bool dh_rx_master_claim(sl_zigbee_af_cluster_command_t* cmd);
void dh_set_master(bool enable);

uint16_t read_light_sensor_attribute(sl_zigbee_af_attribute_id_t attributeID);
bool write_light_sensor_attribute( sl_zigbee_af_attribute_id_t attributeID, uint16_t data);

void save_light_lux_reference(uint16_t lux_ref);
uint16_t get_light_sensor_mean_lux(void);

uint16_t dhs_get_inhibit_bits(void);

#endif /* NWK_MGMT_LIGHT_SENSOR_H_ */
