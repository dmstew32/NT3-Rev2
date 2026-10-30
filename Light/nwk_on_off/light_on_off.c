#include "af.h"
// SDK 2025.6.3: hal.h no longer pulls in the legacy token API, which
// provides the TOKEN_APP_* ids and types from sl_custom_token_header.h.
#include "sl_token_api.h"
#include "light_on_off.h"
#include "RehTOS.h"
#include "hal_utils.h"
#include "hal_pwm.h"
#include "hal_leds.h"
#include "sl_event_handler.h"
#include "nwk_utils.h"
#include "sl_token_manager.h"
#include "nwk_groups.h"
#include "hal_occ_sensor.h"
#include "occ_sensor.h"
#include "adaptiv_config.h"

#include "sl_zigbee_debug_print.h"

static uint8_t _max_light_level = DEFAULT_MAX_LIGHT_LEVEL;
static uint8_t _mid_light_level = DEFAULT_MID_LIGHT_LEVEL;
static uint8_t _min_light_level = DEFAULT_MIN_LIGHT_LEVEL;
static bool _allow_light_off = DEFAULT_ALLOW_LIGHT_OFF;

static sl_zigbee_af_event_t dim_lights_up_down_event;
static uint16_t dim_cycles = 0;
static uint16_t dim_period_Dsecs = 10;
static uint8_t dim_original_level = DEFAULT_MAX_LIGHT_LEVEL;

void dim_lights_up_down_handler(void);


// This occurs, only ONCE after a POR, will turn on the light, init attr's
void power_on_set_light_level(void)
{
  uint8_t initial_level = hal_get_dim_pwm_255();

  write_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID, &initial_level);
  write_light_level_attribute(ZCL_START_UP_CURRENT_LEVEL_ATTRIBUTE_ID, (uint8_t *)&initial_level);

  // Read from non-vol and populate attributes
  // Max, mid, min, allow off, occ delays, color etc
  // This will also set light to max
  init_light_parameters();

  sl_zigbee_app_debug_println("POR Light set to %d%%", (100*initial_level)/254);

  sl_zigbee_af_event_init(&dim_lights_up_down_event, dim_lights_up_down_handler);
}


void dim_lights_up_down_handler(void)
{
  if(dim_cycles)
  {
     if(0x01 & dim_cycles)
     {
        move_to_level_onoff(DEFAULT_MIN_LIGHT_LEVEL_DT, dim_period_Dsecs);
     }
     else
     {
        move_to_level_onoff(get_max_light_level(), dim_period_Dsecs);
     }
     dim_cycles--;
     sl_zigbee_af_event_set_delay_ms(&dim_lights_up_down_event,
                                 ((uint32_t)dim_period_Dsecs*100UL) + 250UL);
  }
  else
  {
     // Make sure
     //move_to_level_onoff(dim_original_level, 0);
     move_to_level_onoff(get_max_light_level(), 0);
  }
}

// Ramp_light up and dn, ramp time Dsecs, typical DSECS_HALF_SECS.
// Cycles = 1 up/dn cycle. 0 cycles = turn off
void dim_lights_up_down(uint16_t cycles, uint16_t ramp_Dsecs)
{
  dim_cycles = 2*(0x7FFF & cycles);
  dim_period_Dsecs = ramp_Dsecs;

  if(!sl_zigbee_af_event_is_scheduled(&dim_lights_up_down_event))
  {
     dim_original_level = get_present_light_level();
     sl_zigbee_af_event_set_active(&dim_lights_up_down_event);
  }
  else if(0 == cycles)
  {
     // Turn off flashing
     sl_zigbee_af_event_set_inactive(&dim_lights_up_down_event);
     //move_to_level_onoff(dim_original_level, 0);
     move_to_level_onoff(get_max_light_level(), 0);

  }
  else
  {
     sl_zigbee_app_debug_println("Warning:dim_lights_up_down already active");
  }
}

bool are_lights_dimming_up_dowm(void)
{
  return((0 != dim_cycles) || sl_zigbee_af_event_is_scheduled(&dim_lights_up_down_event));
}

/***************************************************************************/
/**************************  LEVELS  ***************************************/
/***************************************************************************/
// parm0 = time stamp, parm1 = delay
static void set_light_to_max_level_task(taskParm_t parm0, taskParm_t parm1)
{

  if(has_timed_out(parm0.u32, parm1.u32) && !are_lights_dimming_up_dowm())
  {
     removeTask(set_light_to_max_level_task);
     move_to_level_onoff(get_max_light_level(), 0);
  }
}
// New requirement, 6/21/26
static void start_task_move_to_max_level(void)
{
  taskParm_t arg0, arg1;

  arg0.u32 = get_time_ms();
  arg1.u32 = 1000UL;
  addTask(set_light_to_max_level_task, arg0, arg1);
}

