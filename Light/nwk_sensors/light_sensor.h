#ifndef NWK_MGMT_LIGHT_SENSOR_H_
#define NWK_MGMT_LIGHT_SENSOR_H_

#include "hal_light_sensor.h"

typedef struct
{
  uint16_t mean_lux;     // 20 s average, calibrated
} light_sensor_data_t;

void light_sensor_init(void);
void start_reading_light_sensor(void);
void stop_reading_light_sensor(void);    // Only the unit tests use it
void execute_light_sensor_logic(light_sensor_info_t *light_sensor_info);

bool suspend_light_sensor_control(uint32_t delay_ms);

bool allow_light_sensor_control(void);

bool dh_rx_level_adjust(sl_zigbee_af_cluster_command_t* cmd);
bool dh_rx_master_claim(sl_zigbee_af_cluster_command_t* cmd);
void dh_set_master(bool enable);
void dh_disable(void);
void dh_enable(void);
void light_sensor_on_reading(uint16_t reading);

bool write_light_sensor_attribute( sl_zigbee_af_attribute_id_t attributeID, uint16_t data);

void save_light_lux_reference(uint16_t lux_ref);
uint16_t get_light_sensor_mean_lux(void);

uint16_t dhs_get_inhibit_bits(void);

#endif /* NWK_MGMT_LIGHT_SENSOR_H_ */
