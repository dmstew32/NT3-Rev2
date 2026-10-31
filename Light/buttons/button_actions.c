#include <stdlib.h>
#include "app/framework/include/af.h"
#include "RehTOS.h"
#include "hal_utils.h"
#include "hal_leds.h"
#include "hal_occ_sensor.h"
#include "occ_sensor.h"
#include "hal_buttons.h"
#include "hal_pwm.h"
#include "factory_reset.h"
#include "light_on_off.h"
#include "nwk_utils.h"
#include "nwk_multicast.h"
#include "nwk_groups.h"
#include "adaptiv_config.h"
#include "auto_comm.h"
#include "app_button.h"
#include "button_actions.h"
#include "fw_version_display.h"

#include "sl_zigbee_debug_print.h"

// Both buttons pressed selection
#define N_BUT0_PRESSES_FOR_RESET      (10)
#define N_BUT1_PRESSES_FOR_RESET      (10)

#define N_BUT0_PRESSES_FOR_50_PERCENT (2)
#define N_BUT1_PRESSES_FOR_50_PERCENT (2)

#define N_BUT0_PRESSES_FOR_EM_MODE    (4)
#define N_BUT1_PRESSES_FOR_EM_MODE    (4)

#define N_BUT0_PRESSES_BX_MANUAL_ADD  (6)
#define N_BUT1_PRESSES_BX_MANUAL_ADD  (6)

#define N_BUT0_PRESSES_FW_VERSION     (8)
#define N_BUT1_PRESSES_FW_VERSION     (8)

// Single button selections
#define MAX_BUT1_PRESSES_FOR_OCC_DELAY  (12)
#define MIN_BUT1_PRESSES_FOR_OCC_DELAY  (1)
#define MAX_BUT0_PRESSES_FOR_OCC_DELAY  (0)

#define MAX_BUT0_PRESSES_FOR_DIM_VALUES (6)
#define MIN_BUT0_PRESSES_FOR_DIM_VALUES (1)
#define MAX_BUT1_PRESSES_FOR_DIM_VALUES (0)

#define LIGHT_LEVEL_100_PERCENT    (255)
#define LIGHT_LEVEL_80_PERCENT     ((80*255)/100)
#define LIGHT_LEVEL_60_PERCENT     ((60*255)/100)
#define LIGHT_LEVEL_40_PERCENT     ((40*255)/100)
#define LIGHT_LEVEL_20_PERCENT     ((20*255)/100)
#define LIGHT_LEVEL_10_PERCENT     ((10*255)/100)

// Holds
#define HOLD_TIME_MS               (5000UL-1000UL)  // all holds start after this
#define COLOR_CYCLE_TIME_MS        (2500UL)         // next color while T is held
#define COLOR_STEP_K               (500)
#define COLOR_MIN_K                (3000)
#define COLOR_MAX_K                (5000)

// Blinks: show # of presses on success, one long blink on failure
#define show_presses(n_)   hal_start_flashing_red_led((uint32_t)(n_), 250UL, 250UL)
#define show_fail()        hal_start_flashing_red_led(1UL, 1500UL, 50UL)


/**********************  OCC SENSOR DELAYS  (D presses)  *******************/
typedef struct
{
  uint8_t presses;
  bool threshold_only;          // true: only sets the occupancy threshold
  uint16_t unoccupied_delay;    // seconds, 0 = occ sensor off
  uint16_t delay_to_off;        // seconds
  uint16_t threshold;
} occ_setting_t;

static const occ_setting_t m_occ_delays[] =
{
  { 1,  false, 10*60,                        5*60, 0 },
  { 2,  false, 20*60,                        5*60, 0 },
  { 3,  false, 30*60,                        5*60, 0 },
  { 4,  false, 45*60,                        5*60, 0 },
  { 5,  false, 60*60,                        5*60, 0 },
  { 8,  false, 30,                           30,   0 },
  { 9,  false, 0,                            31,   0 },  // 0 unoccupied_delay = occ sensor off
  { 10, false, DEFAULT_OCC_SENSOR_XXL_DELAY, 5*60, 0 },
  /*** NOTE: 11 - 12 sets occupancy sensors threshold ONLY ***/
  { 11, true,  0, 0, DEFAULT_OCC_SENSOR_THRESHOLD },
  { 12, true,  0, 0, DEFAULT_OCC_SENSOR_THRESHOLD_LOW },
};

/**********************  MAX-MIN LIGHT LEVELS  (T presses)  *******************/
typedef struct
{
  uint8_t presses;
  uint8_t max_level;
  uint8_t mid_level;
  uint8_t min_level;
} light_levels_t;

