#include "af.h"
// SDK 2025.6.3: hal.h no longer pulls in the legacy token API, which
// provides the TOKEN_APP_* ids and types from sl_custom_token_header.h.
#include "sl_token_api.h"
#include "RehTOS.h"
#include "factory_reset.h"
#include "hal_leds.h"
#include "hal_utils.h"
#include "em_chip.h"
#include "hal_pwm.h"
#include "sl_event_handler.h"
#include "light_on_off.h"
#include "sl_hal_wdog.h"
#include "sl_token_manager.h"
#include "occ_sensor.h"

#include "basic.h"
#include "attribute-storage.h"
#include "nwk_utils.h"
#include "scenes.h"

#include "sl_zigbee_debug_print.h"

static bool disable_petting_wdog = false;
static sl_zigbee_af_event_t factory_reset_event;
static uint8_t factory_reset_state = 0;
static reset_reason_t factory_reset_reason = eRESET_NORMAL;
static bool factory_reset_cmd = false;
#ifdef TC_LINK_KEY_FIX
static uint8_t tc_key_counter = 0;
#endif

sl_zigbee_af_zcl_request_status_t sl_zigbee_af_groups_cluster_remove_all_groups_cb(void);
//sl_zigbee_af_zcl_request_status_t sl_zigbee_af_groups_cluster_remove_all_groups_local(uint8_t ep);

