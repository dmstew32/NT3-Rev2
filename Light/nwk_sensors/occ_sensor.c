#include "af.h"
#include <stdlib.h>

#include "RehTOS.h"
#include "hal_occ_sensor.h"
#include "hal_utils.h"
#include "occ_sensor.h"
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

#include "sl_zigbee_debug_print.h"

#ifndef OCC_PRINT
#define OCC_PRINT 0    /* Default to 0 */
#elif (OCC_PRINT != 0)
#warning "OCC_PRINT active"
#endif

//#define USE_ZIGBEE_ATTRIBUTE_DELAY

extern sl_zigbee_af_zcl_request_status_t sl_zigbee_af_on_off_cluster_off_cb(void);

static uint32_t occ_time_stamp = 0UL;
static occ_sensor_state_t queue_state = eOCCUPIED_OCCUPIED_ON;
static uint32_t queue_time;

static uint16_t this_macID = 0;
static occ_sensor_state_t present_local_state = eOCCUPIED_OCCUPIED_ON;
static sl_zigbee_af_event_t occ_announce_event;
occupancy_sensor_info_t occupancy_sensor_info = {0};
static bool allow_occupancy_sensor_control = true;
static bool occupancy_sensor_initialized = false;
static volatile uint8_t occupancy_on_light_level = DEFAULT_MAX_LIGHT_LEVEL;

// Forward declarations
static void set_occupied_attribute(bool occupied);
static void set_occ_sensor_local_state(occ_sensor_state_t next_state);
static void occ_announce_handler(sl_zigbee_af_event_t *event);
static void (*occ_sensor_state)(void) = NULL;

static void occupied_occupied_on(void);
static void occupied_unoccupied_on_wait(void);
static void unoccupied_unoccupied_mid_wait(void);
static void unoccupied_unoccupied_mid(void);
static void unoccupied_unoccupied_off(void);
static void unoccupied_unoccupied_off_wait(void);
static void unoccupied_unoccupied_off_em_mode(void);

static void occupied_occupied_on_dt(void);
static void occupied_unoccupied_on_wait_dt(void);
static void unoccupied_unoccupied_off_dt(void);

static void occupied_occupied_on_nn(void);
static void occupied_unoccupied_on_wait_nn(void);
static void unoccupied_unoccupied_mid_wait_nn(void);
static void unoccupied_unoccupied_off_nn(void);

static void occ_sensor_nop(void);
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
        occ_sensor_state = occupied_occupied_on;
        present_local_state = eOCCUPIED_OCCUPIED_ON;
        // NOTE: Since we just joined, have each light set to the same state,
        // occupied_occupied_on and don't broadcast new state..
        //sl_zigbee_af_event_set_delay_ms(&occ_announce_event,
        //                             get_random_number(5000UL, 1000UL));
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
  sl_802154_long_addr_t thisEui64;
  uint16_t unoccupied_delay_secs = 0;

  hal_init_occ_sensor();

  sl_zigbee_af_event_init(&occ_announce_event, occ_announce_handler);

  // Occ sensor active?
  sl_token_get_data(TOKEN_APP_UNOCCUPIED_DELAY, 0, &unoccupied_delay_secs, sizeof(unoccupied_delay_secs));
  if(0 == unoccupied_delay_secs)
  {
     // Delay time of 0 = turn off occ sensor
     hal_stop_occ_sensor();
     sl_zigbee_app_debug_println("OCC occ_sensor_init  occ DISABLED");
     return;
  }

  sl_zigbee_af_get_eui64(thisEui64);
  this_macID = GET_16_FROM_ARRAY(thisEui64, 0);

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
     occ_sensor_state = occupied_occupied_on;
     present_local_state = eOCCUPIED_OCCUPIED_ON;
     suspend_light_sensor_control(DSECS_5_SECS);
     occupancy_sensor_info.time_stamp_unoccupied_to_occupied = get_time_ms();
     occupancy_sensor_info.time_stamp_occupied_to_unoccupied = get_time_ms();
     occ_time_stamp = get_time_ms();
     occupancy_on_light_level = get_max_light_level();
     // NOTE: Since we just powered on, have each light set to the same state,
     // occupied_occupied_on and don't broadcast new state..
     //sl_zigbee_af_event_set_delay_ms(&occ_announce_event,
     //                             get_random_number(5000UL, 1000UL));
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

