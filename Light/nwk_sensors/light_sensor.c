#include "af.h"
#include <stdlib.h>
#include <math.h>

#include "RehTOS.h"
#include "hal_utils.h"
#include "hal_light_sensor.h"
#include "hal_pwm.h"
#include "light_sensor.h"
#include "dh_control.h"
#include "dh_debug.h"
#include "broadcast_monitor.h"
#include "light_on_off.h"
#include "nwk_utils.h"
#include "nwk_groups.h"
#include "occ_sensor.h"
#include "nwk_multicast.h"
#include "adaptiv_config.h"
#include "sl_token_api.h"
#include "sl_token_manager.h"

#include "sl_zigbee_debug_print.h"

// 0 = off  1 = lite  2 = all
#ifndef LIGHT_SENSOR_PRINT
#define LIGHT_SENSOR_PRINT 0    /* Default to 0 */
#elif (LIGHT_SENSOR_PRINT != 0)
#warning "LIGHT_SENSOR_PRINT active"
#endif

// Inhibit bits, see get_dhs_inhibit_bits().  Bit 1 is unused (was: not calibrated).
#define DHS_INHIBIT_NOT_ADAPTIV   (1U << 0)
#define DHS_INHIBIT_OFF           (1U << 2)
#define DHS_INHIBIT_NOT_IN_GROUP  (1U << 3)
#define DHS_INHIBIT_SUSPENDED     (1U << 4)
#define DHS_INHIBIT_NO_TARGET     (1U << 5)
#define DHS_INHIBIT_LOW_LEVEL     (1U << 6)
#define DHS_INHIBIT_OCC_MID       (1U << 7)
#define DHS_INHIBIT_DIMMING       (1U << 8)
#define DHS_INHIBIT_DISABLED      (1U << 9)
#define DHS_INHIBIT_NOT_MASTER    (1U << 10)

// Only the master needs a target; any light in the group follows the master
#define DHS_INHIBITS_FOR_FOLLOWER ((uint16_t)~(DHS_INHIBIT_NO_TARGET | DHS_INHIBIT_NOT_MASTER))

static int16_t light_sensor_reference = 0;
static bool light_sensor_control_suspended = true;
static uint32_t light_sensor_suspend_end_ms = 0UL;
static bool light_sensor_control_suspended_dimming = false;
static light_sensor_data_t light_sensor_data;
static uint16_t dhs_inhibit_bits = 0;

static dh_master_t dh_master;
// Something changed the light or lit the sensor since the last average
static bool dhs_average_disturbed = false;

/****************************  SENDING  ****************************/
static bool dh_send_level(void *ctx, uint8_t level, uint8_t ramp_ds)
{
  (void)ctx;
  bool sent = tx_dh_level(level, ramp_ds);

  if(sent)
  {
     dh_debug_send(light_sensor_data.mean_lux, (uint16_t)light_sensor_reference, hal_get_dim_pwm_255(),
                   is_app_flags_nonvol_set(B_DHS_MASTER_LIGHT) ? DH_DEBUG_MODE_MASTER : 0U, level);
  }

  #if (1 <= LIGHT_SENSOR_PRINT)
  sl_zigbee_app_debug_println("DHS MASTER  tx level:%d %s  lux:%d  ref:%d   <%d>", level, sent ? "" : "REFUSED",
                              light_sensor_data.mean_lux, light_sensor_reference, get_time_ms());
  #endif
  return(sent);
}

static bool dh_send_master_claim(void *ctx)
{
  (void)ctx;
  return(tx_dh_master_claim());
}

static bool dh_broadcast_traffic_heavy(void *ctx)
{
  (void)ctx;
  return(BROADCASTING_HEAVY == broadcast_monitor_get_traffic());
}

static void dh_retry_task(taskParm_t parm0, taskParm_t parm1)
{
  if(has_timed_out(parm0.u32, parm1.u32))
  {
     removeTask(dh_retry_task);
     dh_master_retry(&dh_master, allow_light_sensor_control());
  }
}