static void adjust_light_level_for_new_min_limit(uint8_t new_min_limit)
{
  if(get_present_light_level() < new_min_limit)
  {
     move_to_level_onoff(new_min_limit, 0);
  }
}
static void adjust_light_level_for_new_mid_limit(uint8_t new_mid_limit)
{
  if((get_max_light_level() > new_mid_limit) && (get_min_light_level() < new_mid_limit))
  {
     return;
  }
  else
  {
     move_to_level_onoff(get_max_light_level(), 0);
  }
}
static void adjust_light_level_for_new_max_limit(uint8_t new_max_limit)
{
  if(get_present_light_level() > new_max_limit)
  {
     move_to_level_onoff(new_max_limit, 0);
  }
}

// Called from both the Adaptiv & Daintree write attribute cmd, and from
// button presses on this light.   Will non-vol save & init in both Adaptive &
// Daintree's attributes
void save_min_light_level(uint8_t new_min_level)
{
  adjust_light_level_for_new_min_limit(new_min_level);

  // Save local variable and write novol
  _min_light_level = new_min_level;
  sl_token_set_data(TOKEN_APP_LIGHT_LEVEL_MIN, 0, &_min_light_level, 1);

  if(write_level_config_attribute(ZCL_MIN_LIGHT_LEVEL_ATTRIBUTE_ID, _min_light_level))
  {
     sl_zigbee_app_debug_println("MIN level nvm = %d", _min_light_level);
  }
  init_min_max_silab_levels();
}

// Save and init only Adaptiv attribute
void save_mid_light_level(uint8_t new_mid_level)
{
  adjust_light_level_for_new_mid_limit(new_mid_level);

  // Save local variable and write novol
  _mid_light_level = new_mid_level;
  sl_token_set_data(TOKEN_APP_LIGHT_LEVEL_MID, 0, &_mid_light_level, 1);

  if(write_level_config_attribute(ZCL_MID_LIGHT_LEVEL_ATTRIBUTE_ID, _mid_light_level))
  {
     sl_zigbee_app_debug_println("MID level nvm = %d", _mid_light_level);
  }
}

// Called from both the Adaptiv & Daintree write attribute cmd, and from
// button presses on this light.   Will non-vol save & init in both Adaptive &
// Daintree's attributes
void save_max_light_level(uint8_t new_max_level)
{
  adjust_light_level_for_new_max_limit(new_max_level);

  // Save local variable and write novol
  _max_light_level = new_max_level;
  sl_token_set_data(TOKEN_APP_LIGHT_LEVEL_MAX, 0, &_max_light_level, 1);

  if(write_level_config_attribute(ZCL_MAX_LIGHT_LEVEL_ATTRIBUTE_ID, _max_light_level))
  {
     sl_zigbee_app_debug_println("MAX level nvm = %d", _max_light_level);
  }
  init_min_max_silab_levels();

  // If new max level, go there
  start_task_move_to_max_level();
}

// Daintree only
void save_allow_light_off(bool new_allow_off)
{
  // Save local variable and write novol
  _allow_light_off = new_allow_off;
  sl_token_set_data(TOKEN_APP_ALLOW_LIGHT_OFF, 0, &_allow_light_off, 1);

  sl_zigbee_af_status_t status;
  status = sl_zigbee_af_write_manufacturer_specific_server_attribute(LIGHT_ENDPOINT,
                                                                     ZCL_CONFIGURATION_CLUSTER_ID,
                                                                     ZCL_ALLOW_OFF_ATTRIBUTE_ID,
                                                                     DAINTREE_ZIGBEE_MFG_ID,
                                                                     (uint8_t *)&_allow_light_off,
                                                                     ZCL_BOOLEAN_ATTRIBUTE_TYPE);
  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != status)
  {
     sl_zigbee_app_debug_println("ERR Daintree wrt attr allow off");
  }
  else
  {
     sl_zigbee_app_debug_println("Allow light off nvm = %d", _allow_light_off);
  }
}

uint8_t get_min_light_level(void)
{
  return(_min_light_level);
}
uint8_t get_mid_light_level(void)
{
  return(_mid_light_level);
}
uint8_t get_max_light_level(void)
{
  return(_max_light_level);
}
bool allow_light_off(void)
{
  return(_allow_light_off);
}

