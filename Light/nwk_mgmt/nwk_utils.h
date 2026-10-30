#ifndef NWK_NWK_UTILS_H_
#define NWK_NWK_UTILS_H_

uint32_t get_random_number(uint32_t max, uint32_t min);
uint8_t get_a_sequence_number(void);
void fill_in_apsFrame(sl_zigbee_af_cluster_command_t* cmd, sl_zigbee_aps_frame_t *apsFrame);

void send_zdo_device_announce(uint8_t capabilities);
void ping_coordinator(void);

uint16_t read_on_off_attribute(uint16_t attributeID);
sl_zigbee_af_status_t write_on_off_attribute(bool onOff);
uint16_t read_light_level_attribute(sl_zigbee_af_attribute_id_t attributeID);
bool write_light_level_attribute(sl_zigbee_af_attribute_id_t attributeID, uint8_t *data);

void check_on_off_level(void);
uint8_t save_present_light_level_to_on_level_attribute(void);
uint8_t get_present_light_level(void);
uint8_t get_on_light_level(void);

bool nwk_comms_timeout(void);
bool nwk_comms_timeout_long_daintree(void);
bool nwk_comms_timeout_short_daintree(void);

void spew_app_info(void);

bool set_app_flags_nonvol(uint32_t bit, bool value);
bool is_app_flags_nonvol_set(uint32_t flag);
uint32_t get_app_flags_nonvol(void);

void adaptiv_disco_request_callback(const sl_zigbee_af_service_discovery_result_t* result);

void init_sink_debug(void);
bool sink_debug_enabled(void);
void sink_setup(uint8_t cmd);
sl_status_t tx_source_debug(uint8_t *data, uint8_t length);
bool rx_sink_debug(sl_zigbee_af_cluster_command_t* cmd);

#endif /* NWK_NWK_UTILS_H_ */
