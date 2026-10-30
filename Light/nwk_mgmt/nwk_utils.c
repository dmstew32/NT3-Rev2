#include "af.h"

#include "af-main.h"
#include "RehTOS.h"
#include "factory_reset.h"
#include "hal_leds.h"
#include "hal_utils.h"
#include "hal_buttons.h"
#include "hal_occ_sensor.h"

#include "em_chip.h"
#include "hal_pwm.h"
#include "sl_event_handler.h"
#include "light_on_off.h"
#include "sl_hal_wdog.h"
#include "sl_token_manager.h"
#include "occ_sensor.h"
#include "basic.h"
#include "attribute-storage.h"
#include "auto_comm.h"
#include "nwk_utils.h"
#include "nwk_commission.h"
#include "reporting.h"
#include "light_on_off.h"
#include "light_sensor.h"
#include "adaptiv_config.h"
#include "nwk_multicast.h"
#include "nwk_groups.h"


#include "sl_zigbee_debug_print.h"

static uint32_t time_of_last_cmd = 0UL;
static uint8_t local_sequence_number = 0;


/***************************************************************************/
/*********************************   MISC  *********************************/
/***************************************************************************/
// Return random number between min - max
uint32_t get_random_number(uint32_t max, uint32_t min)
{
  uint32_t random = ((uint32_t)sl_zigbee_get_pseudo_random_number()
                                                  * (max - min)) >> 16;
  random += min;
  return(random);
}

uint8_t get_a_sequence_number(void)
{
  local_sequence_number++;
  return(local_sequence_number);
}

// Modify single bit in TOKEN_APP_FLAGS
bool set_app_flags_nonvol(uint32_t bit, bool value)
{
  uint32_t token_value = 0UL;

  sl_token_get_data(TOKEN_APP_FLAGS, 0, &token_value, sizeof(token_value));
  sl_zigbee_app_debug_print("NEW app flags 0x%04X -> ", token_value);
  if(value)
  {
      token_value |= bit;
  }
  else
  {
      token_value &= ~(bit);
  }
  sl_token_set_data(TOKEN_APP_FLAGS, 0, &token_value, sizeof(token_value));
  sl_zigbee_app_debug_println("0x%04X", token_value);
  return(true);
}

uint32_t get_app_flags_nonvol(void)
{
  uint32_t token = 0UL;

  sl_token_get_data(TOKEN_APP_FLAGS, 0, &token, sizeof(token));
  return(token);
}

bool is_app_flags_nonvol_set(uint32_t flag)
{
  uint32_t token = 0UL;

  sl_token_get_data(TOKEN_APP_FLAGS, 0, &token, sizeof(token));
  return(0 != (flag & token));
}


void fill_in_apsFrame(sl_zigbee_af_cluster_command_t* cmd, sl_zigbee_aps_frame_t *apsFrame)
{
  apsFrame->profileId = HA_PROFILE_ID;
  apsFrame->clusterId = cmd->apsFrame->clusterId;
  apsFrame->destinationEndpoint = cmd->apsFrame->sourceEndpoint;
  apsFrame->sourceEndpoint = cmd->apsFrame->destinationEndpoint;
  apsFrame->sequence = cmd->seqNum;
  apsFrame->options = SL_ZIGBEE_APS_OPTION_NONE;
  apsFrame->groupId = 0;
  apsFrame->radius = 255;
}

void send_zdo_device_announce(uint8_t capabilities)
{
  uint8_t message[END_DEVICE_ANNOUNCE_MINIMUM_LENGTH] = {0};
  sl_zigbee_aps_frame_t apsFrameZDO =
  {
     .profileId = SL_ZIGBEE_ZDO_PROFILE_ID,
     .clusterId = END_DEVICE_ANNOUNCE_RESPONSE,
     .sourceEndpoint = SL_ZIGBEE_ZDO_ENDPOINT,
     .destinationEndpoint = SL_ZIGBEE_ZDO_ENDPOINT,
     .options = SL_ZIGBEE_APS_OPTION_SOURCE_EUI64 | SL_ZIGBEE_APS_OPTION_USE_ALIAS_SEQUENCE_NUMBER,
     .groupId = 0,
  };

  sl_802154_long_addr_t myEui64;
  sl_zigbee_af_get_eui64(myEui64);
  uint16_t nodeID = sl_zigbee_af_get_node_id();

  // Form the APS message for Bcast
  message[0] = 0;
  message[1] = (uint8_t)nodeID; //NodeId
  message[2] = (uint8_t)(nodeID >> 8); //NodeId
  memcpy(&message[3], myEui64, EUI64_SIZE); //IEEE Address
  message[11] = capabilities; // Capability

  uint8_t apsSequence = 0;

  sl_zigbee_send_broadcast(nodeID,
                           SL_ZIGBEE_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS, // dst
                           0,               //uint8_t nwkSequence,
                           &apsFrameZDO,    //sl_zigbee_aps_frame_t *apsFrame,
                           0xFF,            //uint8_t radius,
                           0xFF,            // Tag Id
                           sizeof(message), //uint8_t messageLength,
                           message,         //uint8_t *messageContents,
                           &apsSequence);
}

