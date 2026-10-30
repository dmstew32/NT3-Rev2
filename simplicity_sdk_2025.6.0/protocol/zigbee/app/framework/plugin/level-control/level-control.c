/***************************************************************************//**
 * @file
 * @brief Routines for the Level Control plugin, which implements the
 *        Level Control cluster.
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

// this file contains all the common includes for clusters in the util
#include "sl_common.h"
#include "app/framework/include/af.h"

// clusters specific header
#include "app/framework/plugin/level-control/level-control.h"

#include "level-control-config.h"
#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif
#include "zap-cluster-command-parser.h"
#ifdef SL_CATALOG_ZIGBEE_REPORTING_PRESENT
#include "app/framework/plugin/reporting/reporting.h"
#endif
#ifdef ADAPTIV_LIGHT

#include "light_on_off.h"

 void suspend_occ_sensor_control(uint16_t Dsecs);
 bool suspend_light_sensor_control(uint16_t Dsecs);
// Need to do ths to force silabs code to work with daintree and 2 step dim
static uint8_t _min_level = DEFAULT_MIN_LIGHT_LEVEL_DT;
static uint8_t _min_level_on_off = (DEFAULT_MIN_LIGHT_LEVEL_DT-1);
static bool dimming_button_active = false;

#define MIN_LEVEL _min_level
#define MIN_LEVEL_ON_OFF _min_level_on_off
#define MAX_LEVEL get_max_light_level()

// 0 = off  1 = min  2 = all
#ifndef LEVEL_PRINT
#define LEVEL_PRINT 0    /* Default to 0 */
#elif (LEVEL_PRINT != 0)
#warning "LEVEL_PRINT active"
#endif

#if 0
#define MIN_LEVEL DEFAULT_MIN_LIGHT_LEVEL_DT
#define MAX_LEVEL get_max_light_level()
#if (0 != DEFAULT_MIN_LIGHT_LEVEL_DT)
#define MIN_LEVEL_ON_OFF (DEFAULT_MIN_LIGHT_LEVEL_DT - 1)
#else
#error "ZERO Min Level"
#endif
#endif

#else  // ADAPTIV_LIGHT
#ifdef SL_CATALOG_ZIGBEE_ZLL_LEVEL_CONTROL_SERVER_PRESENT
#include "zll-level-control-server.h"
#define MIN_LEVEL SL_ZIGBEE_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER_MINIMUM_LEVEL
#define MAX_LEVEL SL_ZIGBEE_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER_MAXIMUM_LEVEL
#else // !SL_CATALOG_ZIGBEE_ZLL_LEVEL_CONTROL_SERVER_PRESENT
#define MIN_LEVEL SL_ZIGBEE_AF_PLUGIN_LEVEL_CONTROL_MINIMUM_LEVEL
#define MAX_LEVEL SL_ZIGBEE_AF_PLUGIN_LEVEL_CONTROL_MAXIMUM_LEVEL
#endif // SL_CATALOG_ZIGBEE_ZLL_LEVEL_CONTROL_SERVER_PRESENT
#endif // ADAPTIV_LIGHT
#ifdef SL_CATALOG_ZIGBEE_COLOR_CONTROL_SERVER_PRESENT
#include "app/framework/plugin/color-control-server/color-control-server.h"
#include "color-control-server-config.h"
#if (SL_ZIGBEE_AF_PLUGIN_COLOR_CONTROL_SERVER_TEMP == 1)
#define COLOR_CONTROL_SERVER_TEMP
#endif
#endif
#ifdef ADAPTIV_LIGHT



#include "RehTOS.h"
#include "hal_pwm.h"
#include "hal_utils.h"
#include "nwk_utils.h"
#include "occ_sensor.h"
#include "light_sensor.h"

extern void suspend_all_sensors(uint16_t Dsecs);
#endif // ADAPTIV_LIGHT

#ifdef ZCL_USING_LEVEL_CONTROL_CLUSTER_START_UP_CURRENT_LEVEL_ATTRIBUTE
static bool areStartUpLevelControlServerAttributesTokenized(uint8_t endpoint);
#endif

#if (SL_ZIGBEE_AF_PLUGIN_LEVEL_CONTROL_RATE == 0)
  #define FASTEST_TRANSITION_TIME_MS 0
#else
  #define FASTEST_TRANSITION_TIME_MS (MILLISECOND_TICKS_PER_SECOND / SL_ZIGBEE_AF_PLUGIN_LEVEL_CONTROL_RATE)
#endif

#define INVALID_STORED_LEVEL 0xFFFF

#define STARTUP_CURRENT_LEVEL_USE_DEVICE_MINIMUM 0x00
#define STARTUP_CURRENT_LEVEL_USE_PREVIOUS_LEVEL 0xFF

typedef struct {
  uint8_t commandId;
  uint8_t moveToLevel;
  bool increasing;
  bool useOnLevel;
  uint8_t onLevel;
  uint16_t storedLevel;
  uint32_t eventDurationMs;
  uint32_t transitionTimeMs;
  uint32_t elapsedTimeMs;
} EmberAfLevelControlState;

static EmberAfLevelControlState stateTable[SL_ZIGBEE_ZCL_LEVEL_CONTROL_CLUSTER_SERVER_ENDPOINT_COUNT];

static EmberAfLevelControlState *getState(uint8_t endpoint);

static void moveToLevelHandler(uint8_t commandId,
                               uint8_t level,
                               uint16_t transitionTimeDs,
                               uint8_t optionMask,
                               uint8_t optionOverride,
                               uint16_t storedLevel);
static void moveHandler(uint8_t commandId,
                        uint8_t moveMode,
                        uint8_t rate,
                        uint8_t optionMask,
                        uint8_t optionOverride);
static void stepHandler(uint8_t commandId,
                        uint8_t stepMode,
                        uint8_t stepSize,
                        uint16_t transitionTimeDs,
                        uint8_t optionMask,
                        uint8_t optionOverride);
static void stopHandler(uint8_t commandId,
                        uint8_t optionMask,
                        uint8_t optionOverride);

static void setOnOffValue(uint8_t endpoint, bool onOff);
static void writeRemainingTime(uint8_t endpoint, uint16_t remainingTimeMs);
static bool shouldExecuteIfOff(uint8_t endpoint,
                               uint8_t commandId,
                               uint8_t optionMask,
                               uint8_t optionOverride);

#if defined(ZCL_USING_LEVEL_CONTROL_CLUSTER_OPTIONS_ATTRIBUTE) \
  && defined(COLOR_CONTROL_SERVER_TEMP)
static void reallyUpdateCoupledColorTemp(uint8_t endpoint);
#define updateCoupledColorTemp(endpoint) reallyUpdateCoupledColorTemp(endpoint)
#else
#define updateCoupledColorTemp(endpoint)
#endif // ZCL_USING_LEVEL_CONTROL_CLUSTER_OPTIONS_ATTRIBUTE && COLOR_CONTROL_SERVER_TEMP

static void schedule(uint8_t endpoint, uint32_t delayMs)
{
  sl_zigbee_zcl_schedule_server_tick_extended(endpoint,
                                              ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                              delayMs,
                                              SL_ZIGBEE_AF_LONG_POLL,
                                              SL_ZIGBEE_AF_OK_TO_SLEEP);
}

static void deactivate(uint8_t endpoint)
{
  sl_zigbee_zcl_deactivate_server_tick(endpoint, ZCL_LEVEL_CONTROL_CLUSTER_ID);
}

static EmberAfLevelControlState *getState(uint8_t endpoint)
{
  uint8_t ep = sl_zigbee_af_find_cluster_server_endpoint_index(endpoint,
                                                               ZCL_LEVEL_CONTROL_CLUSTER_ID);

  return (ep == SL_ZIGBEE_AF_INVALID_ENDPOINT_INDEX ? NULL : &stateTable[ep]);
}

#if defined(ZCL_USING_LEVEL_CONTROL_CLUSTER_OPTIONS_ATTRIBUTE) \
  && defined(COLOR_CONTROL_SERVER_TEMP)
static void reallyUpdateCoupledColorTemp(uint8_t endpoint)
{
  uint8_t options;
  sl_zigbee_af_status_t status = sl_zigbee_af_read_server_attribute(endpoint,
                                                                    ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                                                    ZCL_OPTIONS_ATTRIBUTE_ID,
                                                                    &options,
                                                                    sizeof(options));
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_level_control_cluster_println("Unable to read Options attribute: 0x%02X",
                                               status);
    return;
  }

  if (READBITS(options, SL_ZIGBEE_ZCL_LEVEL_CONTROL_OPTIONS_COUPLE_COLOR_TEMP_TO_LEVEL)) {
    sl_zigbee_af_level_control_coupled_color_temp_change_cb(endpoint);
  }
}
#endif // ZCL_USING_LEVEL_CONTROL_CLUSTER_OPTIONS_ATTRIBUTE && COLOR_CONTROL_SERVER_TEMP

