#include "app/framework/include/af.h"
// SDK 2025.6.3: hal.h no longer pulls in the legacy token API, which
// provides the TOKEN_APP_* ids and types from sl_custom_token_header.h.
#include "sl_token_api.h"
#include "sl_clock_manager.h"
#include "sl_hal_gpio.h"
#include "em_gpio.h"
#include "sl_event_handler.h"
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
#include "sl_token_manager.h"

#include "sl_zigbee_debug_print.h"

static uint8_t original_light_level;

static void toggle_50_percent_mode(void);
static void toggle_em_mode(void);
static void start_local_auto_commission(void);
static void start_display_fw_version(void);


// 0 = no debug   1 = some
#ifndef BUTTON_PRINT
#define BUTTON_PRINT 1//0    /* Default to 0 */
#elif (BUTTON_PRINT != 0)
#warning "BUTTON_PRINT active"
#endif

/*
 * BUTTON 0 also known as T
 * BUTTON 1 also known as D, closest to sensor
 *
 * Present button functions
 *
 * FACTORY RESET
 * Press and release both buttons 0 & 1, (buttons T & D), 10 times
 * Internal - press and hold both buttons for 3 seconds
 *
 * OCC SENSOR DELAYS
 * Press and release button 0, (button D), between 1-4 times to select delay
 *
 * MAX-MIN LIGHT LEVELS
 * Press and release button 1, (button T), between 1-9 times to select min-max
 *
 * COLOR
 * Press and hold button 1, (button T), for minimum 5 seconds, it will cycle
 * through different light temperatures.  Release will select last color
 *
 * INTERNAL FACTORY RESET
 * Press and hold button 0, (button D), for minimum 5 seconds,
 *
 */

static button_info_t button[2] = {{0}};

// Since both buttons on same isr
static int8_t one_shot_isr_num = -1;
#define buttons_active (button[0].active || button[1].active)
static sl_zigbee_af_event_t button_done_event;
static sl_zigbee_af_event_t button_poll_event;

// Fwd functions
static bool button_0_pressed(void);
static bool button_1_pressed(void);




// Returns last valid number of button presses
bool get_button_presses(uint8_t *button_0, uint8_t *button_1)
{
  *button_0 = button[0].edges;
  *button_1 = button[1].edges;
  return(!buttons_active);
}

static void leds_off_isr(void)
{

}

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

