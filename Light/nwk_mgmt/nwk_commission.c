#include "af.h"
// SDK 2025.6.3: hal.h no longer pulls in the legacy token API, which
// provides the TOKEN_APP_* ids and types from sl_custom_token_header.h.
#include "sl_token_api.h"
#include "af-main.h"

#include "sl_event_handler.h"
#include "sl_token_manager.h"
#include "attribute-storage.h"
#include "sl_zigbee_address_info.h"
#include "sl_zigbee_dynamic_commissioning.h"
#include "factory_reset.h"
#include "nwk_groups.h"
#include "nwk_utils.h"
#include "nwk_commission.h"

#include "sl_zigbee_debug_print.h"

//#define PRINT_COMMISSION

// Local vars
static commissioning_set_t m_active_commissioning_set = {0};
static uint8_t m_config_sequence = 0;

// Fwd declarations
static void _commissioning_set_load_default(commissioning_set_t *p_set);



static void tx_commission_response(sl_zigbee_af_cluster_command_t *cmd, uint8_t rsp_id, sl_status_t status)
{
  uint8_t message[4] ={0};
  message[0] = (ZCL_FRAME_CONTROL_SERVER_TO_CLIENT | ZCL_CLUSTER_SPECIFIC_COMMAND);
  message[1] = cmd->seqNum;
  message[2] = rsp_id;
  message[3] = status;

  sl_zigbee_aps_frame_t apsFrame = {0};
  fill_in_apsFrame(cmd, &apsFrame);

  sl_zigbee_af_send_unicast(SL_ZIGBEE_OUTGOING_DIRECT,
                            cmd->source,
                            &apsFrame,
                            sizeof(message),
                            (uint8_t *)&message[0]);
}

static bool read_commisioning_attributes(sl_zigbee_af_attribute_id_t attribute,
                                                             uint8_t *data,
                                                             uint8_t size)
{
  sl_zigbee_af_status_t status;
  memset(data, 0, size);
  status = sl_zigbee_af_read_server_attribute(LIGHT_ENDPOINT,
                                              ZCL_COMMISSIONING_CLUSTER_ID,
                                              attribute,
                                              data,
                                              size);

  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != status)
  {
     sl_zigbee_app_debug_println("ERR:0x%X reading commission attribute 0x%X",
                                                         status, attribute);
  }
  return(SL_ZIGBEE_ZCL_STATUS_SUCCESS == status);
}

static sl_zigbee_af_status_t _copy_commissioning_attributes_to_set(commissioning_set_t *p_set)
{
  sl_zigbee_af_status_t status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;

  status |= sl_zigbee_af_read_server_attribute(LIGHT_ENDPOINT,
                                               ZCL_COMMISSIONING_CLUSTER_ID,
                                               ZCL_STACK_PROFILE_ATTRIBUTE_ID,
                                               (uint8_t *)&p_set->stack_profile,
                                               sizeof(p_set->stack_profile));
  status |= sl_zigbee_af_read_server_attribute(LIGHT_ENDPOINT,
                                               ZCL_COMMISSIONING_CLUSTER_ID,
                                               ZCL_STARTUP_CONTROL_ATTRIBUTE_ID,
                                               (uint8_t *)&p_set->startup_control,
                                               sizeof(p_set->startup_control));
  status |= sl_zigbee_af_read_server_attribute(LIGHT_ENDPOINT,
                                               ZCL_COMMISSIONING_CLUSTER_ID,
                                               ZCL_PROTOCOL_VERSION_ATTRIBUTE_ID,
                                               (uint8_t *)&p_set->protocol_version,
                                               sizeof(p_set->protocol_version));
  status |= sl_zigbee_af_read_server_attribute(LIGHT_ENDPOINT,
                                               ZCL_COMMISSIONING_CLUSTER_ID,
                                               ZCL_PAN_ID_ATTRIBUTE_ID,
                                               (uint8_t *)&p_set->panId,
                                               sizeof(p_set->panId));
  status |= sl_zigbee_af_read_server_attribute(LIGHT_ENDPOINT,
                                               ZCL_COMMISSIONING_CLUSTER_ID,
                                               ZCL_CHANNEL_MASK_ATTRIBUTE_ID,
                                               (uint8_t *)&p_set->channel_mask,
                                               sizeof(p_set->channel_mask));
  status |= sl_zigbee_af_read_server_attribute(LIGHT_ENDPOINT,
                                               ZCL_COMMISSIONING_CLUSTER_ID,
                                               ZCL_EXTENDED_PAN_ID_ATTRIBUTE_ID,
                                               (uint8_t *)&p_set->extended_pan_id,
                                               sizeof(p_set->extended_pan_id));
  status |= sl_zigbee_af_read_server_attribute(LIGHT_ENDPOINT,
                                               ZCL_COMMISSIONING_CLUSTER_ID,
                                               ZCL_PRECONFIGURED_LINK_KEY_ATTRIBUTE_ID,
                                               (uint8_t *)&p_set->preconfig_link_key,
                                               sizeof(p_set->preconfig_link_key));

  #ifdef PRINT_COMMISSION
  sl_zigbee_app_debug_println("_copy_commissioning_attributes_to_set status 0x%X", status);
  #endif

  return(status);
}