static void dh_schedule_retry(void *ctx, uint32_t delay_ms)
{
  (void)ctx;
  taskParm_t arg0, arg1;

  arg0.u32 = get_time_ms();
  arg1.u32 = delay_ms;
  addTask(dh_retry_task, arg0, arg1);
}

static uint32_t dh_random_ms(void *ctx, uint32_t max_ms)
{
  (void)ctx;
  return(get_random_number(max_ms, 0UL));
}

static const dh_ops_t dh_ops =
{
  .ctx = NULL,
  .send_level = dh_send_level,
  .broadcast_traffic_heavy = dh_broadcast_traffic_heavy,
  .schedule_retry = dh_schedule_retry,
  .random_ms = dh_random_ms,
  .send_master_claim = dh_send_master_claim,
};

/*
 * Lights upgraded from the firmware with calibration: nothing is a master until
 * selected with the buttons.  A calibration cut short by a power loss also left
 * occupancy disabled.  B_DISABLE_LIGHT_SENSOR now only lasts while the firmware
 * version is displayed.
 */
static void clear_old_dhs_flags(void)
{
  if(is_app_flags_nonvol_set(B_DHS_OLD_CALIBRATING))
  {
     set_app_flags_nonvol(B_DISABLE_OCC_SENSOR, 0);
  }
  if(is_app_flags_nonvol_set(B_DHS_OLD_FLAGS))
  {
     set_app_flags_nonvol((B_DHS_MASTER_LIGHT | B_DHS_OLD_FLAGS), 0);
  }
  if(is_app_flags_nonvol_set(B_DISABLE_LIGHT_SENSOR))
  {
     set_app_flags_nonvol(B_DISABLE_LIGHT_SENSOR, 0);
  }
}

/*
 * The light sensor is an i2c interface that is sampled every LIGHT_SENSOR_HAL_SAMPLE_TIME_MS.
 * The average is generated over N_LIGHT_SAMPLES. The function execute_light_sensor_logic()
 * is called at the rate as N_LIGHT_SAMPLES * LIGHT_SENSOR_HAL_SAMPLE_TIME_MS.
 */
void light_sensor_init(void)
{
  clear_old_dhs_flags();

  sl_token_get_data(TOKEN_APP_LIGHT_SENSOR_LUX, 0, &light_sensor_reference, sizeof(light_sensor_reference));

  // i2c etc
  hal_light_sensor_init();

  write_light_sensor_attribute(ZCL_ILLUM_MIN_MEASURED_VALUE_ATTRIBUTE_ID, 2);
  write_light_sensor_attribute(ZCL_ILLUM_MAX_MEASURED_VALUE_ATTRIBUTE_ID, 1023);
  memset(&light_sensor_data, 0, sizeof(light_sensor_data_t));

  light_sensor_control_suspended = false;
  light_sensor_control_suspended_dimming = false;
  dhs_average_disturbed = false;

  suspend_light_sensor_control(LIGHT_SENSOR_UPDATE_AVERAGE_MS);

  dh_master_init(&dh_master, &dh_ops);

  // TODO: Read from non-vol to initialize attributes
  sl_zigbee_app_debug_println("Light Sensor Starting");
}

uint16_t dhs_get_inhibit_bits(void)
{
  return(dhs_inhibit_bits);
}

