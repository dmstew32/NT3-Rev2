#include <stdlib.h>
#include "zigbee_app_framework_event.h"
#include "sl_gpio.h"
#include "sl_zigbee_debug_print.h"
#include "hal_utils.h"
#include "em_gpio.h"
#include "sl_token_manager.h"
#include "sl_custom_token_header.h"

#include "em_timer.h"
#include "em_cmu.h"
#include "sl_device_clock.h"
#include "sl_clock_manager.h"

#include "sl_hal_letimer.h"

/*
 * Timer0 -
 * Timer1 - 1 mSec timer, 3 user timer isr's
 * Timer2 - PWM1 PWM2
 * Timer3 - 1uS
 */

// TP_0 on N_PORT into driver?
//#define USE_TP_0

uint32_t upper_timer1_count = 0UL;
static void (*timer_CC0_func)(void) = NULL;
static void (*timer_CC1_func)(void) = NULL;
static void (*timer_CC2_func)(void) = NULL;


static void init_test_pt_0(void)
{
#ifdef USE_TP_0
  sl_gpio_t enable_tp_gpio =
  {
    .port = TP_0_PORT,
    .pin = TP_0_PIN,
  };
  sl_gpio_set_pin_mode(&enable_tp_gpio, SL_GPIO_MODE_PUSH_PULL, 1);
#endif
}
#ifdef USE_TP_0
void set_TP_0(void)
{
  GPIO_PinOutSet(TP_0_PORT, TP_0_PIN);
}
void clear_TP_0(void)
{
  GPIO_PinOutClear(TP_0_PORT, TP_0_PIN);
}
void toggle_TP_0(void)
{
  GPIO_PinOutToggle(TP_0_PORT, TP_0_PIN);
}
#else
void set_TP_0(void) {}
void clear_TP_0(void) {}
void toggle_TP_0(void) {}
#endif

static void init_test_pt_1(void)
{
  sl_gpio_t enable_tp_gpio =
  {
    .port = TP_1_PORT,
    .pin = TP_1_PIN,
  };
  sl_gpio_set_pin_mode(&enable_tp_gpio, SL_GPIO_MODE_PUSH_PULL, 1);
}
void set_TP_1(void)
{
  GPIO_PinOutSet(TP_1_PORT, TP_1_PIN);
}
void clear_TP_1(void)
{
  GPIO_PinOutClear(TP_1_PORT, TP_1_PIN);
}
void toggle_TP_1(void)
{
  GPIO_PinOutToggle(TP_1_PORT, TP_1_PIN);
}


static void init_test_pt_2(void)
{
  sl_gpio_t enable_tp_gpio =
  {
    .port = TP_2_PORT,
    .pin = TP_2_PIN,
  };
  sl_gpio_set_pin_mode(&enable_tp_gpio, SL_GPIO_MODE_PUSH_PULL, 1);
}
void set_TP_2(void)
{
  GPIO_PinOutSet(TP_2_PORT, TP_2_PIN);
}
void clear_TP_2(void)
{
  GPIO_PinOutClear(TP_2_PORT, TP_2_PIN);
}
void toggle_TP_2(void)
{
  GPIO_PinOutToggle(TP_2_PORT, TP_2_PIN);
}


static void init_test_pt_3(void)
{
  sl_gpio_t enable_tp_gpio =
  {
    .port = TP_3_PORT,
    .pin = TP_3_PIN,
  };
  sl_gpio_set_pin_mode(&enable_tp_gpio, SL_GPIO_MODE_PUSH_PULL, 1);
}
void set_TP_3(void)
{
  GPIO_PinOutSet(TP_3_PORT, TP_3_PIN);
}
void clear_TP_3(void)
{
  GPIO_PinOutClear(TP_3_PORT, TP_3_PIN);
}
void toggle_TP_3(void)
{
  GPIO_PinOutToggle(TP_3_PORT, TP_3_PIN);
}

void hal_test_pts_init(void)
{
  init_test_pt_0();
  init_test_pt_1();
  init_test_pt_2();
  init_test_pt_3();
}

