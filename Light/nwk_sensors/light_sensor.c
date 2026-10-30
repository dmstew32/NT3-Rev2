#include "af.h"
#include <stdlib.h>
#include <math.h>

#include "RehTOS.h"
#include "hal_utils.h"
#include "hal_light_sensor.h"
#include "hal_pwm.h"
#include "light_sensor.h"
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
//#define LIGHT_SENSOR_CAL_PRINT

extern void suspend_all_sensors(uint16_t Dsecs);


static int16_t light_sensor_reference = 300;
static void (*light_sensor_state)(void) = NULL;
static uint32_t light_sensor_time_stamp = 0UL;
static bool light_sensor_control_suspended = true;
static bool light_sensor_control_suspended_dimming = true;
static light_sensor_table_t light_sensor_neighbor_table[N_SENSOR_NEIGHBOR_TABLE_SIZE] = {0};
static light_sensor_data_t light_sensor_data;

static uint8_t calibration_counter = 0;
static uint8_t cal_next_level = DEFAULT_MAX_LIGHT_LEVEL;
static uint32_t sum_lux = 0;
static uint8_t saved_original_max_level = DEFAULT_MAX_LIGHT_LEVEL;
static uint8_t saved_original_min_level = 0;

static master_light_sensor_t master_light = {0};
static sl_802154_long_addr_t thisEui64;

static uint16_t dhs_inhibit_bits = 0;

// Fwd declarations
static void dhs_init_cal_state(void);
static void sensor_idle_state(void);
static void sensor_active_state(void);

static void send_this_neighbor_table_info(void);
static void get_stats_neighbor_table(light_sensor_stats_t *sensor_stats);
static int16_t saturate_pwm_level(int16_t pwm_value);

/*
 * The light sensor is an i2c interface that is sampled every LIGHT_SENSOR_HAL_SAMPLE_TIME_MS.
 * The average is generated over N_LIGHT_SAMPLES. The function execute_light_sensor_logic()
 * is called at the rate as N_LIGHT_SAMPLES * LIGHT_SENSOR_HAL_SAMPLE_TIME_MS.
 */
void light_sensor_init(void)
{
  sl_token_get_data(TOKEN_APP_LIGHT_SENSOR_LUX, 0, &light_sensor_reference, sizeof(light_sensor_reference));

  // i2c etc
  hal_light_sensor_init();

  write_light_sensor_attribute(ZCL_ILLUM_MIN_MEASURED_VALUE_ATTRIBUTE_ID, 2);
  write_light_sensor_attribute(ZCL_ILLUM_MAX_MEASURED_VALUE_ATTRIBUTE_ID, 1023);
  memset(&light_sensor_neighbor_table, 0, sizeof(light_sensor_neighbor_table));
  memset(&light_sensor_data, 0, sizeof(light_sensor_data_t));

  light_sensor_state = sensor_idle_state;
  light_sensor_time_stamp = get_time_ms();
  light_sensor_control_suspended = true;
  light_sensor_control_suspended_dimming = false;

  suspend_light_sensor_control(LIGHT_SENSOR_UPDATE_AVERAGE_MS);  // xx secs

  memset(&master_light, 0, sizeof(master_light_sensor_t));
  master_light.pwm_value = DEFAULT_MAX_LIGHT_LEVEL;
  master_light.dim_time_dsec = DSECS_1_SECS;
  master_light.update_time_stamp = get_time_ms();
  sl_token_get_data(TOKEN_APP_DHS_GAIN, 0, &master_light.gain, sizeof(master_light.gain));

  sl_zigbee_af_get_eui64(thisEui64);

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
  dhs_inhibit_bits |= (eADAPTIV_NWK != get_present_network()) ? (1 << 0) : 0;
  dhs_inhibit_bits |= !is_app_flags_nonvol_set(B_DHS_CALIBRATED) ? (1 << 1) : 0;
  dhs_inhibit_bits |= (0 == read_on_off_attribute(ZCL_ON_OFF_ATTRIBUTE_ID)) ? (1 << 2) : 0;
  dhs_inhibit_bits |= !device_is_in_a_group() ? (1 << 3) : 0;
  dhs_inhibit_bits |= light_sensor_control_suspended ? (1 << 4) : 0;
  dhs_inhibit_bits |= (0 == light_sensor_reference) ? (1 << 5) : 0;
  dhs_inhibit_bits |= (MIN_ALLOWABLE_LIGHT_LEVEL > hal_get_dim_pwm_255()) ? (1 << 6) : 0;
  dhs_inhibit_bits |= is_occ_mid_or_lower() ? (1 << 7) : 0;
  dhs_inhibit_bits |= light_sensor_control_suspended_dimming ? (1 << 8) : 0;
  dhs_inhibit_bits |= is_app_flags_nonvol_set(B_DISABLE_LIGHT_SENSOR) ? (1 << 9) : 0;
  dhs_inhibit_bits |= is_app_flags_nonvol_set(B_DHS_MASTER_LIGHT) ? 0 : (1 << 10);


  #if (1 <= LIGHT_SENSOR_PRINT)
  sl_zigbee_app_debug_print("DHS %s", (0 == dhs_inhibit_bits) ? "YES" : "NO");
  sl_zigbee_app_debug_println("  %d(nwk) %d(!cal) %d(off) %d(!grp) %d(susp) %d(0ref) %d(0pwm) %d(occ) %d(dim) %d(inh) %d(mstr)",
    (0 != (dhs_inhibit_bits & (1 << 0))), (0 != (dhs_inhibit_bits & (1 << 1))), (0 != (dhs_inhibit_bits & (1 << 2))), (0 != (dhs_inhibit_bits & (1 << 3))),
    (0 != (dhs_inhibit_bits & (1 << 4))), (0 != (dhs_inhibit_bits & (1 << 5))), (0 != (dhs_inhibit_bits & (1 << 6))), (0 != (dhs_inhibit_bits & (1 << 7))),
    (0 != (dhs_inhibit_bits & (1 << 8))), (0 != (dhs_inhibit_bits & (1 << 9))), (0 != (dhs_inhibit_bits & (1 << 10))));
  #endif

  return(dhs_inhibit_bits);
}