static uint16_t get_dhs_inhibit_bits(void)
{
  dhs_inhibit_bits = 0;
  dhs_inhibit_bits |= (eADAPTIV_NWK != get_present_network()) ? DHS_INHIBIT_NOT_ADAPTIV : 0;
  dhs_inhibit_bits |= (0 == read_on_off_attribute(ZCL_ON_OFF_ATTRIBUTE_ID)) ? DHS_INHIBIT_OFF : 0;
  dhs_inhibit_bits |= !device_is_in_a_group() ? DHS_INHIBIT_NOT_IN_GROUP : 0;
  dhs_inhibit_bits |= light_sensor_control_suspended ? DHS_INHIBIT_SUSPENDED : 0;
  dhs_inhibit_bits |= (0 == light_sensor_reference) ? DHS_INHIBIT_NO_TARGET : 0;
  dhs_inhibit_bits |= (MIN_ALLOWABLE_LIGHT_LEVEL > hal_get_dim_pwm_255()) ? DHS_INHIBIT_LOW_LEVEL : 0;
  dhs_inhibit_bits |= is_occ_mid_or_lower() ? DHS_INHIBIT_OCC_MID : 0;
  dhs_inhibit_bits |= light_sensor_control_suspended_dimming ? DHS_INHIBIT_DIMMING : 0;
  dhs_inhibit_bits |= is_app_flags_nonvol_set(B_DISABLE_LIGHT_SENSOR) ? DHS_INHIBIT_DISABLED : 0;
  dhs_inhibit_bits |= is_app_flags_nonvol_set(B_DHS_MASTER_LIGHT) ? 0 : DHS_INHIBIT_NOT_MASTER;

  #if (1 <= LIGHT_SENSOR_PRINT)
  sl_zigbee_app_debug_print("DHS %s", (0 == dhs_inhibit_bits) ? "YES" : "NO");
  sl_zigbee_app_debug_println("  %d(nwk) %d(off) %d(!grp) %d(susp) %d(0ref) %d(0pwm) %d(occ) %d(dim) %d(inh) %d(mstr)",
    (0 != (dhs_inhibit_bits & DHS_INHIBIT_NOT_ADAPTIV)), (0 != (dhs_inhibit_bits & DHS_INHIBIT_OFF)),
    (0 != (dhs_inhibit_bits & DHS_INHIBIT_NOT_IN_GROUP)), (0 != (dhs_inhibit_bits & DHS_INHIBIT_SUSPENDED)),
    (0 != (dhs_inhibit_bits & DHS_INHIBIT_NO_TARGET)), (0 != (dhs_inhibit_bits & DHS_INHIBIT_LOW_LEVEL)),
    (0 != (dhs_inhibit_bits & DHS_INHIBIT_OCC_MID)), (0 != (dhs_inhibit_bits & DHS_INHIBIT_DIMMING)),
    (0 != (dhs_inhibit_bits & DHS_INHIBIT_DISABLED)), (0 != (dhs_inhibit_bits & DHS_INHIBIT_NOT_MASTER)));
  #endif

  return(dhs_inhibit_bits);
}

bool allow_light_sensor_control(void)
{
  return(0 == get_dhs_inhibit_bits());
}

// The red LED reaches the sensor (hal_set_red_led())
void set_dhs_time_stamp(void)
{
  dhs_average_disturbed = true;
}

static void unsuspend_light_sensor_task(taskParm_t parm0, taskParm_t parm1)
{
  if(has_timed_out(parm0.u32, parm1.u32))
  {
     light_sensor_control_suspended = false;
     removeTask(unsuspend_light_sensor_task);

    #if (1 <= LIGHT_SENSOR_PRINT)
    sl_zigbee_app_debug_println("DHS Re-enable   <%d>", get_time_ms());
    #endif
  }
}

// Disable light sensor modifying the light pwm for a period of time, in ms.
// Can be called from isr or non-isr cb's.  A shorter suspend does not cut a longer
// one short.
bool suspend_light_sensor_control(uint32_t delay_ms)
{
  taskParm_t arg0, arg1;
  uint32_t now_ms = get_time_ms();
  uint32_t end_ms = now_ms + delay_ms;

  if(!light_sensor_control_suspended || (0 < (int32_t)(end_ms - light_sensor_suspend_end_ms)))
  {
     arg0.u32 = now_ms;
     arg1.u32 = delay_ms;
     addTask(unsuspend_light_sensor_task, arg0, arg1);
     light_sensor_suspend_end_ms = end_ms;
  }
  light_sensor_control_suspended = true;
  dhs_average_disturbed = true;

  #if (1 <= LIGHT_SENSOR_PRINT)
  sl_zigbee_app_debug_println("DHS Disabled for %dmS   <%d>", delay_ms, get_time_ms());
  #endif

  return(true);
}