/************************  TIMER 1  *******************************/
// 1747.62 mSecs per interrupt
void TIMER1_IRQHandler(void)
{
  uint32_t iflags = 0UL;

  iflags = TIMER1->IEN;
  iflags &= TIMER1->IF;

  // OV for 32 bit 1mS timer
  if(TIMER_IEN_OF & iflags)
  {
      upper_timer1_count++;
      TIMER1->IF_CLR = TIMER_IF_OF;
  }

  // Timer events
  if(TIMER_IEN_CC0 & iflags)
  {
     // Turn off interrupt.
     TIMER1->IF_CLR = TIMER_IF_CC0;
     TIMER1->IEN_CLR = TIMER_IEN_CC0;
     timer_CC0_func();
  }
  if(TIMER_IEN_CC1 & iflags)
  {
     // Turn off interrupt.
     TIMER1->IF_CLR = TIMER_IF_CC1;
     TIMER1->IEN_CLR = TIMER_IEN_CC1;
     timer_CC1_func();
  }
  if(TIMER_IEN_CC2 & iflags)
  {
     // Turn off interrupt.
     TIMER1->IF_CLR = TIMER_IF_CC2;
     TIMER1->IEN_CLR = TIMER_IEN_CC2;
     timer_CC2_func();
  }
}

// Xtal input 38.4MHz / 1024 = 37.5KHz = 26.66uS
void hal_timer1_init(void)
{

  const TIMER_Init_TypeDef init =
  {
     .enable = true,
     .debugRun = false,
     .prescale = timerPrescale1024,
     .clkSel = timerClkSelHFPerClk,
     .count2x = false,
     .ati = false,
     .rssCoist = false,
     .fallAction = timerInputActionNone,
     .riseAction = timerInputActionNone,
     .mode = timerModeUp,
     .dmaClrAct = false,
     .quadModeX4 = false,
     .oneShot = false,
     .sync = false,
     .disSyncOut = false,
  };
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_TIMER1);

  // Set up CCx
  TIMER1->CC[0].CFG = timerCCModeCompare;
  TIMER1->CC[1].CFG = timerCCModeCompare;
  TIMER1->CC[2].CFG = timerCCModeCompare;

  TIMER1->IF_CLR = _TIMER_IF_MASK;
  TIMER1->IEN_CLR = _TIMER_IEN_MASK;
  TIMER1->IEN_SET = TIMER_IEN_OF;

  TIMER_Init(TIMER1, &init);
  NVIC_EnableIRQ(TIMER1_IRQn);
}

// 38.4MHz/1024 = 37.5KHz or 26.66uS/tick of TIMER1
// Returns 32 bit free running count at 1mS per count
uint32_t get_time_ms(void)
{
  return((uint32_t)(1747.6267F * (float)upper_timer1_count + 26.66666e-3F * (float)TIMER1->CNT));
}

bool has_timed_out(uint32_t timeStamp, uint32_t delay)
{
  return((get_time_ms() - timeStamp) >= delay);
}

// Timer interrupts have 26.66uS resolution
// Max delay = 26.66uS * 2^16 = 1.748 secs
// Min delay = 26.66uS * 2 = 60uS (round up)
int8_t set_one_shot_us_timer_isr(void (*func)(void), uint32_t delay_us)
{
  int8_t int_num;
  // Check valid delay range & func; min = 60uS  max = 1.7secs
  if((60UL > delay_us) || (1700000UL < delay_us) || (NULL == func))
  {
     return(false);
  }

  uint16_t compare_value = TIMER1->CNT + (uint16_t)((float)(delay_us)/26.6666F);

  // Find open CCx
  if(0 == (TIMER_IEN_CC0 & TIMER1->IEN))
  {
     timer_CC0_func = func;
     TIMER1->CC[0].OC = compare_value;
     TIMER1->IF_CLR = TIMER_IF_CC0;
     TIMER1->IEN_SET = TIMER_IEN_CC0;
     int_num = 0;
  }
  else if(0 == (TIMER_IEN_CC1 & TIMER1->IEN))
  {
     timer_CC1_func = func;
     TIMER1->CC[1].OC = compare_value;
     TIMER1->IF_CLR = TIMER_IF_CC1;
     TIMER1->IEN_SET = TIMER_IEN_CC1;
     int_num = 1;
  }
  else if(0 == (TIMER_IEN_CC2 & TIMER1->IEN))
  {
     timer_CC2_func = func;
     TIMER1->CC[2].OC = compare_value;
     TIMER1->IF_CLR = TIMER_IF_CC2;
     TIMER1->IEN_SET = TIMER_IEN_CC2;
     int_num = 2;
  }
  else
  {
      int_num = -1;
  }
  return(int_num);
}

