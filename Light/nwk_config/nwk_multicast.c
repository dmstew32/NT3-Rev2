#include "af.h"

#include "af-main.h"
#include "RehTOS.h"
#include "hal_utils.h"
#include "hal_leds.h"
#include "hal_occ_sensor.h"
#include "sl_event_handler.h"
#include "occ_sensor.h"
#include "nwk_utils.h"
#include "light_on_off.h"
#include "nwk_groups.h"
#include "nwk_multicast.h"
#include "adaptiv_config.h"

#include "sl_zigbee_debug_print.h"

static const sl_zigbee_aps_frame_t _apsFrame = {
     .profileId = HA_PROFILE_ID,
     .clusterId = ZCL_ADAPTIV_CONFIGURATION_CLUSTER_ID,
     .sourceEndpoint = LIGHT_ENDPOINT,//////// NOTE: OCC_SENSOR_ENDPOINT or LIGHT_SENSOR_ENDPOINT
     .destinationEndpoint = SL_ZIGBEE_BROADCAST_ENDPOINT,
     .options =  SL_ZIGBEE_APS_OPTION_NONE,
     .groupId = 0x0000,   /* Set below */
     .radius = ADAPTIV_MAX_HOPS
};

// Common call for individual multicasts
static bool tx_multicast(sl_zigbee_aps_frame_t *apsFrame, uint8_t *message, uint8_t length)
{
  uint8_t apsSequence = 0;
  sl_status_t status = SL_STATUS_OK;

  if(SL_ZIGBEE_JOINED_NETWORK != sl_zigbee_af_network_state())
  {
     return(false);
  }

  // Loop in case multiple groups
  for(uint8_t i = 0; i < get_number_of_multicast_bindings(); i++)
  {
     device_binding_info_t device_binding_info = {0};

     get_multicast_info(i, &device_binding_info);

     if(LIGHT_ENDPOINT != device_binding_info.dst_ep)
     {
        continue;
     }

     apsFrame->groupId = device_binding_info.group_address;

     status = sl_zigbee_send_multicast(apsFrame,
                                       ADAPTIV_MAX_HOPS,         // radius
                                       SL_ZIGBEE_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS,  // broadcastAddr
                                       SL_ZIGBEE_NULL_NODE_ID,   // alias
                                       0,                        // nwkSequence
                                       0x1ADB,                   // messageTag
                                       length,                   // messageLength
                                       message,                  // message
                                       &apsSequence);

     if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != status)
     {
        sl_zigbee_app_debug_println("ERR TXX multicast 0x%X", status);
        return(false);
     }
  }
  return(true);
}

// If this device is in a group(s) will send a multicast to the  group(s).
// Will send a ZCL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID to the group
bool tx_move_to_new_light_level(uint8_t level, uint16_t xtion_time)
{
  uint8_t message[8] = {0};

  sl_zigbee_aps_frame_t apsFrame = _apsFrame;
  apsFrame.clusterId = ZCL_LEVEL_CONTROL_CLUSTER_ID;

  // Don't spread bad info
  occ_info_t occ_information = {0};
  hal_get_occ_sensor_info(&occ_information);
  if(occ_information.b_bad_occ_sensor)
  {
     return(false);
  }

  message[0] = (ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER |
                ZCL_DISABLE_DEFAULT_RESPONSE_MASK);
  message[1] = sl_zigbee_af_next_sequence();
  message[2] = ZCL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID;
  // Payload
  message[3] = level;
  message[4] = GET_LSB_FROM_16(xtion_time);    // xtion time lsb
  message[5] = GET_MSB_FROM_16(xtion_time);
  message[6] = 0;     // Options
  message[7] = 0;

  tx_multicast(&apsFrame, &message[0], sizeof(message));

  return(true);
}