void zdo_discover_adaptiv(void)
{
  uint8_t message[8] = {0};
  sl_status_t status = 0;
  sl_zigbee_aps_frame_t apsFrameZDO =
  {
     .profileId = SL_ZIGBEE_ZDO_PROFILE_ID,
     .clusterId = SYSTEM_SERVER_DISCOVERY_REQUEST,
     .sourceEndpoint = SL_ZIGBEE_ZDO_ENDPOINT,
     .destinationEndpoint = SL_ZIGBEE_ZDO_ENDPOINT,
     .options = SL_ZIGBEE_APS_OPTION_SOURCE_EUI64 | SL_ZIGBEE_APS_OPTION_USE_ALIAS_SEQUENCE_NUMBER,
     .groupId = 0,
  };

  status = sl_zigbee_send_unicast(SL_ZIGBEE_OUTGOING_DIRECT,
                                  0x0000,
                                  &apsFrameZDO,
                                  0x0000,  // Tag
                                  sizeof(message), //uint8_t messageLength,
                                  message,         //uint8_t *messageContents,
                                  NULL);
  if(status)
  {
     sl_zigbee_app_debug_println("FAIL: ping_coordinator 0x%X", status);
  }
}

void ping_coordinator(void)
{
  uint8_t message[8] = {0};
  sl_status_t status = 0;
  sl_zigbee_aps_frame_t apsFrameZDO =
  {
     .profileId = SL_ZIGBEE_ZDO_PROFILE_ID,
     .clusterId = IEEE_ADDRESS_REQUEST,
     .sourceEndpoint = SL_ZIGBEE_ZDO_ENDPOINT,
     .destinationEndpoint = SL_ZIGBEE_ZDO_ENDPOINT,
     .options = SL_ZIGBEE_APS_OPTION_SOURCE_EUI64 | SL_ZIGBEE_APS_OPTION_USE_ALIAS_SEQUENCE_NUMBER,
     .groupId = 0,
  };

  status = sl_zigbee_send_unicast(SL_ZIGBEE_OUTGOING_DIRECT,
                                  0x0000,
                                  &apsFrameZDO,
                                  0x0000,  // Tag
                                  sizeof(message), //uint8_t messageLength,
                                  message,         //uint8_t *messageContents,
                                  NULL);
  if(status)
  {
     sl_zigbee_app_debug_println("FAIL: ping_coordinator 0x%X", status);
  }
}

static void device_leave_task(taskParm_t parm0, taskParm_t parm1)
{
  force_factory_reset(500UL, eRESET_NORMAL);
  removeTask(device_leave_task);
  sl_zigbee_app_debug_println("Nwk Leave   <%d>", get_time_ms());
}

bool sl_zigbee_af_pre_zdo_message_received_cb(sl_802154_short_addr_t sl_zigbee_node_id,
                                              sl_zigbee_aps_frame_t* apsFrame,
                                              int8u* message,
                                              int16u length)
{
  if((LEAVE_REQUEST == apsFrame->clusterId) && (sl_zigbee_af_get_node_id() == sl_zigbee_node_id))
  {
     taskParm_t arg0, arg1;
     arg0.u32 = 0UL;
     arg1.u8 = 0;
     addTask(device_leave_task, arg0, arg1);
  }
  return false;
}

// This gets called from commissioning_event_handler() when joined to a nwk.
// Will try and determine if a daintree nwk or not.
void adaptiv_disco_request_callback(const sl_zigbee_af_service_discovery_result_t* results)
{
  uint8_t disco_counter = 0;
  sl_802154_long_addr_t coord_eui64 = {0};

  if(SL_ZIGBEE_AF_UNICAST_SERVICE_DISCOVERY_COMPLETE_WITH_RESPONSE == results->status)
  {
     if(IEEE_ADDRESS_REQUEST == results->zdoRequestClusterId)
     {
        memcpy(coord_eui64, (uint8_t*)(results->responseData), EUI64_SIZE);
        #if 0
        sl_zigbee_app_debug_print("Coordinator %X %X",coord_eui64[7], coord_eui64[6]);
        sl_zigbee_app_debug_println(" %X   %X %X %X",coord_eui64[5], coord_eui64[2], coord_eui64[1], coord_eui64[0]);
        sl_zigbee_af_print_big_endian_eui64(( uint8_t*)&coord_eui64);
        sl_zigbee_app_debug_println("");
        #endif
     }
     else
     {
        sl_zigbee_app_debug_println("NO IEEE");
        return;
     }
     // Look for dt id 00:22:81 in upper bytes
     uint8_t dt_mfg_id[] = {0x81, 0x22, 0x00};  // Daintree
     uint8_t ad_mfg_id[] = {0x64, 0x1F, 0x8C};  // Adaptive
     if( 0 == memcmp(dt_mfg_id, &coord_eui64[5], sizeof(dt_mfg_id)))
     {
        set_present_network(eDAINTREE_NWK);
     }
     else if( 0 == memcmp(ad_mfg_id, &coord_eui64[5], sizeof(ad_mfg_id)))
     {
        set_present_network(eADAPTIV_NWK);
     }
     else if(12 >= disco_counter)
     {
        disco_counter += (12 >= disco_counter) ? 1 : 0;
        sl_zigbee_app_debug_print("GOT IEEE, NOT DAINTREE or ADAPTIV");
        set_present_network(eUNKNOWN_NWK);
        //set_present_network(eADAPTIV_NWK);
        //set_present_network(eDAINTREE_NWK);
     }
     else
     {

     }
   }
}


