#include "af.h"
#include <stdlib.h>

#include "RehTOS.h"
#include "hal_occ_sensor.h"
#include "hal_utils.h"
#include "occ_sensor.h"
#include "occ_group.h"
#include "hal_leds.h"
#include "light_on_off.h"

#include "sl_token_api.h"
#include "sl_token_manager.h"
#include "nwk_utils.h"
#include "nwk_groups.h"
#include "nwk_multicast.h"
#include "adaptiv_config.h"
#include "light_on_off.h"
#include "light_sensor.h"
#include "broadcast_monitor.h"
#include "auto_comm.h"

#include "sl_zigbee_debug_print.h"

#ifndef OCC_PRINT
#define OCC_PRINT 0    /* Default to 0 */
#elif (OCC_PRINT != 0)
#warning "OCC_PRINT active"
#endif

//#define USE_ZIGBEE_ATTRIBUTE_DELAY

#define OCC_SWITCH_OFF_HOLD_MS  (5UL*60UL*1000UL)   // After an off from a switch, John 3/30

extern sl_zigbee_af_zcl_request_status_t sl_zigbee_af_on_off_cluster_off_cb(void);

static uint32_t occ_time_stamp = 0UL;

static occ_sensor_state_t present_local_state = eOCC_SENSOR_ADAPTIV;
occupancy_sensor_info_t occupancy_sensor_info = {0};
static bool allow_occupancy_sensor_control = true;
static bool occupancy_sensor_initialized = false;

// Adaptiv grouped lights: group occupancy, see occ_group.h
static occ_group_t occ_group;
static uint8_t switch_level = DEFAULT_MAX_LIGHT_LEVEL;   // occupancy wakes to the last level set from a switch
static uint32_t off_time_stamp = 0UL;                    // OFF entered, or last occupancy during a switch-off hold

// Forward declarations
static void set_occupied_attribute(bool occupied);
static void (*occ_sensor_state)(void) = NULL;
static void adaptiv_init(void);
void suspend_occ_sensor_control(uint16_t Dsecs);

static void occupied_occupied_on_dt(void);
static void occupied_unoccupied_on_wait_dt(void);
static void unoccupied_unoccupied_off_dt(void);

static void occupied_occupied_on_nn(void);
static void occupied_unoccupied_on_wait_nn(void);
static void unoccupied_unoccupied_mid_wait_nn(void);
static void unoccupied_unoccupied_off_nn(void);

static void spew_state(occ_sensor_state_t state, char *state_string);

// Called when nwk changes.  Typically when joined to a new nwk.
// Func occ_sensor_init() has already been called at por.
// Since it has already set up events, only modify what is required.
void occ_sensor_init_task(taskParm_t parm0, taskParm_t parm1)
{
  if(has_timed_out(parm0.u32, 250UL))
  {
     removeTask(occ_sensor_init_task);
     if((eNO_NWK == get_present_network()) || (eSTAND_ALONE == get_present_network()))
     {
        occ_sensor_state = occupied_occupied_on_nn;
        present_local_state = eOCCUPIED_OCCUPIED_ON_NN;
        hal_start_occ_sensor();
        set_occupied_attribute(true);
        sl_zigbee_app_debug_println("OCC  occ_sensor_init_task for   eNO_NWK");
     }
     else if(eADAPTIV_NWK != get_present_network())
     {
        // Daintree & other gateways
        occ_sensor_state = occupied_occupied_on_dt;
        present_local_state = eOCCUPIED_OCCUPIED_ON_DT;
        hal_start_occ_sensor();
        set_occupied_attribute(true);
        sl_zigbee_app_debug_println("OCC  occ_sensor_init_task for  eDAINTREE, eUNKNOWN_NWK");

     }
     else if(eADAPTIV_NWK == get_present_network())
     {
        // NOTE: Since we just joined, every light starts ON with a full timer
        // and nothing is sent
        adaptiv_init();
        hal_start_occ_sensor();
        set_occupied_attribute(true);
        sl_zigbee_app_debug_println("OCC  occ_sensor_init_task for  eADAPTIV_NWK");
     }
     else
     {
         // Not in a known nwk
         hal_stop_occ_sensor();
         set_occupied_attribute(false);
         sl_zigbee_app_debug_println("OCC  occ_sensor_init_task for   WTF_NWK");
     }
  }
}

// Called at por, if nwk is known, will init that personality.  If not, will
// wait for a nwk change and initialize the different functionality via
// occ_sensor_init_task()
void occ_sensor_init(void)
{
  uint16_t unoccupied_delay_secs = 0;

  hal_init_occ_sensor();

  // Valid before anything can reach it (received keepalives, a later delay change)
  adaptiv_init();

  // Occ sensor active?
  sl_token_get_data(TOKEN_APP_UNOCCUPIED_DELAY, 0, &unoccupied_delay_secs, sizeof(unoccupied_delay_secs));
  if(0 == unoccupied_delay_secs)
  {
     // Delay time of 0 = turn off occ sensor
     hal_stop_occ_sensor();
     sl_zigbee_app_debug_println("OCC occ_sensor_init  occ DISABLED");
     return;
  }

  if((eNO_NWK == get_present_network()) || (eSTAND_ALONE == get_present_network()))
  {
     occ_sensor_state = occupied_occupied_on_nn;
     present_local_state = eOCCUPIED_OCCUPIED_ON_NN;
     hal_start_occ_sensor();
     set_occupied_attribute(true);
     occupancy_sensor_initialized = true;
     sl_zigbee_app_debug_println("OCC occ_sensor_init   eNO_NWK");
  }
  else if(eADAPTIV_NWK != get_present_network())
  {
     // Daintree & other gateways
     occ_sensor_state = occupied_occupied_on_dt;
     present_local_state = eOCCUPIED_OCCUPIED_ON_DT;
     hal_start_occ_sensor();
     set_occupied_attribute(true);
     occupancy_sensor_initialized = true;
     sl_zigbee_app_debug_println("OCC occ_sensor_init   eDAINTREE, eUNKNOWN_NWK");
  }
  else if(eADAPTIV_NWK == get_present_network())
  {
     suspend_light_sensor_control(5000UL);
     occupancy_sensor_info.time_stamp_unoccupied_to_occupied = get_time_ms();
     occupancy_sensor_info.time_stamp_occupied_to_unoccupied = get_time_ms();
     occ_time_stamp = get_time_ms();
     // NOTE: Since we just powered on, every light starts ON with a full timer
     // and nothing is sent
     hal_start_occ_sensor();
     set_occupied_attribute(true);
     occupancy_sensor_initialized = true;
     sl_zigbee_app_debug_println("OCC occ_sensor_init   eADAPTIV_NWK");
  }
  else
  {
     // Not in a known nwk?
     hal_stop_occ_sensor();
     set_occupied_attribute(false);
     sl_zigbee_app_debug_println("OCC occ_sensor_init   WTF_NWK");
  }
}