// Parse presses & set LEDS for good or bad.  If any button is held to long,
// Set timer to turn off LEDS
static void button_done_handler(sl_zigbee_af_event_t *event)
{
  uint32_t button_0_delta_time = get_time_ms() - button[0].time_stamp;
  uint32_t button_1_delta_time = get_time_ms() - button[1].time_stamp;

  #if (BUTTON_PRINT != 0)
  sl_zigbee_app_debug_println("Edges  [0]=%d  [1]=%d  delta time [0]:%d  [1]%d",
       button[0].edges, button[1].edges, button_0_delta_time, button_1_delta_time);
  #endif

  if(buttons_active)
  {
     sl_zigbee_core_debug_print("Buttons still active [button_done_handler]");
     return;
  }

  // Check if button presses are long times?  That's in button_poll_handler
  if((button[0].edges && (button_0_delta_time > 2000UL))
               || (button[1].edges && (button_1_delta_time > 2000UL)))
  {
     sl_zigbee_core_debug_print("Time out [button_done_handler]");
     return;
  }

  // Start parse of buttons
  leds_off_isr();   // Clear residual crap

  /***********************  FACTORY RESET  *********************/
  if((abs(N_BUT0_PRESSES_FOR_RESET - button[0].edges) <= 1) && (abs(N_BUT1_PRESSES_FOR_RESET - button[1].edges) <= 1))
  {
     // Blow our brains out
     force_factory_reset(250UL, eRESET_NORMAL);
     return;    // Only for this light
  }
  /***********************     50% MODE    *********************/
  else if((N_BUT0_PRESSES_FOR_50_PERCENT == button[0].edges) && (N_BUT1_PRESSES_FOR_50_PERCENT == button[1].edges))
  {
     hal_start_flashing_red_led((uint32_t)button[1].edges, 250UL, 250UL);
     toggle_50_percent_mode();
     return;    // Only for this light
  }
  /***********************      EM MODE    *********************/
  else if((N_BUT0_PRESSES_FOR_EM_MODE == button[0].edges) && (N_BUT1_PRESSES_FOR_EM_MODE == button[1].edges))
  {
     hal_start_flashing_red_led((uint32_t)button[1].edges, 250UL, 250UL);
     toggle_em_mode();
     return;    // Only for this light
  }
  /***********************   DO TWITCH's JOB  *********************/
  else if((N_BUT0_PRESSES_BX_MANUAL_ADD == button[0].edges) && (N_BUT1_PRESSES_BX_MANUAL_ADD == button[1].edges))
  {
     if((eADAPTIV_NWK == get_present_network()) && !device_is_in_a_group())
     {
        hal_start_flashing_red_led((uint32_t)button[1].edges, 250UL, 250UL);
        start_local_auto_commission();
     }
     else
     {
        hal_start_flashing_red_led(1UL, 1500UL, 50UL);
     }
     return;    // Only for this light
  }
  #ifdef ALLOW_DISPLAY_VERSION
  /***********************  SHOW FW VERSION  *********************/
  else if((N_BUT0_PRESSES_FW_VERSION == button[0].edges) && (N_BUT1_PRESSES_FW_VERSION == button[1].edges))
  {
     start_display_fw_version();
     return;    // Only for this light
  }
  #endif
  /**********************  YOU SHALL NOT PASS  *******************/
  else if(!are_buttons_allowed())
  {
     // Show fail
     hal_start_flashing_red_led(1UL, 1500UL, 50UL);
     return;
  }
  /**********************  OCC SENSOR DELAYS *******************/
  else if((MAX_BUT0_PRESSES_FOR_OCC_DELAY == button[0].edges) &&  // BUTTON 0 also known as T
          (MIN_BUT1_PRESSES_FOR_OCC_DELAY <= button[1].edges) &&  // BUTTON 1 also known as D
          (MAX_BUT1_PRESSES_FOR_OCC_DELAY >= button[1].edges))
  {
     // Set occupancy sensor time out.
     uint8_t blinky = button[1].edges;

     switch(button[1].edges)
     {
        // Times in seconds
        case 1:
           save_adaptiv_unoccupied_delay(10*60);
           save_adaptiv_unoccupied_delay_to_off(5*60);
           break;
        case 2:
           save_adaptiv_unoccupied_delay(20*60);
           save_adaptiv_unoccupied_delay_to_off(5*60);
           break;
        case 3:
           save_adaptiv_unoccupied_delay(30*60);
           save_adaptiv_unoccupied_delay_to_off(5*60);
           break;
        case 4:
           save_adaptiv_unoccupied_delay(45*60);
           save_adaptiv_unoccupied_delay_to_off(5*60);
           break;
        case 5:
           save_adaptiv_unoccupied_delay(60*60);
           save_adaptiv_unoccupied_delay_to_off(5*60);
           break;
        case 8:
           save_adaptiv_unoccupied_delay(30);
           save_adaptiv_unoccupied_delay_to_off(30);
           break;
        case 9:
           save_adaptiv_unoccupied_delay(0); // 0 unoccupied_delay = occ sensor off
           save_adaptiv_unoccupied_delay_to_off(31);
           break;
        case 10:
           save_adaptiv_unoccupied_delay(DEFAULT_OCC_SENSOR_XXL_DELAY);
           save_adaptiv_unoccupied_delay_to_off(5*60);
           break;
        /*** NOTE: 11 - 12 sets occupancy sensors threshold ONLY ***/
        case 11:
           set_occupancy_threshold(DEFAULT_OCC_SENSOR_THRESHOLD);
           break;
        case 12:
            set_occupancy_threshold(DEFAULT_OCC_SENSOR_THRESHOLD_LOW);
            break;

        default:
           sl_zigbee_core_debug_println("Occ button press %d", blinky );
           blinky = 0;
           break;
     }

     if(blinky)
     {
        // Send to every one in the group
        tx_occ_sensor_new_delays(get_unoccupied_delay(), get_unoccupied_delay_to_off());
        hal_start_flashing_red_led((uint32_t)button[1].edges, 250UL, 250UL);
     }
     else
     {
        hal_start_flashing_red_led(1UL, 1500UL, 50UL);
     }
  }
  /**********************  MAX-MIN LIGHT LEVELS  *******************/
  else if ((MAX_BUT1_PRESSES_FOR_DIM_VALUES == button[1].edges) &&     // BUTTON 1 also known as D
           (MIN_BUT0_PRESSES_FOR_DIM_VALUES <= button[0].edges) &&     // BUTTON 0 also known as T
           (MAX_BUT0_PRESSES_FOR_DIM_VALUES >= button[0].edges))
  {
     // This handles 1-4 dim levels %80-0, %60-0, %40-0, %100-0
     uint8_t blinky = button[0].edges;

     switch(button[0].edges)
     {
       case 1:
           save_max_light_level(LIGHT_LEVEL_100_PERCENT);
           save_mid_light_level(LIGHT_LEVEL_20_PERCENT);
           save_min_light_level(0);
           break;
        case 2:
           save_max_light_level(LIGHT_LEVEL_80_PERCENT);
           save_mid_light_level(LIGHT_LEVEL_20_PERCENT);
           save_min_light_level(0);
           break;
        case 3:
           save_max_light_level(LIGHT_LEVEL_60_PERCENT);
           save_mid_light_level(LIGHT_LEVEL_20_PERCENT);
           save_min_light_level(0);
           break;
        case 4:
           save_max_light_level(LIGHT_LEVEL_40_PERCENT);
           save_mid_light_level(LIGHT_LEVEL_20_PERCENT);
           save_min_light_level(0);
           break;
        case 5:
           save_max_light_level(LIGHT_LEVEL_20_PERCENT);
           save_mid_light_level(LIGHT_LEVEL_10_PERCENT);
           save_min_light_level(0);
           break;
        case 6:
           save_max_light_level(LIGHT_LEVEL_10_PERCENT);
           save_mid_light_level(LIGHT_LEVEL_10_PERCENT);
           save_min_light_level(0);
           break;
        default:
           blinky = 0;
           break;
     }

     if(blinky)
     {
        // Send to every one in the group
        tx_new_light_levels();
        // Show # of presses
        hal_start_flashing_red_led((uint32_t)button[0].edges, 250UL, 250UL);
     }
     else
     {
        // Show fail
        hal_start_flashing_red_led(1UL, 1500UL, 50UL);
        sl_zigbee_app_debug_println("Button FAIL: [0]:%d [1]:%d   <%d>", button[0].edges,
                                    button[1].edges, get_time_ms());
     }
  }
  else if(false)
  {

  }
  else
  {
     // Error no valid presses
     hal_start_flashing_red_led(1UL, 1500UL, 50UL);
  }

  // Turn off red or green led
  set_one_shot_us_timer_isr(leds_off_isr, 1000000UL);
}