static sl_zigbee_af_status_t write_commissioning_attribute(sl_zigbee_af_attribute_id_t attributeID, uint8_t *data)
{
  sl_zigbee_af_attribute_type_t type;
  sl_zigbee_af_status_t status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;

  switch(attributeID)
  {
     case ZCL_STACK_PROFILE_ATTRIBUTE_ID:
        type = ZCL_INT8U_ATTRIBUTE_TYPE;
        break;
     case ZCL_STARTUP_CONTROL_ATTRIBUTE_ID:
        type = ZCL_ENUM8_ATTRIBUTE_TYPE;
        break;
     case ZCL_PROTOCOL_VERSION_ATTRIBUTE_ID:
        type = ZCL_INT8U_ATTRIBUTE_TYPE;
        break;
     case ZCL_PAN_ID_ATTRIBUTE_ID:
        type = ZCL_INT16U_ATTRIBUTE_TYPE;
        break;
     case ZCL_CHANNEL_MASK_ATTRIBUTE_ID:
        type = ZCL_BITMAP32_ATTRIBUTE_TYPE;
        break;
     case ZCL_EXTENDED_PAN_ID_ATTRIBUTE_ID:
        type = ZCL_IEEE_ADDRESS_ATTRIBUTE_TYPE;
        break;
     case ZCL_PRECONFIGURED_LINK_KEY_ATTRIBUTE_ID:
        type = ZCL_SECURITY_KEY_ATTRIBUTE_TYPE;
        break;
     default:
       sl_zigbee_app_debug_println("Unsupported commision attr");
       return(SL_ZIGBEE_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE);
  }

  status = sl_zigbee_af_write_server_attribute(LIGHT_ENDPOINT,
                                               ZCL_COMMISSIONING_CLUSTER_ID,
                                               attributeID,
                                               data,
                                               type);
  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != status)
  {
     sl_zigbee_app_debug_println("ERR:0x%X write_commissioning_attribute 0x%X",
                                                          status, attributeID);
  }

  #ifdef PRINT_COMMISSION
  sl_zigbee_app_debug_println("write_commissioning_attribute 0x%X status 0x%X",
                              attributeID, status);
  #endif

  return(status);
}