// If this device is in a group(s) will send a multicast to the group(s).
// Send a ZCL_WRITE_ATTRIBUTES_COMMAND_ID to all in the group with new delays
bool tx_occ_sensor_new_delays(uint16_t delay_unoccupied, uint16_t delay_to_off)
{
  uint8_t message[15] = {0};

  sl_zigbee_aps_frame_t apsFrame = _apsFrame;
  apsFrame.clusterId = ZCL_ADAPTIV_CONFIGURATION_CLUSTER_ID;

  message[0] = (ZCL_GLOBAL_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER |
          ZCL_MANUFACTURER_SPECIFIC_MASK | ZCL_DISABLE_DEFAULT_RESPONSE_MASK);
  message[1] = GET_LSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[2] = GET_MSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[3] = sl_zigbee_af_next_sequence();
  message[4] = ZCL_WRITE_ATTRIBUTES_COMMAND_ID;
  // Payload
  message[5] = GET_LSB_FROM_16(ZCL_MFG_UNOCCUPIED_DELAY_ATTRIBUTE_ID);
  message[6] = GET_MSB_FROM_16(ZCL_MFG_UNOCCUPIED_DELAY_ATTRIBUTE_ID),
  message[7] = ZCL_INT16U_ATTRIBUTE_TYPE;
  message[8] = GET_LSB_FROM_16(delay_unoccupied);
  message[9] = GET_MSB_FROM_16(delay_unoccupied);

  message[10] = GET_LSB_FROM_16(ZCL_MFG_DELAY_TO_OFF_ATTRIBUTE_ID);
  message[11] = GET_MSB_FROM_16(ZCL_MFG_DELAY_TO_OFF_ATTRIBUTE_ID),
  message[12] = ZCL_INT16U_ATTRIBUTE_TYPE;
  message[13] = GET_LSB_FROM_16(delay_to_off);
  message[14] = GET_MSB_FROM_16(delay_to_off);

  tx_multicast(&apsFrame, &message[0], sizeof(message));

  return(true);
}

// If this device is in a group(s) will send a multicast to the  group(s).
// Send ZCL_WRITE_ATTRIBUTES_COMMAND_ID; max, mid and min levels to the group
bool tx_new_light_levels(void)
{
  uint8_t message[17] = {0};

  sl_zigbee_aps_frame_t apsFrame = _apsFrame;
  apsFrame.clusterId = ZCL_ADAPTIV_CONFIGURATION_CLUSTER_ID;

  message[0] = (ZCL_GLOBAL_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER |
             ZCL_MANUFACTURER_SPECIFIC_MASK | ZCL_DISABLE_DEFAULT_RESPONSE_MASK);
  message[1] = GET_LSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[2] = GET_MSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[3] = sl_zigbee_af_next_sequence();
  message[4] = ZCL_WRITE_ATTRIBUTES_COMMAND_ID;
  // Payload
  message[5] = GET_LSB_FROM_16(ZCL_MIN_LIGHT_LEVEL_ATTRIBUTE_ID);
  message[6] = GET_MSB_FROM_16(ZCL_MIN_LIGHT_LEVEL_ATTRIBUTE_ID);
  message[7] = ZCL_INT8U_ATTRIBUTE_TYPE;
  message[8] = get_min_light_level();

  message[9] = GET_LSB_FROM_16(ZCL_MID_LIGHT_LEVEL_ATTRIBUTE_ID);
  message[10] = GET_MSB_FROM_16(ZCL_MID_LIGHT_LEVEL_ATTRIBUTE_ID);
  message[11] = ZCL_INT8U_ATTRIBUTE_TYPE;
  message[12] = get_mid_light_level();

  message[13] = GET_LSB_FROM_16(ZCL_MAX_LIGHT_LEVEL_ATTRIBUTE_ID);
  message[14] = GET_MSB_FROM_16(ZCL_MAX_LIGHT_LEVEL_ATTRIBUTE_ID);
  message[15] = ZCL_INT8U_ATTRIBUTE_TYPE;
  message[16] = get_max_light_level();

  tx_multicast(&apsFrame, &message[0], sizeof(message));

  return(true);
}