static void unsuspend_light_sensor_dimming_task(taskParm_t parm0, taskParm_t parm1)
{
  if(has_timed_out(parm0.u32, parm1.u32))
  {
     light_sensor_control_suspended_dimming = false;
     removeTask(unsuspend_light_sensor_dimming_task);

    #if (1 <= LIGHT_SENSOR_PRINT)
    sl_zigbee_app_debug_println("DHS Re-enable (Dim) Re-enable   <%d>", get_time_ms());
    #endif
  }
}

// Disable light sensor modifying the light pwm for a period of time, in ms.
// Can be called from isr or non-isr cb's. From dimming cmd.
bool suspend_light_sensor_control_dimming(uint32_t delay_ms)
{
  taskParm_t arg0, arg1;

  arg0.u32 = get_time_ms();
  arg1.u32 = delay_ms;
  addTask(unsuspend_light_sensor_dimming_task, arg0, arg1);
  light_sensor_control_suspended_dimming = true;
  dhs_average_disturbed = true;

  #if (1 <= LIGHT_SENSOR_PRINT)
  sl_zigbee_app_debug_println("DHS Disabled (Dim) for %dmS   <%d>", delay_ms, get_time_ms());
  #endif

  return(true);
}

// One 20 s average: the master decides whether the group's level changes
static void dhs_on_average(void)
{
  dh_average_t in;

  in.allowed = allow_light_sensor_control() && !dhs_average_disturbed;
  dhs_average_disturbed = false;
  in.level = hal_get_dim_pwm_255();
  in.lux = light_sensor_data.mean_lux;
  in.target = (uint16_t)light_sensor_reference;
  in.floor = (MIN_ALLOWABLE_LIGHT_LEVEL > get_min_light_level()) ? MIN_ALLOWABLE_LIGHT_LEVEL : get_min_light_level();
  in.ceiling = get_max_light_level();

  if(is_app_flags_nonvol_set(B_50_PERCENT_MODE))
  {
     // The rest of the group may not be in 50% mode, so this light's limit is not
     // the group's.  When this light is held at its limit, the group's level is
     // the one last sent.
     sl_token_get_data(TOKEN_APP_LIGHT_LEVEL_MAX_STASHED, 0, &in.ceiling, sizeof(in.ceiling));
     if((get_max_light_level() == in.level) && (dh_master_last_level(&dh_master) > in.level))
     {
        in.level = dh_master_last_level(&dh_master);
     }
  }

  dh_master_on_average(&dh_master, &in);
}

/****************************************************************************
 * execute_light_sensor_logic
 *
 * Called from update_light_sensor_task() after it has a new buffer filled with
 * sensor data, ie. running average of last N_LIGHT_SAMPLES which occurs
 * every LIGHT_SENSOR_UPDATE_AVERAGE_MS
 *
 ****************************************************************************/
void execute_light_sensor_logic(light_sensor_info_t *light_sensor_info)
{
  uint16_t _lux = 0;

if(eDAINTREE_NWK != get_present_network())
{
  // All but daintree
  _lux = 3*light_sensor_info->mean_lux + (light_sensor_info->mean_lux >> 1);  //  3.5 X
}
else
{
  // Add a fudge factor to the raw average lux read out of the sensor.
  // Piece-wise linear curve is used.
  if(80 <= light_sensor_info->mean_lux)
  {
     _lux = (uint16_t)(((uint32_t)light_sensor_info->mean_lux * 24248) >> 16) + 120;  // 0.37 * raw + 120
  }
  else if(3 > light_sensor_info->mean_lux)
  {
     _lux = 0;
  }
  else  // 3 < light_sensor_info->mean_lux < 80
  {
     _lux = (uint16_t)(((uint32_t)light_sensor_info->mean_lux * 112720) >> 16) + 5;  // 1.72 * raw + 5
  }
}

  // Copy local
  light_sensor_data.last_mean_lux = light_sensor_data.mean_lux;
  light_sensor_data.mean_lux = _lux;
  light_sensor_data.pwm_value = read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID);

  // Generate reported lux value, save in attribute, For Daintree only!!
  uint16_t reported_lux = (uint16_t)(10000.0*log10f((float)_lux))+1; // per spec
  write_light_sensor_attribute(ZCL_ILLUM_MEASURED_VALUE_ATTRIBUTE_ID, reported_lux);

  #if (1 <= LIGHT_SENSOR_PRINT)
  sl_zigbee_app_debug_println("DHS Reported_lux:%d  Sensor_lux:%d  pwm:%d   <%d>",
      reported_lux, light_sensor_data.mean_lux, light_sensor_data.pwm_value, get_time_ms());
  #endif

  #if defined(DISABLE_LIGHT_SENSOR_CONTROL) || defined(DHS_SINK_DEVICE) /* Data collection */
  #if (1 <= LIGHT_SENSOR_PRINT)
  sl_zigbee_app_debug_println("This is either a SINK or DHS=DISABLED");
  #endif
  return;
  #endif

  dhs_on_average();
}