static const light_levels_t m_light_levels[] =
{
  { 1, LIGHT_LEVEL_100_PERCENT, LIGHT_LEVEL_20_PERCENT, 0 },
  { 2, LIGHT_LEVEL_80_PERCENT,  LIGHT_LEVEL_20_PERCENT, 0 },
  { 3, LIGHT_LEVEL_60_PERCENT,  LIGHT_LEVEL_20_PERCENT, 0 },
  { 4, LIGHT_LEVEL_40_PERCENT,  LIGHT_LEVEL_20_PERCENT, 0 },
  { 5, LIGHT_LEVEL_20_PERCENT,  LIGHT_LEVEL_10_PERCENT, 0 },
  { 6, LIGHT_LEVEL_10_PERCENT,  LIGHT_LEVEL_10_PERCENT, 0 },
};


static bool are_buttons_allowed(void)
{
  // NOTE: Color can be changed via the Twitch only!
  if(eDAINTREE_NWK == get_present_network())
  {
     return(false);
  }
  else if((eADAPTIV_NWK == get_present_network()) && !device_is_in_a_group())
  {
     return(false);
  }
  else if((eADAPTIV_NWK == get_present_network()) && device_is_in_a_group())
  {
     return(true);
  }
  else if(eNO_NWK == get_present_network())
  {
     return(true);
  }
  else if(eSTAND_ALONE == get_present_network())
  {
     return(true);
  }
  else
  {
     // Don't know what we are
     return(false);
  }
}

static bool allow_color_change(void)
{
  return(device_is_in_a_group() || (eNO_NWK == get_present_network()));
}

static bool adaptiv_not_in_group(void)
{
  return((eADAPTIV_NWK == get_present_network()) && !device_is_in_a_group());
}

static void set_occ_delays(uint8_t presses)
{
  const occ_setting_t *p_setting = NULL;

  for(uint8_t i = 0; i < (sizeof(m_occ_delays) / sizeof(m_occ_delays[0])); i++)
  {
     if(presses == m_occ_delays[i].presses)
     {
        p_setting = &m_occ_delays[i];
        break;
     }
  }

  if(NULL == p_setting)
  {
     sl_zigbee_core_debug_println("Occ button press %d", presses);
     show_fail();
     return;
  }

  if(p_setting->threshold_only)
  {
     set_occupancy_threshold(p_setting->threshold);
  }
  else
  {
     save_adaptiv_unoccupied_delay(p_setting->unoccupied_delay);
     save_adaptiv_unoccupied_delay_to_off(p_setting->delay_to_off);
  }

  // Send to every one in the group
  tx_occ_sensor_new_delays(get_unoccupied_delay(), get_unoccupied_delay_to_off());
  show_presses(presses);
}

static void set_light_levels(uint8_t presses_t, uint8_t presses_d)
{
  const light_levels_t *p_levels = NULL;

  for(uint8_t i = 0; i < (sizeof(m_light_levels) / sizeof(m_light_levels[0])); i++)
  {
     if(presses_t == m_light_levels[i].presses)
     {
        p_levels = &m_light_levels[i];
        break;
     }
  }

  if(NULL == p_levels)
  {
     // Show fail
     show_fail();
     sl_zigbee_app_debug_println("Button FAIL: [0]:%d [1]:%d   <%d>", presses_t,
                                 presses_d, get_time_ms());
     return;
  }

  save_max_light_level(p_levels->max_level);
  save_mid_light_level(p_levels->mid_level);
  save_min_light_level(p_levels->min_level);

  // Send to every one in the group
  tx_new_light_levels();
  // Show # of presses
  show_presses(presses_t);
}

