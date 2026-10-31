#include "app/framework/include/af.h"
#include "sl_clock_manager.h"
#include "em_gpio.h"
#include "sl_event_handler.h"
#include "hal_leds.h"
#include "hal_utils.h"

#include "sl_zigbee_debug_print.h"

void set_dhs_time_stamp(void);

//#define LED_PRINTF

// Note: Put these in another file
#define RED_LED_PORT  gpioPortD
#define RED_LED_PIN   2

#define GREEN_LED_PORT  gpioPortD
#define GREEN_LED_PIN   1


static led_info_t red_led =
{
    .on_ms = 250UL,
    .off_ms = 250UL,
    .n_toggles = 1,
};
static led_info_t green_led =
{
    .on_ms = 250UL,
    .off_ms = 250UL,
    .n_toggles = 1,
};

static sl_zigbee_af_event_t red_led_event;
static sl_zigbee_af_event_t green_led_event;

// Fwd
static void red_led_handler(sl_zigbee_af_event_t *event);
static void green_led_handler(sl_zigbee_af_event_t *event);


void hal_leds_init(void)
{
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO); // Redundant?
  GPIO_PinModeSet(RED_LED_PORT, RED_LED_PIN, gpioModeWiredAndPullUp, 1UL);
  GPIO_PinModeSet(GREEN_LED_PORT, GREEN_LED_PIN, gpioModeWiredAndPullUp, 1UL);

  sl_zigbee_af_event_init(&red_led_event, red_led_handler);
  sl_zigbee_af_event_init(&green_led_event, green_led_handler);
}

void hal_set_red_led(uint8_t on)
{
  if(on)
  {
      GPIO_PinOutClear(RED_LED_PORT, RED_LED_PIN);
      set_dhs_time_stamp();
  }
  else
  {
      GPIO_PinOutSet(RED_LED_PORT, RED_LED_PIN);
  }
}

void hal_set_green_led(uint8_t on)
{
  if(on)
  {
      GPIO_PinOutClear(GREEN_LED_PORT, GREEN_LED_PIN);
      set_dhs_time_stamp();
  }
  else
  {
      GPIO_PinOutSet(GREEN_LED_PORT, GREEN_LED_PIN);
  }
}

void hal_leds_off(void)
{
  hal_set_red_led(0);
  hal_set_green_led(0);
}

void hal_toggle_red_led(void)
{
  GPIO_PinOutToggle(RED_LED_PORT, RED_LED_PIN);
}

void hal_toggle_green_led(void)
{
  GPIO_PinOutToggle(GREEN_LED_PORT, GREEN_LED_PIN);
}

static bool is_red_led_on(void)
{
  return(0 == GPIO_PinInGet(RED_LED_PORT, RED_LED_PIN));
}

static void red_led_handler(sl_zigbee_af_event_t *event)
{
  hal_toggle_red_led();

  if(is_red_led_on())
  {
     sl_zigbee_af_event_set_delay_ms(&red_led_event, red_led.on_ms);
  }
  else
  {
     sl_zigbee_af_event_set_delay_ms(&red_led_event, red_led.off_ms);
  }

  red_led.n_toggles--;
  if(0 == red_led.n_toggles)
  {
      hal_stop_flashing_red_led();
  }
}

// Always starts with turning led off, here, then toggles led in the handler and
// ends always ends with the led off
bool hal_start_flashing_red_led(uint32_t n_flashes, uint32_t led_on_time_ms, uint32_t led_off_time_ms)
{
  if((0 == n_flashes) || sl_zigbee_af_event_is_scheduled(&red_led_event))
  {
      return(false);
  }
  red_led.n_toggles = 2UL * n_flashes;  // 2 toggles = 1 flash
  red_led.on_ms = led_on_time_ms;
  red_led.off_ms = led_off_time_ms;

  hal_set_red_led(0);
  sl_zigbee_af_event_set_active(&red_led_event);
  #ifdef LED_PRINTF
  sl_zigbee_app_debug_println("hal_start_flashing_red_led  n:%d on:0ff %d:%d mS",
                              n_flashes, red_led.on_ms,red_led.off_ms);
  #endif
  return(true);
}

void hal_stop_flashing_red_led(void)
{
  sl_zigbee_af_event_set_inactive(&red_led_event);
  hal_set_red_led(0);
  #ifdef LED_PRINTF
  sl_zigbee_app_debug_println("hal_stop_flashing_red_led   <%d>", get_time_ms());
  #endif
}

bool is_red_led_free(void)
{
  return(!sl_zigbee_af_event_is_scheduled(&red_led_event));
}

static void green_led_handler(sl_zigbee_af_event_t *event)
{
  if(0x01 & green_led.n_toggles)  // Odd?
  {
     hal_set_green_led(0);
     sl_zigbee_af_event_set_delay_ms(&green_led_event, green_led.off_ms);
  }
  else
  {
     hal_set_green_led(1);
     sl_zigbee_af_event_set_delay_ms(&green_led_event, green_led.on_ms);
  }
  green_led.n_toggles--;
  if(0 == green_led.n_toggles)  // Toggles always even, don't wait for last state
  {
      hal_stop_flashing_green_led();
  }
}

// Always starts with turning led off here, then toggles led in the handler and
// ends always ends with the led off
bool hal_start_flashing_green_led(uint32_t n_flashes, uint32_t led_on_time_ms, uint32_t led_off_time_ms)
{
  if(0 == n_flashes)
  {
      return(false);
  }
  green_led.n_toggles = 2UL * n_flashes;  // 2 toggles = 1 flash
  green_led.on_ms = led_on_time_ms;
  green_led.off_ms = led_off_time_ms;

  hal_set_green_led(0);
  sl_zigbee_af_event_set_active(&green_led_event);
  #ifdef LED_PRINTF
  sl_zigbee_app_debug_println("hal_start_flashing_green_led  %d flashed on %dmS",
                              n_flashes, green_led.on_ms);
  #endif
  return(true);
}

void hal_stop_flashing_green_led(void)
{
  sl_zigbee_af_event_set_inactive(&green_led_event);
  hal_set_green_led(0);
  #ifdef LED_PRINTF
  sl_zigbee_app_debug_println("hal_stop_flashing_green_led");
  #endif
}