// All reporting, bindings, attributes gets cleared/default when stack goes down
static void factory_reset_handler(sl_zigbee_af_event_t *event)
{
  sl_zigbee_af_event_set_delay_ms(&factory_reset_event, 500UL);
  sl_zigbee_network_status_t nwk_status = sl_zigbee_af_network_state();

  switch(factory_reset_state)
  {
     case 0:
        hal_set_red_led(1);
        // Stay in this state till not on a nwk
        if(SL_ZIGBEE_JOINED_NETWORK == nwk_status)
        {
           sl_status_t status = sl_zigbee_leave_network(SL_ZIGBEE_LEAVE_NWK_WITH_NO_OPTION);
           if(SL_STATUS_OK == status)
           {
              sl_zigbee_app_debug_println("   Leaving nwk in progress  <%d>", get_time_ms());
           }
           else
           {
              sl_zigbee_app_debug_println("   Waiting to leaving nwk 0x%04X   <%d>",
                                                        status, get_time_ms());
           }
        }
        else if(SL_ZIGBEE_NO_NETWORK == nwk_status)
        {
           sl_zigbee_app_debug_println("   SL_ZIGBEE_NO_NETWORK   <%d>", get_time_ms());
           factory_reset_state++;
        }
        else
        {
            sl_zigbee_app_debug_println("   Waiting for nwk down  0x%X   <%d>",
                                                    nwk_status, get_time_ms());
        }
        break;

     case 1:
        if(SL_STATUS_OK == sl_zigbee_af_clear_report_table_cb())
        {
           sl_zigbee_app_debug_println("   Reporting table cleared  <%d>", get_time_ms());
           factory_reset_state++;
        }
        else
        {
           sl_zigbee_app_debug_println("   Waiting for reporting table to clear   <%d>",
                                                                         get_time_ms());
        }
        break;

     case 2:
        for(uint8_t ep = 1; ep <= sl_zigbee_af_endpoint_count(); ep++)
        {
           #if 1
           sl_zigbee_af_groups_cluster_clear_group_table_cb(ep);
           sl_zigbee_app_debug_println("   Groups ep:%d cleared  <%d>",
                                                                     ep, get_time_ms());
           #else
           if(SL_ZIGBEE_ZCL_STATUS_SUCCESS == sl_zigbee_af_groups_cluster_remove_all_groups_local(ep))
           {
              sl_zigbee_app_debug_println("   Groups ep:%d cleared  <%d>",
                                                          ep, get_time_ms());
           }
           else
           {
              sl_zigbee_app_debug_println("   Groups ep:%d FAILED  <%d>",
                                                          ep, get_time_ms());
           }
           #endif
        }

        factory_reset_state++;
        break;

     // TODO: Remove Scenes

     case 3:
        if(SL_ZIGBEE_ZCL_STATUS_SUCCESS == sl_zigbee_clear_binding_table())
        {
           sl_zigbee_app_debug_println("   Binding table cleared  <%d>", get_time_ms());
           factory_reset_state++;
        }
        else
        {
           sl_zigbee_app_debug_println("   Waiting for bindings to clear   <%d>",
                                                                  get_time_ms());
        }
        break;

     case 4:
        for(uint8_t i = 0; i < SL_ZIGBEE_AF_PLUGIN_SCENES_TABLE_SIZE; i++)
        {
           sl_zigbee_af_scene_table_entry_t entry;
           sl_zigbee_af_scenes_server_retrieve_scene_entry(entry, i);
           if(entry.endpoint == LIGHT_ENDPOINT)
           {
              entry.endpoint = SL_ZIGBEE_AF_SCENE_TABLE_UNUSED_ENDPOINT_ID;
              sl_zigbee_af_scenes_server_save_scene_entry(entry, i);
              sl_zigbee_af_scenes_server_decr_num_scene_entries_in_use();
           }
        }
        sl_zigbee_app_debug_println("   All scenes are cleared   <%d>", get_time_ms());
        factory_reset_state++;
        break;

     case 5:
        if(nwk_status != SL_ZIGBEE_JOINED_NETWORK)
        {
           // Clear all tokens, optional Commissioning and APP_FLAGS
           set_light_configurations_to_default(factory_reset_reason);

           uint8_t data = (uint8_t)eNO_NWK;
           sl_token_set_data(TOKEN_APP_WHICH_NETWORK, 0, &data, 1);

           if(eRESET_NORMAL == factory_reset_reason)
           {
              set_app_flags_nonvol((B_DHS_MASTER_LIGHT | B_DHS_CALIBRATING | B_DHS_CALIBRATED | B_EM_MODE |
                                    B_OCC_XXL_DELAY_ENABLED | B_MANUAL_ADD_MODE | B_DISABLE_LIGHT_SENSOR |
                                    B_DISABLE_OCC_SENSOR | B_MANUAL_ON_MODE | B_50_PERCENT_MODE), 0);
              sl_zigbee_app_debug_println("   ALL tokens set to defaults   <%d>", get_time_ms());
           }
           else
           {
              sl_zigbee_app_debug_println("   ALL tokens cleared EXCEPT Commissioning  <%d>", get_time_ms());
           }
           factory_reset_state++;

           #ifdef TC_LINK_KEY_FIX
           tc_key_counter = 0;
           #endif
        }
           break;

     case 6:
        sl_zigbee_af_basic_reset_to_factory_defaults_cb(LIGHT_ENDPOINT);
        sl_zigbee_af_basic_reset_to_factory_defaults_cb(OCC_SENSOR_ENDPOINT);
        sl_zigbee_af_basic_reset_to_factory_defaults_cb(LIGHT_SENSOR_ENDPOINT);
        sl_zigbee_app_debug_println("   Set all attributes to defaults   <%d>", get_time_ms());
        factory_reset_state++;
        break;

     case 7:
        sl_zigbee_app_debug_println("   Set light to off   <%d>", get_time_ms());
        move_to_level_onoff(12, 0);   // 5%
        factory_reset_state++;
        break;

     case 8:
        #if 0   //
        move_to_level_onoff((uint8_t)read_light_level_attribute(ZCL_ON_LEVEL_ATTRIBUTE_ID), 0);
        sl_zigbee_app_debug_println("   Set light on to default level   <%d>", get_time_ms());
        #endif
        factory_reset_state++;
        break;

     case 9:
        //JGR if(factory_reset_cmd)
        if(true)
        {
           // Reset chip only from factory reset command
           sl_zigbee_app_debug_println("Initiate SW reset   <%d>", get_time_ms());
           CHIP_Reset();
        }
        factory_reset_state++;
        hal_set_red_led(0);
        break;

     default:
        hal_set_red_led(0);
        factory_reset_state = 0;
        factory_reset_cmd = false;
        sl_zigbee_af_event_set_inactive(&factory_reset_event);
        sl_zigbee_app_debug_println("   DONE   <%d>\n\n", get_time_ms());
        break;
  }
}

void force_factory_reset(uint32_t delay_ms, reset_reason_t reason)
{
  if(sl_zigbee_af_event_is_scheduled(&factory_reset_event))
  {
     return;
  }
  sl_zigbee_app_debug_println("\nFACTORY RESET in %dmS  reason: %d ", delay_ms, reason);

  factory_reset_state = 0;
  factory_reset_reason = reason;
  factory_reset_cmd = true;
  sl_zigbee_af_event_init(&factory_reset_event, factory_reset_handler);
  sl_zigbee_af_event_set_delay_ms(&factory_reset_event, delay_ms);
}

