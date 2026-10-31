#include "app/framework/include/af.h"
#include "sl_clock_manager.h"
#include "sl_hal_gpio.h"
#include "em_gpio.h"
#include "hal_utils.h"
#include "hal_leds.h"
#include "hal_buttons.h"

#include "sl_zigbee_debug_print.h"

typedef struct
{
  uint8_t edges;       // Copy of isr edges in Non-isr context
  uint8_t edges_isr;   // Incs in isr only
  uint32_t time_stamp; // Last change
  uint32_t time_stamp_isr; // Last change
  bool active;
}button_info_t;

static button_info_t button[HAL_BUTTON_COUNT] = {{0}};

// Since both buttons on same isr
static int8_t one_shot_isr_num = -1;
#define buttons_active (button[0].active || button[1].active)
static sl_zigbee_af_event_t button_done_event;

static hal_buttons_sequence_cb_t sequence_cb = NULL;


void hal_buttons_register_sequence_cb(hal_buttons_sequence_cb_t cb)
{
  sequence_cb = cb;
}

// A press sequence finished (buttons_done_isr).  Hand the presses to the
// registered callback, unless a new sequence has already started.
static void button_done_handler(sl_zigbee_af_event_t *event)
{
  (void)event;
  uint32_t button_0_delta_time = get_time_ms() - button[0].time_stamp;
  uint32_t button_1_delta_time = get_time_ms() - button[1].time_stamp;

  #if (BUTTON_PRINT != 0)
  sl_zigbee_app_debug_println("Edges  [0]=%d  [1]=%d  delta time [0]:%d  [1]%d",
       button[0].edges, button[1].edges, button_0_delta_time, button_1_delta_time);
  #else
  (void)button_0_delta_time;
  (void)button_1_delta_time;
  #endif

  if(buttons_active)
  {
     sl_zigbee_core_debug_print("Buttons still active [button_done_handler]");
     return;
  }

  if(NULL != sequence_cb)
  {
     hal_buttons_sequence_t sequence =
     {
        .presses = { button[0].edges, button[1].edges },
        .last_press_ms = { button[0].time_stamp, button[1].time_stamp },
     };
     sequence_cb(&sequence);
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

     // This one-shot has fired; its timer slot may now be given to another
     // module, so it must not be disabled on the next button interrupt.
     one_shot_isr_num = -1;
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
  if(!hal_button_is_pressed(HAL_BUTTON_T) && !hal_button_is_pressed(HAL_BUTTON_D))
  {
     one_shot_isr_num = set_one_shot_us_timer_isr(buttons_done_isr, BUTTONS_DONE_US);
  }
}

// Returns true if button pressed
bool hal_button_is_pressed(hal_button_t which)
{
  bool pin_input = false;

  if(HAL_BUTTON_T == which)
  {
     CORE_ATOMIC_SECTION(
     pin_input = ((GPIO->P[BUTTON_0_PORT].DIN) >> BUTTON_0_PIN) & 1UL;
     )
  }
  else if(HAL_BUTTON_D == which)
  {
     CORE_ATOMIC_SECTION(
     pin_input = ((GPIO->P[BUTTON_1_PORT].DIN) >> BUTTON_1_PIN) & 1UL;
     )
  }
  else
  {
     return(false);
  }
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

  return SL_STATUS_OK;
}
