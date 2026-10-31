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
#define LED_PREEMPT_GAP_MS         (500UL)    // red off between a cut network status blink and the button feedback

// Who started the red flash that is running (valid while !is_red_led_free())
typedef enum
{
  RED_BY_NETWORK_STATUS,    // low priority: button feedback replaces it
  RED_BY_FEEDBACK,          // buttons, firmware version
} red_owner_t;

static sl_zigbee_af_event_t network_status_event;
static red_owner_t red_owner = RED_BY_NETWORK_STATUS;
static bool factory_reset_active = false;   // red is held on

static uint32_t occ_led_timeStamp = 0UL;
static bool occ_led_on = false;             // what the occupancy indication wants green to be
static bool green_lux_hold = false;         // green held off for a light sensor reading

static void network_status_flash(uint32_t n_flashes, uint32_t on_ms, uint32_t off_ms)
{
  if(hal_start_flashing_red_led(n_flashes, on_ms, off_ms))
  {
     red_owner = RED_BY_NETWORK_STATUS;
  }
}

// Button feedback and firmware version digits.  Replace a network status blink;
// ignored while other feedback is running (as before) or during a factory reset.
static void feedback_flash(uint32_t n_flashes, uint32_t on_ms, uint32_t off_ms)
{
  uint32_t start_delay_ms = 0UL;

  if(factory_reset_active)
  {
     return;
  }
  if(!is_red_led_free() && (RED_BY_NETWORK_STATUS == red_owner))
  {
     hal_stop_flashing_red_led();
     start_delay_ms = LED_PREEMPT_GAP_MS;
  }
  if(hal_start_flashing_red_led_after(start_delay_ms, n_flashes, on_ms, off_ms))
  {
     red_owner = RED_BY_FEEDBACK;
  }
}

// Green: a green flash comes first, then a lux reading hold, then occupancy.
static bool green_is_ours(void)
{
  return(!green_lux_hold && is_green_led_free());
}


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
  if(factory_reset_active)
  {
     // Red stays on until the reset is done
     sl_zigbee_af_event_set_delay_ms(&network_status_event, LED_CHECK_PERIOD_MS);
  }
  else if((SL_ZIGBEE_JOINED_NETWORK == nwk_status) && device_is_in_a_group())
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
        network_status_flash(2UL, LED_ON_TIME, LED_OFF_TIME);
     }
     sl_zigbee_af_event_set_delay_ms(&network_status_event, LED_CHECK_PERIOD_MS);
  }
  else if(por_not_joined_red_led_done)
  {
     if(is_red_led_free())
     {
        // Not joined and initial long por red led happened, 1 blink
        network_status_flash(1UL, LED_ON_TIME, LED_OFF_TIME);
     }
     sl_zigbee_af_event_set_delay_ms(&network_status_event, LED_CHECK_PERIOD_MS);
  }
  else
  {
     // Not joined and initial long por red led not happened
     por_not_joined_red_led_done = true;
     if(is_red_led_free())
     {
        network_status_flash(1UL, LED_POR_BLINK_ON_MS, LED_POR_BLINK_OFF_MS);
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
  feedback_flash((uint32_t)presses, LED_PRESS_ON_MS, LED_PRESS_OFF_MS);
}

void app_leds_show_fail(void)
{
  feedback_flash(1UL, LED_FAIL_ON_MS, LED_FAIL_OFF_MS);
}

void app_leds_show_dim_step(void)
{
  feedback_flash(1UL, LED_DIM_STEP_ON_MS, LED_DIM_STEP_OFF_MS);
}

/***********************  FACTORY RESET  *********************/
// Red on, and kept on: no other red indication runs until active = false
void app_leds_factory_reset(bool active)
{
  if(active && !factory_reset_active)
  {
     hal_stop_flashing_red_led();
  }
  factory_reset_active = active;
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
     feedback_flash(count, APP_LEDS_VERSION_BLINK_MS, APP_LEDS_VERSION_BLINK_MS);
  }
}

/***********************  OCCUPANCY SENSOR  *********************/
// On while motion is detected, off APP_LEDS_OCCUPANCY_HOLD_MS after the last detection.
// Waits while a green flash runs or a lux reading holds green off; the next sample
// after that restores it.
void app_leds_on_occupancy_sample(bool detected)
{
  if(detected)
  {
     occ_led_on = true;
     occ_led_timeStamp = get_time_ms();
  }
  else if(has_timed_out(occ_led_timeStamp, APP_LEDS_OCCUPANCY_HOLD_MS))
  {
     occ_led_on = false;
  }
  else
  {
  }
  if(green_is_ours())
  {
     hal_set_green_led(occ_led_on ? 1 : 0);
  }
}

void app_leds_on_bad_occ_sensor(void)
{
  occ_led_on = false;
  if(green_is_ours())
  {
     hal_toggle_green_led();
  }
}

void app_leds_on_occ_sensor_stopped(void)
{
  occ_led_on = false;
  if(green_is_ours())
  {
     hal_set_green_led(0);
  }
}

/***********************  LIGHT SENSOR  *********************/
bool app_leds_green_off_for_lux(void)
{
  if(!is_green_led_free())
  {
     return(false);    // a green flash pattern has priority; read later
  }
  green_lux_hold = true;
  hal_set_green_led(0);
  return(true);
}

void app_leds_green_lux_done(void)
{
  green_lux_hold = false;
  if(is_green_led_free())
  {
     hal_set_green_led(occ_led_on ? 1 : 0);
  }
}