/***************************************************************************/
/******************************   PRE-CMD   ********************************/
/***************************************************************************/
bool sl_zigbee_af_pre_command_received_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  time_of_last_cmd = get_time_ms();
  bool return_status = false;

  // Normal, (non-Adaptive, non-Daintree), command?
  if(!cmd->mfgSpecific)
  {
     //sl_zigbee_app_debug_println("ClusId:0x%X cmd:0x%X ",cmd->apsFrame->clusterId, cmd->commandId);
     return(return_status);
  }
  else if((ADAPTIV_ZIGBEE_MFG_ID == cmd->mfgCode) && (ZCL_FRAME_CONTROL_CLIENT_TO_SERVER == cmd->direction))
  {
     // NOTE: ALL Adaptive mfg cmds are on end point 1.  Including occ and light sensor!!!!
     switch(cmd->apsFrame->clusterId)
     {
        case ZCL_ADAPTIV_AUTOCOMMISSIONING_CLUSTER_ID:
        {
           return_status = auto_commission_incoming_cmd(cmd);
           break;
        }
        case ZCL_ADAPTIV_CONFIGURATION_CLUSTER_ID:
        {
           if(adaptiv_configuration_incoming_cmd(cmd))
           {
              // Added color support for Daintree, so don't flash lights
              if(eADAPTIV_NWK == get_present_network())
              {
                 dim_lights_up_down(1, DSECS_HALF_SECS);
              }
              return_status = true;
           }
           break;
        }
        case ZCL_OCCUPANCY_SENSING_CLUSTER_ID:
        {
           if(ZCL_WRITE_OCCUPANCY_SENSOR_COMMAND_ID == cmd->commandId)
           {
              set_new_occ_state(cmd);
              return_status = true;
           }
           break;
        }
        case ZCL_ILLUM_MEASUREMENT_CLUSTER_ID:
        {
           if(ZCL_SUSPEND_LIGHT_SENSOR_COMMAND_ID == cmd->commandId)
           {
              uint16_t Dsecs = GET_16_FROM_ARRAY(cmd->buffer, cmd->payloadStartIndex);
              return_status = suspend_light_sensor_control(Dsecs);
           }
           else if(ZCL_WRITE_LIGHT_SENSOR_COMMAND_ID == cmd->commandId)
           {
              update_light_sensor_neighbor_table(cmd);
              return_status = true;
           }
           break;
        }
        case ZCL_ADAPTIV_DEBUG_CLUSTER_ID:
        {
          if(ZCL_ADAPTIV_DEBUG_COMMAND_ID == cmd->commandId)
          {
              return_status = rx_sink_debug(cmd);
          }
           break;
        }
        default:
           return(false);
           break;
     }
     return(return_status);
  }
  else if((DAINTREE_ZIGBEE_MFG_ID == cmd->mfgCode) && (ZCL_FRAME_CONTROL_CLIENT_TO_SERVER == cmd->direction) &&
         (ZCL_CONFIGURATION_CLUSTER_ID == cmd->apsFrame->clusterId))
  {
     if((ZCL_WRITE_ATTRIBUTES_COMMAND_ID == cmd->commandId) ||
       (ZCL_WRITE_ATTRIBUTES_NO_RESPONSE_COMMAND_ID == cmd->commandId))
     {
        #if 1
        set_daintree_configuration_cmd(cmd);
        return(false);
        #else
        return(set_daintree_configuration_cmd(cmd)); // Should always be false
        #endif
     }
     if(ZCL_READ_ATTRIBUTES_COMMAND_ID == cmd->commandId)
     {
        #if 1
        return(false);
        #else
        return(get_daintree_configuration_cmd(cmd));
        #endif
     }
     else
     {
        return(false);
     }
  }
  else
  {
     // This command is mfg specific, but not the ones we support
     return(false);
  }
  return(false);
}

/***************************************************************************/
/******************************  IDENTIFY   ********************************/
/***************************************************************************/
//WEAK(void sl_zigbee_af_identify_start_feedback_cb(uint8_t endpoint, uint16_t identifyTime))
void sl_zigbee_af_identify_start_feedback_cb(uint8_t endpoint, uint16_t identifyTime)
{
  if(0 != identifyTime)
  {
     //set_occ_sensor_onOff(false);
  }
  else
  {
     //set_occ_sensor_onOff(true);
  }
}
//WEAK(void sl_zigbee_af_identify_stop_feedback_cb(uint8_t endpoint))
void sl_zigbee_af_identify_stop_feedback_cb(uint8_t endpoint)
{
  //JGRset_occ_sensor_onOff(true);
}


/***************************************************************************/
/********************************   ON-OFF  ********************************/
/***************************************************************************/

uint16_t read_on_off_attribute(uint16_t attributeID)
{
  uint8_t onOffAttr[2] = {0};
  uint8_t size;

  switch(attributeID)
  {
     case ZCL_ON_OFF_ATTRIBUTE_ID:
     default:
        size = 1;
        break;
     case ZCL_ON_TIME_ATTRIBUTE_ID:
        size = 2;
        break;
     case ZCL_OFF_WAIT_TIME_ATTRIBUTE_ID:
        size = 2;
        break;
     case ZCL_START_UP_ON_OFF_ATTRIBUTE_ID:
        size = 1;
        break;
  }
  if(sl_zigbee_af_read_server_attribute(LIGHT_ENDPOINT,
                                        ZCL_ON_OFF_CLUSTER_ID,
                                        attributeID,
                                        (uint8_t *)&onOffAttr,
                                        size))
  {
      sl_zigbee_app_debug_println("ERR: read_on_off_attr");
  }
  return(((uint16_t)onOffAttr[1] << 8) | (uint16_t)onOffAttr[0]);
}