static sl_zigbee_af_status_t _commissioning_save_set_to_nonvol(commissioning_set_t *p_set)
{
  sl_zigbee_af_status_t status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
  assert(p_set != NULL);
  status = sl_token_set_data(TOKEN_APP_COMMISSION_CHANNEL_MASK, 0,
                            &p_set->channel_mask, sizeof(p_set->channel_mask));
  status |= sl_token_set_data(TOKEN_APP_COMMISSION_EXTENDED_PAN_ID, 0,
                              p_set->extended_pan_id, sizeof(p_set->extended_pan_id));
  status |= sl_token_set_data(TOKEN_APP_COMMISSION_PAN_ID, 0,
                            &p_set->panId, sizeof(p_set->panId));
  status |= sl_token_set_data(TOKEN_APP_COMMISSION_LINK_KEY, 0,
                              p_set->preconfig_link_key, sizeof(p_set->preconfig_link_key));
  status |= sl_token_set_data(TOKEN_APP_COMMISSION_STARTUP_CONTROL, 0,
                            &p_set->startup_control, sizeof(p_set->startup_control));

  #ifdef PRINT_COMMISSION
  sl_zigbee_app_debug_println("_commissioning_save_set_to_nonvol  status 0x%X", status);
  #endif

  return(status);
}

static sl_zigbee_af_status_t _commissioning_copy_nonvol_to_set(commissioning_set_t *p_set)
{
  assert(p_set != NULL);
  sl_zigbee_af_status_t status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
  // Since not all attributes are saved to nonvol, first read everything from defaults.  This will
  // correctly populate any attributes that are not saved in nonvol, as if the default values were
  // saved to nonvol.
  _commissioning_set_load_default(p_set);


  status |= sl_token_get_data(TOKEN_APP_COMMISSION_CHANNEL_MASK, 0,
                    &p_set->channel_mask, sizeof(p_set->channel_mask));

  status |= sl_token_get_data(TOKEN_APP_COMMISSION_EXTENDED_PAN_ID, 0,
                              p_set->extended_pan_id, sizeof(p_set->extended_pan_id));

  status |= sl_token_get_data(TOKEN_APP_COMMISSION_PAN_ID, 0,
                    &p_set->panId, sizeof(p_set->panId));

  status |= sl_token_get_data(TOKEN_APP_COMMISSION_LINK_KEY, 0,       // was working with '1' (one)
                              p_set->preconfig_link_key, sizeof(p_set->preconfig_link_key));

  status |= sl_token_get_data(TOKEN_APP_COMMISSION_STARTUP_CONTROL, 0,
                      &p_set->startup_control, sizeof(p_set->startup_control));

  #ifdef PRINT_COMMISSION
  sl_zigbee_app_debug_println("_commissioning_copy_nonvol_to_set  status 0x%X", status);
  #endif

  return(status);
}

bool check_switch_network(void)
{
  uint8_t data[1] = {0};

  if(read_commisioning_attributes(ZCL_STARTUP_CONTROL_ATTRIBUTE_ID, data, 1))
  {
     return( (0 == data[0]) ? false : true);
  }
  return(false);
}

static void _commissioning_set_load_default(commissioning_set_t *p_set)
{
  assert(p_set != NULL);
  memcpy((uint8_t*)p_set, (uint8_t*)&m_default_commissioning_set, sizeof(commissioning_set_t));
  m_config_sequence++;

  #ifdef PRINT_COMMISSION
  sl_zigbee_app_debug_println("_commissioning_set_load_default, seq=%d", m_config_sequence);
  #endif
}

static bool _is_commissioning_set_valid(commissioning_set_t *p_set)
{
    assert(p_set != NULL);
    bool valid_channel_mask = ((p_set->channel_mask & SL_ZIGBEE_ALL_802_15_4_CHANNELS_MASK) == p_set->channel_mask);
    bool key_is_valid = false;
    for(uint8_t i = 0; i < SL_ZIGBEE_ENCRYPTION_KEY_SIZE; i++)
    {
        if((p_set->preconfig_link_key[i] != 0) && (p_set->preconfig_link_key[i] != 0xFF))
        {
            // at least one byte is not 0x00 nor 0xFF
            key_is_valid = true;
            break;
        }
    }
    bool is_valid = ((p_set->stack_profile == 2) &&
            (p_set->protocol_version == SL_ZIGBEE_STACK_PROFILE_ZIGBEE_PRO) &&
            ((p_set->startup_control == STARTUP_CONTROL_ASSOCIATION) || (p_set->startup_control == STARTUP_CONTROL_REJOIN)) &&
            (valid_channel_mask == true) && (key_is_valid == true));

    #ifdef PRINT_COMMISSION
    sl_zigbee_app_debug_println("_is_commissioning_set_valid=%s  (%d.%d.%d.%d.%d, 0x%04x)", is_valid ? "Yes" : "No", p_set->stack_profile, p_set->protocol_version,
                                    p_set->startup_control,valid_channel_mask, key_is_valid, p_set->channel_mask);
    #endif

    return is_valid;
}

