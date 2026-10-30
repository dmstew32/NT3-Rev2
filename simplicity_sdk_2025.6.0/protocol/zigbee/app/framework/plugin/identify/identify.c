/***************************************************************************//**
 * @file
 * @brief Routines for the Identify plugin, which implements the Identify
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

// *******************************************************************
// * identify.c
// *
// *
// * Copyright 2007 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

// this file contains all the common includes for clusters in the util
#include "app/framework/include/af.h"
#include "app/framework/util/common.h"
#include "zap-cluster-command-parser.h"

#include "identify.h"

#include "sl_zigbee_debug_print.h"
#include "hal_pwm.h"
#include "light_on_off.h"
#include "nwk_utils.h"
#include "level-control-config.h"

extern sl_zigbee_af_zcl_request_status_t sl_zigbee_af_on_off_cluster_off_cb(void);
//#define IDENTIFY_PRINT

typedef struct {
  bool identifying;
  uint16_t identifyTime;
} sli_zigbee_af_identify_state;

static bool id_commanded = false;
static uint8_t saved_level = DEFAULT_MAX_LIGHT_LEVEL;

static sli_zigbee_af_identify_state stateTable[SL_ZIGBEE_ZCL_IDENTIFY_CLUSTER_SERVER_ENDPOINT_COUNT];

static sl_zigbee_af_status_t readIdentifyTime(uint8_t endpoint, uint16_t *identifyTime);
static sl_zigbee_af_status_t writeIdentifyTime(uint8_t endpoint, uint16_t identifyTime);
static sl_status_t scheduleIdentifyTick(uint8_t endpoint);

static sli_zigbee_af_identify_state *getIdentifyState(uint8_t endpoint);



static sli_zigbee_af_identify_state *getIdentifyState(uint8_t endpoint)
{
  uint8_t ep = sl_zigbee_af_find_cluster_server_endpoint_index(endpoint, ZCL_IDENTIFY_CLUSTER_ID);
  return (ep == SL_ZIGBEE_AF_INVALID_ENDPOINT_INDEX ? NULL : &stateTable[ep]);
}

void sl_zigbee_af_identify_cluster_server_init_cb(uint8_t endpoint)
{
  writeIdentifyTime(1, 0x0000);//JGR
  scheduleIdentifyTick(endpoint);
}

void sl_zigbee_af_identify_cluster_server_tick_cb(uint8_t endpoint)
{
  uint16_t identifyTime;
  if (readIdentifyTime(endpoint, &identifyTime) == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    // This tick writes the new attribute, which will trigger the Attribute
    // Changed callback below, which in turn will schedule or cancel the tick.
    // Because of this, the tick does not have to be scheduled here.
    writeIdentifyTime(endpoint, (identifyTime == 0 ? 0 : identifyTime - 1));

    /*** Send to light, restore when done JGR ***/
    #ifdef IDENTIFY_PRINT
    sl_zigbee_app_debug_println("Identify tick time %d  ep %d",identifyTime,endpoint );//JGR
    #endif
    move_to_level_onoff((0x0001 & identifyTime) ? get_max_light_level() : DEFAULT_MIN_LIGHT_LEVEL_DT, 2);

    if( 1 >= identifyTime)
    {
        // Restore original level, even off
        if(0 != saved_level)
        {
           move_to_level_onoff(saved_level, 2);
        }
        else
        {
           sl_zigbee_af_on_off_cluster_off_cb();
        }
    }
  }
}

