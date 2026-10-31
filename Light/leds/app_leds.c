#include "app/framework/include/af.h"
#include "hal_utils.h"
#include "hal_leds.h"
#include "nwk_groups.h"
#include "app_leds.h"

// Network status
#define LED_ON_TIME                500UL
#define LED_OFF_TIME               500UL
#define LED_CHECK_PERIOD_MS        (5000UL + LED_ON_TIME + LED_OFF_TIME)
#define LED_POR_BLINK_ON_MS        (2000UL)
#define LED_POR_BLINK_OFF_MS       (5UL)
#define LED_STATUS_FIRST_CHECK_MS  (750UL)

// Buttons
#define LED_PRESS_ON_MS            (250UL)
#define LED_PRESS_OFF_MS           (250UL)
#define LED_FAIL_ON_MS             (1500UL)
#define LED_FAIL_OFF_MS            (50UL)
#define LED_DIM_STEP_ON_MS         (500UL)
#define LED_DIM_STEP_OFF_MS        (50UL)

static sl_zigbee_af_event_t network_status_event;
static uint32_t occ_led_timeStamp = 0UL;


/***********************  STARTUP  *********************/
void app_leds_on_startup(void)
{
  hal_leds_init();
  hal_set_green_led(1);
  hal_set_red_led(1);
}

void app_leds_on_startup_done(void)
{
  hal_set_green_led(0);
  hal_set_red_led(0);
}

/***********************  NETWORK STATUS  *********************/
// Long 2 sec at por if not joined
// 1 quick if joined but not grouped
// 2 quick if not joined
static void network_status_event_handler(sl_zigbee_af_event_t *event)
{
  static bool por_not_joined_red_led_done = false;  // Reset only at por
  sl_zigbee_network_status_t nwk_status = sl_zigbee_af_network_state();

  (void)event;
  if((SL_ZIGBEE_JOINED_NETWORK == nwk_status) && device_is_in_a_group())
  {
     sl_zigbee_af_event_set_delay_ms(&network_status_event, 2UL*LED_CHECK_PERIOD_MS);
     if(is_red_led_free())
     {
        hal_set_red_led(0);
     }
  }
  else if((SL_ZIGBEE_JOINED_NETWORK == nwk_status) && !device_is_in_a_group())
  {
     if(is_red_led_free())
     {
        // Joined, not grouped, 2 blinks
        hal_start_flashing_red_led(2UL, LED_ON_TIME, LED_OFF_TIME);
     }
     sl_zigbee_af_event_set_delay_ms(&network_status_event, LED_CHECK_PERIOD_MS);
  }
  else if(por_not_joined_red_led_done)
  {
     if(is_red_led_free())
     {
        // Not joined and initial long por red led happened, 1 blink
        hal_start_flashing_red_led(1UL, LED_ON_TIME, LED_OFF_TIME);
     }
     sl_zigbee_af_event_set_delay_ms(&network_status_event, LED_CHECK_PERIOD_MS);
  }
  else
  {
     // Not joined and initial long por red led not happened
     por_not_joined_red_led_done = true;
     if(is_red_led_free())
     {
        hal_start_flashing_red_led(1UL, LED_POR_BLINK_ON_MS, LED_POR_BLINK_OFF_MS);
     }
     sl_zigbee_af_event_set_delay_ms(&network_status_event, LED_CHECK_PERIOD_MS + LED_POR_BLINK_ON_MS);
  }
}

void app_leds_network_status_start(void)
{
  sl_zigbee_af_event_init(&network_status_event, network_status_event_handler);
  sl_zigbee_af_event_set_delay_ms(&network_status_event, LED_STATUS_FIRST_CHECK_MS);
}

/***********************  BUTTONS  *********************/
void app_leds_show_presses(uint8_t presses)
{
  hal_start_flashing_red_led((uint32_t)presses, LED_PRESS_ON_MS, LED_PRESS_OFF_MS);
}

void app_leds_show_fail(void)
{
  hal_start_flashing_red_led(1UL, LED_FAIL_ON_MS, LED_FAIL_OFF_MS);
}

void app_leds_show_dim_step(void)
{
  hal_start_flashing_red_led(1UL, LED_DIM_STEP_ON_MS, LED_DIM_STEP_OFF_MS);
}

/***********************  FACTORY RESET  *********************/
void app_leds_factory_reset(bool active)
{
  hal_set_red_led(active ? 1 : 0);
}

/***********************  FIRMWARE VERSION  *********************/
void app_leds_off(void)
{
  hal_set_green_led(0);
  hal_set_red_led(0);
}

void app_leds_show_version_digit(app_led_t led, uint32_t count)
{
  if(APP_LED_GREEN == led)
  {
     hal_start_flashing_green_led(count, APP_LEDS_VERSION_BLINK_MS, APP_LEDS_VERSION_BLINK_MS);
  }
  else
  {
     hal_start_flashing_red_led(count, APP_LEDS_VERSION_BLINK_MS, APP_LEDS_VERSION_BLINK_MS);
  }
}

/***********************  OCCUPANCY SENSOR  *********************/
// On while motion is detected, off APP_LEDS_OCCUPANCY_HOLD_MS after the last detection
void app_leds_on_occupancy_sample(bool detected)
{
  if(detected)
  {
     hal_set_green_led(1);
     occ_led_timeStamp = get_time_ms();
  }
  else if(has_timed_out(occ_led_timeStamp, APP_LEDS_OCCUPANCY_HOLD_MS))
  {
     hal_set_green_led(0);
  }
  else
  {
  }
}

void app_leds_on_bad_occ_sensor(void)
{
  hal_toggle_green_led();
}

void app_leds_on_occ_sensor_stopped(void)
{
  hal_set_green_led(0);
}