/***********************************************************************************************/
/********************************    ADAPTIV GROUP OCCUPANCY   *********************************/
/***********************************************************************************************/
// The group logic is in occ_group.c.  Here: the light's own actions and the
// special cases (switch off hold, manual on mode, EM mode, 3 hour on level).

static uint8_t get_wake_level(void)
{
  uint8_t level = switch_level;

  if(get_max_light_level() < level)
  {
     level = get_max_light_level();
  }
  if(DEFAULT_MIN_LIGHT_LEVEL_DT > level)
  {
     level = DEFAULT_MIN_LIGHT_LEVEL_DT;
  }
  return(level);
}

void occ_sensor_set_switch_level(uint8_t level)
{
  // Below the minimum = the switch turned the light off: keep the last on level
  if(DEFAULT_MIN_LIGHT_LEVEL_DT <= level)
  {
     switch_level = level;
     #if (OCC_PRINT >= 1)
     sl_zigbee_app_debug_println("OCC switch level %d   <%d>", level, get_time_ms());
     #endif
  }
}

// So that an on command (switch) after dimming restores the wake level, not mid
static void keep_on_level_at_wake_level(void)
{
  uint8_t level = get_wake_level();

  // Reduce constant writes
  if(read_light_level_attribute(ZCL_ON_LEVEL_ATTRIBUTE_ID) != level)
  {
     write_light_level_attribute(ZCL_ON_LEVEL_ATTRIBUTE_ID, &level);
  }
}

// Unoccupied long enough: off, unless min level is not 0 or EM mode (stay at mid)
static bool lights_turn_off(void)
{
  return((0 == get_min_light_level()) && !is_app_flags_nonvol_set(B_EM_MODE));
}

static void enter_off_local(uint32_t now_ms)
{
  clear_on_from_switch();
  off_time_stamp = now_ms;
}

static uint32_t off_settle_ms(void)
{
#ifdef USE_ZIGBEE_ATTRIBUTE_DELAY
  uint32_t delay = (uint32_t)read_occupancy_sensor_attribute(ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_DELAY_ATTRIBUTE_ID);
#else
  uint32_t delay = (uint32_t)DEFAULT_OCC_SENSOR_DELAY_TO_OCC;
#endif
  delay *= 1000UL;  // Attribute delay in seconds

  // If delay = 0, give finite time to settle system
  if(250UL > delay)
  {
     delay = 250UL;    // mS
  }
  return(delay);
}

/*****  occ_group actions  *****/
static uint32_t grp_dim_timeout_ms(void *ctx)
{
  (void)ctx;
  return((uint32_t)get_unoccupied_delay() * 1000UL);
}

static uint32_t grp_off_timeout_ms(void *ctx)
{
  (void)ctx;
  return((uint32_t)get_unoccupied_delay_to_off() * 1000UL);
}

static void grp_go_to_state(void *ctx, occ_group_state_t state)
{
  (void)ctx;
  switch(state)
  {
     case OCC_GROUP_ON:
        move_to_level_onoff(get_wake_level(), DSECS_1_SECS);
        occupancy_sensor_info.time_stamp_unoccupied_to_occupied = get_time_ms();
        occupancy_sensor_info.time_stamp_occupied_to_unoccupied = get_time_ms();
        break;
     case OCC_GROUP_MID:
        set_occupied_attribute(false);
        keep_on_level_at_wake_level();
        if(get_present_light_level() > get_mid_light_level())
        {
           // move_to_level_onoff() adds extra time to the disable sensors
           move_to_level_onoff(get_mid_light_level(), DSECS_1_SECS);
        }
        // Allow occ event interrupt the dim to mid, and go directly to max
        suspend_occ_sensor_control(DSECS_0_SECS);
        break;
     default:
        set_occupied_attribute(false);
        if(lights_turn_off())
        {
           // NOTE: suspend_all_sensors() in cb
           sl_zigbee_af_on_off_cluster_off_cb();
        }
        enter_off_local(get_time_ms());
        // Off cb and em_mode have different sensor disable times, make them the same
        suspend_occ_sensor_control(DSECS_HALF_SECS);
        break;
  }
  #if (OCC_PRINT >= 1)
  sl_zigbee_app_debug_println("OCC -> %s   <%d>", (OCC_GROUP_ON == state) ? "ON" : ((OCC_GROUP_MID == state) ? "MID" : "OFF"), get_time_ms());
  #endif
}