void sl_zigbee_af_identify_cluster_server_attribute_changed_cb(uint8_t endpoint,
                                                               sl_zigbee_af_attribute_id_t attributeId)
{
  #ifdef IDENTIFY_PRINT
  sl_zigbee_app_debug_println("sl_zigbee_af_identify_cluster_server_attribute_changed_cb ep %d",endpoint);
  #endif

  if (attributeId == ZCL_IDENTIFY_TIME_ATTRIBUTE_ID) {
    scheduleIdentifyTick(endpoint);
  }
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_identify_cluster_identify_cb(sl_zigbee_af_cluster_command_t *cmd)
{
  sl_zcl_identify_cluster_identify_command_t cmd_data;
  sl_zigbee_af_status_t status;

  if (zcl_decode_identify_cluster_identify_command(cmd, &cmd_data)
      != SL_ZIGBEE_ZCL_STATUS_SUCCESS ) {
    return SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;
  }

  // This Identify callback writes the new attribute, which will trigger the
  // Attribute Changed callback above, which in turn will schedule or cancel the
  // tick.  Because of this, the tick does not have to be scheduled here.
  sl_zigbee_af_identify_cluster_println("RX identify:IDENTIFY 0x%04X", cmd_data.identifyTime);
  status = writeIdentifyTime(sl_zigbee_af_current_endpoint(), cmd_data.identifyTime);
  return status;
}

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_identify_cluster_identify_query_cb(void)
{
  sl_zigbee_af_status_t status;
  sl_status_t sendStatus;
  uint16_t identifyTime;

  sl_zigbee_af_identify_cluster_println("RX identify:QUERY");

  // According to the 075123r02ZB, a device shall not send an Identify Query
  // Response if it is not currently identifying.  Instead, or if reading the
  // Identify Time attribute fails, send a Default Response.
  status = readIdentifyTime(sl_zigbee_af_current_endpoint(), &identifyTime);
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS || identifyTime == 0) {
    return status;
  }

  sl_zigbee_af_fill_command_identify_cluster_identify_query_response(identifyTime);
  sendStatus = sl_zigbee_af_send_response();
  if (SL_STATUS_OK != sendStatus) {
    sl_zigbee_af_identify_cluster_println("Identify: failed to send %s response: 0x%02X",
                                          "query",
                                          sendStatus);
  }
  return SL_ZIGBEE_ZCL_STATUS_INTERNAL_COMMAND_HANDLED;
}

static sl_zigbee_af_status_t readIdentifyTime(uint8_t endpoint,
                                              uint16_t *identifyTime)
{
  sl_zigbee_af_status_t status = sl_zigbee_af_read_attribute(endpoint,
                                                             ZCL_IDENTIFY_CLUSTER_ID,
                                                             ZCL_IDENTIFY_TIME_ATTRIBUTE_ID,
                                                             CLUSTER_MASK_SERVER,
                                                             (uint8_t *)identifyTime,
                                                             sizeof(*identifyTime),
                                                             NULL); // data type
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_identify_cluster_println("ERR: reading identify time %02X", status);
  }
  return status;
}

static sl_zigbee_af_status_t writeIdentifyTime(uint8_t endpoint, uint16_t identifyTime)
{
  sl_zigbee_af_status_t status = sl_zigbee_af_write_attribute(endpoint,
                                                              ZCL_IDENTIFY_CLUSTER_ID,
                                                              ZCL_IDENTIFY_TIME_ATTRIBUTE_ID,
                                                              CLUSTER_MASK_SERVER,
                                                              (uint8_t *)&identifyTime,
                                                              ZCL_INT16U_ATTRIBUTE_TYPE);
  if (status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    sl_zigbee_af_identify_cluster_println("ERR: writing identify time %02X", status);
  }
  return status;
}

static sl_status_t scheduleIdentifyTick(uint8_t endpoint)
{
  sl_zigbee_af_status_t status;
  sli_zigbee_af_identify_state *state = getIdentifyState(endpoint);
  uint16_t identifyTime;

  if (state == NULL) {
    return SL_STATUS_INVALID_PARAMETER;
  }

  status = readIdentifyTime(endpoint, &identifyTime);
  if (status == SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
    if (!state->identifying) {
      state->identifying = true;
      state->identifyTime = identifyTime;
      // Save original level, restored when done
      saved_level = get_present_light_level();
      sl_zigbee_af_identify_start_feedback_cb(endpoint,
                                              identifyTime);
    }
    if (identifyTime > 0) {
      return sl_zigbee_zcl_schedule_server_tick(endpoint,
                                                ZCL_IDENTIFY_CLUSTER_ID,
                                                MILLISECOND_TICKS_PER_SECOND);
    }
  }

  state->identifying = false;
  sl_zigbee_af_identify_stop_feedback_cb(endpoint);

  return sl_zigbee_zcl_deactivate_server_tick(endpoint, ZCL_IDENTIFY_CLUSTER_ID);
}

uint32_t sl_zigbee_af_identify_cluster_server_command_parse(sl_service_opcode_t opcode,
                                                            sl_service_function_context_t *context)
{
  (void)opcode;
  sl_zigbee_af_cluster_command_t *cmd = (sl_zigbee_af_cluster_command_t *)context->data;
  sl_zigbee_af_zcl_request_status_t status = SL_ZIGBEE_ZCL_STATUS_UNSUP_COMMAND;

  if (!cmd->mfgSpecific) {
    switch (cmd->commandId) {
      case ZCL_IDENTIFY_COMMAND_ID:
      {
        id_commanded = true;  //JGR
        status = sl_zigbee_af_identify_cluster_identify_cb(cmd);
        break;
      }
      case ZCL_IDENTIFY_QUERY_COMMAND_ID:
      {
        status = sl_zigbee_af_identify_cluster_identify_query_cb();
        break;
      }
    }
  }

  return status;
}