void sl_zigbee_af_level_control_cluster_server_tick_cb(uint8_t endpoint)
{
  EmberAfLevelControlState *state = getState(endpoint);
  sl_zigbee_af_status_t status;
  uint8_t currentLevel;

  if (state == NULL) {
    return;
  }
  state->elapsedTimeMs += state->eventDurationMs;

#if !defined(ZCL_USING_LEVEL_CONTROL_CLUSTER_OPTIONS_ATTRIBUTE) \
  && defined(SL_CATALOG_ZIGBEE_ZLL_LEVEL_CONTROL_SERVER_PRESENT)
  if (sl_zigbee_af_zll_level_control_server_ignore_move_to_level_move_step_stop(endpoint,
                                                                                state->commandId)) {
    return;
  }
#endif

  // Read the attribute; print error message and return if it can't be read
  status = sl_zigbee_af_read_server_attribute(endpoint,
                                              ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                              ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                              (uint8_t *)&currentLevel,
                                              sizeof(currentLevel));
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_level_control_cluster_println("ERR: reading current level %02X", status);
    writeRemainingTime(endpoint, 0);
    return;
  }

#ifdef ADAPTIV_LIGHT
  #if LEVEL_PRINT >= 2
  sl_zigbee_af_level_control_cluster_println("Tick0: Move from %d -> %d", currentLevel, state->moveToLevel);
  #endif
#else  // ADAPTIV_LIGHT
  sl_zigbee_af_level_control_cluster_print("Event: move from %d", currentLevel);
#endif // ADAPTIV_LIGHT

  // adjust by the proper amount, either up or down
  if (state->transitionTimeMs == 0) {
    // Immediate, not over a time interval.
    currentLevel = state->moveToLevel;
  } else if (state->increasing) {
    assert(currentLevel < MAX_LEVEL);
    assert(currentLevel < state->moveToLevel);
    currentLevel++;
  } else {
    assert(MIN_LEVEL < currentLevel);
    assert(state->moveToLevel < currentLevel);
    currentLevel--;
  }

#ifndef ADAPTIV_LIGHT
  sl_zigbee_af_level_control_cluster_print(" to %d ", currentLevel);
  sl_zigbee_af_level_control_cluster_println("(diff %c1)",
                                             state->increasing ? '+' : '-');

#endif // ADAPTIV_LIGHT
  status = sl_zigbee_af_write_server_attribute(endpoint,
                                               ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                               ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                               (uint8_t *)&currentLevel,
                                               ZCL_INT8U_ATTRIBUTE_TYPE);
#ifdef ADAPTIV_LIGHT
  /*** Write to PWM ***/
  hal_set_dim_pwm_255(currentLevel);  // JGR
  #if LEVEL_PRINT >= 2
  sl_zigbee_af_level_control_cluster_println("Tick1: Set PWM to %d", currentLevel);
  #endif

#endif // ADAPTIV_LIGHT
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_level_control_cluster_println("ERR: writing current level %02X", status);
    writeRemainingTime(endpoint, 0);
    return;
  }

  updateCoupledColorTemp(endpoint);

  // The level has changed, so the scene is no longer valid.
  if (sl_zigbee_af_contains_server(endpoint, ZCL_SCENES_CLUSTER_ID)) {
    sl_zigbee_af_scenes_cluster_make_invalid_cb(endpoint);
  }

  // Are we at the requested level?
  if (currentLevel == state->moveToLevel) {
#ifdef ADAPTIV_LIGHT
    #if LEVEL_PRINT >= 1
    sl_zigbee_af_level_control_cluster_println("Light level_0 %d == %d   <%d>",
                             currentLevel, state->moveToLevel, get_time_ms());
    #endif

    // Save present level
    save_present_light_level_to_on_level_attribute();

#endif // ADAPTIV_LIGHT
    if (state->commandId == ZCL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID
        || state->commandId == ZCL_MOVE_WITH_ON_OFF_COMMAND_ID
        || state->commandId == ZCL_STEP_WITH_ON_OFF_COMMAND_ID) {
#ifdef ADAPTIV_LIGHT
      setOnOffValue(endpoint, (currentLevel != MIN_LEVEL_ON_OFF));
#else  // ADAPTIV_LIGHT
      setOnOffValue(endpoint, (currentLevel != MIN_LEVEL));
#endif // ADAPTIV_LIGHT
      if (currentLevel == MIN_LEVEL && state->useOnLevel) {
        status = sl_zigbee_af_write_server_attribute(endpoint,
                                                     ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                                     ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                                     (uint8_t *)&state->onLevel,
                                                     ZCL_INT8U_ATTRIBUTE_TYPE);
#ifdef ADAPTIV_LIGHT
        #if LEVEL_PRINT >= 1
        sl_zigbee_af_level_control_cluster_println("275 %d",state->onLevel);
        #endif

#endif // ADAPTIV_LIGHT
        if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
          sl_zigbee_af_level_control_cluster_println("ERR: writing current level %02X",
                                                     status);
        } else {
          updateCoupledColorTemp(endpoint);
        }
      }
#ifdef ADAPTIV_LIGHT
    }

   else {
#else  // ADAPTIV_LIGHT
    } else {
#endif // ADAPTIV_LIGHT
      if (state->storedLevel != INVALID_STORED_LEVEL) {
        uint8_t storedLevel8u = (uint8_t) state->storedLevel;
        status = sl_zigbee_af_write_server_attribute(endpoint,
                                                     ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                                     ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                                     (uint8_t *)&storedLevel8u,
                                                     ZCL_INT8U_ATTRIBUTE_TYPE);
#ifdef ADAPTIV_LIGHT

        #if LEVEL_PRINT >= 1
        sl_zigbee_af_level_control_cluster_println("296 %d",storedLevel8u);
        #endif

#endif // ADAPTIV_LIGHT
        if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
          sl_zigbee_af_level_control_cluster_println("ERR: writing current level %02X",
                                                     status);
        } else {
#ifdef ADAPTIV_LIGHT
            /*** Write to PWM ***/
             hal_set_dim_pwm_255(currentLevel);  //JGR
             #if LEVEL_PRINT >= 2
             sl_zigbee_af_level_control_cluster_println("Tick2: Set PWM to %d", currentLevel);
             #endif
#endif // ADAPTIV_LIGHT
          updateCoupledColorTemp(endpoint);
        }
      }
    }
    writeRemainingTime(endpoint, 0);
  } else {
    writeRemainingTime(endpoint,
                       state->transitionTimeMs - state->elapsedTimeMs);
    schedule(endpoint, state->eventDurationMs);
  }
}

static void writeRemainingTime(uint8_t endpoint, uint16_t remainingTimeMs)
{
#ifdef ZCL_USING_LEVEL_CONTROL_CLUSTER_LEVEL_CONTROL_REMAINING_TIME_ATTRIBUTE
  // Convert milliseconds to tenths of a second, rounding any fractional value
  // up to the nearest whole value.  This means:
  //
  //   0 ms = 0.00 ds = 0 ds
  //   1 ms = 0.01 ds = 1 ds
  //   ...
  //   100 ms = 1.00 ds = 1 ds
  //   101 ms = 1.01 ds = 2 ds
  //   ...
  //   200 ms = 2.00 ds = 2 ds
  //   201 ms = 2.01 ds = 3 ds
  //   ...
  //
  // This is done to ensure that the attribute, in tenths of a second, only
  // goes to zero when the remaining time in milliseconds is actually zero.
  uint16_t remainingTimeDs = (remainingTimeMs + 99) / 100;
  sl_status_t status = sl_zigbee_af_write_server_attribute(endpoint,
                                                           ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                                           ZCL_LEVEL_CONTROL_REMAINING_TIME_ATTRIBUTE_ID,
                                                           (uint8_t *)&remainingTimeDs,
                                                           ZCL_INT16U_ATTRIBUTE_TYPE);
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_level_control_cluster_println("ERR: writing remaining time %02X", status);
  }
#endif
}

static void setOnOffValue(uint8_t endpoint, bool onOff)
{
  if (sl_zigbee_af_contains_server(endpoint, ZCL_ON_OFF_CLUSTER_ID)) {
#ifdef ADAPTIV_LIGHT
  #if (LEVEL_PRINT >= 2)
  sl_zigbee_af_level_control_cluster_println("Level: Setting on/off to %s from a level change",
                                               onOff ? "ON" : "OFF");
  #endif
#else  // ADAPTIV_LIGHT
    sl_zigbee_af_level_control_cluster_println("Setting on/off to %s due to level change",
                                               onOff ? "ON" : "OFF");
#endif // ADAPTIV_LIGHT
    sl_zigbee_af_on_off_cluster_set_value_cb(endpoint,
                                             (onOff ? ZCL_ON_COMMAND_ID : ZCL_OFF_COMMAND_ID),
                                             true);
  }
}