static uint8_t grp_level_for_state(void *ctx, occ_group_state_t state)
{
  (void)ctx;
  switch(state)
  {
     case OCC_GROUP_ON:
        return(get_wake_level());
     case OCC_GROUP_MID:
        return(get_mid_light_level());
     default:
        return(lights_turn_off() ? 0 : get_mid_light_level());
  }
}

static uint8_t grp_current_level(void *ctx)
{
  (void)ctx;
  return(get_present_light_level());
}

static bool grp_send_keepalive(void *ctx, uint16_t time_s, occ_group_state_t state,
                               occ_keepalive_reason_t reason, uint8_t level)
{
  (void)ctx;
  if(global_ac_quiet_active())
  {
     // Not sent, but carried on as if it were: this light runs on its own.  A wake
     // turns only this light on (occ_group: own wake not back in time).
     return(true);
  }
  bool sent = tx_occ_keepalive(time_s, (uint8_t)state, (uint8_t)reason, level);
  #if (OCC_PRINT >= 1)
  sl_zigbee_app_debug_println("OCC TX keepalive %s %d s lvl:%d%s   <%d>",
                              (OCC_KEEPALIVE_WAKE == reason) ? "wake" : "refresh", time_s, level,
                              sent ? "" : " FAILED", get_time_ms());
  #endif
  return(sent);
}

static bool grp_send_level_change(void *ctx, occ_group_state_t state, uint8_t level)
{
  (void)ctx;
  if(global_ac_quiet_active())
  {
     return(true);     // Not sent: this light changes on its own timer
  }
  bool sent = tx_occ_level_change((uint8_t)state, level);
  #if (OCC_PRINT >= 1)
  sl_zigbee_app_debug_println("OCC TX level change %d lvl:%d%s   <%d>", state, level,
                              sent ? "" : " FAILED", get_time_ms());
  #endif
  return(sent);
}

static uint32_t grp_random_ms(void *ctx, uint32_t max_ms)
{
  (void)ctx;
  return(get_random_number(max_ms, 0UL));
}

static bool grp_broadcast_traffic_heavy(void *ctx)
{
  (void)ctx;
  return(BROADCASTING_HEAVY == broadcast_monitor_get_traffic());
}

static void grp_woke_from_off(void *ctx)
{
  (void)ctx;
  dh_enable();
}

static const occ_group_ops_t occ_group_ops =
{
  .ctx = NULL,
  .dim_timeout_ms = grp_dim_timeout_ms,
  .off_timeout_ms = grp_off_timeout_ms,
  .go_to_state = grp_go_to_state,
  .level_for_state = grp_level_for_state,
  .current_level = grp_current_level,
  .send_keepalive = grp_send_keepalive,
  .send_level_change = grp_send_level_change,
  .random_ms = grp_random_ms,
  .broadcast_traffic_heavy = grp_broadcast_traffic_heavy,
  .woke_from_off = grp_woke_from_off,
};

// Every light starts ON with a full timer and sends nothing
static void adaptiv_init(void)
{
  present_local_state = eOCC_SENSOR_ADAPTIV;
  switch_level = get_max_light_level();
  off_time_stamp = get_time_ms();
  occ_group_init(&occ_group, &occ_group_ops, get_time_ms());
}

/*****  OFF: the special cases  *****/
static void adaptiv_off_sample(uint32_t now_ms, bool occupied)
{
  bool em_mode = is_app_flags_nonvol_set(B_EM_MODE);
  uint32_t delay = off_settle_ms();

  if(em_mode)
  {
     clear_off_from_switch();
     restore_unoccupied_delay();
  }
  else if(off_from_switch())
  {
     // Off from a switch: occupancy may not turn the lights on for 5 minutes,
     // every occupancy restarts the 5 minutes
     delay = OCC_SWITCH_OFF_HOLD_MS;
     if(occupied)
     {
        off_time_stamp = now_ms;
     }
  }

  if(has_timed_out(off_time_stamp, delay))
  {
     if(!em_mode)
     {
        clear_off_from_switch();
        restore_unoccupied_delay();
     }

     // Past delay time, allow occ trip to turn on lights
     if(occupied)
     {
        if(em_mode || !is_app_flags_nonvol_set(B_MANUAL_ON_MODE))
        {
           #if (OCC_PRINT >= 1)
           sl_zigbee_app_debug_println("OCC Sensor tripped while off  <%d>", now_ms);
           #endif
           occ_group_on_occupancy(&occ_group, now_ms);
        }
        set_occupied_attribute(true);
     }
     else if(em_mode)
     {
        set_occupied_attribute(true);
     }

     // If no occ trip for a long time, set on level to max light level.
     if(!em_mode && has_timed_out(off_time_stamp, (DEFAULT_OCC_SENSOR_XXXL_DELAY*1000UL)))
     {
        switch_level = get_max_light_level();
        if(get_max_light_level() != get_on_light_level())
        {
            write_light_level_attribute(ZCL_ON_LEVEL_ATTRIBUTE_ID, &switch_level);
        }
     }
  }

  // Lights turned on via external source ie. switch.  (A light that stays
  // on at mid while "off", EM mode or min level, only by an on cmd.)
  if(OCC_GROUP_OFF == occ_group_state(&occ_group))
  {
     if(lights_turn_off() ? (1 == read_on_off_attribute(ZCL_ON_OFF_ATTRIBUTE_ID)) : on_from_switch())
     {
        sl_zigbee_app_debug_println("OCC external on cmd   <%d>", now_ms);
        occ_group_external_on(&occ_group, now_ms);
        set_occupied_attribute(true);
        if(!em_mode)
        {
           clear_off_from_switch();
           clear_on_from_switch();
        }
     }
  }
}