sl_zigbee_af_status_t write_on_off_attribute(bool onOff)
{
  uint8_t newOnOff[1] = {(uint8_t)onOff};

  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != sl_zigbee_af_write_server_attribute(
                                         LIGHT_ENDPOINT,
                                         ZCL_ON_OFF_CLUSTER_ID,
                                         ZCL_ON_OFF_ATTRIBUTE_ID,
                                         (uint8_t *)&newOnOff,
                                         ZCL_BOOLEAN_ATTRIBUTE_TYPE))
  {
     sl_zigbee_app_debug_println("ERR: write_on_off_attr");
     return(SL_ZIGBEE_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE);
  }
  #if 0
  sl_zigbee_app_debug_println("   Wrote onOff attr to %s", onOff ? "ON" : "OFF");
  #endif

  return(SL_ZIGBEE_ZCL_STATUS_SUCCESS);
}

/***************************************************************************/
/****************************     LEVEL    *********************************/
/***************************************************************************/
// Short cut
uint8_t get_present_light_level(void)
{
  return((uint8_t)read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID));
}

uint16_t read_light_level_attribute(sl_zigbee_af_attribute_id_t attributeID)
{
  uint8_t data[2] = {0};
  uint8_t size = sizeof(data);

  switch(attributeID)
  {
     case ZCL_CURRENT_LEVEL_ATTRIBUTE_ID:
     case ZCL_START_UP_CURRENT_LEVEL_ATTRIBUTE_ID:
     case ZCL_LEVEL_CONTROL_MIN_LEVEL_ATTRIBUTE_ID:
     case ZCL_LEVEL_CONTROL_MAX_LEVEL_ATTRIBUTE_ID:
     case ZCL_ON_LEVEL_ATTRIBUTE_ID:
     case ZCL_OPTIONS_ATTRIBUTE_ID:
        size = 1;
        break;
     case ZCL_LEVEL_CONTROL_REMAINING_TIME_ATTRIBUTE_ID:
        size = 2;
        break;
     default:
        sl_zigbee_app_debug_println("Unsupported level attr");
        size = 1;
       break;
  }

  if(sl_zigbee_af_read_server_attribute(LIGHT_ENDPOINT,
                                        ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                        attributeID,
                                        (uint8_t *)&data,
                                        size))
  {
     sl_zigbee_app_debug_println("\nERR: read_light level_attribute\n");
     return(0);
  }
  return(((uint16_t)data[1] << 8) | (uint16_t)data[0]);
}

bool write_light_level_attribute(sl_zigbee_af_attribute_id_t attributeID, uint8_t *data)
{
  sl_zigbee_af_attribute_type_t type;
  bool status = true;

  switch(attributeID)
  {
     case ZCL_CURRENT_LEVEL_ATTRIBUTE_ID:
     case ZCL_START_UP_CURRENT_LEVEL_ATTRIBUTE_ID:
     case ZCL_LEVEL_CONTROL_MIN_LEVEL_ATTRIBUTE_ID:
     case ZCL_LEVEL_CONTROL_MAX_LEVEL_ATTRIBUTE_ID:
     case ZCL_ON_LEVEL_ATTRIBUTE_ID:
        type = ZCL_INT8U_ATTRIBUTE_TYPE;
        break;
     case ZCL_LEVEL_CONTROL_REMAINING_TIME_ATTRIBUTE_ID:
        type = ZCL_INT16U_ATTRIBUTE_TYPE;
        break;
     case ZCL_OPTIONS_ATTRIBUTE_ID:
        type = ZCL_BITMAP8_ATTRIBUTE_TYPE;
        break;
     default:
        type = ZCL_INT8U_ATTRIBUTE_TYPE;
        sl_zigbee_app_debug_println("ERR: write level attr not suppoted");
        break;
  }

  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != sl_zigbee_af_write_server_attribute(LIGHT_ENDPOINT,
                                         ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                         attributeID,
                                         data,
                                         type))
  {
     sl_zigbee_app_debug_println("ERR: write_light_level_attribute");
     status = false;
  }

  #if 1
  if(attributeID == ZCL_ON_LEVEL_ATTRIBUTE_ID)
     sl_zigbee_app_debug_println("ON_LEVEL %d   <%d>", *data, get_time_ms());//JJGR
  #else
     sl_zigbee_app_debug_println("Saved light attr 0x%04X to %d ", attributeID, *data);
  #endif

  return(status);
}

uint8_t save_present_light_level_to_on_level_attribute(void)
{
  uint8_t present_level = hal_get_dim_pwm_255();

  if(get_max_light_level() < present_level)
  {
     present_level = get_max_light_level();
  }
  else if(DEFAULT_MIN_LIGHT_LEVEL_DT > present_level)
  {
     present_level = DEFAULT_MIN_LIGHT_LEVEL_DT;
  }
  else
  {
  }

  write_light_level_attribute(ZCL_ON_LEVEL_ATTRIBUTE_ID, &present_level);
  return(present_level);
}

uint8_t get_on_light_level(void)
{
#if 1 //JGR
  return((uint8_t)read_light_level_attribute(ZCL_ON_LEVEL_ATTRIBUTE_ID));
#else
  uint8_t turn_on_level = (uint8_t)read_light_level_attribute(ZCL_ON_LEVEL_ATTRIBUTE_ID);
  sl_zigbee_app_debug_println("RESTORED ON_LEVEL  %d", turn_on_level);
  return(turn_on_level);
#endif
}

// Make sure attributes match present light state
void check_on_off_level(void)
{
  uint8_t level = (read_on_off_attribute(ZCL_ON_OFF_ATTRIBUTE_ID)) ? read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID) : 0;

  if(hal_get_dim_pwm_255() != level)
  {
     hal_set_dim_pwm_255(level);
     sl_zigbee_app_debug_println("Modifing PWM to %d",level);
  }
}