void nwk_down_clean_up(void)
{
  if(!factory_reset_cmd)
  {
     sl_zigbee_app_debug_println("\nClean up NWK DOWN");

     force_factory_reset(100UL, eRESET_NORMAL);
     //factory_reset_state = 0;
     //sl_zigbee_af_event_init(&factory_reset_event, factory_reset_handler);
     //sl_zigbee_af_event_set_active(&factory_reset_event);
  }
}


// Set all tokens to defaults. May optionally leave the same are:
// TOKEN_APP_ADAPTIV_COMMISH_INFO, TOKEN_APP_FLAGS
// This is called when just joined a nwk or factory reset, ie. input joined
void set_light_configurations_to_default(reset_reason_t default_reason)
{
  adaptiv_commish_info_t adaptiv_commish_info;
  uint8_t data[4] = {0};

  memset(&adaptiv_commish_info, 0, sizeof(adaptiv_commish_info_t));
  if(eRESET_NORMAL == default_reason)
  {
     sl_token_set_data(TOKEN_APP_ADAPTIV_COMMISH_INFO, 0,
                                       &adaptiv_commish_info,
                                       sizeof(adaptiv_commish_info_t));

     sl_token_set_data(TOKEN_APP_FLAGS, 0, &data, 4);
  }

  sl_token_set_data(TOKEN_APP_OTA_DOWNLOAD_IN_PROGRESS, 0, &data, 1);
  sl_token_set_data(TOKEN_APP_ON_OFF_CYCLES, 0, &data, 1);

  data[0] = GET_LSB_FROM_16(DEFAULT_LIGHT_TEMPERATURE_K);
  data[1] = GET_MSB_FROM_16(DEFAULT_LIGHT_TEMPERATURE_K);

  data[2] = GET_LSB_FROM_16(DEFAULT_SENSOR_LUX_TARGET);
  data[3] = GET_MSB_FROM_16(DEFAULT_SENSOR_LUX_TARGET);
  sl_token_set_data(TOKEN_APP_COLOR_TEMP_K, 0, &data, 2);
  sl_token_set_data(TOKEN_APP_LIGHT_SENSOR_LUX, 0, &data[2], 2);

  // Change color now, in case from from new join or por.
  save_light_color(DEFAULT_LIGHT_TEMPERATURE_K);

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
  write_occupancy_sensor_attribute(ZCL_PIR_OCCUPIED_TO_UNOCCUPIED_DELAY_ATTRIBUTE_ID, (uint8_t *)&data[0]);
  save_adaptiv_unoccupied_delay(DEFAULT_OCC_SENSOR_DELAY);

  data[0] = DEFAULT_OCC_SENSOR_DELAY_TO_OCC;
  data[1] = 0;
  write_occupancy_sensor_attribute(ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_DELAY_ATTRIBUTE_ID, (uint8_t *)&data);

  data[0] = GET_LSB_FROM_16(DEFAULT_OCC_SENSOR_THRESHOLD);
  data[1] = GET_MSB_FROM_16(DEFAULT_OCC_SENSOR_THRESHOLD);
  sl_token_set_data(TOKEN_APP_OCC_THRESHOLD, 0, &data[0], 2);

  // Place defaults into local variables
  init_light_parameters();

  // Change level now, in case from from new join, not por.
  move_to_level_onoff(DEFAULT_MAX_LIGHT_LEVEL, 0);

  sl_zigbee_app_debug_println("Light configurations set to DEFAULTS   <%d>", get_time_ms());
 }

#ifdef WDOG0
// Input clock is 38.4MHz/1024 or 26.6uS tick
// 256K div ~ 7 secs
// 64K div ~ 1.7 secs
// 32K div ~ 878mS
void hal_watchdog_init(bool force_reset)
{
  // Initialize WDOG with default configuration
  sl_hal_wdog_init_t wd_init =
  {
     .clear_source = false,     /* The clear bit will clear the WDOG counter. */
     .debug_run = false,
     .em2_run = false,
     .em3_run = false,
     .em4_block = true,
     .lock = false,
     .period_select = SL_WDOG_PERIOD_256k,
     .prs0_missing_reset_enable = false,
     .prs1_missing_reset_enable = false,
     .reset_disable = false,
     .warning_time_select = SL_WDOG_ILLEGAL_WINDOW_DISABLE,
     .window_time_select = SL_WDOG_ILLEGAL_WINDOW_DISABLE,
  };

  sl_hal_wdog_disable(WDOG0);

  if(force_reset)
  {
     wd_init.period_select = SL_WDOG_PERIOD_64k;
     disable_petting_wdog = true;
  }

  // Initialize WDOG with our configuration
  sl_hal_wdog_init(WDOG0, &wd_init);

  // Enable WDOG - this starts the watchdog timer
  sl_hal_wdog_enable(WDOG0);

  if(sl_hal_wdog_is_enabled(WDOG0))
  {
     // Feed the watchdog initially
     sl_hal_wdog_feed(WDOG0);

     // Wait for feed operation to complete
     sl_hal_wdog_wait_sync(WDOG0);
  }
}
#else
void hal_watchdog_init(void) {}
#endif