// Called every sensor sample on an Adaptiv network
static void adaptiv_sample(bool occupied)
{
  uint32_t now_ms = get_time_ms();

  // External off cmd (switch, gateway)
  if((0 == read_on_off_attribute(ZCL_ON_OFF_ATTRIBUTE_ID)) &&
     (OCC_GROUP_OFF != occ_group_state(&occ_group)))
  {
     occ_group_external_off(&occ_group);
     enter_off_local(now_ms);
     set_occupied_attribute(false);
  }

  if(!device_is_in_a_group())
  {
     return;
  }

  // Sensor readings are ignored while the light is changing level
  if(allow_occupancy_sensor_control)
  {
     if(OCC_GROUP_OFF == occ_group_state(&occ_group))
     {
        adaptiv_off_sample(now_ms, occupied);
     }
     else if(occupied)
     {
        occ_group_on_occupancy(&occ_group, now_ms);
        set_occupied_attribute(true);
     }
     else
     {
     }
  }

  // Timers and sends run all the time (a wake goes out while the light ramps up)
  occ_group_tick(&occ_group, now_ms);

  if(OCC_GROUP_MID == occ_group_state(&occ_group))
  {
     // The dim ramp saved mid to the on level
     keep_on_level_at_wake_level();
  }
}

// RX keepalive / level change multicast from another light in the group
void occ_sensor_rx_group_cmd(sl_zigbee_af_cluster_command_t *cmd)
{
  const uint8_t *payload = &cmd->buffer[cmd->payloadStartIndex];
  uint16_t length = (cmd->bufLen > cmd->payloadStartIndex) ? (uint16_t)(cmd->bufLen - cmd->payloadStartIndex) : 0U;
  uint32_t now_ms = get_time_ms();

  bool own = (cmd->source == sl_zigbee_get_node_id());   // our own multicast, looped back

  if(eADAPTIV_NWK != get_present_network())
  {
     return;
  }
  if(own && (ZCL_OCCUPANCY_SENSOR_KEEPALIVE_COMMAND_ID != cmd->commandId))
  {
     return;     // only our own wake matters (occ_group_rx_own_keepalive)
  }
  if(is_app_flags_nonvol_set(B_DISABLE_OCC_SENSOR))
  {
     sl_zigbee_app_debug_println("OCC is Disabled   <%d>", now_ms);
     return;
  }
  if(0 == get_unoccupied_delay())
  {
     return;     // occupancy control is off
  }

  if(ZCL_OCCUPANCY_SENSOR_KEEPALIVE_COMMAND_ID == cmd->commandId)
  {
     if((5U > length) || (OCC_GROUP_OFF < payload[2]))
     {
        return;
     }
     uint16_t time_s = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
     #if (OCC_PRINT >= 1)
     sl_zigbee_app_debug_println(">>OCC [0x%04X]%s keepalive %s %d s lvl:%d   <%d>", cmd->source,
                                 own ? " (own)" : "", payload[3] ? "refresh" : "wake", time_s, payload[4], now_ms);
     #endif
     if(own)
     {
        // Our wake is back: go ON together with the group
        occ_group_rx_own_keepalive(&occ_group, now_ms, time_s, (occ_group_state_t)payload[2],
                                   (occ_keepalive_reason_t)payload[3]);
     }
     else
     {
        occ_group_rx_keepalive(&occ_group, now_ms, time_s, (occ_group_state_t)payload[2],
                               (occ_keepalive_reason_t)payload[3]);
     }
  }
  else
  {
     if((2U > length) || (OCC_GROUP_OFF < payload[0]))
     {
        return;
     }
     #if (OCC_PRINT >= 1)
     sl_zigbee_app_debug_println(">>OCC [0x%04X] level change %d lvl:%d   <%d>", cmd->source,
                                 payload[0], payload[1], now_ms);
     #endif
     occ_group_rx_level_change(&occ_group, now_ms, (occ_group_state_t)payload[0]);
  }
}

static bool sensor_debounced(void)
{
  if(has_timed_out(occupancy_sensor_info.time_stamp_unoccupied_to_occupied, 3000UL) &&
     has_timed_out(occupancy_sensor_info.time_stamp_occupied_to_unoccupied, 3000UL))
  {
     // Occ sensor has stabilized
     return(true);
  }
  else
  {
     #if (OCC_PRINT >= 1)
     sl_zigbee_app_debug_print("%d", occupancy_sensor_info.present_occupied_value);
     #endif
     return(false);
  }
}

bool is_occ_mid_or_lower(void)
{
  if(eADAPTIV_NWK == get_present_network())
  {
     return(OCC_GROUP_ON != occ_group_state(&occ_group));
  }
  else if((eNO_NWK == get_present_network()) || (eSTAND_ALONE == get_present_network()))
  {
     return(eUNOCCUPIED_UNOCCUPIED_MID_WAIT_NN <= present_local_state);
  }
  else
  {
     return(false);
  }
}

