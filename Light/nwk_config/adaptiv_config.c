#include "af.h"

#include "af-main.h"
#include "RehTOS.h"
#include "hal_occ_sensor.h"
#include "em_chip.h"
#include "hal_pwm.h"
#include "hal_utils.h"
#include "sl_event_handler.h"
#include "light_on_off.h"
#include "sl_hal_wdog.h"
#include "sl_token_manager.h"
#include "occ_sensor.h"
#include "attribute-storage.h"
#include "auto_comm.h"
#include "nwk_utils.h"
#include "reporting.h"
#include "light_sensor.h"
#include "factory_reset.h"
#include "adaptiv_config.h"
#include "nwk_groups.h"

#include "sl_zigbee_debug_print.h"

extern void move_to_level_onoff(uint8_t level, uint16_t transitionTimeDs);

/***************************************************************************/
/******************************  RESPONSE   ********************************/
/***************************************************************************/
#if 0
sl_status_t tx_adaptiv_response(sl_zigbee_af_cluster_command_t *cmd,
                                                   uint8_t *payload,
                                                   uint8_t length,
                                                   bool unicast)
{

  sl_status_t status = SL_STATUS_OK;
  //                    FC       Mfg ID    Seq#  cmd   Payload
  //uint8_t message[] = {0x1D, 0x74, 0x15, 0x09, 0x01, 0x00, 0x00, 0x00};
  sl_zigbee_aps_frame_t apsFrame = {0};

  apsFrame.profileId = HA_PROFILE_ID;
  apsFrame.clusterId = cmd->apsFrame->clusterId;
  apsFrame.sourceEndpoint = cmd->apsFrame->destinationEndpoint;
  apsFrame.destinationEndpoint = cmd->apsFrame->sourceEndpoint;
  apsFrame.options = SL_ZIGBEE_AF_DEFAULT_APS_OPTIONS,
  apsFrame.groupId = 0,
  apsFrame.radius = ZA_MAX_HOPS,
  apsFrame.sequence = cmd->seqNum;

  uint16_t shortId = cmd->source;

  if(unicast)
  {
     status = sl_zigbee_af_send_unicast(SL_ZIGBEE_OUTGOING_DIRECT,
                                        shortId,
                                        &apsFrame,
                                        length,
                                        payload);
  }
  else
  {
     status =  sl_zigbee_af_send_broadcast(SL_ZIGBEE_NULL_NODE_ID,
                                           SL_ZIGBEE_BROADCAST_ADDRESS,
                                           apsFrame.sequence,
                                           &apsFrame,
                                           length,
                                           payload);
  }

  if(SL_STATUS_OK != status)
  {
     sl_zigbee_app_debug_println("ERR:tx_commission_info 0x%02X", status);
  }
  return(status);
}
#endif
/***************************************************************************/
/******************************   SET NWK   ********************************/
/***************************************************************************/
which_nwk_t get_present_network(void)
{
  which_nwk_t present_nwk = eNO_NWK;

  sl_token_get_data(TOKEN_APP_WHICH_NETWORK, 0, &present_nwk, 1);
  return(present_nwk);
}

void set_present_network(which_nwk_t new_nwk)
{
  uint8_t nwk = (uint8_t)new_nwk;

  which_nwk_t present_nwk = get_present_network();

  if((eDAINTREE_NWK == present_nwk) || (eADAPTIV_NWK == present_nwk))
  {
     // Once on daintree always on daintree.
     // Only factory reset clears
     return;
  }

  if(new_nwk != present_nwk)
  {
     // Nwk has changed, save and re-initialize
     sl_token_set_data(TOKEN_APP_WHICH_NETWORK, 0, &nwk, 1);

     sl_zigbee_app_debug_print("\nNew Nwk ");
     print_which_network();

     //fat_finger();

     // Re-initialize sensors, after nwk token has been set.
     taskParm_t arg0, arg1;
     arg0.u32 = get_time_ms();
     arg1.u8 = new_nwk;
     addTask(occ_sensor_init_task, arg0, arg1);
  }
}

void print_which_network(void)
{
  which_nwk_t present_nwk = get_present_network();
  char nwk_str[16] = {0};

  switch(present_nwk)
  {
    case eNO_NWK:
     sprintf(nwk_str, "NO_NWK");
     break;
    case eADAPTIV_NWK:
     sprintf(nwk_str, "ADAPTIV");
     break;
    case eDAINTREE_NWK:
     sprintf(nwk_str, "DAINTREE");
     break;
    case eUNKNOWN_NWK:
     sprintf(nwk_str, "eUNKNOWN_NWK");
     break;
    case eSTAND_ALONE:
     sprintf(nwk_str, "STAND_ALONE");
     break;
    default:
     sprintf(nwk_str, "ERR");
     break;
  }
  sl_zigbee_app_debug_println("%s   <%d>", nwk_str, get_time_ms());
}