static bool shouldExecuteIfOff(uint8_t endpoint,
                               uint8_t commandId,
                               uint8_t optionMask,
                               uint8_t optionOverride)
{
#ifdef ZCL_USING_LEVEL_CONTROL_CLUSTER_OPTIONS_ATTRIBUTE
  // From 3.10.2.2.8.1 of ZCL7 document 14-0127-20j-zcl-ch-3-general.docx:
  //   "Command execution SHALL NOT continue beyond the Options processing if
  //    all of these criteria are true:
  //      - The command is one of the 'without On/Off' commands: Move, Move to
  //        Level, Stop, or Step.
  //      - The On/Off cluster exists on the same endpoint as this cluster.
  //      - The OnOff attribute of the On/Off cluster, on this endpoint, is 0x00
  //        (FALSE).
  //      - The value of the ExecuteIfOff bit is 0."
  if (commandId > ZCL_STOP_COMMAND_ID) {
    return true;
  }

  if (!sl_zigbee_af_contains_server(endpoint, ZCL_ON_OFF_CLUSTER_ID)) {
    return true;
  }

  uint8_t options;
  sl_zigbee_af_status_t status = sl_zigbee_af_read_server_attribute(endpoint,
                                                                    ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                                                    ZCL_OPTIONS_ATTRIBUTE_ID,
                                                                    &options,
                                                                    sizeof(options));
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_level_control_cluster_println("Unable to read Options attribute: 0x%02X",
                                               status);
    // If we can't read the attribute, then we should just assume that it has its
    // default value.
    options = 0x00;
  }

  bool on;
  status = sl_zigbee_af_read_server_attribute(endpoint,
                                              ZCL_ON_OFF_CLUSTER_ID,
                                              ZCL_ON_OFF_ATTRIBUTE_ID,
                                              (uint8_t *)&on,
                                              sizeof(on));
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_level_control_cluster_println("Unable to read OnOff attribute: 0x%02X",
                                               status);
    return true;
  }
  // The device is on - hence ExecuteIfOff does not matter
  if (on) {
    return true;
  }
  // The OptionsMask & OptionsOverride fields SHALL both be present or both
  // omitted in the command. A temporary Options bitmap SHALL be created from
  // the Options attribute, using the OptionsMask & OptionsOverride fields, if
  // present. Each bit of the temporary Options bitmap SHALL be determined as
  // follows:
  // Each bit in the Options attribute SHALL determine the corresponding bit in
  // the temporary Options bitmap, unless the OptionsMask field is present and
  // has the corresponding bit set to 1, in which case the corresponding bit in
  // the OptionsOverride field SHALL determine the corresponding bit in the
  // temporary Options bitmap.
  //The resulting temporary Options bitmap SHALL then be processed as defined
  // in section 3.10.2.2.3.

  // ---------- The following order is important in decission making -------
  // -----------more readable ----------
  //
#ifdef ADAPTIV_LIGHT
  #if (LEVEL_PRINT >= 1)
  sl_zigbee_af_level_control_cluster_println("0x%X 0x%X 0x%X 0x%X",optionMask,
                                             optionOverride,
                                             options,
                                             SL_ZIGBEE_ZCL_LEVEL_CONTROL_OPTIONS_EXECUTE_IF_OFF);
  #endif
#endif // ADAPTIV_LIGHT
  if (optionMask == 0xFF && optionOverride == 0xFF) {
    // 0xFF are the default values passed to the command handler when
    // the payload is not present - in that case there is use of option
    // attribute to decide execution of the command
    return READBITS(options, SL_ZIGBEE_ZCL_LEVEL_CONTROL_OPTIONS_EXECUTE_IF_OFF);
  }
  // ---------- The above is to distinguish if the payload is present or not

  if (READBITS(optionMask, SL_ZIGBEE_ZCL_LEVEL_CONTROL_OPTIONS_EXECUTE_IF_OFF)) {
    // Mask is present and set in the command payload, this indicates
    // use the over ride as temporary option
    return READBITS(optionOverride, SL_ZIGBEE_ZCL_LEVEL_CONTROL_OPTIONS_EXECUTE_IF_OFF);
  }
  // if we are here - use the option bits
  return (READBITS(options, SL_ZIGBEE_ZCL_LEVEL_CONTROL_OPTIONS_EXECUTE_IF_OFF));

#else
  // By default, we return true to continue supporting backwards compatibility.
  return true;
#endif
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_level_control_cluster_move_to_level_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_level_control_cluster_move_to_level_command_t cmd_data;

  if (zcl_decode_level_control_cluster_move_to_level_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }
#ifdef ADAPTIV_LIGHT
  #if (LEVEL_PRINT >= 1)
  sl_zigbee_af_level_control_cluster_println("\n%sMOVE_TO_LEVEL lvl:%d t:%d %02X %02X   <%d>",
                                             "RX level-control:",
                                             cmd_data.level,
                                             cmd_data.transitionTime,
                                             cmd_data.optionMask,
                                             cmd_data.optionOverride,
                                             get_time_ms());
  #endif
#else  // ADAPTIV_LIGHT

  sl_zigbee_af_level_control_cluster_println("%sMOVE_TO_LEVEL %02X %04X %02X %02X",
                                             "RX level-control:",
                                             cmd_data.level,
                                             cmd_data.transitionTime,
                                             cmd_data.optionMask,
                                             cmd_data.optionOverride);
#endif // ADAPTIV_LIGHT
  moveToLevelHandler(ZCL_MOVE_TO_LEVEL_COMMAND_ID,
                     cmd_data.level,
                     cmd_data.transitionTime,
                     cmd_data.optionMask,
                     cmd_data.optionOverride,
                     INVALID_STORED_LEVEL); // Don't revert to the stored level
  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_level_control_cluster_move_to_level_with_on_off_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_level_control_cluster_move_to_level_with_on_off_command_t cmd_data;

  if (zcl_decode_level_control_cluster_move_to_level_with_on_off_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }
#ifdef ADAPTIV_LIGHT
  #if (LEVEL_PRINT >= 1)
  sl_zigbee_af_level_control_cluster_println("\n%sMOVE_TO_LEVEL_WITH_ON_OFF lvl:%d t:%d   <%d>",
                                             "RX level-control:",
                                             cmd_data.level,
                                             cmd_data.transitionTime,
                                             get_time_ms());
  #endif
#else  // ADAPTIV_LIGHT

  sl_zigbee_af_level_control_cluster_println("%sMOVE_TO_LEVEL_WITH_ON_OFF %02X %04X",
                                             "RX level-control:",
                                             cmd_data.level,
                                             cmd_data.transitionTime);
#endif // ADAPTIV_LIGHT
  moveToLevelHandler(ZCL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID,
                     cmd_data.level,
                     cmd_data.transitionTime,
                     0xFF,
                     0xFF,
                     INVALID_STORED_LEVEL); // Don't revert to the stored level
  return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_level_control_cluster_move_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_level_control_cluster_move_command_t cmd_data;

  if (zcl_decode_level_control_cluster_move_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }
#ifdef ADAPTIV_LIGHT
  #if (LEVEL_PRINT >= 1)
  sl_zigbee_af_level_control_cluster_println("%sMOVE %02X %02X   <%d>",
                                             "RX level-control:",
                                             cmd_data.moveMode ? "DN" : "UP",
                                             cmd_data.rate,
                                             get_time_ms());
  #endif
#else  // ADAPTIV_LIGHT

  sl_zigbee_af_level_control_cluster_println("%sMOVE %02X %02X",
                                             "RX level-control:",
                                             cmd_data.moveMode,
                                             cmd_data.rate);
#endif // ADAPTIV_LIGHT
  moveHandler(ZCL_MOVE_COMMAND_ID, cmd_data.moveMode, cmd_data.rate, cmd_data.optionMask, cmd_data.optionOverride);
  return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_level_control_cluster_move_with_on_off_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_level_control_cluster_move_with_on_off_command_t cmd_data;

  if (zcl_decode_level_control_cluster_move_with_on_off_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }
#ifdef ADAPTIV_LIGHT
  #if (LEVEL_PRINT >= 1)
  sl_zigbee_af_level_control_cluster_println("%sMOVE_WITH_ON_OFF %02X %02X   <%d>",
                                             "RX level-control:",
                                             cmd_data.moveMode ? "DN" : "UP",
                                             cmd_data.rate,
                                             get_time_ms());
  #endif
#else  // ADAPTIV_LIGHT

  sl_zigbee_af_level_control_cluster_println("%sMOVE_WITH_ON_OFF %02X %02X",
                                             "RX level-control:",
                                             cmd_data.moveMode,
                                             cmd_data.rate);