/***************************************************************************/
/****************************     NWK    ***********************************/
/***************************************************************************/
bool nwk_comms_timeout_short_daintree(void)
{
  return(has_timed_out(time_of_last_cmd, (30UL*60UL*1000UL)));  // 30 minutes
}

bool nwk_comms_timeout_long_daintree(void)
{
  return(has_timed_out(time_of_last_cmd, (24UL*60UL*60UL*1000UL)));  // 24 hours
}

/***************************************************************************/
/****************************    INFO    ***********************************/
/***************************************************************************/
//  NOTE:  Assumes caller has 16 bytes for return data
static sl_zigbee_af_status_t read_attributes(uint8_t endpoint, uint16_t clusterID, uint16_t attributeID, uint8_t *data)
{
  sl_zigbee_af_status_t status;

  status = sl_zigbee_af_read_server_attribute(endpoint,
                                              clusterID,
                                              attributeID,
                                              data,
                                              16);
  if(status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
     sl_zigbee_app_debug_println("ERR:reading attribute 0x%X", status);
  }

  return(status);
}

// Called from within "info" cli cmd
void spew_app_info(void)
{
  uint8_t data[16] = {0};   // For strings, data[0] is length

  sl_zigbee_app_debug_println("\r\nLight attributes info");
  read_attributes(LIGHT_ENDPOINT, ZCL_BASIC_CLUSTER_ID, ZCL_MANUFACTURER_NAME_ATTRIBUTE_ID, (uint8_t *)&data[0]);
  sl_zigbee_app_debug_println("  Mfg: %s", (char *)&data[1]);
  memset(data, 0, sizeof(data));

  read_attributes(LIGHT_ENDPOINT, ZCL_BASIC_CLUSTER_ID, ZCL_MODEL_IDENTIFIER_ATTRIBUTE_ID, (uint8_t *)&data[0]);
  sl_zigbee_app_debug_println("  Model: %s", (char *)&data[1]);
  memset(data, 0, sizeof(data));

  read_attributes(LIGHT_ENDPOINT, ZCL_BASIC_CLUSTER_ID, ZCL_DATE_CODE_ATTRIBUTE_ID, (uint8_t *)&data[0]);
  sl_zigbee_app_debug_println("  Date: %s", (char *)&data[1]);
  memset(data, 0, sizeof(data));

  read_attributes(LIGHT_ENDPOINT, ZCL_BASIC_CLUSTER_ID, ZCL_SW_BUILD_ID_ATTRIBUTE_ID, (uint8_t *)&data[0]);
  sl_zigbee_app_debug_println("  S/W ver: %s", (char *)&data[1]);
  memset(data, 0, sizeof(data));

  read_attributes(LIGHT_ENDPOINT, ZCL_BASIC_CLUSTER_ID, ZCL_HW_VERSION_ATTRIBUTE_ID, (uint8_t *)&data[0]);
  sl_zigbee_app_debug_println("  H/W ver: 0x%02X", data[0]);
  memset(data, 0, sizeof(data));

  read_attributes(LIGHT_ENDPOINT, ZCL_DIAGNOSTICS_CLUSTER_ID, 0x0000, (uint8_t *)&data[0]);
  sl_zigbee_app_debug_println("  Resets: %d", ((uint16_t)data[1] << 8) | (uint16_t)data[0]);
  memset(data, 0, sizeof(data));



  read_attributes(LIGHT_ENDPOINT, ZCL_ON_OFF_CLUSTER_ID, ZCL_ON_OFF_ATTRIBUTE_ID, (uint8_t *)&data[0]);
  sl_zigbee_app_debug_println("On/off: %s", data[0] ? "ON" : "OFF");
  memset(data, 0, sizeof(data));

  data[0] = read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID);
  data[1] = read_light_level_attribute(ZCL_ON_LEVEL_ATTRIBUTE_ID);
  data[2] = read_light_level_attribute(ZCL_START_UP_CURRENT_LEVEL_ATTRIBUTE_ID);
  sl_zigbee_app_debug_println("  Cur Level: %d   pwm: %d", data[0], hal_get_dim_pwm_255());
  sl_zigbee_app_debug_println("  Off -> On Level: %d", data[1]);
  sl_zigbee_app_debug_println("  POR Level: %d", data[2]);
  memset(data, 0, sizeof(data));

  sl_zigbee_af_read_manufacturer_specific_server_attribute(LIGHT_ENDPOINT,
                                                           ZCL_CONFIGURATION_CLUSTER_ID,
                                                           ZCL_MIN_LIGHT_LEVEL_ATTRIBUTE_ID,
                                                           DAINTREE_ZIGBEE_MFG_ID,
                                                           (uint8_t *)&data[0],
                                                           1);
  sl_zigbee_af_read_manufacturer_specific_server_attribute(LIGHT_ENDPOINT,
                                                           ZCL_CONFIGURATION_CLUSTER_ID,
                                                           ZCL_MAX_LIGHT_LEVEL_ATTRIBUTE_ID,
                                                           DAINTREE_ZIGBEE_MFG_ID,
                                                           (uint8_t *)&data[1],
                                                           1);
  sl_zigbee_af_read_manufacturer_specific_server_attribute(LIGHT_ENDPOINT,
                                                           ZCL_CONFIGURATION_CLUSTER_ID,
                                                           ZCL_ALLOW_OFF_ATTRIBUTE_ID,
                                                           DAINTREE_ZIGBEE_MFG_ID,
                                                           (uint8_t *)&data[2],
                                                           1);
  sl_zigbee_app_debug_println("  Max/Mid/Min Lvl: %d/xx/%d  Daintree", data[1], data[0]);
  sl_zigbee_app_debug_println("  Max/Mid/Min Lvl: %d/%d/%d  Adaptiv", get_max_light_level(), get_mid_light_level(), get_min_light_level());
  sl_zigbee_app_debug_println("  Dim to %d", DEFAULT_MIN_LIGHT_LEVEL_DT);
  sl_zigbee_app_debug_println("  Allow off: %d %d", data[2], allow_light_off());
  memset(data, 0, sizeof(data));

  sl_zigbee_af_read_manufacturer_specific_server_attribute(LIGHT_ENDPOINT,
                                                           ZCL_ADAPTIV_CONFIGURATION_CLUSTER_ID,
                                                           ZCL_COLOR_TEMPERATURE_ATTRIBUTE_ID,
                                                           ADAPTIV_ZIGBEE_MFG_ID,
                                                           (uint8_t *)&data[0],
                                                           2);
  sl_zigbee_app_debug_println("  Color: %dK", GET_16_FROM_ARRAY(data, 0));
  memset(data, 0, sizeof(data));

  sl_zigbee_app_debug_print("Occ sensor ");
  uint16_t unoccupied_delay_secs;
  sl_token_get_data(TOKEN_APP_UNOCCUPIED_DELAY, 0, &unoccupied_delay_secs, sizeof(unoccupied_delay_secs));
  sl_zigbee_app_debug_println("%s", (0 == unoccupied_delay_secs) ? "OFF" : "ON");
  read_attributes(OCC_SENSOR_ENDPOINT, ZCL_OCCUPANCY_SENSING_CLUSTER_ID, ZCL_OCCUPANCY_ATTRIBUTE_ID, (uint8_t *)&data[0]);
  sl_zigbee_app_debug_println("  Occ attr now: %d", data[0]);
  memset(data, 0, sizeof(data));

  read_attributes(OCC_SENSOR_ENDPOINT, ZCL_OCCUPANCY_SENSING_CLUSTER_ID, ZCL_PIR_OCCUPIED_TO_UNOCCUPIED_DELAY_ATTRIBUTE_ID, (uint8_t *)&data[0]);
  sl_zigbee_app_debug_println("  Occ->UnOcc delay: %d secs  Daintree", GET_16_FROM_ARRAY(data, 0));
  sl_zigbee_app_debug_println("  Occ->UnOcc delay: %d secs  Adaptiv", get_unoccupied_delay());
  memset(data, 0, sizeof(data));

  sl_zigbee_app_debug_println("  UnOcc->off delay: %d secs",  get_unoccupied_delay_to_off());
  memset(data, 0, sizeof(data));

  read_attributes(OCC_SENSOR_ENDPOINT, ZCL_OCCUPANCY_SENSING_CLUSTER_ID, ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_DELAY_ATTRIBUTE_ID, (uint8_t *)&data[0]);
  sl_zigbee_app_debug_println("  UnOcc->Occ delay: %d secs", ((uint16_t)data[1] << 8) | (uint16_t)data[0]);
  memset(data, 0, sizeof(data));

  read_attributes(OCC_SENSOR_ENDPOINT, ZCL_OCCUPANCY_SENSING_CLUSTER_ID, ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_ATTRIBUTE_ID, (uint8_t *)&data[0]);
  sl_zigbee_app_debug_println("  UnOcc->Occ thresh: %d", ((uint16_t)data[1] << 8) | (uint16_t)data[0]);
  memset(data, 0, sizeof(data));

  sl_zigbee_app_debug_println("  Sensor threshold: %d", get_occupancy_threshold());

  sl_zigbee_app_debug_print("  Occ present state: ");  spew_occ_state();


  sl_zigbee_app_debug_println("Illum sensor %s  [0x%X]", allow_light_sensor_control() ? "ON" : "OFF", dhs_get_inhibit_bits());
  sl_zigbee_app_debug_println("  Illum Master: %s", is_app_flags_nonvol_set(B_DHS_MASTER_LIGHT) ? "YES" : "NO");

  read_attributes(LIGHT_SENSOR_ENDPOINT, ZCL_ILLUM_MEASUREMENT_CLUSTER_ID, ZCL_ILLUM_MEASURED_VALUE_ATTRIBUTE_ID, (uint8_t *)&data[0]);
  sl_zigbee_app_debug_println("  Illum lux: %d/%d", GET_16_FROM_ARRAY(data, 0), get_light_sensor_mean_lux());
  sl_token_get_data(TOKEN_APP_LIGHT_SENSOR_LUX, 0, &data, 2);
  sl_zigbee_app_debug_println("  Illum lux ref: %d", GET_16_FROM_ARRAY(data, 0));
  memset(data, 0, sizeof(data));



  sl_zigbee_app_debug_println("Binding Table size[%d]", sl_zigbee_af_get_binding_table_size());
  for (uint8_t i = 0; i < sl_zigbee_af_get_binding_table_size(); i++)
  {
     sl_zigbee_binding_table_entry_t result;
     if(SL_STATUS_OK == sl_zigbee_get_binding(i, &result))
     {
        if(SL_ZIGBEE_UNUSED_BINDING == result.type)
        {
           break;
        }

        sl_zigbee_app_debug_print(" Binding Tbl[%d] is 0x%X", i,result.type);
        switch(result.type)
        {
           default:
           case SL_ZIGBEE_UNUSED_BINDING:
              sl_zigbee_app_debug_print(" UNUSED");
              break;
           case SL_ZIGBEE_UNICAST_BINDING:
              sl_zigbee_app_debug_print(" UNCAST 64");
              break;
           case SL_ZIGBEE_MANY_TO_ONE_BINDING:
              sl_zigbee_app_debug_print(" MTO 64");
              break;
           case SL_ZIGBEE_MULTICAST_BINDING:
                sl_zigbee_app_debug_print(" MULTICAST");
                break;
        }
        sl_zigbee_app_debug_print("  Addr 0x%04X", ((uint16_t)result.identifier[1] << 8) |
                                                         (uint16_t)result.identifier[0]);
        sl_zigbee_app_debug_println(" ep %d clusterID 0x%04X",result.local, result.clusterId);
     }
  }

  sl_zigbee_app_debug_println("Report Table entries[%d]", sli_zigbee_af_reporting_num_entries());
  for( uint8_t i = 0; i < sli_zigbee_af_reporting_num_entries(); i++)
  {
     sl_zigbee_af_plugin_reporting_entry_t entry;

     sli_zigbee_af_reporting_get_entry(i, &entry);
     if(entry.endpoint != SL_ZIGBEE_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID)
     {
        sl_zigbee_app_debug_print("  [%d] ep %02X clus %04X attr %04X svr %c",
                                     i,
                                     entry.endpoint,
                                     entry.clusterId,
                                     entry.attributeId,
                                     (entry.mask == CLUSTER_MASK_SERVER ? 'Y' : 'N'));

        if(entry.direction == SL_ZIGBEE_ZCL_REPORTING_DIRECTION_REPORTED)
        {
           sl_zigbee_app_debug_print(" report min %04X max %04X rpt-chg %08X",
                                       entry.data.reported.minInterval,
                                       entry.data.reported.maxInterval,
                                       entry.data.reported.reportableChange);
           sl_zigbee_af_reporting_flush();
        }
        else
        {
           sl_zigbee_app_debug_print(" receive from %04X ep %02X timeout %04X",
                                       entry.data.received.source,
                                       entry.data.received.endpoint,
                                       entry.data.received.timeout);
        }
     }
     sl_zigbee_app_debug_println("");
  }

  print_which_network();
  sl_zigbee_app_debug_println("Light is %s a group",device_is_in_a_group() ? "IN" : "NOT in");
  uint32_t mode_flags = get_app_flags_nonvol();
  sl_zigbee_app_debug_println("Mode flags 0x%02X", mode_flags);
  if(0 != mode_flags)
  {
     if(B_50_PERCENT_MODE & mode_flags) { sl_zigbee_app_debug_println("  50%% mode"); }
     if(B_MANUAL_ON_MODE & mode_flags) { sl_zigbee_app_debug_println("  Manual On"); }
     if(B_MANUAL_ADD_MODE & mode_flags) { sl_zigbee_app_debug_println("  Manual group"); }
     if(B_OCC_XXL_DELAY_ENABLED & mode_flags) { sl_zigbee_app_debug_println("  XXL Delay"); }
     if(B_DHS_MASTER_LIGHT & mode_flags) { sl_zigbee_app_debug_println("  DHS Master"); }
     if(B_EM_MODE & mode_flags) { sl_zigbee_app_debug_println("  EM Mode"); }
  }
  sl_zigbee_app_debug_println("No cmd rx time %d", get_time_ms() - time_of_last_cmd);
  sl_zigbee_app_debug_println("uP Time %d\n", get_time_ms());

  sl_zigbee_app_debug_println("***************************************************");
  #ifdef DHS_SINK_DEVICE
  sl_zigbee_app_debug_println("  DHS_SINK_DEVICE is set");
  #warning "DHS_SINK_DEVICE is defined"
  #endif
  #ifdef DHS_SOURCE_DEVICE
  sl_zigbee_app_debug_println("  DHS_SOURCE_DEVICE is set");
  #warning "DHS_SOURCE_DEVICE is defined"
  #endif
  #ifdef DHS_DOUBLE
  sl_zigbee_app_debug_println("  DHS_DOUBLE is set");
  #warning "DHS_DOUBLE is defined"
  #endif
  #ifdef DISABLE_LIGHT_SENSOR_CONTROL
  sl_zigbee_app_debug_println("  DISABLE_LIGHT_SENSOR_CONTROL is set");
  #warning "DISABLE_LIGHT_SENSOR_CONTROL is defined"
  #endif

  //#endif  //defined(DHS_SINK_DEVICE) || defined(DHS_SOURCE_DEVICE) || defined(DHS_DOUBLE) || defined(DISABLE_LIGHT_SENSOR_CONTROL)
  if(sink_debug_enabled())
  {
     sl_zigbee_app_debug_println("  WARNING:  Debug is set");
  }



}