bool allow_light_sensor_control(void)
{
  return(0 == get_dhs_inhibit_bits());
}

void set_dhs_time_stamp(void)
{
  master_light.update_time_stamp = get_time_ms();
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

// Disable light sensor modifying the light pwm for a period of time, in Dsecs.
// Can be called from isr or non-isr cb's.  Over writes previous suspend.
bool suspend_light_sensor_control(uint16_t Dsecs)
{
  taskParm_t arg0, arg1;

  arg0.u32 = get_time_ms();
  arg1.u32 = 100UL*(uint32_t)Dsecs;
  addTask(unsuspend_light_sensor_task, arg0, arg1);
  light_sensor_control_suspended = true;

  #if (1 <= LIGHT_SENSOR_PRINT)
  sl_zigbee_app_debug_println("DHS Disabled for %dmS   <%d>", Dsecs*100, get_time_ms());
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

// Disable light sensor modifying the light pwm for a period of time, in Dsecs.
// Can be called from isr or non-isr cb's. From dimming cmd.
bool suspend_light_sensor_control_dimming(uint16_t Dsecs)
{
  taskParm_t arg0, arg1;

  arg0.u32 = get_time_ms();
  arg1.u32 = 100UL*(uint32_t)Dsecs;
  addTask(unsuspend_light_sensor_dimming_task, arg0, arg1);
  light_sensor_control_suspended_dimming = true;

  #if (1 <= LIGHT_SENSOR_PRINT)
  sl_zigbee_app_debug_println("DHS Disabled (Dim) for %dmS   <%d>", Dsecs*100, get_time_ms());
  #endif

  return(true);
}

static void send_dhs_table_info_task(taskParm_t parm0, taskParm_t parm1)
{
  if(has_timed_out(parm0.u32, parm1.u32))
  {
     removeTask(send_dhs_table_info_task);
     send_this_neighbor_table_info();
  }
}
// Randomly send this devices's dhs info
static void send_dhs_table_info(void)
{
  taskParm_t arg0, arg1;

  arg0.u32 = get_time_ms();
  if(is_app_flags_nonvol_set(B_DHS_CALIBRATING))
  {
     arg1.u32 = get_random_number((LIGHT_SENSOR_CAL_UPDATE_AVERAGE_MS - 500UL), 500UL);
  }
  else
  {
     // Normal, only the master broadcasts
     arg1.u32 = get_random_number(LIGHT_SENSOR_UPDATE_AVERAGE_MS, LIGHT_SENSOR_UPDATE_AVERAGE_MS - 2000UL);
  }
  addTask(send_dhs_table_info_task, arg0, arg1);
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

  light_sensor_state();

  if(sink_debug_enabled())
  {
     spew_on_dhs_changes();
  }
}




/***************************************************************************/
/************                 DHS STATES             ***********************/
/***************************************************************************/

// Stay here till the light is on and timed out letting sensor filter get primed
static void sensor_idle_state(void)
{
  if(has_timed_out(light_sensor_time_stamp, LIGHT_SENSOR_UPDATE_AVERAGE_MS))
  {
     if(allow_light_sensor_control())
     {
        light_sensor_state = sensor_active_state;
        light_sensor_time_stamp = get_time_ms();
     }
     #if (2 <= LIGHT_SENSOR_PRINT)
     sl_zigbee_app_debug_println("DHS Moving to active: lux:%d  pwm:%d   <%d>",
       light_sensor_data.mean_lux, light_sensor_data.pwm_value, get_time_ms());
     #endif
  }
  else
  {
     #if (2 <= LIGHT_SENSOR_PRINT)
     sl_zigbee_app_debug_println("DHS Idle: lux:%d  pwm:%d   <%d>",
       light_sensor_data.mean_lux, light_sensor_data.pwm_value, get_time_ms());
     #endif
  }
}

// Main active state, determine next light level if master device
static void sensor_active_state(void)
{
  if(false == allow_light_sensor_control())
  {
     return;
  }

  if(false == is_app_flags_nonvol_set(B_DHS_MASTER_LIGHT))
  {
     return;  // Not master, so just adjust pwm when told to
  }

  /*** To get here means we are MASTER ***/

  if(!has_timed_out(master_light.update_time_stamp, ((uint32_t)master_light.dim_time_dsec*100UL)))
  {
     // Must let ramp finish before using average lux samples
     return;
  }

  int16_t sensor_lux_error = light_sensor_reference - (int16_t)light_sensor_data.mean_lux;

  master_light.pwm_value = (int16_t)hal_get_dim_pwm_255();

  // If lux error within 5% of target, bail
  if(((5*light_sensor_reference)/100) > abs(sensor_lux_error))
  {
     #if (1 <= LIGHT_SENSOR_PRINT)
     sl_zigbee_app_debug_println("DHS MASTER  No update  lux_ref:%d  lux:%d   <%d>",
             light_sensor_reference, light_sensor_data.mean_lux, get_time_ms());
     #endif

     return;
  }

  // Limit lux error
  if(MAX_LUX_ERROR < abs(sensor_lux_error))
  {
     sensor_lux_error = SIGNUM(sensor_lux_error) * MAX_LUX_ERROR;
  }

  // Integrate
  int16_t next_pwm_value = (int16_t)master_light.pwm_value +
                           master_light.gain * sensor_lux_error;

  next_pwm_value = saturate_pwm_level(next_pwm_value);

  uint16_t delta_pwm = abs(next_pwm_value - (int16_t)master_light.pwm_value);

  // If pwm value not to change more than 5% forget about it
  if(((5*MAX_ALLOWABLE_LIGHT_LEVEL)/100) > delta_pwm)
  {
     #if (1 <= LIGHT_SENSOR_PRINT)
     sl_zigbee_app_debug_println("DHS MASTER  No update  pwm:%d  delta pwm:%d   <%d>",
                                 master_light.pwm_value, delta_pwm, get_time_ms());
     #endif

     return;
  }
#if 0
  if(((10*MAX_ALLOWABLE_LIGHT_LEVEL)/100) > abs(delta_pwm))
  {
     next_pwm_value = (int16_t)master_light.pwm_value + delta_pwm;
  }
  else
  {
     next_pwm_value = (int16_t)master_light.pwm_value + (delta_pwm/4);
  }
#endif


  // To get here means change pwm level for group, change
  master_light.pwm_value = (uint8_t)next_pwm_value;

  master_light.dim_time_dsec = (LIGHT_SENSOR_UPDATE_AVERAGE_MS - 1500UL)/100UL;  // Dsec

  master_light.update_time_stamp = get_time_ms();

  send_this_neighbor_table_info(); // Tx Immediate

  master_light.last_pwm_value = master_light.pwm_value;

  #if (1 <= LIGHT_SENSOR_PRINT)
  sl_zigbee_app_debug_println("DHS MASTER  next_pwm_value:%d = present pwm:%d + (%d * %d)   "
      "delta_pwm:%d   lux:%d  dsec:%d   <%d>",  master_light.pwm_value, hal_get_dim_pwm_255(),
      master_light.gain, sensor_lux_error, delta_pwm, light_sensor_data.mean_lux,
      master_light.dim_time_dsec, get_time_ms());
  #endif

  return;
}

#ifdef DHS_SOURCE_DEVICE  // NOTE: To get here means master only
  if(sink_debug_enabled())
  {
     uint8_t data[16] = {0};
     uint8_t length;

     // Op-code
     data[0] = DHS_MST_DATA;   // Daylight harvester
     // Mac id from this device
     data[1] = thisEui64[0];
     data[2] = thisEui64[1];
     // Output
     data[3] = GET_LSB_FROM_16(light_sensor_data.mean_lux);
     data[4] = GET_MSB_FROM_16(light_sensor_data.mean_lux);
     data[5] = master_light.pwm_value;
     data[6] = master_light.dim_time_dsec;
     data[7] = sensor_lux_error;
     length = 8;

     tx_source_debug(data, length);
  }
#endif


static uint8_t get_neighbor_table_index(uint16_t addr)
{
  for(uint8_t i = 0; i < N_SENSOR_NEIGHBOR_TABLE_SIZE; i++)
  {
     if(light_sensor_neighbor_table[i].macID == addr)
     {
        return(i);
     }
  }
  return(0xFF);
}

// RX: This is incoming from a ZCL_WRITE_LIGHT_SENSOR_COMMAND_ID cmd.
// Take new data from another device in the group, and enter the
// data into the table.  The transmit is send_this_neighbor_table_info()
bool update_light_sensor_neighbor_table(sl_zigbee_af_cluster_command_t* cmd)
{
  uint8_t base_idx = cmd->payloadStartIndex;

  uint16_t macID = GET_16_FROM_ARRAY(cmd->buffer, base_idx);
  uint16_t lux = GET_16_FROM_ARRAY(cmd->buffer, (base_idx+2));
  uint8_t pwm = cmd->buffer[base_idx+4];
  uint8_t slave_pwm = cmd->buffer[base_idx+5];
  uint8_t slave_dim_time = cmd->buffer[base_idx+6];
  uint8_t mode = cmd->buffer[base_idx+7];   // [Counter | Mode >> Shifted]

  uint8_t idx = get_neighbor_table_index(macID);
  if(0xFF != idx)
  {
     if(is_app_flags_nonvol_set(B_DHS_HIGH_LUX))
     {
        light_sensor_neighbor_table[idx].lux_high = lux;
     }
     else
     {
        light_sensor_neighbor_table[idx].lux_low = lux;
     }
     light_sensor_neighbor_table[idx].pwm = pwm;
     light_sensor_neighbor_table[idx].mode = mode;

     #if (1 <= LIGHT_SENSOR_PRINT)
     sl_zigbee_app_debug_println(">>DHS[%d] macID:0x%04X lux:%d/%d pwm:%d mode:0x%X",
         idx, macID, light_sensor_neighbor_table[idx].lux_high,
         light_sensor_neighbor_table[idx].lux_low, pwm, mode);
    #endif
  }
  else
  {
     // New mac_id? get next open slot, addr = 0
     idx = get_neighbor_table_index(0);
     if(0xFF != idx)
     {
        light_sensor_neighbor_table[idx].macID = macID;
        if(is_app_flags_nonvol_set(B_DHS_HIGH_LUX))
        {
           light_sensor_neighbor_table[idx].lux_high = lux;
        }
        else
        {
           light_sensor_neighbor_table[idx].lux_low = lux;
        }
        light_sensor_neighbor_table[idx].pwm = pwm;
        light_sensor_neighbor_table[idx].mode = mode;

        #if (1 <= LIGHT_SENSOR_PRINT)
        sl_zigbee_app_debug_println("ADD NEW ENTRY[%d] macID:0x%04X lux:%d pwm:%d mode:0x%X",
                                                         idx, macID, lux, pwm, mode);
        #endif
     }
     else
     {
        sl_zigbee_app_debug_println("LIGHT SENSOR TABLE FULL");
     }
  }

  /*
   * If new rx entry is from the master, and this node is calibrated,
   * then update level
   */
  if((BS_DHS_MASTER_LIGHT & mode) && is_app_flags_nonvol_set(B_DHS_CALIBRATED))
  {
     move_to_level_dhs(slave_pwm, slave_dim_time);
  }

  return(true);
}

// TX: Multi-cast to all in group, this dhs info with the
// ZCL_WRITE_LIGHT_SENSOR_COMMAND_ID cmd.
// The receiving function is update_light_sensor_neighbor_table()
static void send_this_neighbor_table_info(void)
{
  uint8_t data[16] = {0};
  uint8_t length;

  // Mac id from this device
  data[0] = thisEui64[0];
  data[1] = thisEui64[1];
  // Avg lux from this sensor
  data[2] = GET_LSB_FROM_16(light_sensor_data.mean_lux);
  data[3] = GET_MSB_FROM_16(light_sensor_data.mean_lux);
  // PWM from this device
  data[4] = read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID);
  data[5] = master_light.pwm_value;
  data[6] = master_light.dim_time_dsec;
  // [Counter | Mode >> Shifted]
  data[7] = (uint8_t)((get_app_flags_nonvol() >> SH_DHS_MODE_INFO) & 0x000FUL);
  data[7] |= ((calibration_counter << 4) & 0xF0);

  length = 8;

  tx_light_sensor_data(data, length);
}

#ifdef LIGHT_SENSOR_CAL_PRINT
static void spew_neighbor_table(void)
{
  for(uint8_t i = 0; i < N_SENSOR_NEIGHBOR_TABLE_SIZE; i++)
    {
       if(light_sensor_neighbor_table[i].macID != 0)
       {
          sl_zigbee_app_debug_println("macID:0x%X  lux_high:%d  lux_low:%d  mode:0x%X",
            light_sensor_neighbor_table[i].macID, light_sensor_neighbor_table[i].lux_high,
            light_sensor_neighbor_table[i].lux_low, light_sensor_neighbor_table[i].mode);
       }
    }
}
#else
static void spew_neighbor_table(void) { }
#endif

// Returns true if variable calibration_counter > arg samples
// NOTE: Max calibration_counter is 15
static bool all_lights_have_n_samples(uint8_t samples)
{
  bool entries = false;

  for(uint8_t i = 0; i < N_SENSOR_NEIGHBOR_TABLE_SIZE; i++)
    {
       if(light_sensor_neighbor_table[i].macID != 0)
       {
           entries = true;
           if(samples > (0x0F & (light_sensor_neighbor_table[i].mode >> 4)))
           {
              entries = false;
              break;
           }
       }
  }
  return(entries);
}

// Check the table to see if this device has enough samples, cause collisions
static bool this_light_has_n_samples(uint8_t samples)
{
  bool entries = false;
  uint16_t this_mac_id = GET_16_FROM_ARRAY(thisEui64, 0);

    for(uint8_t i = 0; i < N_SENSOR_NEIGHBOR_TABLE_SIZE; i++)
    {
       if(light_sensor_neighbor_table[i].macID == this_mac_id)
       {
          entries = true;
          if(samples >= (0x0F & (light_sensor_neighbor_table[i].mode >> 4)))
          {
             return(false);
          }
          break;
       }
    }
    return(entries);
}

// Return stats gathered from all devices in a group.  No need to initialize.
// Assumes sensor_stats = 0
static void get_stats_neighbor_table(light_sensor_stats_t *sensor_stats)
{
  uint8_t n = 0;

  sensor_stats->lux_high_avg = 0;
  sensor_stats->lux_low_avg = 0;
  sensor_stats->lux_median = 0;
  sensor_stats->lux_max = 0;
  sensor_stats->lux_min = 8192;

  for(uint8_t i = 0; i < N_SENSOR_NEIGHBOR_TABLE_SIZE; i++)
  {
     if(light_sensor_neighbor_table[i].macID != 0)
     {
        sensor_stats->lux_high_avg += light_sensor_neighbor_table[i].lux_high;
        sensor_stats->lux_low_avg += light_sensor_neighbor_table[i].lux_low;
        sensor_stats->lux_median += light_sensor_neighbor_table[i].lux_high -
                                    light_sensor_neighbor_table[i].lux_low;

        if(sensor_stats->lux_max < light_sensor_neighbor_table[i].lux_high)
        {
           sensor_stats->lux_max = light_sensor_neighbor_table[i].lux_high;
        }
        if(sensor_stats->lux_min > light_sensor_neighbor_table[i].lux_low)
        {
           sensor_stats->lux_min = light_sensor_neighbor_table[i].lux_low;
        }
        n++;
     }
  }

  if(0 != n)
  {
     sensor_stats->lux_high_avg /= n;
     sensor_stats->lux_low_avg /= n;
     sensor_stats->lux_median /= n;
  }
  else
  {
     return;
  }

     #ifdef DHS_SOURCE_DEVICE
     uint8_t data[16] = {0};
     uint8_t length;
     uint8_t this_index = get_neighbor_table_index(GET_16_FROM_ARRAY(thisEui64, 0));

     // Op-code
     data[0] = DHS_CAL_DATA;   // Daylight harvester
     // Mac id from this device
     data[1] = thisEui64[0];
     data[2] = thisEui64[1];
     // Errors
     data[3] =  abs(sensor_stats->lux_avg - light_sensor_neighbor_table[this_index].lux);
     data[4] =  abs(sensor_stats->lux_median - light_sensor_neighbor_table[this_index].lux);
     // N devices
     data[5] = n;

     length = 6;

     tx_source_debug(data, length);
     #endif

     #ifdef LIGHT_SENSOR_CAL_PRINT
     sl_zigbee_app_debug_println("Stats[%d]",n);
     #endif
}

uint16_t get_light_sensor_mean_lux(void)
{
  return(light_sensor_data.mean_lux);
}

// Called from incoming config cmd
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

  // Good input?
  if(0 != light_sensor_reference)
  {
      sl_zigbee_app_debug_println("\n\rNEW LUX REF %d   <%d>", light_sensor_reference, get_time_ms());

      // Set & clear proper bits
     set_app_flags_nonvol((B_DHS_MASTER_LIGHT | B_DHS_CALIBRATED | B_DISABLE_LIGHT_SENSOR | B_50_PERCENT_MODE), 0);
     set_app_flags_nonvol((B_DISABLE_OCC_SENSOR | B_DHS_CALIBRATING), 1);

     calibration_counter = 0;
     light_sensor_state = dhs_init_cal_state;
     light_sensor_time_stamp = get_time_ms();

     start_reading_light_sensor_for_calibration();  // Speed up read time
  }
  else
  {
     set_app_flags_nonvol(B_DISABLE_LIGHT_SENSOR, 1);
     start_reading_light_sensor();    // Normal speed
  }
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




/***************************************************************************/
/**********                  CALIBRATION             ***********************/
/***************************************************************************/
static void tx_next_cal_light_level_task(taskParm_t parm0, taskParm_t parm1)
{
  if(has_timed_out(parm0.u32, parm1.u32))
  {
     removeTask(tx_next_cal_light_level_task);

     tx_move_to_new_light_level(cal_next_level, 0);

    #ifdef LIGHT_SENSOR_CAL_PRINT
    sl_zigbee_app_debug_println("DHS 0x%X Cal multi-cast level:%d   <%d>",
             GET_16_FROM_ARRAY(thisEui64, 0), cal_next_level, get_time_ms());
    #endif
  }
}
// Set up to randomly multi-cast next cal light level
static void tx_next_cal_light_level(uint8_t level)
{
  taskParm_t arg0, arg1;

  cal_next_level = level;

  arg0.u32 = get_time_ms();
  arg1.u32 = get_random_number(LIGHT_SENSOR_CAL_UPDATE_AVERAGE_MS, 200UL)/100UL;
  addTask(tx_next_cal_light_level_task, arg0, arg1);
}

// Master is picked on delta
static bool am_i_master(void)
{
  uint16_t master_mac = 0;
  uint16_t min_error = 0xFFFF;
  light_sensor_stats_t sensor_stats = {0};
  uint16_t error[N_SENSOR_NEIGHBOR_TABLE_SIZE] = {0};

  get_stats_neighbor_table(&sensor_stats);

  // Generate error for each entry
  for(uint8_t i = 0; i < N_SENSOR_NEIGHBOR_TABLE_SIZE; i++)
  {
     if(light_sensor_neighbor_table[i].macID != 0)
     {
        error[i] = abs(sensor_stats.lux_high_avg - light_sensor_neighbor_table[i].lux_high);
        error[i] += abs(sensor_stats.lux_low_avg - light_sensor_neighbor_table[i].lux_low);
        uint16_t delta = abs(light_sensor_neighbor_table[i].lux_high - light_sensor_neighbor_table[i].lux_low);
        error[i] += abs(sensor_stats.lux_median - delta);

        #ifdef LIGHT_SENSOR_CAL_PRINT
        sl_zigbee_app_debug_println("Am I macID:0x%X  error %d  high:%d  low:%d",
           light_sensor_neighbor_table[i].macID, error[i], light_sensor_neighbor_table[i].lux_high,
           light_sensor_neighbor_table[i].lux_low);
        #endif
      }
  }

  // Find smallest error
  for(uint8_t i = 0; i < N_SENSOR_NEIGHBOR_TABLE_SIZE; i++)
  {
     if(light_sensor_neighbor_table[i].macID != 0)
     {
        if(min_error > error[i])
        {
           min_error = error[i];
           master_mac = light_sensor_neighbor_table[i].macID;
           master_light.gain = light_sensor_neighbor_table[i].lux_high -
                               light_sensor_neighbor_table[i].lux_low;
           master_light.gain /= MAX_ALLOWABLE_LIGHT_LEVEL -
                                MIN_ALLOWABLE_LIGHT_LEVEL;
           if(0 == master_light.gain)
           {
              master_light.gain = 1;
           }
           sl_token_set_data(TOKEN_APP_DHS_GAIN, 0, &master_light.gain, sizeof(master_light.gain));
        }
     }
  }

  // Is it me?
  if(master_mac == GET_16_FROM_ARRAY(thisEui64, 0))
  {
     set_app_flags_nonvol(B_DHS_MASTER_LIGHT, 1);
     sl_zigbee_app_debug_println("\nI AM MASTER 0x%X   <%d>\n", master_mac, get_time_ms());
  }
  else
  {
    set_app_flags_nonvol(B_DHS_MASTER_LIGHT, 0);
    sl_zigbee_app_debug_println("master mac 0x%X   <%d>", master_mac, get_time_ms());
  }
  return(is_app_flags_nonvol_set(B_DHS_MASTER_LIGHT) ? true : false);
}

static void dhs_nop_cal_state(void)
{
  // FLush, resets timers
  suspend_all_sensors(DSECS_3_SECS);

  light_sensor_state = sensor_idle_state;

  // Restore saved user levels and present level
  save_min_light_level(saved_original_min_level);
  save_max_light_level(saved_original_max_level);

  set_app_flags_nonvol(B_DHS_CALIBRATED, 1);
  set_app_flags_nonvol((B_DISABLE_OCC_SENSOR | B_DISABLE_LIGHT_SENSOR | B_DHS_CALIBRATING), 0);

  start_reading_light_sensor();       // Normal period
}

// Restore original max/min levels, timing, gain, etc
// Determine if we da master
static void dhs_done_cal_state(void)
{
  calibration_counter++;
  //if(has_timed_out(light_sensor_time_stamp, 25UL*100UL))  // From xtion below
  if(calibration_counter)
  {
     // FLush, resets timers
     suspend_all_sensors(DSECS_3_SECS);

     light_sensor_state = sensor_idle_state;

     // Restore saved user levels and present level
     save_min_light_level(saved_original_min_level);
     save_max_light_level(saved_original_max_level);

     if(am_i_master())
     {
        dim_lights_up_down(2, 5);
     }

     set_app_flags_nonvol(B_DHS_CALIBRATED, 1);
     set_app_flags_nonvol((B_DISABLE_OCC_SENSOR | B_DISABLE_LIGHT_SENSOR | B_DHS_CALIBRATING), 0);

     start_reading_light_sensor();       // Normal period

     #ifdef LIGHT_SENSOR_CAL_PRINT
     sl_zigbee_app_debug_println("dhs_done_cal_state DONE flags 0x%X   <%d>\n", get_app_flags_nonvol(), get_time_ms());
     #endif
  }
}

// Delay to make sure we got all devices and enough samples
static void dhs_wait_min_cal_state(void)
{
  if(all_lights_have_n_samples(N_SENSOR_CAL_READINGS+2) ||
      (MAX_ALLOWABLE_LIGHT_LEVEL == hal_get_dim_pwm_255()))
  {
     // Got here because; all devices in the group have enough samples and/or
     // a device in the group determined all devices in the group got all the
     // samples and sent out a broadcast to go to max level
     if(MAX_ALLOWABLE_LIGHT_LEVEL != hal_get_dim_pwm_255())
     {
        // Still at min level, but got all valid data. tell group
         tx_next_cal_light_level(MAX_ALLOWABLE_LIGHT_LEVEL);
     }
     else
     {
        // We're already at MAX_ALLOWABLE_LIGHT_LEVEL & enough samples
        //tx_next_cal_light_level(MAX_ALLOWABLE_LIGHT_LEVEL);
     }

     calibration_counter = 0;

     light_sensor_state = dhs_done_cal_state;

     #ifdef LIGHT_SENSOR_CAL_PRINT
     spew_neighbor_table();
     sl_zigbee_app_debug_println("\ndhs_wait_min_cal_state DONE   <%d>", get_time_ms());
     #endif
  }
  else if(!this_light_has_n_samples(N_SENSOR_CAL_READINGS+2))
  {

      // Only broadcast if haven't meet criteria in neighbor table
     light_sensor_data.mean_lux = sum_lux/ 4;
     calibration_counter += (14 > calibration_counter) ? 1 : 0;
     send_dhs_table_info();
  }
  else if(has_timed_out(light_sensor_time_stamp, 30000UL))
  {
     // Before bailing, see if we got the minimum criteria
     if(all_lights_have_n_samples(N_SENSOR_CAL_READINGS))
     {
        // Broadcast with random time
        tx_next_cal_light_level(MAX_ALLOWABLE_LIGHT_LEVEL);

        set_app_flags_nonvol(B_DHS_HIGH_LUX, 0);

        light_sensor_state = dhs_done_cal_state;

        #ifdef LIGHT_SENSOR_CAL_PRINT
        sl_zigbee_app_debug_println("\ndhs_wait_min_cal_state DONE   <%d>", get_time_ms());
        spew_neighbor_table();
        #endif
     }
     else
     {
        spew_neighbor_table();
        light_sensor_state = dhs_nop_cal_state;
        sl_zigbee_app_debug_println("DHS Cal timeout min   <%d>", get_time_ms());
     }
  }
}


// Lights are ramped down to MIN_ALLOWABLE_LIGHT_LEVEL, from previous state,
// sit here till we get N_SENSOR_CAL_READINGS buffer fill/averages
static void dhs_get_min_lux_cal_state(void)
{
  if(MIN_ALLOWABLE_LIGHT_LEVEL != hal_get_dim_pwm_255())
  {
     move_to_level_onoff(MIN_ALLOWABLE_LIGHT_LEVEL, 0);
     return;
  }

  calibration_counter += (14 > calibration_counter) ? 1 : 0;

  if(1 == calibration_counter)
  {
     sum_lux = 0UL;
     return;     // Toss 1st average
  }

  sum_lux += light_sensor_data.mean_lux;

  #ifdef LIGHT_SENSOR_CAL_PRINT
  sl_zigbee_app_debug_println("macID:0x%X sum_min_lux:%d   <%d>\n", GET_16_FROM_ARRAY(thisEui64, 0),
                              sum_lux, get_time_ms());
  #endif

  if(N_SENSOR_CAL_READINGS == calibration_counter)
  {
     // Got 4 averages, average averages
     light_sensor_data.mean_lux = sum_lux/ 4;
     light_sensor_state = dhs_wait_min_cal_state;
     light_sensor_time_stamp = get_time_ms();
     send_dhs_table_info();

     #ifdef LIGHT_SENSOR_CAL_PRINT
     sl_zigbee_app_debug_println("macID:0x%X  minAvg lux:%d   <%d>\n", GET_16_FROM_ARRAY(thisEui64, 0),
          light_sensor_data.mean_lux, get_time_ms());
     #endif
  }
}

// Wait and make sure we got all lights in our group and each
// light has enough samples.
static void dhs_wait_max_cal_state(void)
{
  if(all_lights_have_n_samples(N_SENSOR_CAL_READINGS+2) ||
      (MIN_ALLOWABLE_LIGHT_LEVEL == hal_get_dim_pwm_255()))
  {
     // Got here because; all devices in the group have enough samples and/or
     // a device in the group determined all devices in the group got all the
     // samples and sent out a broadcast to go to min level
     if(MIN_ALLOWABLE_LIGHT_LEVEL != hal_get_dim_pwm_255())
     {
        // Still at max level, but got all valid data. tell group
        tx_next_cal_light_level(MIN_ALLOWABLE_LIGHT_LEVEL);
     }
     else
     {
        // We're already at MIN_ALLOWABLE_LIGHT_LEVEL & enough samples
        //tx_next_cal_light_level(MIN_ALLOWABLE_LIGHT_LEVEL);
     }

     set_app_flags_nonvol(B_DHS_HIGH_LUX, 0);

     calibration_counter = 0;
     light_sensor_state = dhs_get_min_lux_cal_state;

     #ifdef LIGHT_SENSOR_CAL_PRINT
     spew_neighbor_table();
     sl_zigbee_app_debug_println("\ndhs_wait_max_cal_state DONE   <%d>", get_time_ms());
     #endif
  }
  else if(!this_light_has_n_samples(N_SENSOR_CAL_READINGS+2))
  {
     // Only broadcast if haven't meet criteria in neighbor table
     light_sensor_data.mean_lux = sum_lux/ 4;
     send_dhs_table_info();
     calibration_counter += (14 > calibration_counter) ? 1 : 0;
  }
  else if(has_timed_out(light_sensor_time_stamp, 30000UL))
  {
     // Before bailing, see if we got the minimum criteria
     if(all_lights_have_n_samples(N_SENSOR_CAL_READINGS))
     {
        // Broadcast with random time
        tx_next_cal_light_level(MIN_ALLOWABLE_LIGHT_LEVEL);

        set_app_flags_nonvol(B_DHS_HIGH_LUX, 0);

        calibration_counter = 0;
        light_sensor_state = dhs_get_min_lux_cal_state;

        #ifdef LIGHT_SENSOR_CAL_PRINT
        spew_neighbor_table();
        sl_zigbee_app_debug_println("\ndhs_wait_max_cal_state timed out but DONE   <%d>", get_time_ms());
        #endif
     }
     else
     {
        light_sensor_state = dhs_nop_cal_state;
        sl_zigbee_app_debug_println("DHS Cal timeout max   <%d>", get_time_ms());
        spew_neighbor_table();
     }
  }
}

// Lights are ramped up to max, from previous state, sit here
// till we get N_SENSOR_CAL_READINGS and then generate average
static void dhs_get_max_lux_cal_state(void)
{
  if(MAX_ALLOWABLE_LIGHT_LEVEL != hal_get_dim_pwm_255())
  {
      // This light
      move_to_level_onoff(MAX_ALLOWABLE_LIGHT_LEVEL, 0);
      return;
  }

  calibration_counter += (14 > calibration_counter) ? 1 : 0;

  if(1 == calibration_counter)
  {
     sum_lux = 0UL;
     return;     // Toss 1st average
  }

  sum_lux += light_sensor_data.mean_lux;

  #ifdef LIGHT_SENSOR_CAL_PRINT
  sl_zigbee_app_debug_println("macID:0x%X sum_max_lux:%d   <%d>\n", GET_16_FROM_ARRAY(thisEui64, 0),
                              sum_lux, get_time_ms());
  #endif

  if(N_SENSOR_CAL_READINGS == calibration_counter)
  {
     // Got 4 averages, average averages
     light_sensor_data.mean_lux = (uint16_t)(sum_lux/4);
     light_sensor_state = dhs_wait_max_cal_state;
     light_sensor_time_stamp = get_time_ms();
     send_dhs_table_info();

     #ifdef LIGHT_SENSOR_CAL_PRINT
     sl_zigbee_app_debug_println("macID:0x%X  maxAvg lux:%d   <%d>\n", GET_16_FROM_ARRAY(thisEui64, 0),
          light_sensor_data.mean_lux, get_time_ms());
     sl_zigbee_app_debug_println("dhs_get_max_lux_cal_state DONE   <%d>\n", light_sensor_time_stamp);
     #endif
  }
}

// Once lights are done flashing, to indicate we got the new lux level,
// Save present users min/max light levels, write new cal min/max
// Writing to non-vol min/max level causes lights to go to max
static void dhs_init_cal_state(void)
{
  if(has_timed_out(light_sensor_time_stamp, 2*LIGHT_SENSOR_CAL_UPDATE_AVERAGE_MS))
  {
     // Lights are flashing to show we got new lux level, so delay
     if(!are_lights_dimming_up_dowm())
     {
        // For when we're done, replace original min/max levels
        saved_original_max_level = get_max_light_level();
        saved_original_min_level = get_min_light_level();

        // Set new max/min, this will cause lights to got to max
        save_min_light_level(MIN_ALLOWABLE_LIGHT_LEVEL);
        save_max_light_level(MAX_ALLOWABLE_LIGHT_LEVEL);

        calibration_counter = 0;

        light_sensor_time_stamp = get_time_ms();
        light_sensor_state = dhs_get_max_lux_cal_state;

        move_to_level_onoff(MAX_ALLOWABLE_LIGHT_LEVEL, 0);
        set_app_flags_nonvol(B_DHS_HIGH_LUX, 1);

        memset(&light_sensor_data, 0, sizeof(light_sensor_data));
        memset(&light_sensor_neighbor_table, 0, sizeof(light_sensor_neighbor_table));

       #ifdef LIGHT_SENSOR_CAL_PRINT
        sl_zigbee_app_debug_println("dhs_init_cal_state DONE   <%d>\n", light_sensor_time_stamp);
       #endif
     }
  }
}

static int16_t saturate_pwm_level(int16_t pwm_value)
{
  uint8_t this_max_level = get_max_light_level();
  int16_t valid_pwm_value = (int16_t)this_max_level;

  if(is_app_flags_nonvol_set(B_50_PERCENT_MODE))
  {
     // We are master AND in 50% mode, get_max_light_level() which is set to 1/2
     // of original, so get what it was
     sl_token_get_data(TOKEN_APP_LIGHT_LEVEL_MAX_STASHED, 0, &this_max_level, sizeof(this_max_level));
  }

  // Saturate between max and min!!  Output is master_light.pwm_value
  if(get_min_light_level() > pwm_value)
  {
      valid_pwm_value = (int16_t)get_min_light_level();
  }
  else if((int16_t)this_max_level < pwm_value)
  {
      valid_pwm_value = (int16_t)this_max_level;
  }
  else
  {
      valid_pwm_value = pwm_value;
  }
  return(valid_pwm_value);
}

// Used for over the air debug, assumes periodic calling
void spew_on_dhs_changes(void)
{
  static uint8_t last_pwm = 0;

  bool something_changed = false;

  if(light_sensor_data.last_mean_lux != light_sensor_data.mean_lux)
  {
      something_changed = true;
  }
  if(last_pwm != read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID))
  {
      something_changed = true;
      last_pwm = read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID);
  }
  if(!something_changed)
  {
     sl_zigbee_app_debug_println("[%d] %d/%d %d/%d",something_changed, last_pwm,
     read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID),
     light_sensor_data.mean_lux, light_sensor_data.last_mean_lux);
     return;
  }

  uint8_t data[12] = {0};
  uint8_t length;

  // Op-code
  data[0] = DHS_DATA; // Daylight harvester
  // Mac id from this device
  data[1] = thisEui64[0];
  data[2] = thisEui64[1];
  // Avg lux from this sensor
  data[3] = GET_LSB_FROM_16(light_sensor_data.mean_lux);
  data[4] = GET_MSB_FROM_16(light_sensor_data.mean_lux);
  // PWM from this device
  data[5] = read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID);
  // [Counter | Mode >> Shifted]
  data[6] = (uint8_t)((get_app_flags_nonvol() >> SH_DHS_MODE_INFO) & 0x000FUL);
  data[6] |= ((calibration_counter << 4) & 0xF0);

  length = 7;

  tx_source_debug(data, length);
}