#endif // ADAPTIV_LIGHT
  moveHandler(ZCL_MOVE_WITH_ON_OFF_COMMAND_ID, cmd_data.moveMode, cmd_data.rate, 0xFF, 0xFF);
  return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_level_control_cluster_step_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_level_control_cluster_step_command_t cmd_data;

  if (zcl_decode_level_control_cluster_step_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }
#ifdef ADAPTIV_LIGHT
  #if (LEVEL_PRINT >= 1)
  sl_zigbee_af_level_control_cluster_println("%sSTEP Mode:%s size:%d t:%d   <%d>",
                                             "RX level-control:",
                                             cmd_data.stepMode ? "DN" : "UP",
                                             cmd_data.stepSize,
                                             cmd_data.transitionTime,
                                             get_time_ms());
  #endif
  stepHandler(ZCL_STEP_COMMAND_ID,
              cmd_data.stepMode,
              cmd_data.stepSize,
              cmd_data.transitionTime,
              cmd_data.optionMask,
              cmd_data.optionOverride);
#else  // ADAPTIV_LIGHT

  sl_zigbee_af_level_control_cluster_println("%sSTEP %02X %02X %04X",
                                             "RX level-control:",
                                             cmd_data.stepMode,
                                             cmd_data.stepSize,
                                             cmd_data.transitionTime);
  stepHandler(ZCL_STEP_COMMAND_ID, cmd_data.stepMode, cmd_data.stepSize, cmd_data.transitionTime, cmd_data.optionMask, cmd_data.optionOverride);
#endif // ADAPTIV_LIGHT
  return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_level_control_cluster_step_with_on_off_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_level_control_cluster_step_with_on_off_command_t cmd_data;

  if (zcl_decode_level_control_cluster_step_with_on_off_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }
#ifdef ADAPTIV_LIGHT
  #if (LEVEL_PRINT >= 1)
  sl_zigbee_af_level_control_cluster_println("%sSTEP_WITH_ON_OFF %s size:%d t:%d   <%d>",
                                             "RX level-control:",
                                             cmd_data.stepMode ? "DN" : "UP",
                                             cmd_data.stepSize,
                                             cmd_data.transitionTime,
                                             get_time_ms());
  #endif
#else  // ADAPTIV_LIGHT

  sl_zigbee_af_level_control_cluster_println("%sSTEP_WITH_ON_OFF %02X %02X %04X",
                                             "RX level-control:",
                                             cmd_data.stepMode,
                                             cmd_data.stepSize,
                                             cmd_data.transitionTime);
#endif // ADAPTIV_LIGHT
  stepHandler(ZCL_STEP_WITH_ON_OFF_COMMAND_ID,
              cmd_data.stepMode,
              cmd_data.stepSize,
              cmd_data.transitionTime,
              0xFF,
              0xFF);
  return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_level_control_cluster_stop_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_level_control_cluster_stop_command_t cmd_data;

  if (zcl_decode_level_control_cluster_stop_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }
#ifdef ADAPTIV_LIGHT
  #if (LEVEL_PRINT >= 1)
  sl_zigbee_af_level_control_cluster_println("%sSTOP level:%d   <%d>",
                                                         "RX level-control:",
                                                         hal_get_dim_pwm_255(),
                                                         get_time_ms());
  #endif
#else  // ADAPTIV_LIGHT

  sl_zigbee_af_level_control_cluster_println("%sSTOP", "RX level-control:");
#endif // ADAPTIV_LIGHT
  stopHandler(ZCL_STOP_COMMAND_ID, cmd_data.optionMask, cmd_data.optionOverride);
  return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_level_control_cluster_stop_with_on_off_cb(void)
{
#ifdef ADAPTIV_LIGHT
  #if (LEVEL_PRINT >= 1)
  sl_zigbee_af_level_control_cluster_println("%sSTOP_WITH_ON_OFF level:%d   <%d>",
                                             "RX level-control:",
                                             hal_get_dim_pwm_255(),
                                             get_time_ms());
  #endif
#else  // ADAPTIV_LIGHT
  sl_zigbee_af_level_control_cluster_println("%sSTOP_WITH_ON_OFF", "RX level-control:");
#endif // ADAPTIV_LIGHT
  stopHandler(ZCL_STOP_WITH_ON_OFF_COMMAND_ID, 0xFF, 0xFF);
  return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
}

static void moveToLevelHandler(uint8_t commandId,
                               uint8_t level,
                               uint16_t transitionTimeDs,
                               uint8_t optionMask,
                               uint8_t optionOverride,
                               uint16_t storedLevel)
{
#ifdef ADAPTIV_LIGHT
  uint8_t endpoint = LIGHT_ENDPOINT;//sl_zigbee_af_current_endpoint();
#else  // ADAPTIV_LIGHT
  uint8_t endpoint = sl_zigbee_af_current_endpoint();
#endif // ADAPTIV_LIGHT
  EmberAfLevelControlState *state = getState(endpoint);
  sl_zigbee_af_status_t status;
  uint8_t currentLevel;
  uint8_t actualStepSize;

  if (state == NULL) {
    status = SL_ZIGBEE_ZCL_STATUS_FAILURE;
    goto send_default_response;
  }

  if (!shouldExecuteIfOff(endpoint, commandId, optionMask, optionOverride)) {
    status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
    goto send_default_response;
  }

  // Cancel any currently active command before fiddling with the state.
  deactivate(endpoint);

  status = sl_zigbee_af_read_server_attribute(endpoint,
                                              ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                              ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                              (uint8_t *)&currentLevel,
                                              sizeof(currentLevel));
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_level_control_cluster_println("ERR: reading current level %02X", status);
    goto send_default_response;
  }

#ifdef ADAPTIV_LIGHT
  #if LEVEL_PRINT >= 1
  sl_zigbee_af_level_control_cluster_println("moveToLevelHandler: %d -> %d   pwm=%d",
                                         currentLevel, level, hal_get_dim_pwm_255());
  #endif

#endif // ADAPTIV_LIGHT
  state->commandId = commandId;

  // Move To Level commands cause the device to move from its current level to
  // the specified level at the specified rate.
  SL_IGNORE_TYPE_LIMIT_BEGIN;
  if (MAX_LEVEL < level) {
    state->moveToLevel = MAX_LEVEL;
  } else if (level < MIN_LEVEL) {
    state->moveToLevel = MIN_LEVEL;
  } else {
    state->moveToLevel = level;
  }
  SL_IGNORE_TYPE_LIMIT_END;

  // If the level is decreasing, the On/Off attribute is left unchanged.  This
  // logic is to prevent a light from transitioning from off to bright to dim.
  // Instead, a light that is off will stay off until the target level is
  // reached.
  if (currentLevel <= state->moveToLevel) {
    if (commandId == ZCL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID) {
#ifdef ADAPTIV_LIGHT
      setOnOffValue(endpoint, (state->moveToLevel != MIN_LEVEL_ON_OFF));
#else  // ADAPTIV_LIGHT
      setOnOffValue(endpoint, (state->moveToLevel != MIN_LEVEL));
#endif // ADAPTIV_LIGHT
    }
    if (currentLevel == state->moveToLevel) {
      status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
#ifdef ADAPTIV_LIGHT
      #if LEVEL_PRINT >= 1
      sl_zigbee_af_level_control_cluster_println("Light level_1 lvl %d == %d   <%d>",
                        currentLevel, state->moveToLevel, get_time_ms());
      #endif

      // Save present level
      save_present_light_level_to_on_level_attribute();
#endif // ADAPTIV_LIGHT
      goto send_default_response;
    }
    state->increasing = true;
    actualStepSize = state->moveToLevel - currentLevel;
  } else {
    state->increasing = false;
    actualStepSize = currentLevel - state->moveToLevel;
  }

  // If the Transition time field takes the value 0xFFFF, then the time taken
  // to move to the new level shall instead be determined by the On/Off
  // Transition Time attribute.  If On/Off Transition Time, which is an
  // optional attribute, is not present, the device shall move to its new level
  // as fast as it is able.
  if (transitionTimeDs == 0xFFFF) {
#ifdef ZCL_USING_LEVEL_CONTROL_CLUSTER_ON_OFF_TRANSITION_TIME_ATTRIBUTE
    status = sl_zigbee_af_read_server_attribute(endpoint,
                                                ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                                ZCL_ON_OFF_TRANSITION_TIME_ATTRIBUTE_ID,
                                                (uint8_t *)&transitionTimeDs,
                                                sizeof(transitionTimeDs));
    if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
      sl_zigbee_af_level_control_cluster_println("ERR: reading on/off transition time %02X",
                                                 status);
      goto send_default_response;
    }

    // Transition time comes in (or is stored, in the case of On/Off Transition
    // Time) as tenths of a second, but we work in milliseconds.
    state->transitionTimeMs = (transitionTimeDs
                               * MILLISECOND_TICKS_PER_SECOND
                               / 10);