// Only enable occ sensor
static void unsuspend_occ_sensor_task(taskParm_t parm0, taskParm_t parm1)
{
  if(has_timed_out(parm0.u32, parm1.u32))
  {
     uint32_t now_ms = get_time_ms();

     // Determine if coming out of an off cmd
     if((0 == read_on_off_attribute(ZCL_ON_OFF_ATTRIBUTE_ID)) ||
        (is_app_flags_nonvol_set(B_EM_MODE) && off_from_switch()))
     {
        // Light is off
        if(OCC_GROUP_OFF != occ_group_state(&occ_group))
        {
           occ_group_external_off(&occ_group);
        }
        enter_off_local(now_ms);
        set_occupied_attribute(false);
     }
     else
     {
        // Light is on, reset delay, keep present occ state
        occupancy_sensor_info.time_stamp_unoccupied_to_occupied = now_ms;
        occupancy_sensor_info.time_stamp_occupied_to_unoccupied = now_ms;
        occ_group_restart_timer(&occ_group, now_ms);
        set_occupied_attribute(false);
     }
     // NOTE: This enables the occupancy sensor now.
     //       DHS is also enabled, not here, but from in it's own code
     allow_occupancy_sensor_control = true;
     removeTask(unsuspend_occ_sensor_task);

     #if (OCC_PRINT >= 1)
     char state_string[36] = {0};
     spew_state(present_local_state, state_string);
     sl_zigbee_app_debug_println("OCC Sensor Enabled [%s]   <%d>", state_string, get_time_ms());
     #endif
  }
}

void suspend_occ_sensor_control(uint16_t Dsecs)
{
  taskParm_t arg0, arg1;

  if((eADAPTIV_NWK != get_present_network()) || !occupancy_sensor_initialized)
  {
     return;
  }

  arg0.u32 = get_time_ms();
  arg1.u32 = 100UL*(uint32_t)Dsecs + OCC_SAMPLE_PERIOD_MS;
  addTask(unsuspend_occ_sensor_task, arg0, arg1);

  allow_occupancy_sensor_control = false;

  #if (OCC_PRINT >= 1)
  sl_zigbee_app_debug_println("OCC Sensors Disabled for %dmS   <%d>", arg1.u32, get_time_ms());
  #endif
}

// Suspend both occ and light sensors
void suspend_all_sensors(uint16_t Dsecs)
{
  suspend_occ_sensor_control(Dsecs);

  suspend_light_sensor_control(100UL * (uint32_t)Dsecs);
}



/*
 * Called from hal_occ_sensor at the occ sensor sample period.
 * Will time stamp if occ sensor changed, both positive and negative.
 * Check if we got turned off via a switch.
 */
void occ_sensor_logic(bool occupied)
{
  occupancy_sensor_info.present_occupied_value = occupied;

  occupancy_sensor_info.did_occ_sensor_change = false;

  // Did sensor change? If so new time stamp
  if(occupancy_sensor_info.present_occupied_value != occupancy_sensor_info.last_occupied_value)
  {
     occupancy_sensor_info.did_occ_sensor_change = true;
     //occupancy_sensor_info.last_occupied_value = occupancy_sensor_info.present_occupied_value;

     if(occupancy_sensor_info.present_occupied_value)
     {
        // Went from 0 -> 1
         occupancy_sensor_info.time_stamp_unoccupied_to_occupied = get_time_ms();
     }
     else
     {
        // Went from 1 -> 0
        occupancy_sensor_info.time_stamp_occupied_to_unoccupied = get_time_ms();
     }
  }
  occupancy_sensor_info.last_occupied_value = occupancy_sensor_info.present_occupied_value;

  // Global inhibit occupancy sensor?
  if(0 == get_unoccupied_delay())
  {
     return;
  }

  // Firmware version display
  if(is_app_flags_nonvol_set(B_DISABLE_OCC_SENSOR))
  {
     return;
  }
  #ifdef DHS_SINK_DEVICE
  return;
  #endif



  switch(get_present_network())
  {
     case eADAPTIV_NWK:
        adaptiv_sample(occupied);
        break;

     case eDAINTREE_NWK:
        // Check for valid daintree state
        switch(present_local_state)
        {
           case eOCCUPIED_OCCUPIED_ON_DT:
           case eOCCUPIED_UNOCCUPIED_ON_WAIT_DT:
           case eUNOCCUPIED_UNOCCUPIED_OFF_DT:
              break;
           default:
              occ_sensor_state = occupied_occupied_on_dt;
              present_local_state = eOCCUPIED_OCCUPIED_ON_DT;
              set_occupied_attribute(true);
              break;
        }
        // Run state machine
        if(allow_occupancy_sensor_control)
        {
           occ_sensor_state();
        }
        break;

     case eNO_NWK:
     case eSTAND_ALONE:
        // Check for valid state
        switch(present_local_state)
        {
           case eOCCUPIED_OCCUPIED_ON_NN:
           case eOCCUPIED_UNOCCUPIED_ON_WAIT_NN:
           case eUNOCCUPIED_UNOCCUPIED_MID_WAIT_NN:
           case eUNOCCUPIED_UNOCCUPIED_OFF_NN:
              break;
           default:
              occ_sensor_state = occupied_occupied_on_nn;
              present_local_state = eOCCUPIED_OCCUPIED_ON_NN;
              set_occupied_attribute(true);
              break;
        }
        // Run state machine
        if(allow_occupancy_sensor_control)
        {
           occ_sensor_state();
        }
        break;

     default:
        break;

  }
}

/***********************************************************************************************/
/************************************    DAINTREE++   ******************************************/
/***********************************************************************************************/
static bool allow_occ_controlled_lights(void)
{
  if((eDAINTREE_NWK == get_present_network()) && (0 == get_number_of_multicast_bindings()))
  {
     return(true);
  }
  else  if((eNO_NWK == get_present_network()) || (eSTAND_ALONE == get_present_network()))
  {
     return(true);
  }
  else
  {
     return(false);
  }

}