/*********************************************************************************/
/*********************      SINK/SOURCE DEBUG     ********************************/
/*********************************************************************************/

typedef struct
{
  bool enable;
  sl_802154_short_addr_t sink_short_addr;
  sl_802154_long_addr_t sink_eui64;
} sink_debug_info_t;
static sink_debug_info_t sink_debug_info = {0};

#ifdef DHS_SINK_DEVICE
void init_sink_debug(void)
{
  sl_802154_long_addr_t sink_eui64;
  sink_debug_info.sink_short_addr = sl_zigbee_af_get_node_id();
  sl_zigbee_af_get_eui64(sink_eui64);
  memcpy(sink_debug_info.sink_eui64, sink_eui64, sizeof(sl_802154_long_addr_t));
  sl_zigbee_app_debug_println("SINK init 0x%X   short 0x%X",
      GET_16_FROM_ARRAY(sink_debug_info.sink_eui64, 0), sink_debug_info.sink_short_addr);
}


// Cmd from cli with input cmd
void sink_setup(uint8_t cmd)
{
  uint8_t message[8] = {0};

  switch(cmd)
  {
    case 0:
        sink_debug_info.enable = false;
        message[0] = SINK_SOURCE_DISABLE;
        tx_sink_info(message, 1);
        sl_zigbee_app_debug_println("SINK OFF");
        break;
     case 1:
        sink_debug_info.enable = true;
        message[0] = SINK_SOURCE_ENABLE;
        tx_sink_info(message, 1);
        sl_zigbee_app_debug_println("SINK ON");
        break;
     case 2:
        message[0] = SINK_SHORT_ADDR;
        message[1] = GET_LSB_FROM_16(sink_debug_info.sink_short_addr);
        message[2] = GET_MSB_FROM_16(sink_debug_info.sink_short_addr);
        tx_sink_info(message, 3);
        sl_zigbee_app_debug_println("Multi-cast SINK Addr 0x%X", sink_debug_info.sink_short_addr);
        break;
     default:
        sl_zigbee_app_debug_println("SINK %s Addr 0x%X", (sink_debug_info.enable) ? "ON" : "OFF"
                                             ,sink_debug_info.sink_short_addr);
        break;
  }
}
#else
void init_sink_debug(void) { }
void sink_setup(uint8_t cmd) { }
#endif