#else //ZCL_USING_LEVEL_CONTROL_CLUSTER_ON_OFF_TRANSITION_TIME_ATTRIBUTE
    // If the Transition Time field is 0xFFFF and On/Off Transition Time,
    // which is an optional attribute, is not present, the device shall move to
    // its new level as fast as it is able.
    state->transitionTimeMs = FASTEST_TRANSITION_TIME_MS;
#endif //ZCL_USING_LEVEL_CONTROL_CLUSTER_ON_OFF_TRANSITION_TIME_ATTRIBUTE
  } else {
    // Transition time comes in (or is stored, in the case of On/Off Transition
    // Time) as tenths of a second, but we work in milliseconds.
    state->transitionTimeMs = (transitionTimeDs
                               * MILLISECOND_TICKS_PER_SECOND
                               / 10);
  }

  // The duration between events will be the transition time divided by the
  // distance we must move.
  state->eventDurationMs = state->transitionTimeMs / actualStepSize;
  state->elapsedTimeMs = 0;

  // OnLevel is not used for Move commands.
  state->useOnLevel = false;

  state->storedLevel = storedLevel;

  // The setup was successful, so mark the new state as active and return.
  schedule(endpoint, state->eventDurationMs);
  status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;

  #ifdef SL_CATALOG_ZIGBEE_ZLL_LEVEL_CONTROL_SERVER_PRESENT
  if (commandId == ZCL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID) {
    sl_zigbee_af_zll_level_control_server_move_to_level_with_on_off_zll_extensions(sl_zigbee_af_current_command());
  }
  #endif

  send_default_response:
  if (sl_zigbee_af_current_command()->apsFrame->clusterId
      == ZCL_LEVEL_CONTROL_CLUSTER_ID) {
    sl_zigbee_af_send_immediate_default_response(status);
  }
}

static void moveHandler(uint8_t commandId, uint8_t moveMode, uint8_t rate, uint8_t optionMask, uint8_t optionOverride)
{
  uint8_t endpoint = sl_zigbee_af_current_endpoint();
  EmberAfLevelControlState *state = getState(endpoint);
  sl_zigbee_af_status_t status;
  uint8_t currentLevel;
  uint8_t difference;

  if (state == NULL) {
    status = SL_ZIGBEE_ZCL_STATUS_FAILURE;
    goto send_default_response;
  }

  if (!shouldExecuteIfOff(endpoint, commandId, optionMask, optionOverride)) {
    status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
    goto send_default_response;
  }

  // Cancel any currently active command before fiddling with the state.
  deactivate(endpoint);

  status = sl_zigbee_af_read_server_attribute(endpoint,
                                              ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                              ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                              (uint8_t *)&currentLevel,
                                              sizeof(currentLevel));
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_level_control_cluster_println("ERR: reading current level %02X", status);
    goto send_default_response;
  }

  state->commandId = commandId;

  // Move commands cause the device to move from its current level to either
  // the maximum or minimum level at the specified rate.

  switch (moveMode) {
    case SL_ZIGBEE_ZCL_MOVE_MODE_UP:
      state->increasing = true;
      state->moveToLevel = MAX_LEVEL;
#ifdef ADAPTIV_LIGHT
      if(MAX_LEVEL <= currentLevel)   /* JGR */
      {
         dimming_button_active = false;
         state = NULL;
         status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
         goto send_default_response;
      }
#endif // ADAPTIV_LIGHT
      difference = MAX_LEVEL - currentLevel;
      break;
    case SL_ZIGBEE_ZCL_MOVE_MODE_DOWN:
      state->increasing = false;
      state->moveToLevel = MIN_LEVEL;
#ifdef ADAPTIV_LIGHT
      if(MIN_LEVEL >= currentLevel)   /* JGR */
      {
         dimming_button_active = false;
         state = NULL;
         status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
         goto send_default_response;
      }
#endif // ADAPTIV_LIGHT
      difference = currentLevel - MIN_LEVEL;
      break;
    default:
      status = SL_ZIGBEE_ZCL_STATUS_INVALID_FIELD;
      goto send_default_response;
  }

  // If the level is decreasing, the On/Off attribute is left unchanged.  This
  // logic is to prevent a light from transitioning from off to bright to dim.
  // Instead, a light that is off will stay off until the target level is
  // reached.
  if (currentLevel <= state->moveToLevel) {
    if (commandId == ZCL_MOVE_WITH_ON_OFF_COMMAND_ID) {
#ifdef ADAPTIV_LIGHT
      setOnOffValue(endpoint, (state->moveToLevel != MIN_LEVEL_ON_OFF));
#else  // ADAPTIV_LIGHT
      setOnOffValue(endpoint, (state->moveToLevel != MIN_LEVEL));
#endif // ADAPTIV_LIGHT
    }
    if (currentLevel == state->moveToLevel) {
      status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
#ifdef ADAPTIV_LIGHT
      #if LEVEL_PRINT >= 1
      sl_zigbee_af_level_control_cluster_println("Light level_2 lvl %d == %d   <%d>",
                          currentLevel, hal_get_dim_pwm_255(), get_time_ms());
      #endif
#endif // ADAPTIV_LIGHT
      goto send_default_response;
    }
  }

  // If the Rate field is 0xFF, the DefaultMoveRate SHALL be used
  if (rate == 0xFF) {
#ifdef ZCL_USING_LEVEL_CONTROL_CLUSTER_DEFAULT_MOVE_RATE_ATTRIBUTE
    uint8_t defaultMoveRate = 0;
    status = sl_zigbee_af_read_server_attribute(endpoint,
                                                ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                                ZCL_DEFAULT_MOVE_RATE_ATTRIBUTE_ID,
                                                (uint8_t *)&defaultMoveRate,
                                                sizeof(defaultMoveRate));
    if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
      sl_zigbee_af_level_control_cluster_println("ERR: reading default move rate %02X",
                                                 status);
      goto send_default_response;
    }
    if (defaultMoveRate != 0) {
      state->eventDurationMs = (MILLISECOND_TICKS_PER_SECOND / defaultMoveRate);
    } else {
      goto send_default_response;
    }

#else //ZCL_USING_LEVEL_CONTROL_CLUSTER_DEFAULT_MOVE_RATE_ATTRIBUTE
    // If the Rate field is 0xFF and the DefaultMoveRate attribute is not supported,
    // then the device should move as fast as it is able.
    state->eventDurationMs = FASTEST_TRANSITION_TIME_MS;
#endif //ZCL_USING_LEVEL_CONTROL_CLUSTER_DEFAULT_MOVE_RATE_ATTRIBUTE
  } else {
    // Otherwise, the rate is in units per second.
    state->eventDurationMs = MILLISECOND_TICKS_PER_SECOND / rate;
  }
  state->transitionTimeMs = difference * state->eventDurationMs;
  state->elapsedTimeMs = 0;

  // OnLevel is not used for Move commands.
  state->useOnLevel = false;
  // StoredLevel is not used for Move commands.
  state->storedLevel = INVALID_STORED_LEVEL;

  // The setup was successful, so mark the new state as active and return.
  schedule(endpoint, state->eventDurationMs);
  status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;

  send_default_response:
  sl_zigbee_af_send_immediate_default_response(status);
#ifdef ADAPTIV_LIGHT
  #if LEVEL_PRINT >= 2
  sl_zigbee_af_level_control_cluster_println("moveHandler status 0x%X", status);
  #endif
#endif // ADAPTIV_LIGHT
}