// This light stops being the master, without any indication: the light that
// took over has just shown it.
static void dh_stand_down(uint16_t other_node)
{
  set_app_flags_nonvol(B_DHS_MASTER_LIGHT, 0);
  dh_master_stop(&dh_master);
  sl_zigbee_app_debug_println("DHS no longer master, 0x%04X is   <%d>", other_node, get_time_ms());
}

// From the buttons
void dh_set_master(bool enable)
{
  set_app_flags_nonvol(B_DHS_MASTER_LIGHT, enable);
  if(enable)
  {
     dh_master_claim(&dh_master);
  }
  else
  {
     dh_master_stop(&dh_master);
  }
}

// RX: ZCL_DH_MASTER_CLAIM_COMMAND_ID.  The light selected last is the master.
bool dh_rx_master_claim(sl_zigbee_af_cluster_command_t* cmd)
{
  if((cmd->source != sl_zigbee_get_node_id()) && is_app_flags_nonvol_set(B_DHS_MASTER_LIGHT))
  {
     dh_stand_down(cmd->source);
  }
  return(true);
}

// RX: ZCL_DH_LEVEL_COMMAND_ID from the group's master (this light's own comes
// back too, that is how the master moves).  The transmit is tx_dh_level().
bool dh_rx_level_command(sl_zigbee_af_cluster_command_t* cmd)
{
  uint8_t base_idx = cmd->payloadStartIndex;

  if((uint16_t)(base_idx + 2U) > cmd->bufLen)
  {
     return(true);
  }

  uint8_t level = cmd->buffer[base_idx];
  uint8_t ramp_ds = cmd->buffer[base_idx + 1U];

  // A level from another master: one of the two missed a claim, so neither
  // knows which was selected last.  The lower node ID stays the master.
  if((cmd->source < sl_zigbee_get_node_id()) && is_app_flags_nonvol_set(B_DHS_MASTER_LIGHT))
  {
     dh_stand_down(cmd->source);
  }

  // Off, at mid, suspended, dimmed by a switch: stay as this light is
  if(0 != (get_dhs_inhibit_bits() & DHS_INHIBITS_FOR_FOLLOWER))
  {
     #if (1 <= LIGHT_SENSOR_PRINT)
     sl_zigbee_app_debug_println("DHS level %d ignored [0x%X]   <%d>", level, dhs_inhibit_bits, get_time_ms());
     #endif
     return(true);
  }

  move_to_level_dhs(level, ramp_ds);
  return(true);
}

uint16_t get_light_sensor_mean_lux(void)
{
  return(light_sensor_data.mean_lux);
}

// Called from incoming config cmd.  The target is only used by the master, and
// only set by the Twitch.
void save_light_lux_reference(uint16_t lux_ref)
{
  uint16_t new_ref = lux_ref;

  switch(lux_ref)
  {
     case 0:
     case 300:
     case 500:
     case 800:
        break;
     default:
        new_ref = 300;
        break;
  }
  sl_token_set_data(TOKEN_APP_LIGHT_SENSOR_LUX, 0, &new_ref, sizeof(new_ref));
  light_sensor_reference = new_ref;

  sl_zigbee_app_debug_println("\n\rNEW LUX REF %d   <%d>", light_sensor_reference, get_time_ms());
}