// Uni-casts this devices debug data to a sink device
sl_status_t tx_source_debug(uint8_t *data, uint8_t length)
{
  uint8_t message[16] = {0};
  sl_802154_short_addr_t sink_short_addr = sink_debug_info.sink_short_addr;
  sl_status_t status = SL_STATUS_OK;
  sl_zigbee_aps_frame_t apsFrame =
  {
     .profileId = HA_PROFILE_ID,
     .clusterId = ZCL_ADAPTIV_DEBUG_CLUSTER_ID,
     .sourceEndpoint = LIGHT_ENDPOINT,
     .destinationEndpoint = LIGHT_ENDPOINT,
     .options = SL_ZIGBEE_APS_OPTION_NONE,
     .radius = ZA_MAX_HOPS,
     .groupId = 0,
  };

  if(false == sink_debug_info.enable)
  {
     return(0);
  }

  message[0] = (ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER |
                ZCL_MANUFACTURER_SPECIFIC_MASK | ZCL_DISABLE_DEFAULT_RESPONSE_MASK);
  message[1] = GET_LSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[2] = GET_MSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[3] = sl_zigbee_af_next_sequence();
  message[4] = ZCL_ADAPTIV_DEBUG_COMMAND_ID;

  memcpy(&message[5], data, length);

  uint8_t message_length  = length + 6;
  assert(sizeof(message) > message_length);

  status = sl_zigbee_af_send_unicast(SL_ZIGBEE_OUTGOING_DIRECT,
                                     sink_short_addr,
                                     &apsFrame,
                                     (uint16_t)message_length,
                                     (uint8_t *)&message[0]);

  if(status)
  {
     sl_zigbee_app_debug_println("FAIL: ping_coordinator 0x%X", status);
  }
  return(status);
}

