#ifndef NWK_MGMT_OCC_SENSOR_H_
#define NWK_MGMT_OCC_SENSOR_H_

#include "hal_occ_sensor.h"

// Daintree and stand alone / no network.  Adaptiv grouped lights: occ_group.h
typedef enum
{
  eOCCUPIED_OCCUPIED_ON_DT,
  eOCCUPIED_UNOCCUPIED_ON_WAIT_DT,
  eUNOCCUPIED_UNOCCUPIED_OFF_DT,

  eOCCUPIED_OCCUPIED_ON_NN,
  eOCCUPIED_UNOCCUPIED_ON_WAIT_NN,
  eUNOCCUPIED_UNOCCUPIED_MID_WAIT_NN,
  eUNOCCUPIED_UNOCCUPIED_OFF_NN,

  eOCC_SENSOR_ADAPTIV,     // Adaptiv: state is in occ_group
} occ_sensor_state_t;

typedef struct
{
  bool present_occupied_value;
  bool last_occupied_value;
  bool did_occ_sensor_change;
  uint32_t time_stamp_unoccupied_to_occupied;
  uint32_t time_stamp_occupied_to_unoccupied;
} occupancy_sensor_info_t;

void occ_sensor_init(void);
void occ_sensor_init_task(taskParm_t parm0, taskParm_t parm1);

void occ_sensor_logic(bool occupied);


uint16_t read_occupancy_sensor_attribute(sl_zigbee_af_attribute_id_t attributeID);
bool write_occupancy_sensor_attribute( sl_zigbee_af_attribute_id_t attributeID, uint8_t *data);

// Group occupancy keepalive / level change from another light (Adaptiv)
void occ_sensor_rx_group_cmd(sl_zigbee_af_cluster_command_t *cmd);
// A switch (network level command) left the light at this level: occupancy wakes to it
void occ_sensor_set_switch_level(uint8_t level);

bool is_occ_mid_or_lower(void);
// At mid from an occupancy dim: an off command must not save mid as the on level
bool occ_dimmed_to_mid(void);
void spew_occ_state(void);

#endif /* NWK_MGMT_OCC_SENSOR_H_ */