// Show new state of this occ sensor
bool tx_occ_sensor_state(occ_sensor_state_t state, uint32_t time, uint8_t occ_level)
{
  uint8_t message[9] = {0};

  if(eADAPTIV_NWK != get_present_network())
  {
     return(true);
  }

  sl_zigbee_aps_frame_t apsFrame = _apsFrame;
  apsFrame.clusterId = ZCL_OCCUPANCY_SENSING_CLUSTER_ID;
  sl_802154_long_addr_t thisEui64;

  sl_zigbee_af_get_eui64(thisEui64);

  //message[0] = (ZCL_FRAME_CONTROL_CLIENT_TO_SERVER |
  message[0] = (ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER |
                ZCL_MANUFACTURER_SPECIFIC_MASK | ZCL_DISABLE_DEFAULT_RESPONSE_MASK);
  message[1] = GET_LSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[2] = GET_MSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[3] = sl_zigbee_af_next_sequence();
  message[4] = ZCL_WRITE_OCCUPANCY_SENSOR_COMMAND_ID;
  // Payload
  message[5] = state;
  message[6] = thisEui64[0];
  message[7] = thisEui64[1];
  message[8] = occ_level;

  tx_multicast(&apsFrame, &message[0], sizeof(message));

  return(true);
}

// If this device is in a group(s) will send a multicast to the  group(s).
// Send a ZCL_WRITE_ATTRIBUTES_COMMAND_ID to the color attribute
// NOTE: Does NOT check if in a group or nwk.  Support for stand alone & DT
bool tx_color_new_temp(uint16_t new_color)
{
  uint8_t message[10] = {0};

  sl_zigbee_aps_frame_t apsFrame = _apsFrame;
  apsFrame.clusterId = ZCL_ADAPTIV_CONFIGURATION_CLUSTER_ID;

  message[0] = (ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER |
                ZCL_MANUFACTURER_SPECIFIC_MASK | ZCL_DISABLE_DEFAULT_RESPONSE_MASK);
  message[1] = GET_LSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[2] = GET_MSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[3] = sl_zigbee_af_next_sequence();
  message[4] = ZCL_WRITE_ATTRIBUTES_COMMAND_ID;
  // Payload
  message[5] = GET_LSB_FROM_16(ZCL_COLOR_TEMPERATURE_ATTRIBUTE_ID);
  message[6] = GET_MSB_FROM_16(ZCL_COLOR_TEMPERATURE_ATTRIBUTE_ID),
  message[7] = ZCL_INT16U_ATTRIBUTE_TYPE;
  message[8] = GET_LSB_FROM_16(new_color);
  message[9] = GET_MSB_FROM_16(new_color);

  tx_multicast(&apsFrame, &message[0], sizeof(message));

  return(true);
}

// If this device is in a group(s) will send a multicast to the  group(s).
// Send ZCL_SUSPEND_LIGHT_SENSOR_COMMAND_ID with suspension
bool tx_light_sensor_suspend_time(uint16_t Dsec_time)
{
  uint8_t message[8] = {0};

  if(eADAPTIV_NWK != get_present_network())
  {
     return(true);
  }

  sl_zigbee_aps_frame_t apsFrame = _apsFrame;
  apsFrame.clusterId = ZCL_ILLUM_MEASUREMENT_CLUSTER_ID;

  message[0] = (ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER |
                ZCL_MANUFACTURER_SPECIFIC_MASK | ZCL_DISABLE_DEFAULT_RESPONSE_MASK);
  message[1] = GET_LSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[2] = GET_MSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[3] = sl_zigbee_af_next_sequence();
  message[4] = ZCL_SUSPEND_LIGHT_SENSOR_COMMAND_ID;
  // Payload
  message[5] = GET_LSB_FROM_16(Dsec_time);    // xtion time lsb
  message[6] = GET_MSB_FROM_16(Dsec_time);
  message[7] = 0;     // Options

  tx_multicast(&apsFrame, &message[0], sizeof(message));

  return(true);
}