void pet_the_dog(void)
{
  if(!disable_petting_wdog)
  {
     sl_hal_wdog_feed(WDOG0);
  }
}

// These are for the cycling of power 5 times
uint8_t inc_on_off_counter(void)
{
  uint8_t data = 0;
  sl_token_get_data(TOKEN_APP_ON_OFF_CYCLES, 0, &data, 1);
  data++;
  sl_token_set_data(TOKEN_APP_ON_OFF_CYCLES, 0, &data, 1);
  //sl_zigbee_app_debug_println("On/Off counter %d   <%d>", data, get_time_ms());
  return(data);
}
void reset_on_off_counter(void)
{
  uint8_t data = 0;
  sl_token_set_data(TOKEN_APP_ON_OFF_CYCLES, 0, &data, 1);
  //sl_zigbee_app_debug_println("On/Off counter 0   <%d>", get_time_ms());
}
uint8_t get_on_off_counter(void)
{
  uint8_t data = 0;
  sl_token_get_data(TOKEN_APP_ON_OFF_CYCLES, 0, &data, 1);
  return(data);
}

// Increment reset counter in Diagnostic Cluster
void inc_reset_counter(void)
{
  sl_status_t status;
  uint8_t data[2] = {0x00};

  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS !=sl_zigbee_af_read_server_attribute(
                                        LIGHT_ENDPOINT,
                                        ZCL_DIAGNOSTICS_CLUSTER_ID,
                                        0x0000,
                                        (uint8_t *)&data,
                                        sizeof(data)))
  {
     sl_zigbee_app_debug_println("ERR:inc_reset_counter");
  }

  data[0] += 1;
  data[1] += (0 == data[0]) ? 1 : 0; // Rollover inc msb
  //data[0] = 0x00; data[1] = 0x00;   // NOTE: Reset the reset_counter

  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != sl_zigbee_af_write_server_attribute(
                                               LIGHT_ENDPOINT,
                                               ZCL_DIAGNOSTICS_CLUSTER_ID,
                                               0x0000,
                                               (uint8_t *)&data,
                                               ZCL_INT16U_ATTRIBUTE_TYPE))
  {
     sl_zigbee_app_debug_println("Status 0x%X inc_reset_counter",status);
  }
}

void sl_zigbee_af_basic_reset_to_factory_defaults_cb(uint8_t endpoint)
{
  for(uint8_t ep = 1; ep < sl_zigbee_af_endpoint_count(); ep++)
  {
     sl_zigbee_af_reset_attributes(ep);
  }
}

// Read f/w ver from this code download and write the attribute
void write_firmware_version(void)
{
  char data[32] = {0};

  data[0] = strlen(APP_FIRMWARE_VERSION);
  strcpy(&data[1], APP_FIRMWARE_VERSION);

  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != sl_zigbee_af_write_server_attribute(
                                         LIGHT_ENDPOINT,
                                         ZCL_BASIC_CLUSTER_ID,
                                         ZCL_SW_BUILD_ID_ATTRIBUTE_ID,
                                         (uint8_t *)&data,
                                         ZCL_CHAR_STRING_ATTRIBUTE_TYPE))
  {
     sl_zigbee_app_debug_println("ERR:write_firmware_version");
  }
}

void write_date_version(void)
{
  char data[32] = {0};

  data[0] = strlen(APP_DATE_VERSION);
  strcpy(&data[1], APP_DATE_VERSION);

  if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != sl_zigbee_af_write_server_attribute(
                                      LIGHT_ENDPOINT,
                                      ZCL_BASIC_CLUSTER_ID,
                                      ZCL_DATE_CODE_ATTRIBUTE_ID,
                                      (uint8_t *)&data,
                                      ZCL_CHAR_STRING_ATTRIBUTE_TYPE))
  {
     sl_zigbee_app_debug_println("ERR:write_date_version");
  }
}

#ifdef TC_LINK_KEY_FIX
uint8_t get_tc_key_counter(void)
{
  tc_key_counter++;
  return(tc_key_counter);
}
#endif
