#ifndef NWK_MGMT_OCC_SENSOR_H_
#define NWK_MGMT_OCC_SENSOR_H_

#include "hal_occ_sensor.h"

#define OCC_SENSOR_DIM_TO_OFF_MS  (10000UL)
#define OCC_SENSOR_FORCE_CHECK_MS (OCC_SENSOR_DIM_TO_OFF_MS + 1000UL)

#define N_OCCUPANCY_SENSOR_TABLE_SIZE  (10)

typedef enum
{
  eOCCUPIED_OCCUPIED_ON,
  eOCCUPIED_UNOCCUPIED_ON_WAIT,
  eUNOCCUPIED_UNOCCUPIED_MID,
  eUNOCCUPIED_UNOCCUPIED_MID_WAIT,
  eUNOCCUPIED_UNOCCUPIED_OFF_WAIT,
  eUNOCCUPIED_UNOCCUPIED_OFF,

  eOCCUPIED_OCCUPIED_ON_DT,
  eOCCUPIED_UNOCCUPIED_ON_WAIT_DT,
  eUNOCCUPIED_UNOCCUPIED_OFF_DT,

  eOCCUPIED_OCCUPIED_ON_NN,
  eOCCUPIED_UNOCCUPIED_ON_WAIT_NN,
  eUNOCCUPIED_UNOCCUPIED_MID_WAIT_NN,
  eUNOCCUPIED_UNOCCUPIED_OFF_NN,

  eOCCUPENCY_SENSOR_NOP,
  eUNOCCUPIED_UNOCCUPIED_OFF_EM_MODE
} occ_sensor_state_t;

typedef struct
{
  uint16_t macID;
  occ_sensor_state_t state;
  uint8_t present_attr;
} occ_sensor_table_t;
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

void set_occ_sensor_onOff(bool onOff);
bool update_occ_sensor_neighbor_table(uint16_t macID, occ_sensor_state_t state, uint8_t attr, bool reset);

uint16_t read_occupancy_sensor_attribute(sl_zigbee_af_attribute_id_t attributeID);
bool write_occupancy_sensor_attribute( sl_zigbee_af_attribute_id_t attributeID, uint8_t *data);

void set_new_occ_state(sl_zigbee_af_cluster_command_t *cmd);

bool is_occ_mid_or_lower(void);
void spew_occ_state(void);

#endif /* NWK_MGMT_OCC_SENSOR_H_ */
