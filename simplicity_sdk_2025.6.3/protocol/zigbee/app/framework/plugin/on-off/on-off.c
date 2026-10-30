/*
 * @file
 * @brief Routines for the On-Off plugin, which implements the On-Off server
 *        cluster.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include "app/framework/include/af.h"
#include "on-off.h"

#include "zap-cluster-command-parser.h"
#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_REPORTING_PRESENT
#include "app/framework/plugin/reporting/reporting.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_SCENES_PRESENT
#include "scenes.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_ZLL_ON_OFF_SERVER_PRESENT
#include "zll-on-off-server.h"
#endif
#ifdef SL_CATALOG_ZIGBEE_ZLL_LEVEL_CONTROL_SERVER_PRESENT
  #include "zll-level-control-server.h"
#endif

#ifdef ADAPTIV_LIGHT

#define ONOFF_PRINTF (0)

#include "RehTOS.h"
#include "hal_pwm.h"
#include "hal_utils.h"
#include "light_on_off.h"
#include "nwk_utils.h"
#include "occ_sensor.h"
#include "light_sensor.h"
#include "light_on_off.h"

static bool _off_cmd = false;
static bool _on_cmd = false;


extern void suspend_all_sensors(uint16_t Dsecs);

#endif // ADAPTIV_LIGHT
#ifdef ZCL_USING_ON_OFF_CLUSTER_START_UP_ON_OFF_ATTRIBUTE
static bool areStartUpOnOffServerAttributesTokenized(uint8_t endpoint);
#endif
#ifdef ADAPTIV_LIGHT

extern void move_to_level_onoff(uint8_t level, uint16_t transitionTimeDs);

#endif // ADAPTIV_LIGHT

sl_zigbee_af_status_t sl_zigbee_af_on_off_cluster_set_value_cb(uint8_t endpoint,
                                                               uint8_t command,
                                                               bool initiatedByLevelChange)
{
  sl_zigbee_af_status_t status;
#ifdef ADAPTIV_LIGHT
  bool currentOnOffValue, newValue;
#else  // ADAPTIV_LIGHT
  bool currentValue, newValue;
#endif // ADAPTIV_LIGHT

#ifdef ADAPTIV_LIGHT
  // NOTE: Remove when ready
  if(LIGHT_ENDPOINT != endpoint)
  {
      sl_zigbee_af_on_off_cluster_println("ERR: af_on_off_cluster_set_value_cb ep:%d", endpoint);
      return(SL_ZIGBEE_ZCL_STATUS_FAILURE);
#else  // ADAPTIV_LIGHT
  sl_zigbee_af_on_off_cluster_println("On/Off set value: %02X %02X", endpoint, command);

  // read current on/off value
  status = sl_zigbee_af_read_attribute(endpoint,
                                       ZCL_ON_OFF_CLUSTER_ID,
                                       ZCL_ON_OFF_ATTRIBUTE_ID,
                                       CLUSTER_MASK_SERVER,
                                       (uint8_t *)&currentValue,
                                       sizeof(currentValue),
                                       NULL); // data type
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_on_off_cluster_println("ERR: reading on/off %02X", status);
    return status;
#endif // ADAPTIV_LIGHT
  }

#ifdef ADAPTIV_LIGHT
  #if (ONOFF_PRINTF >= 1)
  sl_zigbee_af_on_off_cluster_println("  On/Off_cb  cmd %s from level_chg %s",
          command == 0 ? "OFF" : "ON", initiatedByLevelChange ? "YES" : "NO");
  #endif

  // Read current on/off value
  currentOnOffValue = read_on_off_attribute(ZCL_ON_OFF_ATTRIBUTE_ID);

#endif // ADAPTIV_LIGHT
  // if the value is already what we want to set it to then do nothing
#ifdef ADAPTIV_LIGHT
  if(((!currentOnOffValue && (command == ZCL_OFF_COMMAND_ID)) ||
     (currentOnOffValue && (command == ZCL_ON_COMMAND_ID))) &&
     !is_app_flags_nonvol_set(B_EM_MODE))
  {
     #if (ONOFF_PRINTF >= 1)
     sl_zigbee_af_on_off_cluster_println("  On/Off_cb  already set to %s, Done",
                                            currentOnOffValue ? "ON" : "OFF" );
     #endif

     return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
#else  // ADAPTIV_LIGHT
  if ((!currentValue && command == ZCL_OFF_COMMAND_ID)
      || (currentValue && command == ZCL_ON_COMMAND_ID)) {
    sl_zigbee_af_on_off_cluster_println("On/off already set to new value");
    return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
#endif // ADAPTIV_LIGHT
  }

  // We either got a toggle, or an on when off, or an off when on,
  // so we need to swap the value
#ifdef ADAPTIV_LIGHT
  suspend_all_sensors(DSECS_2_SECS);
#else  // ADAPTIV_LIGHT
  newValue = !currentValue;
  sl_zigbee_af_on_off_cluster_println("Toggle on/off from %02X to %02X", currentValue, newValue);
#endif // ADAPTIV_LIGHT

#ifdef ADAPTIV_LIGHT
  //newValue = !currentOnOffValue;
  newValue = (bool)command; // JGR EM_MODE bull shit
  #if (ONOFF_PRINTF >= 1)
  sl_zigbee_af_on_off_cluster_println("  On/Off_cb  %s to %s",
              currentOnOffValue ? "ON" : "OFF", newValue ? "ON" : "OFF");
  #endif

  // The sequence of updating on/off attribute and kick off level change effect should
#else  // ADAPTIV_LIGHT
  // the sequence of updating on/off attribute and kick off level change effect should
#endif // ADAPTIV_LIGHT
  // be depend on whether we are turning on or off. If we are turning on the light, we
  // should update the on/off attribute before kicking off level change, if we are
  // turning off the light, we should do the opposite,cmd_data that is kick off level change
  // before updating the on/off attribute.
#ifdef ADAPTIV_LIGHT
  if (newValue)
  {
     // Turning ON
     status = write_on_off_attribute(newValue);
     if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
        sl_zigbee_af_on_off_cluster_println("ERR: writing on/off %02X", status);
        return status;
     }
#else  // ADAPTIV_LIGHT
  if (newValue) {
    // write the new on/off value
    status = sl_zigbee_af_write_attribute(endpoint,
                                          ZCL_ON_OFF_CLUSTER_ID,
                                          ZCL_ON_OFF_ATTRIBUTE_ID,
                                          CLUSTER_MASK_SERVER,
                                          (uint8_t *)&newValue,
                                          ZCL_BOOLEAN_ATTRIBUTE_TYPE);
    if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
      sl_zigbee_af_on_off_cluster_println("ERR: writing on/off %02X", status);
      return status;
    }
#endif // ADAPTIV_LIGHT

#ifdef ADAPTIV_LIGHT
     // If initiatedByLevelChange is false, then we assume that the level change
     // ZCL stuff has not happened and we do it here
     if (!initiatedByLevelChange
        && sl_zigbee_af_contains_server(endpoint, ZCL_LEVEL_CONTROL_CLUSTER_ID))
     {
        move_to_level_onoff(get_on_light_level(), DSECS_1_SECS);
     }
  }
  else if(is_app_flags_nonvol_set(B_EM_MODE))
  {
     // Turning OFF in EM_MODE, so stay where we are
      sl_zigbee_af_on_off_cluster_println("Not allowed OFF cause EM_MODE");
      return(SL_ZIGBEE_ZCL_STATUS_SUCCESS);
  }
  else
  {
     // Turning OFF normal
     stop_ramp();
     // There's also a delay in the occ sensor code, if off by occ
     // TODO: Need this "long" time due to occ supply? bouncing on off xtion
     suspend_all_sensors(DSECS_3_SECS);
#else  // ADAPTIV_LIGHT
    // If initiatedByLevelChange is false, then we assume that the level change
    // ZCL stuff has not happened and we do it here
    if (!initiatedByLevelChange
        && sl_zigbee_af_contains_server(endpoint, ZCL_LEVEL_CONTROL_CLUSTER_ID)) {
      sl_zigbee_af_on_off_cluster_level_control_effect_cb(endpoint,
                                                          newValue);
    }
  } else {
    // If initiatedByLevelChange is false, then we assume that the level change
    // ZCL stuff has not happened and we do it here
    if (!initiatedByLevelChange
        && sl_zigbee_af_contains_server(endpoint, ZCL_LEVEL_CONTROL_CLUSTER_ID)) {
      sl_zigbee_af_on_off_cluster_level_control_effect_cb(endpoint,
                                                          newValue);
    }
#endif // ADAPTIV_LIGHT

#ifdef ADAPTIV_LIGHT
     if(!allow_light_off())
     {
        uint8_t level = DEFAULT_MIN_LIGHT_LEVEL_DT;
        #if 0
        uint8_t saved_level = read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID);
        write_light_level_attribute(ZCL_ON_LEVEL_ATTRIBUTE_ID, &saved_level);
        #endif
        hal_set_dim_pwm_255(level);
        write_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID, &level);
        write_on_off_attribute(true);
        sl_zigbee_af_on_off_cluster_println("Not allowed OFF !Allow mode");
        return(SL_ZIGBEE_ZCL_STATUS_SUCCESS);
     }

     // If initiatedByLevelChange is false, then we assume that the level change
     // ZCL stuff has not happened and we do it here
     if (!initiatedByLevelChange
        && sl_zigbee_af_contains_server(endpoint, ZCL_LEVEL_CONTROL_CLUSTER_ID))
     {
        uint8_t zero = 0;
        hal_set_dim_pwm_255(zero);
        write_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID, &zero);
        //JGRsl_zigbee_af_on_off_cluster_level_control_effect_cb(endpoint, newValue);
        // To support another daintree pos request, we have to wait for the
        // sl_zigbee_af_level_control_cluster_server_tick_cb() to complete.
        // Since this cb is deep in isrs, cannot use events cause can't delay
        // if originated in isr events.
        //save_present_light_level_to_on_level_attribute();
        //sl_zigbee_af_on_off_cluster_println("  OFF %d",save_present_light_level_to_on_level_attribute());
     }

     status = write_on_off_attribute(newValue);
     if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
        sl_zigbee_af_on_off_cluster_println("ERR: writing on/off %02X", status);
        return status;
     }
#else  // ADAPTIV_LIGHT
    // write the new on/off value
    status = sl_zigbee_af_write_attribute(endpoint,
                                          ZCL_ON_OFF_CLUSTER_ID,
                                          ZCL_ON_OFF_ATTRIBUTE_ID,
                                          CLUSTER_MASK_SERVER,
                                          (uint8_t *)&newValue,
                                          ZCL_BOOLEAN_ATTRIBUTE_TYPE);
    if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
      sl_zigbee_af_on_off_cluster_println("ERR: writing on/off %02X", status);
      return status;
    }
#endif // ADAPTIV_LIGHT
  }

#ifdef ADAPTIV_LIGHT
  // The scene has been changed (the value of on/off has changed) so
#else  // ADAPTIV_LIGHT
#ifdef SL_CATALOG_ZIGBEE_ZLL_ON_OFF_SERVER_PRESENT
  if (initiatedByLevelChange) {
    sl_zigbee_af_zll_on_off_server_level_control_zll_extensions(endpoint);
  }
#endif // SL_CATALOG_ZIGBEE_ZLL_ON_OFF_SERVER_PRESENT

  // the scene has been changed (the value of on/off has changed) so
#endif // ADAPTIV_LIGHT
  // the current scene as descibed in the attribute table is invalid,
  // so mark it as invalid (just writes the valid/invalid attribute)
  if (sl_zigbee_af_contains_server(endpoint, ZCL_SCENES_CLUSTER_ID)) {
     sl_zigbee_af_scenes_cluster_make_invalid_cb(endpoint);
  }

  // The returned status is based solely on the On/Off cluster.  Errors in the
  // Level Control and/or Scenes cluster are ignored.
  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_on_off_cluster_off_cb(void)
{
#ifdef ADAPTIV_LIGHT
  sl_zigbee_af_status_t status = sl_zigbee_af_on_off_cluster_set_value_cb(LIGHT_ENDPOINT, // If called not ota
#else  // ADAPTIV_LIGHT
  sl_zigbee_af_status_t status = sl_zigbee_af_on_off_cluster_set_value_cb(sl_zigbee_af_current_endpoint(),
#endif // ADAPTIV_LIGHT
                                                                          ZCL_OFF_COMMAND_ID,
                                                                          false);

#ifdef SL_CATALOG_ZIGBEE_ZLL_ON_OFF_SERVER_PRESENT
  if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_zll_on_off_server_off_zll_extensions(sl_zigbee_af_current_command());
  }
#endif
  return status;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_on_off_cluster_on_cb(void)
{
#ifdef ADAPTIV_LIGHT
  sl_zigbee_af_status_t status = sl_zigbee_af_on_off_cluster_set_value_cb(LIGHT_ENDPOINT,
#else  // ADAPTIV_LIGHT
  sl_zigbee_af_status_t status = sl_zigbee_af_on_off_cluster_set_value_cb(sl_zigbee_af_current_endpoint(),
#endif // ADAPTIV_LIGHT
                                                                          ZCL_ON_COMMAND_ID,
                                                                          false);

#ifdef SL_CATALOG_ZIGBEE_ZLL_ON_OFF_SERVER_PRESENT
  if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_zll_on_off_server_on_zll_extensions(sl_zigbee_af_current_command());
  }
#endif
  return status;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_on_off_cluster_toggle_cb(void)
{
  sl_zigbee_af_status_t status = sl_zigbee_af_on_off_cluster_set_value_cb(sl_zigbee_af_current_endpoint(),
                                                                          ZCL_TOGGLE_COMMAND_ID,
                                                                          false);

#ifdef SL_CATALOG_ZIGBEE_ZLL_ON_OFF_SERVER_PRESENT
  if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_zll_on_off_server_toggle_zll_extensions(sl_zigbee_af_current_command());
  }
#endif
  return status;
}

void sl_zigbee_af_on_off_cluster_server_init_cb(uint8_t endpoint)
{
#ifdef ZCL_USING_ON_OFF_CLUSTER_START_UP_ON_OFF_ATTRIBUTE
  // StartUp behavior relies on OnOff and StartUpOnOff attributes being tokenized.
  if (areStartUpOnOffServerAttributesTokenized(endpoint)) {
    // Read the StartUpOnOff attribute and set the OnOff attribute as per
    // following from zcl 7 14-0127-20i-zcl-ch-3-general.doc.
    // 3.8.2.2.5	StartUpOnOff Attribute
    // The StartUpOnOff attribute SHALL define the desired startup behavior of a
    // lamp device when it is supplied with power and this state SHALL be
    // reflected in the OnOff attribute.  The values of the StartUpOnOff
    // attribute are listed below.
    // Table 3 46. Values of the StartUpOnOff Attribute
    // Value      Action on power up
    // 0x00       Set the OnOff attribute to 0 (off).
    // 0x01       Set the OnOff attribute to 1 (on).
    // 0x02       If the previous value of the OnOff attribute is equal to 0,
    //            set the OnOff attribute to 1.If the previous value of the OnOff
    //            attribute is equal to 1, set the OnOff attribute to 0 (toggle).
    // 0x03-0xfe  These values are reserved.  No action.
    // 0xff       Set the OnOff attribute to its previous value.

    // Initialize startUpOnOff to No action value 0xFE
    uint8_t startUpOnOff = 0xFE;
    sl_zigbee_af_status_t status = sl_zigbee_af_read_attribute(endpoint,
                                                               ZCL_ON_OFF_CLUSTER_ID,
                                                               ZCL_START_UP_ON_OFF_ATTRIBUTE_ID,
                                                               CLUSTER_MASK_SERVER,
                                                               (uint8_t *)&startUpOnOff,
                                                               sizeof(startUpOnOff),
                                                               NULL);
    if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
      // Initialise updated value to 0
      bool updatedOnOff = 0;
      status = sl_zigbee_af_read_attribute(endpoint,
                                           ZCL_ON_OFF_CLUSTER_ID,
                                           ZCL_ON_OFF_ATTRIBUTE_ID,
                                           CLUSTER_MASK_SERVER,
                                           (uint8_t *)&updatedOnOff,
                                           sizeof(updatedOnOff),
                                           NULL);
      if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
        switch (startUpOnOff) {
          case SL_ZIGBEE_ZCL_START_UP_ON_OFF_VALUE_SET_TO_OFF:
            updatedOnOff = 0; // Off
            break;
          case SL_ZIGBEE_ZCL_START_UP_ON_OFF_VALUE_SET_TO_ON:
            updatedOnOff = 1; //On
            break;
          case SL_ZIGBEE_ZCL_START_UP_ON_OFF_VALUE_SET_TO_TOGGLE:
            updatedOnOff = !updatedOnOff;
            break;
          case SL_ZIGBEE_ZCL_START_UP_ON_OFF_VALUE_SET_TO_PREVIOUS:
          default:
            // All other values 0x03- 0xFE are reserved - no action.
            // When value is 0xFF - update with last value - that is as good as
            // no action.
            break;
        }
        status = sl_zigbee_af_write_attribute(endpoint,
                                              ZCL_ON_OFF_CLUSTER_ID,
                                              ZCL_ON_OFF_ATTRIBUTE_ID,
                                              CLUSTER_MASK_SERVER,
                                              (uint8_t *)&updatedOnOff,
                                              ZCL_BOOLEAN_ATTRIBUTE_TYPE);
      }
    }
  }
#endif
  sl_zigbee_af_on_off_cluster_server_post_init_cb(endpoint);
}

#ifdef ZCL_USING_ON_OFF_CLUSTER_START_UP_ON_OFF_ATTRIBUTE
static bool areStartUpOnOffServerAttributesTokenized(uint8_t endpoint)
{
  sl_zigbee_af_attribute_metadata_t *metadata;

  metadata = sl_zigbee_af_locate_attribute_metadata(endpoint,
                                                    ZCL_ON_OFF_CLUSTER_ID,
                                                    ZCL_ON_OFF_ATTRIBUTE_ID,
                                                    CLUSTER_MASK_SERVER,
                                                    SL_ZIGBEE_AF_NULL_MANUFACTURER_CODE);
  if (!sl_zigbee_af_attribute_is_tokenized(metadata)) {
    return false;
  }

  metadata = sl_zigbee_af_locate_attribute_metadata(endpoint,
                                                    ZCL_ON_OFF_CLUSTER_ID,
                                                    ZCL_START_UP_ON_OFF_ATTRIBUTE_ID,
                                                    CLUSTER_MASK_SERVER,
                                                    SL_ZIGBEE_AF_NULL_MANUFACTURER_CODE);
  if (!sl_zigbee_af_attribute_is_tokenized(metadata)) {
    return false;
  }

  return true;
}
#endif

uint32_t sl_zigbee_af_on_off_cluster_server_command_parse(sl_service_opcode_t opcode,
                                                          sl_service_function_context_t *context)
{
  (void)opcode;

  sl_zigbee_af_zcl_request_status_t status = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  sl_zigbee_af_cluster_command_t *cmd = (sl_zigbee_af_cluster_command_t *)context->data;

  if (!cmd->mfgSpecific) {
    switch (cmd->commandId) {
      case ZCL_OFF_COMMAND_ID:
      {
#ifdef ADAPTIV_LIGHT
        _on_cmd = false;
        if(0 == read_on_off_attribute(ZCL_ON_OFF_ATTRIBUTE_ID))
        {
            // Already off and off button pressed, off from occ sensor
           break;
        }
        if(false == _off_cmd)
        {
           // 1st off command?
            save_present_light_level_to_on_level_attribute();
        }
        _off_cmd = true;

#endif // ADAPTIV_LIGHT
        status = sl_zigbee_af_on_off_cluster_off_cb();
        break;
      }
      case ZCL_ON_COMMAND_ID:
      {
#ifdef ADAPTIV_LIGHT
        _on_cmd = true;
        _off_cmd = false;

#endif // ADAPTIV_LIGHT
        status = sl_zigbee_af_on_off_cluster_on_cb();
        break;
      }
      case ZCL_TOGGLE_COMMAND_ID:
      {
#ifdef ADAPTIV_LIGHT
        _off_cmd = read_on_off_attribute(ZCL_ON_OFF_ATTRIBUTE_ID) ? true : false;
        _on_cmd = !_off_cmd;
        if(_off_cmd)
        {
            save_present_light_level_to_on_level_attribute();
        }

#endif // ADAPTIV_LIGHT
        status = sl_zigbee_af_on_off_cluster_toggle_cb();
        break;
      }
    }
#ifdef ADAPTIV_LIGHT

    // On or Off, will restore occ delay if long delay
    restore_unoccupied_delay();
#endif // ADAPTIV_LIGHT
  }

  return status;
}

#ifdef ADAPTIV_LIGHT
bool off_from_switch(void)
{
  return(_off_cmd);
}
void clear_off_from_switch(void)
{
  _off_cmd = false;
}
bool on_from_switch(void)
{
  return(_on_cmd);
}
void clear_on_from_switch(void)
{
  _on_cmd = false;
}

#endif // ADAPTIV_LIGHT