// TODO: Since eNO_NWK has own state machine remove references to it
static uint16_t get_daintree_occupancy_delay(void)
{
  uint8_t data[2] = {0};
  sl_zigbee_af_status_t status;

  if(eDAINTREE_NWK == get_present_network())
  {
     if(0 != get_number_of_multicast_bindings())
     {
        // We are grouped, so use ep 2 delay
        status = sl_zigbee_af_read_server_attribute(OCC_SENSOR_ENDPOINT,
                                                    ZCL_OCCUPANCY_SENSING_CLUSTER_ID,
                                                    ZCL_PIR_OCCUPIED_TO_UNOCCUPIED_DELAY_ATTRIBUTE_ID,
                                                    (uint8_t *)&data,
                                                    sizeof(data));
        if(status != SL_ZIGBEE_ZCL_STATUS_SUCCESS)
        {
           sl_zigbee_app_debug_println("ERR:DT reading delay attr 0x%X", status);
           return(60);
        }
        return(GET_16_FROM_ARRAY(data, 0));
     }
     else
     {
        // We are not grouped, so use xxl delay.....what a bag of puss
        return(DEFAULT_OCC_SENSOR_XXL_DELAY);
     }
  }
  else if((eNO_NWK == get_present_network()) || (eSTAND_ALONE == get_present_network()))
  {
      // If stand alone, use Adaptiv delay......more crap that hasn't been thought through
      uint16_t delay = get_unoccupied_delay();
      delay += get_unoccupied_delay_to_off();
      return(delay);
  }
  else
  {
     return(DEFAULT_OCC_SENSOR_XXL_DELAY);
  }
}

// Wait till occ = 0 after sensor debounce
static void occupied_occupied_on_dt(void)
{
  if(sensor_debounced())
  {
     if(false == occupancy_sensor_info.present_occupied_value)
     {
        occ_sensor_state = occupied_unoccupied_on_wait_dt;
        present_local_state = eOCCUPIED_UNOCCUPIED_ON_WAIT_DT;
        #if (OCC_PRINT != 0)
        sl_zigbee_app_debug_println("\nOCC DT to eOCCUPIED_UNOCCUPIED_ON_WAIT_DT %d   <%d>", get_daintree_occupancy_delay(), get_time_ms());
        #endif
     }
     else if(true == occupancy_sensor_info.present_occupied_value)
     {
        // Stay in this state
        set_occupied_attribute(true);
     }
  }
}

// Check if occ sensor -> occupied.  If not stay here till we meet criteria
// which is continuous no occ for delay time
static void occupied_unoccupied_on_wait_dt(void)
{
  uint32_t delay = (uint32_t)get_daintree_occupancy_delay() * 1000UL; // Delay attr in seconds

  if(true == occupancy_sensor_info.present_occupied_value)
  {
     set_occupied_attribute(true);
     occ_sensor_state = occupied_occupied_on_dt;
     present_local_state = eOCCUPIED_OCCUPIED_ON_DT;
     if(allow_occ_controlled_lights())
     {
        move_to_level_onoff(get_on_light_level(), DSECS_1_SECS);
     }

     #if (OCC_PRINT != 0)
     sl_zigbee_app_debug_println("OCC DT to eOCCUPIED_OCCUPIED_ON %d   <%d>", get_daintree_occupancy_delay(), get_time_ms());
     #endif
  }
  else if(has_timed_out(occupancy_sensor_info.time_stamp_occupied_to_unoccupied, delay))
  {
     // Officially unoccupied
     set_occupied_attribute(false);
     occ_sensor_state = unoccupied_unoccupied_off_dt;
     present_local_state = eUNOCCUPIED_UNOCCUPIED_OFF_DT;
     if(allow_occ_controlled_lights())
     {
        sl_zigbee_af_on_off_cluster_off_cb();
     }
     occ_time_stamp = get_time_ms();  // JGR
     #if (OCC_PRINT != 0)
     sl_zigbee_app_debug_println("OCC DT to eUNOCCUPIED_UNOCCUPIED_OFF_DT %d   <%d>", get_daintree_occupancy_delay(), get_time_ms());
     #endif
  }
  else
  {

  }
}

// Do nothing but wait for occ -> 1
static void unoccupied_unoccupied_off_dt(void)
{
  uint32_t delay = (uint32_t)read_occupancy_sensor_attribute(ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_DELAY_ATTRIBUTE_ID);
  delay *= 1000UL;  // Attribute delay in seconds

  //if(has_timed_out(occupancy_sensor_info.time_stamp_unoccupied_to_occupied, delay))
  {
     if(true == occupancy_sensor_info.present_occupied_value)
     {
        set_occupied_attribute(true);
        occ_sensor_state = occupied_occupied_on_dt;
        present_local_state = eOCCUPIED_OCCUPIED_ON_DT;

        if(allow_occ_controlled_lights())
        {
           move_to_level_onoff(get_on_light_level(), DSECS_1_SECS);
        }

        #if (OCC_PRINT != 0)
        sl_zigbee_app_debug_println("OCC DT to eOCCUPIED_OCCUPIED_ON_DTv from OFF_DT %d   <%d>",
                read_occupancy_sensor_attribute(ZCL_OCCUPANCY_ATTRIBUTE_ID), get_time_ms());
        #endif
     }
  }
}



/***********************************************************************************************/
/************************************   STANDALONE   *******************************************/
/***********************************************************************************************/
// Wait till occ = 0 after sensor debounce
static void occupied_occupied_on_nn(void)
{
  if(sensor_debounced())
  {
     if(false == occupancy_sensor_info.present_occupied_value)
     {
        occ_sensor_state = occupied_unoccupied_on_wait_nn;
        present_local_state = eOCCUPIED_UNOCCUPIED_ON_WAIT_NN;
        occ_time_stamp = get_time_ms();

        #if (OCC_PRINT != 0)
        sl_zigbee_app_debug_println("\nOCC NN to eOCCUPIED_UNOCCUPIED_ON_WAIT_NN %d   <%d>", get_unoccupied_delay(), get_time_ms());
        #endif
     }
     else if(true == occupancy_sensor_info.present_occupied_value)
     {
        // Stay in this state
        set_occupied_attribute(true);
     }
  }
}