// This device will braodcast Adaptiv unique info
// Send ZCL_WRITE_LIGHT_SENSOR_COMMAND_ID to ZCL_ILLUM_MEASUREMENT_CLUSTER_ID
bool tx_light_sensor_data(uint8_t *data, uint8_t length)
{
  uint8_t message[24] = {0};

  if(eADAPTIV_NWK != get_present_network())
  {
     return(true);
  }

  sl_zigbee_aps_frame_t apsFrame = _apsFrame;
  apsFrame.clusterId = ZCL_ILLUM_MEASUREMENT_CLUSTER_ID;

  message[0] = (ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER |
                ZCL_MANUFACTURER_SPECIFIC_MASK | ZCL_DISABLE_DEFAULT_RESPONSE_MASK);
  message[1] = GET_LSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[2] = GET_MSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[3] = sl_zigbee_af_next_sequence();
  message[4] = ZCL_WRITE_LIGHT_SENSOR_COMMAND_ID;

  memcpy(&message[5], data, length);

  tx_multicast(&apsFrame, &message[0], length+5);

  return(true);
}

#ifdef DHS_SINK_DEVICE
// This device will broadcast the sink cmd info; addr, on/off etc
bool tx_sink_info(uint8_t *data, uint8_t length)
{
  uint8_t message[12] = {0};

  sl_zigbee_aps_frame_t apsFrame = _apsFrame;
  apsFrame.clusterId = ZCL_ADAPTIV_DEBUG_CLUSTER_ID;

  message[0] = (ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER |
                ZCL_MANUFACTURER_SPECIFIC_MASK | ZCL_DISABLE_DEFAULT_RESPONSE_MASK);
  message[1] = GET_LSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[2] = GET_MSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[3] = sl_zigbee_af_next_sequence();
  message[4] = ZCL_ADAPTIV_DEBUG_COMMAND_ID;
  // Op-code, info, etc.
  memcpy(&message[5], data, length);

  tx_multicast(&apsFrame, &message[0], length+5);

  return(true);
}
#else
bool tx_sink_info(uint8_t *data, uint8_t length) { return(false); }
#endif

// Doing twitch's job
sl_status_t broadcast_autocommission_announce(void)
{
  uint8_t message[17] = {0, };
  sl_zigbee_aps_frame_t apsFrame = _apsFrame;
  apsFrame.radius = ZA_MAX_HOPS;
  sl_802154_short_addr_t this_short_addr = sl_zigbee_af_get_node_id();
  sl_status_t bx_status = SL_STATUS_OK;

  message[0] = (ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER |
                ZCL_MANUFACTURER_SPECIFIC_MASK | ZCL_DISABLE_DEFAULT_RESPONSE_MASK);
  message[1] = GET_LSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[2] = GET_MSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[3] = sl_zigbee_af_next_sequence();
  message[4] = ZCL_ADAPTIV_COMMISH_CMD_ANNOUNCE_GROUP;

  message[5] = true;            // Enable
  message[6] = false;           // Global AC
  message[7] = 1;               // # resets
  message[8] = 10;              // session lsb
  message[9] = 10;
  message[10] = GET_LSB_FROM_16(this_short_addr);   // Group id lsb
  message[11] = GET_MSB_FROM_16(this_short_addr);
  message[12] = 20;            // Timeout secs
  message[13] = 0;
  message[14] = GET_LSB_FROM_16(this_short_addr);   // Group id lsb
  message[15] = GET_MSB_FROM_16(this_short_addr);
  message[16] = LIGHT_ENDPOINT; // Requester ep

  apsFrame.clusterId = ZCL_ADAPTIV_AUTOCOMMISSIONING_CLUSTER_ID;

  bx_status = sl_zigbee_af_send_broadcast(SL_ZIGBEE_NULL_NODE_ID,
                                          SL_ZIGBEE_BROADCAST_ADDRESS,
                                          sl_zigbee_af_next_sequence(),
                                          &apsFrame,
                                          sizeof(message),
                                          (uint8_t*)&message);

  return(bx_status);
}
