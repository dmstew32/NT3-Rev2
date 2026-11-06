#ifndef NWK_MULTICAST_H_
#define NWK_MULTICAST_H_

#define GROUP_MULTICAST_RADIUS  3   // hops: group multicasts stay close to the sender

bool tx_move_to_new_light_level(uint8_t level, uint16_t xtion_time);
bool tx_occ_sensor_new_delays(uint16_t delay_unoccupied, uint16_t delay_to_off);
bool tx_color_new_temp(uint16_t new_color);
bool tx_new_light_levels(void);
bool tx_dh_level(uint8_t level, uint8_t ramp_ds);
bool tx_dh_master_claim(void);

// Group occupancy (occ_group.h): state is an occ_group_state_t, reason an occ_keepalive_reason_t
bool tx_occ_keepalive(uint16_t time_until_expiration_sec, uint8_t state, uint8_t reason, uint8_t level);
bool tx_occ_level_change(uint8_t state, uint8_t level);

bool tx_sink_info(uint8_t *data, uint8_t length);
sl_status_t broadcast_autocommission_announce(void);

bool tx_occ_sensor_debug_delays(uint16_t delay);

#endif /* NWK_MULTICAST_H_ */