/***************************************************************************/
/******************************   DAINTREE   *******************************/
/***************************************************************************/
// Since Daintree and Adaptiv attributes overlap
bool write_level_config_attribute(sl_zigbee_af_attribute_id_t attribute, uint8_t level)
{
  sl_zigbee_af_status_t status;
  uint8_t data[1] = {0};
  data[0] = level;

  // Both Adaptiv & Daintree support these attributes
  if((ZCL_MIN_LIGHT_LEVEL_ATTRIBUTE_ID == attribute) || (ZCL_MAX_LIGHT_LEVEL_ATTRIBUTE_ID == attribute))
  {
     status = sl_zigbee_af_write_manufacturer_specific_server_attribute(LIGHT_ENDPOINT,
                                                                   ZCL_CONFIGURATION_CLUSTER_ID,
                                                                   attribute,
                                                                   DAINTREE_ZIGBEE_MFG_ID,
                                                                   data,
                                                                   ZCL_INT8U_ATTRIBUTE_TYPE);
     if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != status)
     {
        sl_zigbee_app_debug_println("Err Daintree attr 0x%X", attribute);
        return(false);
     }

     status = sl_zigbee_af_write_manufacturer_specific_server_attribute(LIGHT_ENDPOINT,
                                                                   ZCL_ADAPTIV_CONFIGURATION_CLUSTER_ID,
                                                                   attribute,
                                                                   ADAPTIV_ZIGBEE_MFG_ID,
                                                                   data,
                                                                   ZCL_INT8U_ATTRIBUTE_TYPE);
     if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != status)
     {
        sl_zigbee_app_debug_println("ERR: Adaptiv attr 0x%X", attribute);
        return(false);
     }
  }
  else if(ZCL_MID_LIGHT_LEVEL_ATTRIBUTE_ID == attribute)
  {
     status = sl_zigbee_af_write_manufacturer_specific_server_attribute(LIGHT_ENDPOINT,
                                                                     ZCL_ADAPTIV_CONFIGURATION_CLUSTER_ID,
                                                                     attribute,
                                                                     ADAPTIV_ZIGBEE_MFG_ID,
                                                                     data,
                                                                     ZCL_INT8U_ATTRIBUTE_TYPE);
     if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != status)
     {
        sl_zigbee_app_debug_println("ERR: Adaptiv attr 0x%X", attribute);
        return(false);
     }
  }
  else
  {
     sl_zigbee_app_debug_println("ERR: Bad level attribute 0x%X", attribute);
     return(false);
  }
  return(true);
}

// Write only adaptiv mfg attribute.
bool write_adaptiv_occupancy_delay_attribute(sl_zigbee_af_attribute_id_t attribute, uint16_t delay)
{
  uint8_t data[2] = {0};
  sl_zigbee_af_status_t status;

  data[0] = GET_LSB_FROM_16(delay);
  data[1] = GET_MSB_FROM_16(delay);

  if((ZCL_MFG_UNOCCUPIED_DELAY_ATTRIBUTE_ID != attribute) &&
     (ZCL_MFG_DELAY_TO_OFF_ATTRIBUTE_ID != attribute))
  {
      sl_zigbee_app_debug_println("ERR: Bad delay attribute 0x%X", attribute);
      return(false);
  }
  status = sl_zigbee_af_write_manufacturer_specific_server_attribute(LIGHT_ENDPOINT,////////////////////////////////////OCC_SENSOR_ENDPOINT,
                                                                  ZCL_ADAPTIV_CONFIGURATION_CLUSTER_ID,
                                                                  attribute,
                                                                  ADAPTIV_ZIGBEE_MFG_ID,
                                                                  data,
                                                                  ZCL_INT16U_ATTRIBUTE_TYPE);
  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != status)
  {
     sl_zigbee_app_debug_println("ERR: Adaptiv delay attr 0x%X", attribute);
     return(false);
  }

  return(true);
}

bool write_light_temperature_attribute(uint16_t temperature)
{
  uint8_t data[2] = {0};
  sl_zigbee_af_status_t status;

  data[0] = GET_LSB_FROM_16(temperature);
  data[1] = GET_MSB_FROM_16(temperature);

  status = sl_zigbee_af_write_manufacturer_specific_server_attribute(LIGHT_ENDPOINT,
                                                                     ZCL_ADAPTIV_CONFIGURATION_CLUSTER_ID,
                                                                     ZCL_COLOR_TEMPERATURE_ATTRIBUTE_ID,
                                                                     ADAPTIV_ZIGBEE_MFG_ID,
                                                                     data,
                                                                     ZCL_INT16U_ATTRIBUTE_TYPE);
  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != status)
  {
     sl_zigbee_app_debug_println("ERR: Adaptiv color temp attr 0x%X", status);
     return(false);
  }
  return(true);
}