// Read nonvol and place into vars and attributes via save_ funcs
// This occurs once at power up
void init_light_parameters(void)
{
  /***  DIM LEVELS ***/
  sl_token_get_data(TOKEN_APP_LIGHT_LEVEL_MIN, 0, &_min_light_level, 1);
  save_min_light_level(_min_light_level);

  sl_token_get_data(TOKEN_APP_LIGHT_LEVEL_MID, 0, &_mid_light_level, 1);
  save_mid_light_level(_mid_light_level);

  sl_token_get_data(TOKEN_APP_LIGHT_LEVEL_MAX, 0, &_max_light_level, 1);
  save_max_light_level(_max_light_level);

  sl_token_get_data(TOKEN_APP_ALLOW_LIGHT_OFF, 0, &_allow_light_off, 1);
  save_allow_light_off(_allow_light_off);

  /***  OCC SENSOR  ***/
  uint16_t delay;
  sl_token_get_data(TOKEN_APP_UNOCCUPIED_DELAY, 0, &delay, sizeof(delay));
  save_adaptiv_unoccupied_delay(delay);
  // NOTE: Daintree delay is tokenized on EP 2 Cluster 0x406

  sl_token_get_data(TOKEN_APP_UNOCCUPIED_DELAY_TO_OFF, 0, &delay, sizeof(delay));
  save_adaptiv_unoccupied_delay_to_off(delay);

  uint16_t data = DEFAULT_OCC_SENSOR_DELAY_TO_OCC;
  write_occupancy_sensor_attribute(ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_DELAY_ATTRIBUTE_ID, (uint8_t *)&data);

  sl_token_get_data(TOKEN_APP_COLOR_TEMP_K, 0, &delay, 2);
  write_light_temperature_attribute(delay);
}

void fat_finger(void)
{
  uint8_t data[4] = {0,};

  data[0] = DEFAULT_MAX_LIGHT_LEVEL;
  data[1] = DEFAULT_MID_LIGHT_LEVEL;
  data[2] = DEFAULT_MIN_LIGHT_LEVEL;
  data[3] = DEFAULT_ALLOW_LIGHT_OFF;
  sl_token_set_data(TOKEN_APP_LIGHT_LEVEL_MAX, 0, &data[0], 1);
  sl_token_set_data(TOKEN_APP_LIGHT_LEVEL_MID, 0, &data[1], 1);
  sl_token_set_data(TOKEN_APP_LIGHT_LEVEL_MIN, 0, &data[2], 1);
  sl_token_set_data(TOKEN_APP_ALLOW_LIGHT_OFF, 0, &data[3], 1);

  data[0] = GET_LSB_FROM_16(DEFAULT_OCC_SENSOR_DELAY);
  data[1] = GET_MSB_FROM_16(DEFAULT_OCC_SENSOR_DELAY);
  data[2] = GET_LSB_FROM_16(DEFAULT_OCC_SENSOR_DELAY_TO_OFF);
  data[3] = GET_MSB_FROM_16(DEFAULT_OCC_SENSOR_DELAY_TO_OFF);
  sl_token_set_data(TOKEN_APP_UNOCCUPIED_DELAY, 0, &data[0], 2);
  sl_token_set_data(TOKEN_APP_UNOCCUPIED_DELAY_TO_OFF, 0, &data[2], 2);

  init_light_parameters();
  sl_zigbee_app_debug_println("FAT FINGER");
}

/***************************************************************************/
/****************************  DAINTREE  ***********************************/
/***************************************************************************/
// Called from sl_zigbee_af_pre_command_received_cb()
// Returns true if command is executed here
bool get_daintree_configuration_cmd(sl_zigbee_af_cluster_command_t* cmd)
{
  uint8_t n_payload_bytes = (cmd->bufLen - cmd->payloadStartIndex);
  uint8_t index = cmd->payloadStartIndex;
  uint16_t attributes[4] = {0};
  uint8_t n_attrs = 0;

  // Read all requested attributes.  Make sure at least 1 attribute in payload
  if(2 <= n_payload_bytes)
  {
     // Get 1st attr
      attributes[n_attrs] = (uint16_t)cmd->buffer[index] | ((uint16_t)cmd->buffer[index+1] << 8);
      n_attrs++;
      index += 2;
      if(4 <= n_payload_bytes)
      {
        attributes[n_attrs] = (uint16_t)cmd->buffer[index] | ((uint16_t)cmd->buffer[index+1] << 8);
        n_attrs++;
        index += 2;
      }
      if(6 <= n_payload_bytes)
      {
        attributes[n_attrs] = (uint16_t)cmd->buffer[index] | ((uint16_t)cmd->buffer[index+1] << 8);
        n_attrs++;
        index += 2;
      }
  }
  else
  {
     return(false); // No attributes
  }

  uint8_t message[32] = {0};
  index = 0;
  message[index++] = (ZCL_MANUFACTURER_SPECIFIC_MASK | ZCL_FRAME_CONTROL_SERVER_TO_CLIENT);
  message[index++] = GET_LSB_FROM_16(cmd->mfgCode);
  message[index++] = GET_MSB_FROM_16(cmd->mfgCode);
  message[index++] = cmd->seqNum;
  message[index++] = ZCL_READ_ATTRIBUTES_RESPONSE_COMMAND_ID;

  uint8_t i = 0;
  uint8_t data;
  uint8_t type;
  do
  {
     message[index++] = GET_LSB_FROM_16(attributes[i]);
     message[index++] = GET_MSB_FROM_16(attributes[i]);
     message[index++] = SL_STATUS_OK;
     switch(attributes[i])
     {
        case ZCL_MIN_LIGHT_LEVEL_ATTRIBUTE_ID:
           type = ZCL_INT8U_ATTRIBUTE_TYPE;
           data = get_min_light_level();
           break;
        case ZCL_MID_LIGHT_LEVEL_ATTRIBUTE_ID:
           type = ZCL_INT8U_ATTRIBUTE_TYPE;
           data = get_mid_light_level();
           break;
        case ZCL_MAX_LIGHT_LEVEL_ATTRIBUTE_ID:
           type = ZCL_INT8U_ATTRIBUTE_TYPE;
           data = get_max_light_level();
           break;
        case ZCL_ALLOW_OFF_ATTRIBUTE_ID:
           type = ZCL_BOOLEAN_ATTRIBUTE_TYPE;
           data = allow_light_off();
           break;
        default:
           sl_zigbee_app_debug_println("Unknown get config attr 0x%X", attributes[i]);
           message[index-1] = SL_ZIGBEE_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE;
           type = 0;
           data = 0;
     }
     message[index++] = type;
     message[index++] = data;
     i++;
  } while(i < n_attrs);

  sl_zigbee_aps_frame_t apsFrame = {0};
  fill_in_apsFrame(cmd, &apsFrame);

  sl_zigbee_af_send_unicast(SL_ZIGBEE_OUTGOING_DIRECT,
                              cmd->source,
                              &apsFrame,
                              (uint16_t)(index),
                              (uint8_t *)&message[0]);
  return(true);
}