static sl_zigbee_af_status_t _copy_commissioning_set_to_attributes(commissioning_set_t *p_set)
{
  sl_zigbee_af_status_t status = SL_ZIGBEE_ZCL_STATUS_FAILURE;
  assert(p_set != NULL);
  if(_is_commissioning_set_valid(&m_active_commissioning_set) == true)
  {
     status = write_commissioning_attribute(ZCL_EXTENDED_PAN_ID_ATTRIBUTE_ID, p_set->extended_pan_id);
     status |= write_commissioning_attribute(ZCL_PAN_ID_ATTRIBUTE_ID, (uint8_t*)&p_set->panId);
     status |= write_commissioning_attribute(ZCL_CHANNEL_MASK_ATTRIBUTE_ID, (uint8_t*)&p_set->channel_mask);
     status |= write_commissioning_attribute(ZCL_PROTOCOL_VERSION_ATTRIBUTE_ID, &p_set->protocol_version);
     status |= write_commissioning_attribute(ZCL_STACK_PROFILE_ATTRIBUTE_ID, &p_set->stack_profile);
     status |= write_commissioning_attribute(ZCL_STARTUP_CONTROL_ATTRIBUTE_ID, (uint8_t*)&p_set->startup_control);
     status |= write_commissioning_attribute(ZCL_PRECONFIGURED_LINK_KEY_ATTRIBUTE_ID, p_set->preconfig_link_key);
  }

  #ifdef PRINT_COMMISSION
  sl_zigbee_app_debug_println("_copy_commissioning_set_to_attributes status:0x%X", status);
  #endif

  return status;
}