// Event for multicasts to new occ state
static void occ_announce_handler(sl_zigbee_af_event_t *event)
{
  tx_occ_sensor_state(queue_state, queue_time, get_present_light_level());
}

// Queue with random delay for a multicast to other devices
// Called from this device, will place this device in nop state and wait for
// multicast to place it in the new_state.
// If rx of an occ multicast before this queue is executed, will remove request
// from the queue, and set_new_occ_state().
static void delay_tx_occ_sensor_present_state(occ_sensor_state_t present_state)
{
  queue_state = present_state;
  occ_sensor_state = occ_sensor_nop;
  present_local_state = eOCCUPENCY_SENSOR_NOP;

  occ_time_stamp = get_time_ms();

  sl_zigbee_af_event_set_inactive(&occ_announce_event);

  sl_zigbee_af_event_set_delay_ms(&occ_announce_event, get_random_number(12500UL, 7500UL));  // Average 10 seconds

  #if (OCC_PRINT >= 1)
  char state_string[36] = {0};
  spew_state(queue_state, state_string);
  sl_zigbee_app_debug_println("OCC QUEUE [0x%04X] %s   <%d>", this_macID, state_string, get_time_ms());
  #endif
}

// Initialize local variables for next time
static void set_occ_sensor_local_state(occ_sensor_state_t next_state)
{
  present_local_state = next_state;

  switch(next_state)
  {
     case eOCCUPIED_OCCUPIED_ON:
        occ_sensor_state = occupied_occupied_on;
        if(allow_occupancy_sensor_control)
        {
           move_to_level_onoff(get_on_light_level(), DSECS_1_SECS);
           //move_to_level_onoff(occupancy_on_light_level, DSECS_1_SECS);
           //move_to_level_onoff(get_max_light_level(), DSECS_1_SECS);
        }
        occupancy_sensor_info.time_stamp_unoccupied_to_occupied = get_time_ms();
        occupancy_sensor_info.time_stamp_occupied_to_unoccupied = get_time_ms();
        break;
     case eOCCUPIED_UNOCCUPIED_ON_WAIT:
        occ_sensor_state = occupied_unoccupied_on_wait;
        break;
     case eUNOCCUPIED_UNOCCUPIED_MID:
        occ_sensor_state = unoccupied_unoccupied_mid;
        break;
     case eUNOCCUPIED_UNOCCUPIED_MID_WAIT:
        occ_sensor_state = unoccupied_unoccupied_mid_wait;
        break;
     case eUNOCCUPIED_UNOCCUPIED_OFF_WAIT:
        sl_zigbee_af_event_set_inactive(&occ_announce_event);
        occ_sensor_state = unoccupied_unoccupied_off_wait;
        break;
     case eUNOCCUPIED_UNOCCUPIED_OFF:
        sl_zigbee_af_event_set_inactive(&occ_announce_event);
        occ_sensor_state = unoccupied_unoccupied_off;
        break;
     case eOCCUPENCY_SENSOR_NOP:
        occ_sensor_state = occ_sensor_nop;
        break;
     case eUNOCCUPIED_UNOCCUPIED_OFF_EM_MODE:
        sl_zigbee_af_event_set_inactive(&occ_announce_event);
        occ_sensor_state = unoccupied_unoccupied_off_em_mode;
        break;
     default:
        occ_sensor_state = occupied_occupied_on;
        suspend_light_sensor_control(DSECS_5_SECS);
        //move_to_level_onoff(get_on_light_level(), DSECS_TO_ON);
        occupancy_sensor_info.time_stamp_unoccupied_to_occupied = get_time_ms();
        occupancy_sensor_info.time_stamp_occupied_to_unoccupied = get_time_ms();
        break;
  }
  occ_time_stamp = get_time_ms();

  #if (OCC_PRINT >= 1)
  char state_string[36] = {0};
  spew_state(next_state, state_string);
  sl_zigbee_app_debug_println("OCC LOCAL [0x%04X] -> %s   <%d>", this_macID, state_string, get_time_ms());
  #endif
}