static bool parse_daintree_attribute(uint16_t attr, uint8_t data)
{
  bool status = true;

  switch(attr)
  {
     case ZCL_MIN_LIGHT_LEVEL_ATTRIBUTE_ID:
        // We're guessing here cause we don't know what daintree is doing
        if(DEFAULT_MIN_LIGHT_LEVEL_DT > data)
        {
           data = DEFAULT_MIN_LIGHT_LEVEL_DT;
        }

        if(data > get_present_light_level())
        {
           move_to_level_onoff(data, 0);
        }

        save_min_light_level(data);
        break;
     case ZCL_MAX_LIGHT_LEVEL_ATTRIBUTE_ID:
        if(data < get_present_light_level())
        {
           move_to_level_onoff(data, 0);
        }

        save_max_light_level(data);
        break;
     case ZCL_ALLOW_OFF_ATTRIBUTE_ID:
        if((0 == data) && (0 == read_on_off_attribute(ZCL_ON_OFF_ATTRIBUTE_ID)))
        {
            move_to_level_onoff(get_max_light_level(), 0);
        }

        save_allow_light_off(data);
        break;
     default:
      sl_zigbee_app_debug_println("Unknown daintree config attr 0x%X", attr);
      status = false;
  }

  return(status);
}

// Called from sl_zigbee_af_pre_command_received_cb()
// Returns true if command is executed here
// NOTE:  Now only used to save nvm of attr's
bool set_daintree_configuration_cmd(sl_zigbee_af_cluster_command_t* cmd)
{
  uint8_t n_payload_bytes = (cmd->bufLen - cmd->payloadStartIndex);
  uint8_t index = cmd->payloadStartIndex;
  uint16_t attr = 0;
  uint8_t data = 0;
  bool executed_cmd = false;

  // Write attribute with at least 1 attribute in payload
  if(4 <= n_payload_bytes)
  {
     // Parse
     attr = (uint16_t)cmd->buffer[index] | ((uint16_t)cmd->buffer[index+1] << 8);
     // type = cmd->buffer[index+2]
     data = cmd->buffer[index+3];
     executed_cmd = parse_daintree_attribute(attr, data);
     if(!executed_cmd)
     {
        return(false);  // Failed, bail
     }
     // Next attribute
     if(8 <= n_payload_bytes)
     {
        attr = (uint16_t)cmd->buffer[index+4] | ((uint16_t)cmd->buffer[index+5] << 8);
        // type = cmd->buffer[index+6]
        data = cmd->buffer[index+7];
        executed_cmd = parse_daintree_attribute(attr, data);
        if(!executed_cmd)
        {
           return(false);
        }
     }
     if(12 <= n_payload_bytes)
     {
        attr = (uint16_t)cmd->buffer[index+8] | ((uint16_t)cmd->buffer[index+9] << 8);
        // type = cmd->buffer[index+10]
        data = cmd->buffer[index+11];
        executed_cmd = parse_daintree_attribute(attr, data);
        if(!executed_cmd)
        {
           return(false);
        }
     }
  }
  else
  {
     return(false);
  }

  return(false);
}