static void stepHandler(uint8_t commandId,
                        uint8_t stepMode,
                        uint8_t stepSize,
                        uint16_t transitionTimeDs,
                        uint8_t optionMask,
                        uint8_t optionOverride)
{
  uint8_t endpoint = sl_zigbee_af_current_endpoint();
  EmberAfLevelControlState *state = getState(endpoint);
  sl_zigbee_af_status_t status;
  uint8_t currentLevel;
  uint8_t actualStepSize = stepSize;

  if (state == NULL) {
    status = SL_ZIGBEE_ZCL_STATUS_FAILURE;
    goto send_default_response;
  }

  if (!shouldExecuteIfOff(endpoint, commandId, optionMask, optionOverride)) {
    status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
#ifdef ADAPTIV_LIGHT
    #if LEVEL_PRINT >= 1
    sl_zigbee_af_level_control_cluster_println("Step 1");
    #endif
#endif // ADAPTIV_LIGHT
    goto send_default_response;
  }

  // Cancel any currently active command before fiddling with the state.
  deactivate(endpoint);

  status = sl_zigbee_af_read_server_attribute(endpoint,
                                              ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                              ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                              (uint8_t *)&currentLevel,
                                              sizeof(currentLevel));
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_level_control_cluster_println("ERR: reading current level %02X", status);
    goto send_default_response;
  }

  state->commandId = commandId;
  //sl_zigbee_af_level_control_cluster_println("lvl %d  mode 0x%X",currentLevel, stepMode);//JGR

  // Step commands cause the device to move from its current level to a new
  // level over the specified transition time.
  switch (stepMode) {
    case SL_ZIGBEE_ZCL_STEP_MODE_UP:
      state->increasing = true;
#ifdef ADAPTIV_LIGHT
      if(MAX_LEVEL <= currentLevel)   /* JGR */
      {
         dimming_button_active = false;
         state = NULL;
         status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
         goto send_default_response;
      }
#endif // ADAPTIV_LIGHT
      if (MAX_LEVEL - currentLevel < stepSize) {
        state->moveToLevel = MAX_LEVEL;
        actualStepSize = (MAX_LEVEL - currentLevel);
      } else {
        state->moveToLevel = currentLevel + stepSize;
      }
      break;
    case SL_ZIGBEE_ZCL_STEP_MODE_DOWN:
      state->increasing = false;
#ifdef ADAPTIV_LIGHT
      if(MIN_LEVEL >= currentLevel)   /* JGR */
      {
         dimming_button_active = false;
         state = NULL;
         status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
         goto send_default_response;
      }
#endif // ADAPTIV_LIGHT
      if (currentLevel - MIN_LEVEL < stepSize) {
        state->moveToLevel = MIN_LEVEL;
        actualStepSize = (currentLevel - MIN_LEVEL);
      } else {
        state->moveToLevel = currentLevel - stepSize;
      }
      break;
    default:
      status = SL_ZIGBEE_ZCL_STATUS_INVALID_FIELD;
      goto send_default_response;
  }
#ifdef ADAPTIV_LIGHT
  #if (LEVEL_PRINT >= 2)
  sl_zigbee_af_level_control_cluster_println("Step Tick Mode:%d lvl:%d pwm:%d   <%d>", stepMode, currentLevel, hal_get_dim_pwm_255(), get_time_ms());
  #endif
#endif // ADAPTIV_LIGHT

  // If the level is decreasing, the On/Off attribute is left unchanged.  This
  // logic is to prevent a light from transitioning from off to bright to dim.
  // Instead, a light that is off will stay off until the target level is
  // reached.
  if (currentLevel <= state->moveToLevel) {
    if (commandId == ZCL_STEP_WITH_ON_OFF_COMMAND_ID) {
#ifdef ADAPTIV_LIGHT
      setOnOffValue(endpoint, (state->moveToLevel != MIN_LEVEL_ON_OFF));
#else  // ADAPTIV_LIGHT
      setOnOffValue(endpoint, (state->moveToLevel != MIN_LEVEL));
#endif // ADAPTIV_LIGHT
    }
    if (currentLevel == state->moveToLevel) {
      status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
#ifdef ADAPTIV_LIGHT
      #if LEVEL_PRINT >= 1
      sl_zigbee_af_level_control_cluster_println("Light level_3 lvl %d == %d   <%d>",
                       currentLevel, hal_get_dim_pwm_255(), get_time_ms());
      #endif
#endif // ADAPTIV_LIGHT
      goto send_default_response;
    }
  }

  // If the Transition Time field is 0xFFFF, the device should move as fast as
  // it is able.
  if (transitionTimeDs == 0xFFFF) {
    state->transitionTimeMs = FASTEST_TRANSITION_TIME_MS;
  } else {
    // Transition time comes in as tenths of a second, but we work in
    // milliseconds.
    state->transitionTimeMs = (transitionTimeDs
                               * MILLISECOND_TICKS_PER_SECOND
                               / 10);
    // If the new level was pegged at the minimum level, the transition time
    // shall be proportionally reduced.  This is done after the conversion to
    // milliseconds to reduce rounding errors in integer division.
    if (stepSize != actualStepSize) {
      state->transitionTimeMs = (state->transitionTimeMs
                                 * actualStepSize
                                 / stepSize);
    }
  }

  // The duration between events will be the transition time divided by the
  // distance we must move.
  state->eventDurationMs = state->transitionTimeMs / actualStepSize;
  state->elapsedTimeMs = 0;

  // OnLevel is not used for Step commands.
  state->useOnLevel = false;
  // StoredLevel is not used for Step commands.
  state->storedLevel = INVALID_STORED_LEVEL;

  // The setup was successful, so mark the new state as active and return.
  schedule(endpoint, state->eventDurationMs);
  status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;

  send_default_response:
  sl_zigbee_af_send_immediate_default_response(status);
}

static void stopHandler(uint8_t commandId,
                        uint8_t optionMask,
                        uint8_t optionOverride)
{
  uint8_t endpoint = sl_zigbee_af_current_endpoint();
  EmberAfLevelControlState *state = getState(endpoint);
  sl_zigbee_af_status_t status;

#ifdef ADAPTIV_LIGHT
  dimming_button_active = false;

#endif // ADAPTIV_LIGHT
  if (state == NULL) {
    status = SL_ZIGBEE_ZCL_STATUS_FAILURE;
    goto send_default_response;
  }

  if (!shouldExecuteIfOff(endpoint, commandId, optionMask, optionOverride)) {
    status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
    goto send_default_response;
  }

  // Cancel any currently active command.
  deactivate(endpoint);
  writeRemainingTime(endpoint, 0);
  status = SL_ZIGBEE_ZCL_STATUS_SUCCESS;
#ifdef ADAPTIV_LIGHT
  save_present_light_level_to_on_level_attribute();
#endif // ADAPTIV_LIGHT

  send_default_response:
  sl_zigbee_af_send_immediate_default_response(status);
}

// Follows 07-5123-04 (ZigBee Cluster Library doc), section 3.10.2.1.1.
// Quotes are from table 3.46.
// Called from on/off cmd where newValue is on/off = 1/0 and is not
// initiated from a level change, ie. staight on/off cmd.
// If on cmd, the on/off attribute is set prior to this call.  If off cmd the
// on/off attribute is set after this call, JGR
void sl_zigbee_af_on_off_cluster_level_control_effect_cb(uint8_t endpoint,
                                                         bool newValue)
{
  uint8_t temporaryCurrentLevelCache;
  uint16_t currentOnOffTransitionTime;
#ifdef ADAPTIV_LIGHT
  uint8_t currentOnLevel;
#else  // ADAPTIV_LIGHT
  uint8_t resolvedLevel;
#endif // ADAPTIV_LIGHT
  uint8_t minimumLevelAllowedForTheDevice = MIN_LEVEL;
  sl_zigbee_af_status_t status;

  // "Temporarily store CurrentLevel."
  status = sl_zigbee_af_read_server_attribute(endpoint,
                                              ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                              ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                              (uint8_t *)&temporaryCurrentLevelCache,
                                              sizeof(temporaryCurrentLevelCache));
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_level_control_cluster_println("ERR: reading current level %02X", status);
    return;
  }

  // Read the OnLevel attribute.
#ifdef ZCL_USING_LEVEL_CONTROL_CLUSTER_ON_LEVEL_ATTRIBUTE
  status = sl_zigbee_af_read_server_attribute(endpoint,
                                              ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                              ZCL_ON_LEVEL_ATTRIBUTE_ID,
#ifdef ADAPTIV_LIGHT
                                              (uint8_t *)&currentOnLevel, // OnLevel value
                                              sizeof(currentOnLevel));
#else  // ADAPTIV_LIGHT
                                              (uint8_t *)&resolvedLevel, // OnLevel value
                                              sizeof(resolvedLevel));
#endif // ADAPTIV_LIGHT
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_level_control_cluster_println("ERR: reading current level %02X", status);
    return;
  }

#ifdef ADAPTIV_LIGHT
  if (currentOnLevel == 0xFF) {
#else  // ADAPTIV_LIGHT
  if (resolvedLevel == 0xFF) {
#endif // ADAPTIV_LIGHT
    // OnLevel has undefined value; fall back to CurrentLevel.
#ifdef ADAPTIV_LIGHT
    currentOnLevel = temporaryCurrentLevelCache;
#else  // ADAPTIV_LIGHT
    resolvedLevel = temporaryCurrentLevelCache;
#endif // ADAPTIV_LIGHT
  }
#else
#ifdef ADAPTIV_LIGHT
  currentOnLevel = temporaryCurrentLevelCache;
#else  // ADAPTIV_LIGHT
  resolvedLevel = temporaryCurrentLevelCache;
#endif // ADAPTIV_LIGHT
#endif

  // Read the OnOffTransitionTime attribute.
#ifdef ZCL_USING_LEVEL_CONTROL_CLUSTER_ON_OFF_TRANSITION_TIME_ATTRIBUTE
  status = sl_zigbee_af_read_server_attribute(endpoint,
                                              ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                              ZCL_ON_OFF_TRANSITION_TIME_ATTRIBUTE_ID,
                                              (uint8_t *)&currentOnOffTransitionTime,
                                              sizeof(currentOnOffTransitionTime));
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_level_control_cluster_println("ERR: reading current level %02X", status);
    return;
  }