// Polls for button(s) being pressed for long time.
//
// If button[0] T, is held BUTTON_0_INITIAL_PRESS_TIME_MS, AND
// if button[1] D, is not held
// Will continuously change COLOR TEMPERATURE.
//
// If button[0] T, is held for greater than 3 secs
// if button[1] D, is held for greater than 3 secs
// MANUAL GROUP
//
// If button[0] T, is not held
// if button[1] D, is held for greater than than 5 secs
// INTERNAL RESET
//
static void button_poll_handler(sl_zigbee_af_event_t *event)
{
  static bool button_0_active = false;
  static bool button_1_active = false;
  static uint32_t button_0_timestamp = 0UL;
  static uint32_t button_0_timeout = BUTTON_0_INITIAL_PRESS_TIME_MS;
  static uint32_t button_1_timestamp = 0UL;
  static uint32_t button_1_timeout = BUTTON_1_INITIAL_PRESS_TIME_MS;

  /***                  COLOR                   ***/
  /***     BUTTON 0,  Also known as T button    ***/
  /*** Hold 5 seconds then rolls through colors ***/
  if(button_0_pressed() && !button_0_active && !button_1_pressed())
  {
     // 1st time thru
     button_0_timestamp = get_time_ms();
     button_0_active = true;
     button_0_timeout = BUTTON_0_INITIAL_PRESS_TIME_MS;
  }
  else if(button_0_pressed() && button_0_active && !button_1_pressed() &&
          has_timed_out(button_0_timestamp, button_0_timeout))
  {
     // To get here means button has been pressed for at least BUTTON_0_INITIAL_PRESS_TIME_MS
     button_0_timestamp = get_time_ms();
     button_0_timeout = 2500UL;

     // Button held long enough, start changing light color
     uint16_t new_color = get_light_color() + 500;
     if(5000 < new_color)
     {
        new_color = 3000;
     }

     if(allow_color_change())
     {
        save_light_color(new_color);
        tx_color_new_temp(new_color);
     }
  }
  else
  {
  }

  /***                    MANUAL DIM                     ***/
  /***           BUTTON 1  Also know as D button         ***/
  /*** Hold 5 seconds then dims to min, then max, repeat ***/
  if(button_1_pressed() && !button_1_active && !button_0_pressed())
  {
     // 1st time thru
     button_1_timestamp = get_time_ms();
     button_1_active = true;
     button_1_timeout = BUTTON_1_INITIAL_PRESS_TIME_MS;
  }
  else if(button_1_pressed() && button_1_active && !button_0_pressed() &&
         has_timed_out(button_1_timestamp, button_1_timeout))
  {
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
  else
  {
  }

  /***            MANUAL ADD TO GROUP              ***/
  /***   BUTTON 0 & 1  Also know as T & D button   ***/
  /*** Hold both buttons for 5 seconds, manual add ***/
  if((eADAPTIV_NWK == get_present_network()) && !device_is_in_a_group())
  {
     if(button_0_pressed() && !button_0_active && button_1_pressed() && !button_1_active)
     {
        // 1st time thru
        button_0_timestamp = get_time_ms();
        button_0_active = true;
        button_0_timeout = BUTTON_1_INITIAL_PRESS_TIME_MS;
        button_1_timestamp = get_time_ms();
        button_1_active = true;
        button_1_timeout = BUTTON_1_INITIAL_PRESS_TIME_MS;
     }
     else if(button_0_pressed() && button_0_active && button_1_pressed() && button_1_active &&
     has_timed_out(button_0_timestamp, button_0_timeout) &&
     has_timed_out(button_1_timestamp, button_1_timeout))
     {
        //hal_start_flashing_red_led(10UL, 250UL, 250UL);
        place_in_manual_add_into_group_mode();
        // To prevent multiple calls to place_in_manual_add_into_group_mode()
        button_0_active = false;
        button_1_active = false;
        #if(BUTTON_PRINT > 0)
        sl_zigbee_core_debug_println("Setting MANUAL ADD mode   <%d>", get_time_ms());
        #endif
     }
     else
     {
     }
  }

  /*** Must always come through here ***/
  if(!button_0_pressed())
  {
     button_0_active = false;
     button_0_timeout = BUTTON_0_INITIAL_PRESS_TIME_MS;
  }
  if(!button_1_pressed())
  {
     button_1_active = false;
     button_1_timeout = BUTTON_1_INITIAL_PRESS_TIME_MS;
  }

  if(!button_1_pressed() && !button_1_pressed())
  {
     sl_zigbee_af_event_set_delay_ms(&button_poll_event, BUTTONS_POLL_TIME_MS);
  }
  else
  {
     sl_zigbee_af_event_set_delay_ms(&button_poll_event, BUTTONS_ACTIVE_POLL_TIME_MS);
  }
}

// Got here cause we received no button presses for BUTTONS_DONE_US
// Save presses/edges, start event, outside of isr
static void buttons_done_isr(void)
{
  CORE_ATOMIC_SECTION(
     button[0].edges = button[0].edges_isr; // Copy
     button[1].edges = button[1].edges_isr;

     button[0].edges_isr = 0; // Clear for next time
     button[1].edges_isr = 0;

     button[0].time_stamp = button[0].time_stamp_isr;
     button[1].time_stamp = button[1].time_stamp_isr;

     hal_set_red_led(0);     // Make sure they're off
     hal_set_green_led(0);

     button[0].active = false;
     button[1].active = false;
  )
  // Determine what to do outside of isr
  sl_zigbee_af_event_set_active(&button_done_event);
}

// Handles both button_0 and button_1 which presently are on pins 1 & 3,(both odd).
// Interrupt occurs on both rising/falling edges.  Debounce done here, valid
// button press = line must be low for consecutive DEBOUNCE_TIME_MS time.
//
// If BUTTONS_DONE_US times passes with no button presses, then button presses
// are done and buttons_done_isr() takes over.
void GPIO_ODD_IRQHandler(void)
{
  uint32_t iflags = 0UL;

  iflags = GPIO->IEN;
  iflags &= GPIO->IF;

  while(iflags)
  {
     if(BUTTON_0_INT_FLAG & iflags)
     {
        // Falling edge?
        if(0 == (((GPIO->P[BUTTON_0_PORT].DIN) >> BUTTON_0_PIN) & 1UL))
        {
           button[0].time_stamp_isr = get_time_ms();
        }
        else
        {
           // Rising edge, Determine if button low long enough
           if(has_timed_out(button[0].time_stamp_isr, DEBOUNCE_TIME_MS))
           {
               // If button was held low > DEBOUNCE_TIME_MS, valid edge
              button[0].edges_isr++;
           }
        }
        // Clear interrupt.
        GPIO->IF_CLR = BUTTON_0_INT_FLAG;
        button[0].active = true;
     }

     if(BUTTON_1_INT_FLAG & iflags)
     {
        // Falling edge?
        if(0 == (((GPIO->P[BUTTON_1_PORT].DIN) >> BUTTON_1_PIN) & 1UL))
        {
           button[1].time_stamp_isr = get_time_ms();
        }
        else
        {
           // Rising edge, Determine if button low long enough
           if(has_timed_out(button[1].time_stamp_isr, DEBOUNCE_TIME_MS))
           {
              // If button was held low > DEBOUNCE_TIME_MS, valid edge
              button[1].edges_isr++;
           }
        }
        // Clear interrupt.
        GPIO->IF_CLR = BUTTON_1_INT_FLAG;
        button[1].active = true;
     }

     // Make sure, loop again
     iflags = GPIO->IEN;
     iflags &= GPIO->IF;
  }

  // Start new timer isr to determine if button presses are done
  if(buttons_active)
  {
      // Disable pending isr
      disable_one_shot_us_timer_isr(one_shot_isr_num);
  }

  // Start a new isr when all buttons released
  if(!button_0_pressed() && !button_1_pressed())
  {
     one_shot_isr_num = set_one_shot_us_timer_isr(buttons_done_isr, BUTTONS_DONE_US);
  }
}

// Returns true if button pressed
static bool button_0_pressed(void)
{
  bool pin_input = false;

  CORE_ATOMIC_SECTION(
  pin_input = ((GPIO->P[BUTTON_0_PORT].DIN) >> BUTTON_0_PIN) & 1UL;
  )
  return(0 == pin_input);
}
// Returns true if button pressed
static bool button_1_pressed(void)
{
  bool pin_input = false;

  CORE_ATOMIC_SECTION(
  pin_input = ((GPIO->P[BUTTON_1_PORT].DIN) >> BUTTON_1_PIN) & 1UL;
  )
  return(0 == pin_input);
}

// Pins have pull-ups
// Interrupts occur on both edge
// Init isr event
sl_status_t hal_buttons_init(void)
{
  uint32_t int_no;

  sl_gpio_t button_0_gpio =
  {
     .port = BUTTON_0_PORT,
     .pin = BUTTON_0_PIN,
  };

  sl_gpio_t button_1_gpio =
  {
     .port = BUTTON_1_PORT,
     .pin = BUTTON_1_PIN,
  };

  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO);

  GPIO_PinModeSet(BUTTON_0_PORT, BUTTON_0_PIN, gpioModeInputPullFilter, 1UL);

  int_no = sl_hal_gpio_configure_external_interrupt(&button_0_gpio,
                                                    BUTTON_0_INT_NO,
                                                    SL_GPIO_INTERRUPT_RISING_FALLING_EDGE);
  assert(int_no == BUTTON_0_INT_NO);

  GPIO_PinModeSet(BUTTON_1_PORT, BUTTON_1_PIN, gpioModeInputPullFilter, 1UL);

  int_no = sl_hal_gpio_configure_external_interrupt(&button_1_gpio,
                                                    BUTTON_1_INT_NO,
                                                    SL_GPIO_INTERRUPT_RISING_FALLING_EDGE);
  assert(int_no == BUTTON_1_INT_NO);

  // Enable both button interrupts
  GPIO->IF_CLR = BUTTON_0_INT_FLAG | BUTTON_1_INT_FLAG;
  GPIO->IEN_SET = BUTTON_0_INT_FLAG | BUTTON_1_INT_FLAG;
  NVIC_EnableIRQ(GPIO_ODD_IRQn);  // EXTI_1

  sl_zigbee_af_isr_event_init(&button_done_event, button_done_handler);

  sl_zigbee_af_event_init(&button_poll_event, button_poll_handler);
  sl_zigbee_af_event_set_active(&button_poll_event);

  return SL_STATUS_OK;
}

