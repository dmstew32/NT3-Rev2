// REFERENCE for test_app_leds: the network status LED code from app.c as of
// commit cfc1f57, before the LED refactor (copied verbatim).  Do not edit:
// app_leds_network_status_start() must behave exactly like this.
#include "nt3_test_stubs.h"
#include "hal_leds.h"

#define LED_ON_TIME                500UL
#define LED_OFF_TIME               500UL
#define LED_CHECK_PERIOD_MS        (5000UL + LED_ON_TIME + LED_OFF_TIME)

static sl_zigbee_af_event_t red_led_event;

// Long 2 sec at por if not joined
// 1 quick if joined but not grouped
// 2 quick if not joined
static void red_led_event_handler(sl_zigbee_af_event_t *event)
{
  static bool por_not_joined_red_led_done = false;  // Reset only at por
  sl_zigbee_network_status_t nwk_status = sl_zigbee_af_network_state();

  (void)event;
  if((SL_ZIGBEE_JOINED_NETWORK == nwk_status) && device_is_in_a_group())
  {
     sl_zigbee_af_event_set_delay_ms(&red_led_event, 2UL*LED_CHECK_PERIOD_MS);
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
     sl_zigbee_af_event_set_delay_ms(&red_led_event, LED_CHECK_PERIOD_MS);
  }
  else if(por_not_joined_red_led_done)
  {
     if(is_red_led_free())
     {
        // Not joined and initial long por red led happened, 1 blink
        hal_start_flashing_red_led(1UL, LED_ON_TIME, LED_OFF_TIME);
     }
     sl_zigbee_af_event_set_delay_ms(&red_led_event, LED_CHECK_PERIOD_MS);
  }
  else
  {
     // Not joined and initial long por red led not happened
     por_not_joined_red_led_done = true;
     if(is_red_led_free())
     {
        hal_start_flashing_red_led(1UL, 2000UL, 5UL);
     }
     sl_zigbee_af_event_set_delay_ms(&red_led_event, LED_CHECK_PERIOD_MS + 2000UL);
  }
}

// from sl_zigbee_af_main_init_cb()
void ref_network_status_start(void)
{
  sl_zigbee_af_event_init(&red_led_event, red_led_event_handler);
  sl_zigbee_af_event_set_delay_ms(&red_led_event, 750UL);
}