uint32_t adaptiv_commissioning_cluster_handler(sl_service_opcode_t opcode, sl_service_function_context_t *context)
{
  sl_zigbee_af_cluster_command_t *cmd = (sl_zigbee_af_cluster_command_t *)context->data;
  uint8_t options = 0;
  uint8_t delay = 0;
  uint8_t index_base = 0;
  uint8_t jitter = 0;
  uint8_t rsp_id = ZCL_RESTART_DEVICE_RESPONSE_COMMAND_ID;
  sl_zigbee_af_status_t status = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;

  // Should be cluster specific and client to server and have some data
  if((0 == cmd->clusterSpecific) || (0 != cmd->direction) || (2 >= cmd->bufLen))
  {
     sl_zigbee_app_debug_println("COMM BAD %d %d %d",cmd->clusterSpecific,cmd->direction,cmd->bufLen);
     return(status);
  }

  index_base = cmd->payloadStartIndex;
  options = cmd->buffer[index_base];

  switch(cmd->commandId)
  {
     case ZCL_RESTART_DEVICE_COMMAND_ID:
        delay = cmd->buffer[index_base+1];   // Secs
        jitter = cmd->buffer[index_base+2];  // mSecs mask

        uint32_t delay_ms = (uint32_t)delay * 1000UL;
        delay_ms += ((uint32_t)jitter * 80UL) & sl_zigbee_get_pseudo_random_number();
        if(100UL > delay_ms)
        {
           delay_ms = 500UL;
        }
        bool restart_with_startup_params = ((options & 0x07) == 0);

        #ifdef PRINT_COMMISSION
        sl_zigbee_app_debug_println("adaptiv_commissioning_cluster_handler options 0x%X delay %d mS",
                                                                 options, delay_ms);
        #endif

        if(true == restart_with_startup_params)
        {
           // To get here, hopefully Daintree has written to the commissioning
           // attributes, in particular ZCL_STARTUP_CONTROL_ATTRIBUTE_ID
           if(SL_ZIGBEE_ZCL_STATUS_SUCCESS == _copy_commissioning_attributes_to_set(&m_active_commissioning_set))
           {
              if(_is_commissioning_set_valid(&m_active_commissioning_set))
              {
                  _commissioning_save_set_to_nonvol(&m_active_commissioning_set);
                 status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;

                 switch(m_active_commissioning_set.startup_control)
                 {
                   case 0:
                       // Do nothing
                       break;
                    case 1:
                       // Form a nwk
                       status = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
                       break;
                    case 2:
                       // Set up state machine to flip to next nwk
                       set_app_flags_nonvol(B_SWITCH_NWK, 1);
                       force_factory_reset(delay_ms, eRESET_DAINTREE);

                       #ifdef PRINT_COMMISSION
                       sl_zigbee_app_debug_println("Starting to switch to new nwk");
                       spew_commissioning_set(&m_active_commissioning_set);
                       #endif

                       break;
                    case 3:
                       // Start from scratch
                       force_factory_reset(delay_ms, eRESET_NORMAL);

                       #ifdef PRINT_COMMISSION
                       sl_zigbee_app_debug_println("Leave nwk via factory reset");
                       spew_commissioning_set(&m_active_commissioning_set);
                       #endif

                       break;
                    default:
                       status = SL_ZIGBEE_ZCL_STATUS_INCONSISTENT_STARTUP_STATE;
                       break;
                 }
              }
              else
               {
                 // Invalid info
                 status = SL_ZIGBEE_ZCL_STATUS_INCONSISTENT_STARTUP_STATE;
               }
           }
           else
           {
              sl_zigbee_app_debug_println("Restart bad copy");
              status = SL_ZIGBEE_ZCL_STATUS_INCONSISTENT_STARTUP_STATE;
           }
        }
        else
        {
           status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
        }

        rsp_id = ZCL_RESTART_DEVICE_RESPONSE_COMMAND_ID;
        // status = set above
        break;

     case ZCL_SAVE_STARTUP_PARAMETERS_COMMAND_ID:
        sl_zigbee_app_debug_println("ZCL_SAVE_STARTUP_PARAMETERS_COMMAND_ID  not used");

        rsp_id = ZCL_SAVE_STARTUP_PARAMETERS_RESPONSE_COMMAND_ID;
        status = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
        break;

     case ZCL_RESTORE_STARTUP_PARAMETERS_COMMAND_ID:
        sl_zigbee_app_debug_println("ZCL_RESTORE_STARTUP_PARAMETERS_COMMAND_ID  not used");

        rsp_id = ZCL_RESTORE_STARTUP_PARAMETERS_RESPONSE_COMMAND_ID;
        status = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
        break;

     case ZCL_RESET_STARTUP_PARAMETERS_COMMAND_ID:
        if(0x03 & options)
        {
           _commissioning_set_load_default(&m_active_commissioning_set);
           _copy_commissioning_set_to_attributes(&m_active_commissioning_set);
        }
        rsp_id = ZCL_RESET_STARTUP_PARAMETERS_RESPONSE_COMMAND_ID;
        status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;

        #ifdef PRINT_COMMISSION
        sl_zigbee_app_debug_println("ZCL_RESET_STARTUP_PARAMETERS_COMMAND_ID options 0x%X", options);
        #endif

        break;

     default:
        status = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
        break;
  }
  tx_commission_response(cmd, rsp_id, status);

  sl_zigbee_app_debug_println("adaptiv_commissioning_cluster_handler status 0x%X\n", status);
  return(status);
}

// This stack has no call back to the commissioning cluster.  Therefore, we have to
// subscribe to all cmds to the commissioning cluster and implement them.
void zcl_commissioning_init(void)
{
  if(SL_STATUS_OK != sl_zigbee_subscribe_to_zcl_commands(ZCL_COMMISSIONING_CLUSTER_ID,
                                                         0xFFFF,
                                                         ZCL_DIRECTION_CLIENT_TO_SERVER,
                                                         adaptiv_commissioning_cluster_handler))
  {
     sl_zigbee_app_debug_println(">>>ERR: Cannot subscribe commissioning cluster<<<");
  }
  else
  {
     sl_zigbee_app_debug_println("Subscribed to commissioning cluster successful");
  }
}