// Toggle 50% mode, if on->off or off->on
static void toggle_50_percent_mode(void)
{
  uint8_t current_level = read_light_level_attribute(ZCL_CURRENT_LEVEL_ATTRIBUTE_ID);
  uint8_t next_level;
  uint8_t original_level;

  if(is_app_flags_nonvol_set(B_50_PERCENT_MODE))
  {
     // Turn OFF 50% mode, make bright, pop levels
     sl_token_get_data(TOKEN_APP_LIGHT_LEVEL_MAX_STASHED, 0, &original_level, sizeof(original_level));
     save_max_light_level(original_level);
     sl_token_get_data(TOKEN_APP_LIGHT_LEVEL_MID_STASHED, 0, &original_level, sizeof(original_level));
     save_mid_light_level(original_level);
     sl_token_get_data(TOKEN_APP_LIGHT_LEVEL_MIN_STASHED, 0, &original_level, sizeof(original_level));
     save_min_light_level(original_level);

     next_level = current_level << 1;
     set_app_flags_nonvol(B_50_PERCENT_MODE, 0);
     if(next_level >= get_max_light_level())
     {
        next_level = get_max_light_level() - 1;
     }
     move_to_level_onoff(next_level, 0);
  }
  else
  {
     // Show in 50% mode, dim now with present limits
     next_level = current_level >> 1;
     next_level = (next_level < MIN_ALLOWABLE_LIGHT_LEVEL) ? MIN_ALLOWABLE_LIGHT_LEVEL : next_level;
     move_to_level_onoff(next_level, 0);
     set_app_flags_nonvol(B_50_PERCENT_MODE, 1);

     // Save present limits, then 1/2 them
     original_level = get_max_light_level();
     sl_token_set_data(TOKEN_APP_LIGHT_LEVEL_MAX_STASHED, 0, &original_level, sizeof(original_level));
     original_level >>= 1;
     original_level = (original_level < MIN_ALLOWABLE_LIGHT_LEVEL) ? MIN_ALLOWABLE_LIGHT_LEVEL : original_level;
     save_max_light_level(original_level);

     original_level = get_mid_light_level();
     sl_token_set_data(TOKEN_APP_LIGHT_LEVEL_MID_STASHED, 0, &original_level, sizeof(original_level));
     original_level >>= 1;
     original_level = (original_level < MIN_ALLOWABLE_LIGHT_LEVEL) ? MIN_ALLOWABLE_LIGHT_LEVEL : original_level;
     save_mid_light_level(original_level);

     original_level = get_min_light_level();
     sl_token_set_data(TOKEN_APP_LIGHT_LEVEL_MIN_STASHED, 0, &original_level, sizeof(original_level));
     original_level >>= 1;
     save_min_light_level(original_level);
  }

  #if (BUTTON_PRINT > 0)
  sl_zigbee_core_debug_println("Setting 50% mode   <%d>", get_time_ms());
  #endif

  return;
}