// A press sequence finished: decide what the presses mean.
static void on_button_sequence(uint8_t presses_t, uint8_t presses_d)
{
  /***********************  FACTORY RESET  *********************/
  if((abs(N_BUT0_PRESSES_FOR_RESET - presses_t) <= 1) && (abs(N_BUT1_PRESSES_FOR_RESET - presses_d) <= 1))
  {
     // Blow our brains out
     force_factory_reset(250UL, eRESET_NORMAL);
  }
  /***********************     50% MODE    *********************/
  else if((N_BUT0_PRESSES_FOR_50_PERCENT == presses_t) && (N_BUT1_PRESSES_FOR_50_PERCENT == presses_d))
  {
     show_presses(presses_d);
     toggle_50_percent_mode();
  }
  /***********************      EM MODE    *********************/
  else if((N_BUT0_PRESSES_FOR_EM_MODE == presses_t) && (N_BUT1_PRESSES_FOR_EM_MODE == presses_d))
  {
     show_presses(presses_d);
     toggle_em_mode();
  }
  /***********************   DO TWITCH's JOB  *********************/
  else if((N_BUT0_PRESSES_BX_MANUAL_ADD == presses_t) && (N_BUT1_PRESSES_BX_MANUAL_ADD == presses_d))
  {
     if(adaptiv_not_in_group())
     {
        show_presses(presses_d);
        start_local_auto_commission();
     }
     else
     {
        show_fail();
     }
  }
  #ifdef ALLOW_DISPLAY_VERSION
  /***********************  SHOW FW VERSION  *********************/
  else if((N_BUT0_PRESSES_FW_VERSION == presses_t) && (N_BUT1_PRESSES_FW_VERSION == presses_d))
  {
     start_display_fw_version();
  }
  #endif
  /**********************  YOU SHALL NOT PASS  *******************/
  else if(!are_buttons_allowed())
  {
     // Show fail
     show_fail();
  }
  /**********************  OCC SENSOR DELAYS *******************/
  else if((MAX_BUT0_PRESSES_FOR_OCC_DELAY == presses_t) &&   // BUTTON 0 also known as T
          (MIN_BUT1_PRESSES_FOR_OCC_DELAY <= presses_d) &&   // BUTTON 1 also known as D
          (MAX_BUT1_PRESSES_FOR_OCC_DELAY >= presses_d))
  {
     set_occ_delays(presses_d);
  }
  /**********************  MAX-MIN LIGHT LEVELS  *******************/
  else if((MAX_BUT1_PRESSES_FOR_DIM_VALUES == presses_d) &&  // BUTTON 1 also known as D
          (MIN_BUT0_PRESSES_FOR_DIM_VALUES <= presses_t) &&  // BUTTON 0 also known as T
          (MAX_BUT0_PRESSES_FOR_DIM_VALUES >= presses_t))
  {
     // This handles 1-6 dim levels, see m_light_levels[]
     set_light_levels(presses_t, presses_d);
  }
  else
  {
     // Error no valid presses
     show_fail();
  }
}

/***                  COLOR                   ***/
/***     BUTTON 0,  Also known as T button    ***/
/*** Hold 4 seconds then rolls through colors ***/
static void on_hold_color(app_button_t gesture)
{
  (void)gesture;

  // Button held long enough, start changing light color
  uint16_t new_color = get_light_color() + COLOR_STEP_K;
  if(COLOR_MAX_K < new_color)
  {
     new_color = COLOR_MIN_K;
  }

  if(allow_color_change())
  {
     save_light_color(new_color);
     tx_color_new_temp(new_color);
  }
}

/***                    MANUAL DIM                     ***/
/***           BUTTON 1  Also know as D button         ***/
/*** Hold 4 seconds then dims to min, then max, repeat ***/
static void on_hold_dim(app_button_t gesture)
{
  (void)gesture;

  uint8_t next_level = get_present_light_level() - DEFAULT_MIN_LIGHT_LEVEL_DT/4;
  if(DEFAULT_MIN_LIGHT_LEVEL_DT <= next_level)
  {
     move_to_level_onoff(next_level, 0);
  }
  else
  {
     move_to_level_onoff(get_max_light_level(), 0);
  }
  hal_start_flashing_red_led(1UL, 500UL, 50UL);
}

/***            MANUAL ADD TO GROUP              ***/
/***   BUTTON 0 & 1  Also know as T & D button   ***/
/*** Hold both buttons for 4 seconds, manual add ***/
static void on_hold_manual_add(app_button_t gesture)
{
  (void)gesture;

  place_in_manual_add_into_group_mode();
  #if(BUTTON_PRINT > 0)
  sl_zigbee_core_debug_println("Setting MANUAL ADD mode   <%d>", get_time_ms());
  #endif
}

void button_actions_on_startup(void)
{
  app_button_on_startup();

  app_button_register_sequence_cb(on_button_sequence);

  // Color: first step after HOLD_TIME_MS, then every COLOR_CYCLE_TIME_MS
  app_button_register_hold(APP_BUTTON_T, &(app_button_hold_t){
     .first_ms = HOLD_TIME_MS,
     .repeat_ms = COLOR_CYCLE_TIME_MS,
     .cb = on_hold_color });

  // Manual dim: first step after HOLD_TIME_MS, then a step on every poll
  app_button_register_hold(APP_BUTTON_D, &(app_button_hold_t){
     .first_ms = HOLD_TIME_MS,
     .repeat_ms = APP_BUTTON_REPEAT_EVERY_POLL,
     .cb = on_hold_dim });

  // Manual add: once per HOLD_TIME_MS of holding, only on Adaptiv when not in a group
  app_button_register_hold(APP_BUTTON_BOTH, &(app_button_hold_t){
     .first_ms = HOLD_TIME_MS,
     .rearm = true,
     .enabled = adaptiv_not_in_group,
     .cb = on_hold_manual_add });
}