// Check if occ sensor -> occupied.  If not stay here till we meet criteria
// which is continuous no occ for delay time
static void occupied_unoccupied_on_wait_nn(void)
{
  uint32_t delay = (uint32_t)get_unoccupied_delay() * 1000UL; // Delay attr in seconds

  if(true == occupancy_sensor_info.present_occupied_value)
  {
     set_occupied_attribute(true);
     occ_sensor_state = occupied_occupied_on_nn;
     present_local_state = eOCCUPIED_OCCUPIED_ON_NN;

     move_to_level_onoff(get_max_light_level(), DSECS_1_SECS);

     #if (OCC_PRINT != 0)
     sl_zigbee_app_debug_println("OCC NN to eOCCUPIED_OCCUPIED_ON    <%d>", get_time_ms());
     #endif
  }
  else if(has_timed_out(occ_time_stamp, delay))
  {
     // Officially unoccupied
     set_occupied_attribute(false);
     occ_sensor_state = unoccupied_unoccupied_mid_wait_nn;
     present_local_state = eUNOCCUPIED_UNOCCUPIED_MID_WAIT_NN;
     occ_time_stamp = get_time_ms();

     move_to_level_onoff(get_mid_light_level(), DSECS_1_SECS);

     #if (OCC_PRINT != 0)
     sl_zigbee_app_debug_println("OCC NN to eUNOCCUPIED_UNOCCUPIED_MID_NN   <%d>", get_time_ms());
     #endif
  }
  else
  {

  }
}

static void unoccupied_unoccupied_mid_wait_nn(void)
{
  uint32_t delay = (uint32_t)get_unoccupied_delay_to_off() * 1000UL; // Delay attr in seconds

  if(true == occupancy_sensor_info.present_occupied_value)
  {
     set_occupied_attribute(true);
     occ_sensor_state = occupied_occupied_on_nn;
     present_local_state = eOCCUPIED_OCCUPIED_ON_NN;
     occ_time_stamp = get_time_ms();

     move_to_level_onoff(get_max_light_level(), DSECS_1_SECS);

     #if (OCC_PRINT != 0)
     sl_zigbee_app_debug_println("OCC NN to eOCCUPIED_OCCUPIED_ON    <%d>", get_time_ms());
     #endif
  }
  else if(has_timed_out(occ_time_stamp, delay))
  {
     // Officially unoccupied
     set_occupied_attribute(false);
     occ_sensor_state = unoccupied_unoccupied_off_nn;
     present_local_state = eUNOCCUPIED_UNOCCUPIED_OFF_NN;
     occ_time_stamp = get_time_ms();

     if((0 == get_min_light_level()) && (!is_app_flags_nonvol_set(B_EM_MODE)))
     {
        // Normal mode
        sl_zigbee_af_on_off_cluster_off_cb();
     }
     else
     {
        // In EM_MODE, stay at mid level
     }

     #if (OCC_PRINT != 0)
     sl_zigbee_app_debug_println("OCC NN to eUNOCCUPIED_UNOCCUPIED_OFF_NN   <%d>", get_time_ms());
     #endif
  }
  else
  {

  }
}

// Do nothing but wait for occ -> 1
static void unoccupied_unoccupied_off_nn(void)
{
#ifdef USE_ZIGBEE_ATTRIBUTE_DELAY
  uint32_t delay = (uint32_t)read_occupancy_sensor_attribute(ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_DELAY_ATTRIBUTE_ID);
#else
  uint32_t delay = (uint32_t)DEFAULT_OCC_SENSOR_DELAY_TO_OCC;
#endif
  delay *= 1000UL;  // Attribute delay in seconds

  // If delay = 0, give finite time to settle system
  if(250UL > delay)
  {
     delay = 250UL;    // mS
  }

  if(has_timed_out(occ_time_stamp, delay))
  {
     if(true == occupancy_sensor_info.present_occupied_value)
     {
        set_occupied_attribute(true);
        occ_sensor_state = occupied_occupied_on_nn;
        present_local_state = eOCCUPIED_OCCUPIED_ON_NN;

        move_to_level_onoff(get_max_light_level(), DSECS_1_SECS);

        #if (OCC_PRINT != 0)
        sl_zigbee_app_debug_println("OCC NN to eOCCUPIED_OCCUPIED_ON_NN from OFF_NN   <%d>", get_time_ms());
        #endif
     }
  }
}

/***********************************************************************************************/
/*************************************   SUPPORT   *********************************************/
/***********************************************************************************************/