static void toggle_em_mode(void)
{
  if(is_app_flags_nonvol_set(B_EM_MODE))
  {
     // Turn OFF EM mode, place light in mid level
     set_app_flags_nonvol(B_EM_MODE, 0);
  }
  else
  {
     // Turn ON EM mode, place light in max level
     set_app_flags_nonvol(B_EM_MODE, 1);
  }

  #if (BUTTON_PRINT > 0)
  sl_zigbee_core_debug_println("EM mode %s   <%d>",
          is_app_flags_nonvol_set(B_EM_MODE) ? "ON" : "OFF", get_time_ms());
  #endif

  return;
}

// Output auto commish announce cmd 3 times at 3 seconds apart
void bx_auto_announce_task(taskParm_t parm0, taskParm_t parm1)
{
  if(has_timed_out(parm0.u32, 3000UL))
  {
     parm1.u8++;
     if(3 >= parm1.u8)
     {
        taskParm_t arg0, arg1;

        arg0.u32 = get_time_ms();
        arg1.u8 = parm1.u8;
        addTask(bx_auto_announce_task, arg0, arg1);

        broadcast_autocommission_announce();
     }
     else
     {
        // Done
        removeTask(bx_auto_announce_task);
     }
  }
}

static void start_local_auto_commission(void)
{
  taskParm_t arg0, arg1;

  // Make sure this light is in manual add mode
  if(!is_app_flags_nonvol_set(B_MANUAL_ADD_MODE))
  {
     place_in_manual_add_into_group_mode();
  }
  arg0.u32 = get_time_ms();
  arg1.u8 = 0;    // Counter
  addTask(bx_auto_announce_task, arg0, arg1);

}