// Read from nonvol, make sure valid, load into passed in nwkParams
// If all good returns true.
bool load_commissioning_nwk_parameters(sl_zigbee_network_parameters_t *nwkParams)
{
  sl_zigbee_af_status_t status = SL_ZIGBEE_ZCL_STATUS_INCONSISTENT_STARTUP_STATE;

  memset(&m_active_commissioning_set, 0, sizeof(commissioning_set_t));
  status = _commissioning_copy_nonvol_to_set(&m_active_commissioning_set);
  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != status)
  {
     sl_zigbee_app_debug_println("ERR:load_commissioning_nwk_parameters bad read nonvol");
     spew_commissioning_set(&m_active_commissioning_set);
     return(false);
  }

  if(!_is_commissioning_set_valid(&m_active_commissioning_set))
  {
     sl_zigbee_app_debug_println("ERR:load_commissioning_nwk_parameters Not valid");
     spew_commissioning_set(&m_active_commissioning_set);
     return(false);
  }

  nwkParams->channels = m_active_commissioning_set.channel_mask;
  memcpy(&nwkParams->extendedPanId, &m_active_commissioning_set.extended_pan_id,
                          sizeof(m_active_commissioning_set.extended_pan_id));
  nwkParams->panId = m_active_commissioning_set.panId;
  //m_active_commissioning_set.preconfig_link_key
  return(true);
}


bool load_commissioning_preconfiguredKey(sl_zigbee_key_data_t *key)
{
  sl_zigbee_af_status_t status = SL_ZIGBEE_ZCL_STATUS_INCONSISTENT_STARTUP_STATE;

  memset(&m_active_commissioning_set, 0, sizeof(commissioning_set_t));
  status = _commissioning_copy_nonvol_to_set(&m_active_commissioning_set);
  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != status)
  {
     sl_zigbee_app_debug_println("ERR:load_commissioning_preconfiguredKey bad read nonvol");
     spew_commissioning_set(&m_active_commissioning_set);
     return(false);
  }
  memcpy(&key, &m_active_commissioning_set.preconfig_link_key,
                      sizeof(m_active_commissioning_set.preconfig_link_key));
  return(true);
}

sl_zigbee_af_status_t commissioning_copy_nonvol_to_set(commissioning_set_t *p_set)
{
  return(_commissioning_copy_nonvol_to_set(p_set));
}

#ifdef PRINT_COMMISSION
void spew_commissioning_set(commissioning_set_t *p_set)
{
  uint8_t data[16] = {0};

  sl_zigbee_app_debug_println("  -Startup control %d", p_set->startup_control);

  sl_zigbee_app_debug_println("  -Stack profile 0x%X", p_set->stack_profile);

  sl_zigbee_app_debug_println("  -Protocol ver 0x%X", p_set->protocol_version);

  sl_zigbee_app_debug_println("  -PAN 0x%X", p_set->panId);

  sl_zigbee_app_debug_println("  -Chan Mask 0x%X", p_set->channel_mask);

  sl_zigbee_app_debug_print("  -Ext PAN ");
  memcpy(data, &p_set->extended_pan_id, sizeof(p_set->extended_pan_id));
  sl_zigbee_af_print_little_endian_eui64(data);
  sl_zigbee_app_debug_println(" ");

  sl_zigbee_app_debug_print("  -PreConfig Key ");
  memcpy(data, &p_set->preconfig_link_key, sizeof(p_set->preconfig_link_key));
  sl_zigbee_af_print_little_endian_eui64((uint8_t *)&p_set->preconfig_link_key);
  sl_zigbee_app_debug_print(": ");
  sl_zigbee_af_print_little_endian_eui64((uint8_t *)&p_set->preconfig_link_key[8]);
  sl_zigbee_app_debug_println(" ");

}
#else
void spew_commissioning_set(commissioning_set_t *p_set) { (void)(*p_set); }
#endif