#else
  currentOnOffTransitionTime = 0xFFFF;
#endif

#ifdef ADAPTIV_LIGHT
#if LEVEL_PRINT >= 2
sl_zigbee_af_level_control_cluster_println("sl_zigbee_af_on_off_cluster_level_control_effect_cb ep %d on/off %d",
                                                           endpoint, newValue);
#endif

#endif // ADAPTIV_LIGHT
  if (newValue) {
    // If newValue is ZCL_ON_COMMAND_ID...
    // "Set CurrentLevel to minimum level allowed for the device."
    status = sl_zigbee_af_write_server_attribute(endpoint,
                                                 ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                                 ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                                 (uint8_t *)&minimumLevelAllowedForTheDevice,
                                                 ZCL_INT8U_ATTRIBUTE_TYPE);
#ifdef ADAPTIV_LIGHT
    #if LEVEL_PRINT >= 1
    sl_zigbee_af_level_control_cluster_println("1051 %d -> %d", minimumLevelAllowedForTheDevice, currentOnLevel);
    #endif
#endif // ADAPTIV_LIGHT
    if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
      sl_zigbee_af_level_control_cluster_println("ERR: reading current level %02X", status);
      return;
    }

#ifdef ADAPTIV_LIGHT
    // Another Daintree bag o puss mod JGR
    if(currentOnLevel == minimumLevelAllowedForTheDevice)
    {
        currentOnLevel = minimumLevelAllowedForTheDevice + 1;
        sl_zigbee_af_level_control_cluster_println("NEW LEVEL %d",currentOnLevel);
    }
#endif // ADAPTIV_LIGHT
    // "Move CurrentLevel to OnLevel, or to the stored level if OnLevel is not
    // defined, over the time period OnOffTransitionTime."
    moveToLevelHandler(ZCL_MOVE_TO_LEVEL_COMMAND_ID,
#ifdef ADAPTIV_LIGHT
                       currentOnLevel,       /* ZCL_ON_LEVEL_ATTRIBUTE_ID */
#else  // ADAPTIV_LIGHT
                       resolvedLevel,
#endif // ADAPTIV_LIGHT
                       currentOnOffTransitionTime,
                       0xFF,
                       0xFF,
                       INVALID_STORED_LEVEL); // Don't revert to stored level
  } else {
    // ...else if newValue is ZCL_OFF_COMMAND_ID...
    // "Move CurrentLevel to the minimum level allowed for the device over the
    // time period OnOffTransitionTime."
#ifdef ADAPTIV_LIGHT
    #if LEVEL_PRINT >= 1
    sl_zigbee_af_level_control_cluster_println("1050 %d -> 0",minimumLevelAllowedForTheDevice);
    #endif
#if 0   // JGR  more daintree crap
    moveToLevelHandler(ZCL_MOVE_TO_LEVEL_COMMAND_ID,
                       minimumLevelAllowedForTheDevice,
                       currentOnOffTransitionTime,
                       0xFF,
                       0xFF,
                       temporaryCurrentLevelCache); // Original
#else
    moveToLevelHandler(ZCL_MOVE_TO_LEVEL_COMMAND_ID,
                       0,
                       currentOnOffTransitionTime,
                       0xFF,
                       0xFF,
                       INVALID_STORED_LEVEL);//JGR
#endif
#else  // ADAPTIV_LIGHT
    moveToLevelHandler(ZCL_MOVE_TO_LEVEL_COMMAND_ID,
                       minimumLevelAllowedForTheDevice,
                       currentOnOffTransitionTime,
                       0xFF,
                       0xFF,
                       temporaryCurrentLevelCache);
#endif // ADAPTIV_LIGHT

    // "If OnLevel is not defined, set the CurrentLevel to the stored level."
    // The sl_zigbee_af_level_control_cluster_server_tick_cb implementation handles
    // this.
  }
}

void sl_zigbee_af_level_control_cluster_server_init_cb(uint8_t endpoint)
{
#ifdef ADAPTIV_LIGHT
  init_min_max_silab_levels();    // More bull shit

#endif // ADAPTIV_LIGHT
#ifdef ZCL_USING_LEVEL_CONTROL_CLUSTER_START_UP_CURRENT_LEVEL_ATTRIBUTE
  // StartUp behavior relies StartUpCurrentLevel attributes being tokenized.
  if (areStartUpLevelControlServerAttributesTokenized(endpoint)) {
    // Read the StartUpOnOff attribute and set the OnOff attribute as per
    // following from zcl 7 14-0127-20i-zcl-ch-3-general.doc.
    // 3.10.2.2.14	StartUpCurrentLevel Attribute
    // The StartUpCurrentLevel attribute SHALL define the desired startup level
    // for a device when it is supplied with power and this level SHALL be
    // reflected in the CurrentLevel attribute. The values of the StartUpCurrentLevel
    // attribute are listed below:
    // Table 3 58. Values of the StartUpCurrentLevel Attribute
    // Value      Action on power up
    // 0x00       Set the CurrentLevel attribute to the minimum value permitted on the device.
    // 0x01-0xfe  Set the CurrentLevel attribute to this value.
    // 0xff       Set the CurrentLevel attribute to its previous value.

    // Initialize startUpCurrentLevel to assume previous value for currentLevel.
    uint8_t startUpCurrentLevel = STARTUP_CURRENT_LEVEL_USE_PREVIOUS_LEVEL;
    sl_zigbee_af_status_t status = sl_zigbee_af_read_attribute(endpoint,
                                                               ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                                               ZCL_START_UP_CURRENT_LEVEL_ATTRIBUTE_ID,
                                                               CLUSTER_MASK_SERVER,
                                                               (uint8_t *)&startUpCurrentLevel,
                                                               sizeof(startUpCurrentLevel),
                                                               NULL);
    if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
      uint8_t currentLevel = 0;
      status = sl_zigbee_af_read_attribute(endpoint,
                                           ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                           ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                           CLUSTER_MASK_SERVER,
                                           (uint8_t *)&currentLevel,
                                           sizeof(currentLevel),
                                           NULL);
      if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
        switch (startUpCurrentLevel) {
          case STARTUP_CURRENT_LEVEL_USE_DEVICE_MINIMUM:
            currentLevel = MIN_LEVEL;
            break;
          case STARTUP_CURRENT_LEVEL_USE_PREVIOUS_LEVEL:
            // Just fetched it.
            break;
          default:
            // Otherwise set to specified value 0x01-0xFE.
            // But, need to enforce currentLevel's min/max, right?
            // Spec doesn't mention this.
            SL_IGNORE_TYPE_LIMIT_BEGIN;
            if (startUpCurrentLevel < MIN_LEVEL) {
              currentLevel = MIN_LEVEL;
            } else if (startUpCurrentLevel > MAX_LEVEL) {
              currentLevel = MAX_LEVEL;
            } else {
              currentLevel = startUpCurrentLevel;
            }
            SL_IGNORE_TYPE_LIMIT_END;
            break;
        }
        status = sl_zigbee_af_write_attribute(endpoint,
                                              ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                              ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                              CLUSTER_MASK_SERVER,
                                              (uint8_t *)&currentLevel,
                                              ZCL_INT8U_ATTRIBUTE_TYPE);
#ifdef ADAPTIV_LIGHT
        #if LEVEL_PRINT >= 1
        sl_zigbee_af_level_control_cluster_println("1155 %d",currentLevel);
        #endif
        /*** Write to PWM ***/
         hal_set_dim_pwm_255(currentLevel);  //JGR
         #if LEVEL_PRINT >= 2
         sl_zigbee_af_level_control_cluster_println("Tick1: Set PWM to %d", currentLevel);
         #endif
#endif // ADAPTIV_LIGHT
      }
    }
  }
#endif
  sl_zigbee_af_level_control_cluster_server_post_init_cb(endpoint);
}

#ifdef ZCL_USING_LEVEL_CONTROL_CLUSTER_START_UP_CURRENT_LEVEL_ATTRIBUTE
static bool areStartUpLevelControlServerAttributesTokenized(uint8_t endpoint)
{
  sl_zigbee_af_attribute_metadata_t *metadata;

  metadata = sl_zigbee_af_locate_attribute_metadata(endpoint,
                                                    ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                                    ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                                                    CLUSTER_MASK_SERVER,
                                                    SL_ZIGBEE_AF_NULL_MANUFACTURER_CODE);
  if (!sl_zigbee_af_attribute_is_tokenized(metadata)) {
    return false;
  }

  metadata = sl_zigbee_af_locate_attribute_metadata(endpoint,
                                                    ZCL_LEVEL_CONTROL_CLUSTER_ID,
                                                    ZCL_START_UP_CURRENT_LEVEL_ATTRIBUTE_ID,
                                                    CLUSTER_MASK_SERVER,
                                                    SL_ZIGBEE_AF_NULL_MANUFACTURER_CODE);
  if (!sl_zigbee_af_attribute_is_tokenized(metadata)) {
    return false;
  }

  return true;
}
#endif