bool disable_one_shot_us_timer_isr(int8_t int_num)
{
  bool status = true;

  if(0 == int_num)
  {
     timer_CC0_func = NULL;
     TIMER1->IEN_CLR = TIMER_IEN_CC0;
  }
  else if(1 == int_num)
  {
     timer_CC1_func = NULL;
     TIMER1->IEN_CLR = TIMER_IEN_CC1;
  }
  else if(2 == int_num)
  {
     timer_CC2_func = NULL;
     TIMER1->IEN_CLR = TIMER_IEN_CC2;
  }
  else
  {
     status = false;
  }
  return(status);
}

#if 0
/************************  LETIMER0  *******************************/
static void (*letimer_CC0_func)(void) = NULL;
static void (*letimer_CC1_func)(void) = NULL;
void LETIMER0_IRQHandler(void)
{
  uint32_t iflags = 0UL;

  iflags = LETIMER0->IEN;
  iflags &= LETIMER0->IF;

  // Timer events
  if(LETIMER_IF_COMP0 & iflags)
  {
     // Turn off interrupt.
     LETIMER0->IF_CLR = LETIMER_IF_COMP0;
     LETIMER0->IEN_CLR = LETIMER_IEN_COMP1;
     letimer_CC0_func();
  }
  if(LETIMER_IF_COMP1 & iflags)
  {
     // Turn off interrupt.
     LETIMER0->IF_CLR = LETIMER_IF_COMP1;
     LETIMER0->IEN_CLR = LETIMER_IEN_COMP1;
     letimer_CC1_func();
  }
}

void hal_letimer_init(void)
{
  // ULFRCO = 1KHz
  const sl_hal_letimer_config_t init =
  {
     .prescaler                = SL_HAL_LETIMER_PRESCALER_DIV1,               \
     .repeat_mode              = SL_HAL_LETIMER_REPEAT_MODE_FREE,             \
     .underflow_output0_action = SL_HAL_LETIMER_UNDERFLOW_OUTPUT_ACTION_NONE, \
     .underflow_output1_action = SL_HAL_LETIMER_UNDERFLOW_OUTPUT_ACTION_NONE, \
     .debug_run                = true,                                       \
     .enable_top               = true,                                       \
     .buffered_top             = false,                                       \
     .output0_polarity         = false,                                       \
     .output1_polarity         = false
  };
  //sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_ULFRCO);
  //sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_LETIMER0);
  CMU_ClockEnable(cmuClock_LETIMER0, true);
  //CMU->EM23GRPACLKCTRL = 3;   // src = ULFRCO
  LETIMER0->EN_SET = LETIMER_EN_EN;
  LETIMER0->CTRL = 0UL;       // free running
  LETIMER0->CMD_SET = LETIMER_CMD_START;

  LETIMER0->IF_CLR = _TIMER_IF_MASK;
  LETIMER0->IEN_CLR = _TIMER_IEN_MASK;

  //sl_hal_letimer_init(LETIMER0, &init);
  sl_hal_letimer_enable(LETIMER0);
  sl_hal_letimer_start(LETIMER0);
  NVIC_EnableIRQ(LETIMER0_IRQn);
}