#ifdef ALLOW_DISPLAY_VERSION
// Restore states
static void end_display_version_task(taskParm_t parm0, taskParm_t parm1)
{
  // Wait for leds to be done
  if(has_timed_out(parm0.u32, parm1.u32))
  {
     removeTask(end_display_version_task);
     hal_set_dim_pwm_255(original_light_level);
     hal_start_occ_sensor();  // Allows green led
     set_app_flags_nonvol((B_DISABLE_OCC_SENSOR | B_DISABLE_LIGHT_SENSOR), 0);
  }
}
// Minor version on red led
static void display_minor_version_task(taskParm_t parm0, taskParm_t parm1)
{
  // Wait for leds to be done
  if(has_timed_out(parm0.u32, parm1.u32))
  {
     uint32_t minor_fw_version = 0x00FFUL & APP_FIRMWARE_VERSION_U32;
     taskParm_t arg0, arg1;

     removeTask(display_minor_version_task);

     if(10 <= minor_fw_version)
     {
        minor_fw_version = 10UL;
     }

     hal_start_flashing_red_led(minor_fw_version, FW_VERSION_LED_ON_OFF_TIME, FW_VERSION_LED_ON_OFF_TIME);

     // Time to delay till next version
     arg0.u32 = get_time_ms();
     arg1.u32 = (minor_fw_version * FW_VERSION_LED_ON_OFF_TIME * 2UL) + 1000UL;

     addTask(end_display_version_task, arg0, arg1);
  }
}

