#ifndef NWK_MULTICAST_H_
#define NWK_MULTICAST_H_

#define ADAPTIV_MAX_HOPS  3

bool tx_move_to_new_light_level(uint8_t level, uint16_t xtion_time);
bool tx_occ_sensor_new_delays(uint16_t delay_unoccupied, uint16_t delay_to_off);
bool tx_color_new_temp(uint16_t new_color);
bool tx_new_light_levels(void);
bool tx_light_sensor_suspend_time(uint16_t Dsec_time);
bool tx_light_sensor_data(uint8_t *data, uint8_t length);

bool tx_occ_sensor_state(occ_sensor_state_t state, uint32_t time, uint8_t occ_level);

bool tx_sink_info(uint8_t *data, uint8_t length);
sl_status_t broadcast_autocommission_announce(void);

bool tx_occ_sensor_debug_delays(uint16_t delay);

#endif /* NWK_MULTICAST_H_ */
