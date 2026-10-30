#ifndef NWK_LIGHT_ON_OFF_H_
#define NWK_LIGHT_ON_OFF_H_

void move_to_level_onoff(uint8_t level, uint16_t transitionTimeDs);
void move_to_level_dhs(uint8_t level, uint16_t transitionTimeDs);
void stop_ramp(void);
void init_min_max_silab_levels(void);
bool light_level_active(void);

void power_on_set_light_level(void);
uint16_t read_light_level_attribute(sl_zigbee_af_attribute_id_t attributeID);
void dim_lights_up_down(uint16_t cycles, uint16_t dim_time_Dsecs);
bool are_lights_dimming_up_dowm(void);

void init_light_parameters(void);

bool set_daintree_configuration_cmd(sl_zigbee_af_cluster_command_t* cmd);
bool get_daintree_configuration_cmd(sl_zigbee_af_cluster_command_t* cmd);

void save_min_light_level(uint8_t new_min_level);
void save_mid_light_level(uint8_t new_mid_level);
void save_max_light_level(uint8_t new_max_level);
void save_allow_light_off(bool new_allow_off);

uint8_t get_min_light_level(void);
uint8_t get_mid_light_level(void);
uint8_t get_max_light_level(void);
bool allow_light_off(void);

bool tx_new_light_levels(void);

void fat_finger(void);

bool off_from_switch(void);
void clear_off_from_switch(void);
bool on_from_switch(void);
void clear_on_from_switch(void);


bool app_init_done(void);

#endif /* NWK_LIGHT_ON_OFF_H_ */