// Middle version on green led
static void display_middle_version_task(taskParm_t parm0, taskParm_t parm1)
{
  // Wait for leds to be done
  if(has_timed_out(parm0.u32, parm1.u32))
  {
     uint32_t middle_fw_version = 0x00FFUL & (APP_FIRMWARE_VERSION_U32 >> 8);
     taskParm_t arg0, arg1;

     removeTask(display_middle_version_task);

     if(10 < middle_fw_version)
     {
        middle_fw_version = 10UL;
     }

     hal_start_flashing_green_led(middle_fw_version, FW_VERSION_LED_ON_OFF_TIME, FW_VERSION_LED_ON_OFF_TIME);

     // Time to delay till next version
     arg0.u32 = get_time_ms();
     arg1.u32 = (middle_fw_version * FW_VERSION_LED_ON_OFF_TIME * 2UL) + 750UL;
     addTask(display_minor_version_task, arg0, arg1);
  }
}

// Major version on red led
static void display_major_version_task(taskParm_t parm0, taskParm_t parm1)
{
  hal_set_green_led(0);
  hal_set_red_led(0);
  hal_set_dim_pwm_255(0);

  // Wait for light to be off
  if(has_timed_out(parm0.u32, 1500UL))
  {
     uint32_t major_fw_version = 0x00FFUL & (APP_FIRMWARE_VERSION_U32 >> 16);
     taskParm_t arg0, arg1;

     removeTask(display_major_version_task);

     if(10 < major_fw_version)
     {
        major_fw_version = 10UL;
     }

     hal_start_flashing_red_led(major_fw_version, FW_VERSION_LED_ON_OFF_TIME, FW_VERSION_LED_ON_OFF_TIME);

     // Time to delay till next version
     arg0.u32 = get_time_ms();
     arg1.u32 = (major_fw_version * FW_VERSION_LED_ON_OFF_TIME * 2UL) + 750UL;
     addTask(display_middle_version_task, arg0, arg1);
  }
}

static void start_display_fw_version(void)
{
  taskParm_t arg0, arg1;

  original_light_level = hal_get_dim_pwm_255();
  set_app_flags_nonvol((B_DISABLE_OCC_SENSOR | B_DISABLE_LIGHT_SENSOR), 1);
  hal_stop_occ_sensor();  // Keeps green led off
  hal_set_green_led(0);
  hal_set_red_led(0);
  hal_set_dim_pwm_255(0);

  arg0.u32 = get_time_ms();
  arg1.u8 = 0;
  addTask(display_major_version_task, arg0, arg1);
}

#endif // ALLOW_DISPLAY_VERSION