/***************************************************************************/
/**************************  PARSE ATTR  ***********************************/
/***************************************************************************/
static bool parse_new_attribute(uint16_t attr, uint8_t *data)
{
  bool cmd_handled = true;   // Assume cmd handled here
  uint16_t delay = 0;

  // Once again another shoot from the hip request,
  // Allow Adaptiv/twitch color config on dt nwks
  if((eDAINTREE_NWK == get_present_network()) && (ZCL_COLOR_TEMPERATURE_ATTRIBUTE_ID != attr))
  {
     return(false);
  }

  switch(attr)
  {
     case ZCL_MIN_LIGHT_LEVEL_ATTRIBUTE_ID:
        save_min_light_level(*data);
        break;

     case ZCL_MID_LIGHT_LEVEL_ATTRIBUTE_ID:
        save_mid_light_level(*data);
        break;

     case ZCL_MAX_LIGHT_LEVEL_ATTRIBUTE_ID:
        save_max_light_level(*data);
        break;

     case ZCL_ALLOW_OFF_ATTRIBUTE_ID:
        if((0 == *data) && (0 == read_on_off_attribute(ZCL_ON_OFF_ATTRIBUTE_ID)))
        {
           move_to_level_onoff(get_max_light_level(), 0);
        }

        save_allow_light_off(*data);
        break;

     case ZCL_COLOR_TEMPERATURE_ATTRIBUTE_ID:
        uint16_t color = GET_16_FROM_ARRAY(data, 0);
        save_light_color(color);
        break;

     case ZCL_MFG_UNOCCUPIED_DELAY_ATTRIBUTE_ID:
        delay = GET_16_FROM_ARRAY(data, 0);
        save_adaptiv_unoccupied_delay(delay);
        break;

     case ZCL_MFG_DELAY_TO_OFF_ATTRIBUTE_ID:
        delay = GET_16_FROM_ARRAY(data, 0);
        save_adaptiv_unoccupied_delay_to_off(delay);
        break;

     case ZCL_MFG_MANUAL_MODE_ATTRIBUTE_ID:
        set_app_flags_nonvol(B_MANUAL_ON_MODE, (0 == *data) ? 0 : 1);
        break;

     case ZCL_LIGHT_SENSOR_ATTRIBUTE_ID:
        uint16_t lux = GET_16_FROM_ARRAY(data, 0);
        save_light_lux_reference(lux);
        break;

     case ZCL_OCCUPANCY_SENSITIVITY_ATTRIBUTE_ID:
       uint8_t damon = *data;
       set_occupancy_threshold((uint16_t)damon);
       break;

     default:
        sl_zigbee_app_debug_println("ERR: Unknown Adaptiv Config attr 0x%X", attr);
        cmd_handled = false;
        break;
  }

  return(cmd_handled);
}

// Called from sl_zigbee_af_pre_command_received_cb().  If cmd is handled
// here will return true.  Else return false.
// To get here the cmd has already been checked to have;
//   Mfg specific with Adaptiv mfg id
//   Adaptiv Configuration cluster id
//   Direction from client to server
// NOTE:  All attributes for ep 2 or 3 are now handled on ep 1.
bool adaptiv_configuration_incoming_cmd(sl_zigbee_af_cluster_command_t* cmd)
{
  uint8_t n_payload_bytes = (cmd->bufLen - cmd->payloadStartIndex);
  uint8_t base_idx = cmd->payloadStartIndex;
  uint16_t attr = 0;
  uint8_t *data;

  // Write attribute with at least 1 attribute in payload
  if(((ZCL_WRITE_ATTRIBUTES_COMMAND_ID == cmd->commandId) ||
     (ZCL_WRITE_ATTRIBUTES_NO_RESPONSE_COMMAND_ID == cmd->commandId)) &&
     (4 <= n_payload_bytes))
  {
     bool cmd_handled = false;

     while(base_idx < cmd->bufLen)
     {
        // Parse
        attr = GET_16_FROM_ARRAY(cmd->buffer, base_idx);
        uint8_t type = cmd->buffer[base_idx+2];
        data = &cmd->buffer[base_idx+3];

        cmd_handled = parse_new_attribute(attr, data);

        if(!cmd_handled)
        {
           sl_zigbee_app_debug_println("FAIL: Parse wrt attr");
           break;
        }
        base_idx = base_idx + 3 + sl_zigbee_af_get_data_size(type);
     }
     force_an_occupied_event(); // Reset occ timers
     return(cmd_handled);
  }
  // Adaptiv reset commands
  else if((0x00 == cmd->commandId) && (3 <= n_payload_bytes))
  {
     // Reset cmd
     assert(1 == cmd->buffer[base_idx]);  // 1 = factory reset, else just reset
     uint32_t delay = (uint32_t)GET_16_FROM_ARRAY(cmd->buffer, (base_idx + 1)) * 1000UL;
     force_factory_reset(delay, eRESET_NORMAL);
     return(true);
  }
  else if((0x01 == cmd->commandId) && (3 <= n_payload_bytes))
  {
     // Reset cmd if joined but not in group
     if(!device_is_in_a_group())
     {
        assert(1 == cmd->buffer[base_idx]);  // 1 = factory reset, else just reset
        uint32_t delay = (uint32_t)GET_16_FROM_ARRAY(cmd->buffer, (base_idx + 1)) * 1000UL;
        force_factory_reset(delay, eRESET_NORMAL);
     }
     return(true);
  }
  else
  {
     // Unsupported cmd
     return(false);
  }
  return(false);
}