// RX Incoming cmd from multicast
// All multicasts that are directed to the occ sensor come through here
// Sync timers, set new local state, timestamp, check for dups
void set_new_occ_state(sl_zigbee_af_cluster_command_t *cmd)
{
  uint8_t base_idx = cmd->payloadStartIndex;
  bool update_next_state = true;

  occ_sensor_state_t new_state = cmd->buffer[base_idx];
  uint8_t occ_level = cmd->buffer[base_idx+3];

  // Don't respond from others in the group
  if(is_app_flags_nonvol_set(B_DISABLE_OCC_SENSOR))
  {
     sl_zigbee_app_debug_println("OCC is Disabled   <%d>", get_time_ms());
     return;
  }

  // If something in the queue, abort
  sl_zigbee_af_event_set_inactive(&occ_announce_event);

  // Look for dups or timing issues
  if((occ_sensor_state == unoccupied_unoccupied_mid_wait) ||
     (occ_sensor_state == unoccupied_unoccupied_mid))
  {
     if((eUNOCCUPIED_UNOCCUPIED_MID_WAIT == new_state) ||
        (eUNOCCUPIED_UNOCCUPIED_MID == new_state))
     {
        // Dup.  To get here means we are presently in one of the mid states
        // and the incoming cmd says to go to a mid state....toss this cmd.
        update_next_state = false;
        #if (OCC_PRINT >= 1)
        sl_zigbee_app_debug_print("DUP ");
        #endif
     }
     else if(eOCCUPIED_OCCUPIED_ON == new_state)
     {
         // To get here means we are presently in one of the mid states and
         // the incoming cmd says to go to a max/on state.  Force the correct
         // on level, which may not be correct due to active dim/mid state.
        if(read_light_level_attribute(ZCL_ON_LEVEL_ATTRIBUTE_ID) != occupancy_on_light_level)
        {
           write_light_level_attribute(ZCL_ON_LEVEL_ATTRIBUTE_ID, (uint8_t  *)&occupancy_on_light_level);
           #if (OCC_PRINT >= 1)
           sl_zigbee_app_debug_print("LVL ");
           #endif
        }
     }
     else
     {

     }
  }

  #if (OCC_PRINT >= 1)
  char state_string[40] = {0};
  uint16_t mac_id = GET_16_FROM_ARRAY(cmd->buffer, base_idx+1);
  spew_state(new_state, state_string);
  sl_zigbee_app_debug_println(">>OCC [0x%04X] %s lvl:%d   <%d>", mac_id, state_string, occ_level, get_time_ms());
  #else
  (void)occ_level;
  #endif

  if(update_next_state)
  {
     set_occ_sensor_local_state(new_state);
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
     return(eUNOCCUPIED_UNOCCUPIED_MID <= present_local_state);
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
     // Determine if coming out of an off cmd
     if(0 == read_on_off_attribute(ZCL_ON_OFF_ATTRIBUTE_ID))
     {
        // Light is off
        set_occ_sensor_local_state(eUNOCCUPIED_UNOCCUPIED_OFF_WAIT);
        set_occupied_attribute(false);
     }
     else if(is_app_flags_nonvol_set(B_EM_MODE))
     {
        if(off_from_switch())
        {
           set_occ_sensor_local_state(eUNOCCUPIED_UNOCCUPIED_OFF_WAIT);
           set_occupied_attribute(false);
        }
     }
     else
     {
        // Light is on, reset delay, keep present occ state
        occupancy_sensor_info.time_stamp_unoccupied_to_occupied = get_time_ms();
        occupancy_sensor_info.time_stamp_occupied_to_unoccupied = get_time_ms();
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

  suspend_light_sensor_control(Dsecs);
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

  // DHS calibration
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
        if((0 == read_on_off_attribute(ZCL_ON_OFF_ATTRIBUTE_ID)) &&
           (present_local_state != eUNOCCUPIED_UNOCCUPIED_OFF_WAIT) &&
           (present_local_state != eUNOCCUPIED_UNOCCUPIED_OFF) &&
           (present_local_state != eUNOCCUPIED_UNOCCUPIED_OFF_EM_MODE))
        {
           // External off cmd or timing to off
           set_occ_sensor_local_state(eUNOCCUPIED_UNOCCUPIED_OFF_WAIT);
           set_occupied_attribute(false);
        }

        // Run state machine
        if(allow_occupancy_sensor_control && device_is_in_a_group())
        {
           occ_sensor_state();
        }
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

// Reset state, assumed the occ attr is true when entering this state.
// Stay until occ sensor has stabilized and un-occupied
static void occupied_occupied_on(void)
{
  if(sensor_debounced())
  {
     if(false == occupancy_sensor_info.present_occupied_value)
     {
        set_occ_sensor_local_state(eOCCUPIED_UNOCCUPIED_ON_WAIT);
        occupancy_sensor_info.time_stamp_occupied_to_unoccupied = get_time_ms();
     }
     else if(true == occupancy_sensor_info.present_occupied_value)
     {
        // Stay in this state
        set_occupied_attribute(true);
     }
  }
  else
  {
     // Stay here
  }
}

// Attribute still reports occupied, but sensor says unoccupied, light is on.
// Stay here till we meet criteria = continuous no occ for delay time
static void occupied_unoccupied_on_wait(void)
{
  uint32_t delay = (uint32_t)get_unoccupied_delay() * 1000UL; // Delay attr in seconds

  if(true == occupancy_sensor_info.present_occupied_value)
  {
     delay_tx_occ_sensor_present_state(eOCCUPIED_OCCUPIED_ON);
     set_occupied_attribute(true);
  }
  else if(has_timed_out(occupancy_sensor_info.time_stamp_occupied_to_unoccupied, delay) &&
         (false == occupancy_sensor_info.present_occupied_value))
  {
     // Officially unoccupied
     set_occupied_attribute(false);
     occ_time_stamp = get_time_ms();
     occupancy_on_light_level = save_present_light_level_to_on_level_attribute();
     delay_tx_occ_sensor_present_state(eUNOCCUPIED_UNOCCUPIED_MID);
  }
  else
  {

  }
}

// This is a one and done state, will turn light to mid level, and move on
// to the next state.  This is a sync state
static void unoccupied_unoccupied_mid(void)
{
  // If a occ event occurs on another light it will multicast to go to last level.
  // But if last level/next state has not completed, last level won't be valid.
  // So do it here and in the next state.
  occupancy_on_light_level = save_present_light_level_to_on_level_attribute();

  // All occ sensors are in this state.  Issue local dim to mid
  if(get_present_light_level() > get_mid_light_level())
  {
     // move_to_level_onoff() adds extra time to the disable sensors
      move_to_level_onoff (get_mid_light_level (), DSECS_1_SECS);
  }
  set_occ_sensor_local_state(eUNOCCUPIED_UNOCCUPIED_MID_WAIT);

  // Allow occ event interrupt the dim to mid, and go directly to max
  suspend_occ_sensor_control(DSECS_0_SECS);
}

// We're at mid level, wait make sure siLab level ramp is done
static void unoccupied_unoccupied_mid_wait(void)
{
  uint32_t delay = (uint32_t)get_unoccupied_delay_to_off() * 1000UL; // Delay attr in seconds

  // Reduce constant writes
  if(read_light_level_attribute(ZCL_ON_LEVEL_ATTRIBUTE_ID) != occupancy_on_light_level)
  {
      write_light_level_attribute(ZCL_ON_LEVEL_ATTRIBUTE_ID, (uint8_t  *)&occupancy_on_light_level);
  }

  if(true == occupancy_sensor_info.present_occupied_value)
  {
     tx_occ_sensor_state(eOCCUPIED_OCCUPIED_ON, get_time_ms(), get_on_light_level());
  }
  else if(has_timed_out(occ_time_stamp, delay))
  {
     // All occ sensors are now in this state, sync'ed from unoccupied_unoccupied_mid()
     occ_sensor_state_t next_state = eUNOCCUPIED_UNOCCUPIED_OFF_WAIT;

     if(0 == get_min_light_level() && (false == is_app_flags_nonvol_set(B_EM_MODE)))
     {
        // NOTE: suspend_all_sensors() in cb
        sl_zigbee_af_on_off_cluster_off_cb();

        #if (OCC_PRINT >= 1)
        sl_zigbee_app_debug_println("OCC Lights OFF   <%d>", get_time_ms());
        #endif
     }
     else
     {
        // NOTE: We are in EM mode.  Saved level is valid
        // Go to normal _OFF_WAIT which then decides to go to EM mode
        #if (OCC_PRINT >= 1)
        sl_zigbee_app_debug_println("OCC Lights ON em_mode   <%d>", get_time_ms());
        #endif
     }

     set_occ_sensor_local_state(next_state);

     // sl_zigbee_af_on_off_cluster_off_cb() and em_mode will have different
     // sensor disables times.  So over ride it here to make the same.
     suspend_occ_sensor_control(DSECS_HALF_SECS);
  }
  else
  {
      // Waiting
  }
}

// One and done..
// Also entry point for off cmd from switch
// NOTE: Used to Lutron delay crap, which has been scrapped
static void unoccupied_unoccupied_off_wait(void)
{
  clear_on_from_switch();

  if(is_app_flags_nonvol_set(B_EM_MODE))
  {
     set_occ_sensor_local_state(eUNOCCUPIED_UNOCCUPIED_OFF_EM_MODE);
  }
  else
  {
     set_occ_sensor_local_state(eUNOCCUPIED_UNOCCUPIED_OFF);
  }
  occ_time_stamp = get_time_ms();
}

// Final resting place, wait for either occ trip or an on cmd from switch
static void unoccupied_unoccupied_off(void)
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

  // If got here because off button was pressed, delay 5 minutes, then allow
  // occ sensor.  Timer gets reset on every occ trip.
  if(off_from_switch())
  {
     delay = (5UL*60UL*1000UL);   // John 3/30, 5 minutes

     if(true == occupancy_sensor_info.present_occupied_value)
     {
        // Reset timer John 3/30
        occ_time_stamp = get_time_ms();
        #if (OCC_PRINT >= 1)
        sl_zigbee_app_debug_println("OCC Reset timer   <%d>", occ_time_stamp);
        #endif
     }
  }

  // If from off button delay is 5 minutes, else delay is from attribute
  if(has_timed_out(occ_time_stamp, delay))
  {
     clear_off_from_switch();
     restore_unoccupied_delay();

     // Past delay time, allow occ trip to turn on lights
     if(true == occupancy_sensor_info.present_occupied_value)
     {
        if(!is_app_flags_nonvol_set(B_MANUAL_ON_MODE))
        {
           #if (OCC_PRINT >= 1)
           sl_zigbee_app_debug_println("OCC Sensor tripped while off  <%d>", get_time_ms());
           #endif
           tx_occ_sensor_state(eOCCUPIED_OCCUPIED_ON, get_time_ms(), get_on_light_level());
        }
        set_occupied_attribute(true);
     }

     // If no occ trip for a long time, set on level to max light level.
     if(has_timed_out(occ_time_stamp, (DEFAULT_OCC_SENSOR_XXXL_DELAY*1000UL)))
     {
        occupancy_on_light_level = get_max_light_level();
        if(get_max_light_level() != get_on_light_level())
        {
            write_light_level_attribute(ZCL_ON_LEVEL_ATTRIBUTE_ID, (uint8_t  *)&occupancy_on_light_level);
        }
     }
  }

  if(1 == read_on_off_attribute(ZCL_ON_OFF_ATTRIBUTE_ID))
  {
    // Lights turned on via external source ie. switch, another occ sensor etc
    sl_zigbee_app_debug_println("OCC external on cmd   <%d>", get_time_ms());

    set_occ_sensor_local_state(eOCCUPIED_OCCUPIED_ON);
    set_occupied_attribute(true);
    clear_off_from_switch();
    clear_on_from_switch();
  }
}

static void unoccupied_unoccupied_off_em_mode(void)
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

  clear_off_from_switch();
  restore_unoccupied_delay();

  if(has_timed_out(occ_time_stamp, delay))
  {
     // Past delay time, allow occ trip to turn on lights
     if(true == occupancy_sensor_info.present_occupied_value)
     {
        #if (OCC_PRINT >= 1)
        sl_zigbee_app_debug_println("OCC Sensor tripped while off  <%d>", get_time_ms());
        #endif
        tx_occ_sensor_state(eOCCUPIED_OCCUPIED_ON, get_time_ms(), get_on_light_level());
     }
     set_occupied_attribute(true);
  }

  if(on_from_switch())
  {
    // Lights turned on via external source ie. switch, another occ sensor etc
    sl_zigbee_app_debug_println("OCC external on cmd   <%d>", get_time_ms());

    set_occ_sensor_local_state(eOCCUPIED_OCCUPIED_ON);
    set_occupied_attribute(true);
  }
}


// Wait here until in new state is received
static void occ_sensor_nop(void)
{
  if(has_timed_out(occ_time_stamp, 15000UL))  // Larger than random delay
  {
     sl_zigbee_app_debug_println("TIMED OUT NOP OCC STATE %d   <%d>", queue_state, get_time_ms());
     tx_occ_sensor_state(eOCCUPIED_OCCUPIED_ON, get_time_ms(), get_max_light_level());
     //delay_tx_occ_sensor_present_state(queue_state);
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
  switch(state)
  {
     case eOCCUPIED_OCCUPIED_ON:
        sprintf(state_string, "eOCCUPIED_OCCUPIED_ON");
        break;
     case eOCCUPIED_UNOCCUPIED_ON_WAIT:
        sprintf(state_string, "eOCCUPIED_UNOCCUPIED_ON_WAIT");
        break;
     case eUNOCCUPIED_UNOCCUPIED_MID_WAIT:
        sprintf(state_string, "eUNOCCUPIED_UNOCCUPIED_MID_WAIT");
        break;
     case eUNOCCUPIED_UNOCCUPIED_MID:
        sprintf(state_string, "eUNOCCUPIED_UNOCCUPIED_MID");
        break;
     case eUNOCCUPIED_UNOCCUPIED_OFF:
        sprintf(state_string, "eUNOCCUPIED_UNOCCUPIED_OFF");
        break;
     case eUNOCCUPIED_UNOCCUPIED_OFF_WAIT:
        sprintf(state_string, "eUNOCCUPIED_UNOCCUPIED_OFF_WAIT");
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
     case eOCCUPENCY_SENSOR_NOP:
        sprintf(state_string, "eOCCUPENCY_SENSOR_NOP");
        break;
     case eUNOCCUPIED_UNOCCUPIED_OFF_EM_MODE:
        sprintf(state_string, "eUNOCCUPIED_UNOCCUPIED_OFF_EM_MODE");
        break;
     default:
        sprintf(state_string, "WARNING UNKNOW OCC STATE");
        break;
  }
}