uint32_t sl_zigbee_af_level_control_cluster_server_command_parse(sl_service_opcode_t opcode,
                                                                 sl_service_function_context_t *context)
{
  (void)opcode;

  sl_zigbee_af_cluster_command_t *cmd = (sl_zigbee_af_cluster_command_t *)context->data;
  sl_zigbee_af_zcl_request_status_t status = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;

  if (!cmd->mfgSpecific) {
#ifdef ADAPTIV_LIGHT
    uint32_t Dsecs = (uint32_t)get_unoccupied_delay() * 10UL; // Delay in seconds, make dsecs

    suspend_light_sensor_control(Dsecs);
    suspend_occ_sensor_control(DSECS_5_SECS);
    suspend_light_sensor_control(DSECS_5_SECS);
    dimming_button_active = false;

#endif // ADAPTIV_LIGHT
    switch (cmd->commandId) {
      case ZCL_MOVE_TO_LEVEL_COMMAND_ID:
      {
        status = sl_zigbee_af_level_control_cluster_move_to_level_cb(cmd);
        break;
      }
      case ZCL_MOVE_COMMAND_ID:
      {
#ifdef ADAPTIV_LIGHT
        dimming_button_active = true;
        suspend_light_sensor_control_dimming(get_unoccupied_delay() * 10); // Delay attr in seconds
#endif // ADAPTIV_LIGHT
        status = sl_zigbee_af_level_control_cluster_move_cb(cmd);
        break;
      }
      case ZCL_STEP_COMMAND_ID:
      {
#ifdef ADAPTIV_LIGHT
        dimming_button_active = true;
        suspend_light_sensor_control_dimming(get_unoccupied_delay() * 10); // Delay attr in seconds
#endif // ADAPTIV_LIGHT
        status = sl_zigbee_af_level_control_cluster_step_cb(cmd);
        break;
      }
      case ZCL_STOP_COMMAND_ID:
      {
        status = sl_zigbee_af_level_control_cluster_stop_cb(cmd);
        break;
      }
      case ZCL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID:
      {
        status = sl_zigbee_af_level_control_cluster_move_to_level_with_on_off_cb(cmd);
        break;
      }
      case ZCL_MOVE_WITH_ON_OFF_COMMAND_ID:
      {
#ifdef ADAPTIV_LIGHT
        dimming_button_active = true;
        suspend_light_sensor_control_dimming(get_unoccupied_delay() * 10); // Delay attr in seconds
#endif // ADAPTIV_LIGHT
        status = sl_zigbee_af_level_control_cluster_move_with_on_off_cb(cmd);
        break;
      }
      case ZCL_STEP_WITH_ON_OFF_COMMAND_ID:
      {
#ifdef ADAPTIV_LIGHT
        dimming_button_active = true;
        suspend_light_sensor_control_dimming(get_unoccupied_delay() * 10); // Delay attr in seconds
#endif // ADAPTIV_LIGHT
        status = sl_zigbee_af_level_control_cluster_step_with_on_off_cb(cmd);
        break;
      }
      case ZCL_STOP_WITH_ON_OFF_COMMAND_ID:
      {
        status = sl_zigbee_af_level_control_cluster_stop_with_on_off_cb();
        break;
      }
#ifdef ADAPTIV_LIGHT
      default:
      {
        sl_zigbee_af_level_control_cluster_println("ERR: sl_zigbee_af_level_control_cluster_server_command_parse cmd 0x%02X", cmd->commandId);
        break;
      }
#endif // ADAPTIV_LIGHT
    }
  }

  return status;
}

#ifdef ADAPTIV_LIGHT
/************************************************************************************/
/*******************************  ADAPTIV  ******************************************/
/************************************************************************************/
void move_to_level_onoff(uint8_t level, uint16_t transitionTimeDs)
{
  if(dimming_button_active)
  {
     // Set when switch sends nove/step cmd.  Else cleared
     #if 1 // Debug
     sl_zigbee_app_debug_println("NT-3: Aborted dim active   <%d>", get_time_ms());
     #endif

     return;
  }
  // This stops all sensors and forces occupancy sensor to reset to on_unoccupied state
  // Also suspend_light_sensor_control()
  suspend_all_sensors(transitionTimeDs + DSECS_1_SECS);

  // Stop any occurring level changes
  stop_ramp();

  // Saturate levels?
  if(get_max_light_level() < level)
  {
     level = get_max_light_level();
  }
  if(0 != get_min_light_level())
  {
     if(get_min_light_level() > level)
     {
       level = get_min_light_level();
     }
  }

  moveToLevelHandler(ZCL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID,
                     level,
                     transitionTimeDs,
                     0xFF,  // options = SL_ZIGBEE_ZCL_LEVEL_CONTROL_OPTIONS_EXECUTE_IF_OFF
                     0xFF,
                     INVALID_STORED_LEVEL); // Don't revert to the stored level);

  #if 1 // Debug
  sl_zigbee_app_debug_println("NT-3: %d -> %d (cmdLvl)  xtime:%d.%d secs   <%d>",
                            read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID),
                            level,
                            transitionTimeDs/10, transitionTimeDs - (10*(transitionTimeDs/10)),
                            get_time_ms());
  #endif
}

// For DHS
void move_to_level_dhs(uint8_t level, uint16_t transitionTimeDs)
{
  if(dimming_button_active)
  {
     // Set when switch sends nove/step cmd.  Else cleared
     #if 1 // Debug
     sl_zigbee_app_debug_println("DHS: Aborted dim active   <%d>", get_time_ms());
     #endif

     return;
  }

  // Saturate max level
  if(get_max_light_level() < level)
  {
     level = get_max_light_level();
  }

  moveToLevelHandler(ZCL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID,
                     level,
                     transitionTimeDs,
                     0xFF,  // options = SL_ZIGBEE_ZCL_LEVEL_CONTROL_OPTIONS_EXECUTE_IF_OFF
                     0xFF,
                     INVALID_STORED_LEVEL); // Don't revert to the stored level);

  #if 1 // Debug
  sl_zigbee_app_debug_println("DHS: %d -> %d (cmdLvl)  xtime:%d.%d secs   <%d>",
                            read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID),
                            level,
                            transitionTimeDs/10, transitionTimeDs - (10*(transitionTimeDs/10)),
                            get_time_ms());
  #endif
}

// If level ramping is in progress, will halt.  No harm if no ramp in progress
void stop_ramp(void)
{
  deactivate(LIGHT_ENDPOINT);
  writeRemainingTime(LIGHT_ENDPOINT, 0);
  dimming_button_active = false;
}

// Called every time attributes change
void init_min_max_silab_levels(void)
{
  //  Set up MIN_LEVEL since it's hard coded in this level code
  if(get_min_light_level() <= DEFAULT_MIN_LIGHT_LEVEL_DT)
  {
     _min_level = DEFAULT_MIN_LIGHT_LEVEL_DT;
     _min_level_on_off = DEFAULT_MIN_LIGHT_LEVEL_DT - 1;
     //sl_zigbee_app_debug_println("1 %d %d",_min_level, _min_level_on_off);
  }
  else if(get_min_light_level() > DEFAULT_MIN_LIGHT_LEVEL_DT)
  {
     _min_level = get_min_light_level();
     _min_level_on_off = _min_level - 1;
     //sl_zigbee_app_debug_println("2 %d %d",_min_level, _min_level_on_off);
  }
  else
  {
     _min_level = DEFAULT_MIN_LIGHT_LEVEL_DT;
     _min_level_on_off = DEFAULT_MIN_LIGHT_LEVEL_DT - 1;
     //sl_zigbee_app_debug_println("3 %d %d",_min_level, _min_level_on_off);
  }
}

bool light_level_active(void)
{

  EmberAfLevelControlState *state = getState(LIGHT_ENDPOINT);
#if 0
  if(NULL != state)
  {
     sl_zigbee_app_debug_println("Lvls %d %d  t %d   <%d>", state->moveToLevel,
         read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID),
         read_light_level_attribute(ZCL_LEVEL_CONTROL_REMAINING_TIME_ATTRIBUTE_ID),
         get_time_ms());
  }
  else
  {
     sl_zigbee_app_debug_println("NULL light_level_active");
  }
  #endif

  if (state == NULL)
  {
    return(false);
  }
  else if((state->moveToLevel == read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID)) &&
      (0 == read_light_level_attribute(ZCL_LEVEL_CONTROL_REMAINING_TIME_ATTRIBUTE_ID)))
  {
    return(false);
  }
  else
  {
    return(true);
  }
}

#endif // ADAPTIV_LIGHT