// This is incoming from an tx_source_debug() cmd.  Used both sink and source
bool rx_sink_debug(sl_zigbee_af_cluster_command_t* cmd)
{
  uint8_t base_idx = cmd->payloadStartIndex;

  uint8_t op_code = cmd->buffer[base_idx+0];
  uint16_t macID = GET_16_FROM_ARRAY(cmd->buffer, base_idx+1);
  uint16_t lux = GET_16_FROM_ARRAY(cmd->buffer, (base_idx+3));
  uint8_t pwm = cmd->buffer[base_idx+5];
  uint8_t mode = cmd->buffer[base_idx+6];   // [Counter | Mode >> Shifted]

  switch(op_code)
  {
     case SINK_SOURCE_DISABLE:
        sink_debug_info.enable = false;
        sl_zigbee_app_debug_println("Sinking OFF");
        break;
     case SINK_SOURCE_ENABLE:
        sink_debug_info.enable = true;
        sl_zigbee_app_debug_println("Sinking ON");
        break;
     case SINK_SHORT_ADDR:
        sl_zigbee_app_debug_println("Sink short 0x%X", macID);
        sink_debug_info.sink_short_addr = macID;
        break;
     case DHS_DATA:
        sl_zigbee_app_debug_print("[mac:0x%04X, time:%d, ", macID, get_time_ms()/1000UL);
        sl_zigbee_app_debug_println("lux:%d, level:%d, mode:%d]", lux, pwm, mode);
        break;
     case DHS_CAL_DATA:
        sl_zigbee_app_debug_print("[CAL id:0x%04X, t:%d, ", macID, get_time_ms()/1000UL);
        sl_zigbee_app_debug_println("mean_err:%d, median_err:%d, n:%d]", cmd->buffer[base_idx+3],
                                    cmd->buffer[base_idx+4], cmd->buffer[base_idx+5]);
        break;
     case DHS_MST_DATA:
       sl_zigbee_app_debug_print("[MST id:0x%04X, t:%d, ", macID, get_time_ms()/1000UL);
       sl_zigbee_app_debug_println("mean_lux:%d, pwm:%d, dim_t:%d, err:%d]", lux, pwm, mode,
                                           (int8_t)cmd->buffer[base_idx+7]);
       break;
     default:
        break;
  }
  return(true);
}

bool sink_debug_enabled(void)
{
  return(sink_debug_info.enable);
}
