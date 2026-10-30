#ifndef ADAPTIV_CONFIG_H_
#define ADAPTIV_CONFIG_H_


sl_status_t tx_adaptiv_response(sl_zigbee_af_cluster_command_t *cmd, uint8_t *payload, uint8_t length, bool unicast);

which_nwk_t get_present_network(void);
void set_present_network(which_nwk_t new_nwk);
void print_which_network(void);

bool write_level_config_attribute(sl_zigbee_af_attribute_id_t attribute, uint8_t level);
bool write_adaptiv_occupancy_delay_attribute(sl_zigbee_af_attribute_id_t attribute, uint16_t delay);
bool write_light_temperature_attribute(uint16_t temperature);

bool adaptiv_configuration_incoming_cmd(sl_zigbee_af_cluster_command_t* cmd);


#endif /* ADAPTIV_CONFIG_H_ */