uint16_t read_light_sensor_attribute(sl_zigbee_af_attribute_id_t attributeID)
{
  uint8_t data[2] = {0};
  uint8_t size = sizeof(data);
  sl_zigbee_af_status_t status;

  switch(attributeID)
  {
     case ZCL_ILLUM_MEASURED_VALUE_ATTRIBUTE_ID:
     case ZCL_ILLUM_MIN_MEASURED_VALUE_ATTRIBUTE_ID:
     case ZCL_ILLUM_MAX_MEASURED_VALUE_ATTRIBUTE_ID:
     case ZCL_MEASUREMENT_LIGHT_SENSOR_TYPE_ATTRIBUTE_ID:
     case ZCL_ILLUM_MEASUREMENT_CLUSTER_CLUSTER_REVISION_SERVER_ATTRIBUTE_ID:
        size = 2;
        break;
     case ZCL_ILLUM_MEASUREMENT_CLUSTER_REPORTING_STATUS_SERVER_ATTRIBUTE_ID:
        size = 1;
        break;
     default:
       sl_zigbee_app_debug_println("Unsupported light sensor attr");
       break;
  }
  status = sl_zigbee_af_read_server_attribute(LIGHT_SENSOR_ENDPOINT,
                                              ZCL_ILLUM_MEASUREMENT_CLUSTER_ID,
                                              attributeID,
                                              (uint8_t *)&data,
                                              size);
  if(status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
     sl_zigbee_app_debug_println("ERR:read_light_sensor_attribute 0x%X", status);
  }
  return(GET_16_FROM_ARRAY(data, 0));
}

bool write_light_sensor_attribute(sl_zigbee_af_attribute_id_t attributeID, uint16_t data)
{
  sl_zigbee_af_attribute_type_t type;
  bool status = true;

  uint8_t bytes[2] = {0};
  bytes[0] = GET_LSB_FROM_16(data);
  bytes[1] = GET_MSB_FROM_16(data);

  switch(attributeID)
  {
     case ZCL_ILLUM_MEASURED_VALUE_ATTRIBUTE_ID:
     case ZCL_ILLUM_MIN_MEASURED_VALUE_ATTRIBUTE_ID:
     case ZCL_ILLUM_MAX_MEASURED_VALUE_ATTRIBUTE_ID:
     case ZCL_MEASUREMENT_LIGHT_SENSOR_TYPE_ATTRIBUTE_ID:
     case ZCL_ILLUM_MEASUREMENT_CLUSTER_CLUSTER_REVISION_SERVER_ATTRIBUTE_ID:
        type = ZCL_INT16U_ATTRIBUTE_TYPE;
        break;
     case ZCL_ILLUM_MEASUREMENT_CLUSTER_REPORTING_STATUS_SERVER_ATTRIBUTE_ID:
        type = ZCL_ENUM8_ATTRIBUTE_TYPE;
        break;
    default:
      sl_zigbee_app_debug_println("Unsupported light sensor attr");
      break;
  }
  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != sl_zigbee_af_write_server_attribute(
                                         LIGHT_SENSOR_ENDPOINT,
                                         ZCL_ILLUM_MEASUREMENT_CLUSTER_ID,
                                         attributeID,
                                         (uint8_t *)&bytes,
                                         type))
  {
     sl_zigbee_app_debug_println("ERR:write_light_sensor_attribute");
     status = false;
  }
  #if (3 <= LIGHT_SENSOR_PRINT)
     sl_zigbee_app_debug_print("Write ILLUM attr 0x%04X value %d ",
                                                               attributeID,
                                                               data);
     sl_zigbee_app_debug_println("Rd %d   <%d>",read_light_sensor_attribute(attributeID),
                                                 get_time_ms());
  #endif
  return(status);
}