uint16_t read_occupancy_sensor_attribute(sl_zigbee_af_attribute_id_t attributeID)
{
  uint8_t data[2] = {0};
  uint8_t size = sizeof(data);
  sl_zigbee_af_status_t status;

  switch(attributeID)
  {
     case ZCL_OCCUPANCY_ATTRIBUTE_ID:
     case ZCL_OCCUPANCY_SENSOR_TYPE_ATTRIBUTE_ID:
     case ZCL_OCCUPANCY_SENSOR_TYPE_BITMAP_ATTRIBUTE_ID:
        size = 1;
        break;
     case ZCL_PIR_OCCUPIED_TO_UNOCCUPIED_DELAY_ATTRIBUTE_ID:
        size = 2;
        break;
     case ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_DELAY_ATTRIBUTE_ID:
        size = 2;
        break;
     case ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_ATTRIBUTE_ID:
        size = 2;
        break;
     default:
       sl_zigbee_app_debug_println("Unsupported occ attr");
       break;
  }
  status = sl_zigbee_af_read_server_attribute(OCC_SENSOR_ENDPOINT,
                                              ZCL_OCCUPANCY_SENSING_CLUSTER_ID,
                                              attributeID,
                                              (uint8_t *)&data,
                                              size);
  if(status != SL_ZIGBEE_ZCL_STATUS_SUCCESS) {
     sl_zigbee_app_debug_println("ERR:read_occupancy_attribute 0x%X", status);
  }
  #if (OCC_PRINT >= 3) && (OCC_PRINT != 9)
  if(ZCL_OCCUPANCY_SENSOR_TYPE_BITMAP_ATTRIBUTE_ID < attributeID)
  {
     sl_zigbee_app_debug_println("Read OCC attr 0x%04X value %d   <%d>",
                                                               attributeID,
                            (((uint16_t)data[1] << 8) | (uint16_t)data[0]),
                                                            get_time_ms());
  }
  #endif

  return(((uint16_t)data[1] << 8) | (uint16_t)data[0]);
}

bool write_occupancy_sensor_attribute(sl_zigbee_af_attribute_id_t attributeID, uint8_t *data)
{
  sl_zigbee_af_attribute_type_t type;
  bool status = true;

  switch(attributeID)
  {
     case ZCL_OCCUPANCY_ATTRIBUTE_ID:
        type = ZCL_BITMAP8_ATTRIBUTE_TYPE;
        break;
     case ZCL_OCCUPANCY_SENSOR_TYPE_ATTRIBUTE_ID:
        type = ZCL_ENUM8_ATTRIBUTE_TYPE;
        break;
     case ZCL_OCCUPANCY_SENSOR_TYPE_BITMAP_ATTRIBUTE_ID:
        type = ZCL_BITMAP8_ATTRIBUTE_TYPE;
        break;
     case ZCL_PIR_OCCUPIED_TO_UNOCCUPIED_DELAY_ATTRIBUTE_ID:
        type = ZCL_INT16U_ATTRIBUTE_TYPE;
        break;
     case ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_DELAY_ATTRIBUTE_ID:
        type = ZCL_INT16U_ATTRIBUTE_TYPE;
        break;
     case ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_THRESHOLD_ATTRIBUTE_ID:
        type = ZCL_INT16U_ATTRIBUTE_TYPE;
        break;
     default:
       status = false;
       sl_zigbee_app_debug_println("Unsupported occ attr");
       break;
  }

  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != sl_zigbee_af_write_server_attribute(
                                         OCC_SENSOR_ENDPOINT,
                                         ZCL_OCCUPANCY_SENSING_CLUSTER_ID,
                                         attributeID,
                                         data,
                                         type))
  {
     sl_zigbee_app_debug_println("ERR:write_occupancy_attribute");
     status = false;
  }
  else if(ZCL_OCCUPANCY_ATTRIBUTE_ID == attributeID)
  {
     #if (OCC_PRINT >= 2)
     sl_zigbee_app_debug_println("%sOCCUPIED    <%d>", data[0] ? "" : "UN-", get_time_ms());
     #endif
  }

  #if (OCC_PRINT >= 2) && (OCC_PRINT != 9)
     sl_zigbee_app_debug_println("Wrote OCC attr 0x%04X value %d   <%d>",
                                                               attributeID,
                                                               *data,
                                                               get_time_ms());
  #endif

  return(status);
}

static void set_occupied_attribute(bool occupied)
{
  uint8_t byte[1] = {occupied ? 1 : 0};
  write_occupancy_sensor_attribute(ZCL_OCCUPANCY_ATTRIBUTE_ID, (uint8_t *)&byte);
}

void spew_occ_state(void)
{
  char state_string[36] = {0};
  spew_state(present_local_state, state_string);
  sl_zigbee_app_debug_println("%s", state_string);
}

static void spew_state(occ_sensor_state_t state, char *state_string)
{
  static const char *group_state[] = { "ON", "MID", "OFF" };

  switch(state)
  {
     case eOCC_SENSOR_ADAPTIV:
        sprintf(state_string, "ADAPTIV %s %lu s", group_state[occ_group_state(&occ_group)],
                (unsigned long)(occ_group_remaining_ms(&occ_group, get_time_ms()) / 1000UL));
        break;
     case eOCCUPIED_OCCUPIED_ON_DT:
        sprintf(state_string, "eOCCUPIED_OCCUPIED_ON_DT");
        break;
     case eOCCUPIED_UNOCCUPIED_ON_WAIT_DT:
        sprintf(state_string, "eOCCUPIED_UNOCCUPIED_ON_WAIT_DT");
        break;
     case eUNOCCUPIED_UNOCCUPIED_OFF_DT:
        sprintf(state_string, "eUNOCCUPIED_UNOCCUPIED_OFF_DT");
        break;
     case eOCCUPIED_OCCUPIED_ON_NN:
        sprintf(state_string, "eOCCUPIED_OCCUPIED_ON_NN");
        break;
     case eOCCUPIED_UNOCCUPIED_ON_WAIT_NN:
        sprintf(state_string, "eOCCUPIED_UNOCCUPIED_ON_WAIT_NN");
        break;
     case eUNOCCUPIED_UNOCCUPIED_MID_WAIT_NN:
        sprintf(state_string, "eUNOCCUPIED_UNOCCUPIED_MID_WAIT_NN");
        break;
     case eUNOCCUPIED_UNOCCUPIED_OFF_NN:
        sprintf(state_string, "eUNOCCUPIED_UNOCCUPIED_OFF_NN");
        break;
     default:
        sprintf(state_string, "WARNING UNKNOW OCC STATE");
        break;
  }
}