int8_t set_one_shot_letimer_isr(void (*func)(void), uint32_t delay_ms)
{
  int8_t int_num;
  // Check valid delay range & func; min = 300uS  max = 1.7secs
  if((1UL > delay_ms) || (170000UL < delay_ms) || (NULL == func))
  {
      sl_zigbee_app_debug_print("LE 1");
     return(false);
  }

  // Down counter
  //uint32_t compare_value = (LETIMER0->CNT - delay_ms) & 0x0FFFFFFFUL;
  uint32_t compare_value = LETIMER0->CNT;
  sl_zigbee_app_debug_print("LE 0x%X  0x%X", sl_hal_letimer_get_counter(LETIMER0), compare_value );

  // Find open CCx
  if(0 == (LETIMER_IEN_COMP0 & LETIMER0->IEN))
  {
     letimer_CC0_func = func;
     LETIMER0->IF_CLR = LETIMER_IF_COMP0;
     LETIMER0->IEN_CLR = LETIMER_IEN_COMP0;
     int_num = 0;
  }
  else if(0 == (LETIMER_IEN_COMP1 & TIMER1->IEN))
  {
     letimer_CC1_func = func;
     LETIMER0->COMP0 = compare_value;
     LETIMER0->IF_CLR = LETIMER_IF_COMP1;
     LETIMER0->IEN_CLR = LETIMER_IEN_COMP1;
     int_num = 1;
  }
  else
  {
      int_num = -1;
  }
  return(int_num);
}

void letimer_func(void)
{

  sl_zigbee_app_debug_print("1 ");
}
void unit_test_letimer(void)
{

  set_one_shot_letimer_isr(letimer_func, 25UL);

}
#endif  // letimer

#if 0
/************************  TIMER 3  *******************************/
void hal_timer3_init(void)
{
  // 38.4MHz
  const TIMER_Init_TypeDef init =
  {
     .enable = true,
     .debugRun = false,
     .prescale = timerPrescale32,
     .clkSel = timerClkSelHFPerClk,
     .count2x = false,
     .ati = false,
     .rssCoist = false,
     .fallAction = timerInputActionNone,
     .riseAction = timerInputActionNone,
     .mode = timerModeUp,
     .dmaClrAct = false,
     .quadModeX4 = false,
     .oneShot = false,
     .sync = false,
     .disSyncOut = false,
  };
  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_TIMER3);

  TIMER3->IF_CLR = _TIMER_IF_MASK;
  TIMER3->IEN_CLR = _TIMER_IEN_MASK;
  //TIMER3->IEN_SET = TIMER_IEN_OF;

  TIMER_Init(TIMER3, &init);
  //NVIC_EnableIRQ(TIMER1_IRQn);
}
// 38.4MHz/32 = 1.2MHz or 833.3nS/tick of TIMER3
// Returns 32 bit free running count at  per count
uint32_t get_time_1us(void)
{
  return((uint32_t)(TIMER3->CNT));
}

bool has_timed_out_uS(uint32_t timeStamp, uint32_t delay)
{
  return((get_time_ms() - timeStamp) >= delay);
}

//sl_token_init();
uint32_t get_app_token_flags(void)
{
  int32_t data = 0UL;
  //sl_token_get_data(TOKEN_APP_NV_FLAGS, 0, &data, sizeof(data));
  //if(SL_STATUS_OK != sl_token_get_data(TOKEN_APP_MFG_MODE_BOOST_COUNTER, 0, &data, sizeof(data)))
  {
      sl_zigbee_app_debug_println("FAIL TOKEN");
  }

  return(data);
}
/*
if(EMBER_SUCCESS == sl_token_get_data(TOKEN_APP_MFG_MODE_BOOST_COUNTER, 0,
                                         &nv_mfg_mode_boost_counter, sizeof(nv_mfg_mode_boost_counter)))
  sl_status_t sl_token_get_data(uint32_t token,
                                uint32_t index,
                                void *data,
                                uint32_t length);
sl_status_t sl_token_set_data(uint32_t token,
                              uint32_t index,
                              void *data,
                              uint32_t length);
                              */
#endif
