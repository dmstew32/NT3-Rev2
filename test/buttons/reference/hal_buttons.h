#ifndef HAL_BUTTONS_H_
#define HAL_BUTTONS_H_

#define BUTTON_0_PORT   gpioPortB
#define BUTTON_0_PIN    1
#define BUTTON_0_INT_NO (1UL) /* Base + pin = 0 + 1 */
#define BUTTON_0_INT_FLAG (1UL << BUTTON_0_INT_NO)

#define BUTTON_1_PORT   gpioPortD
#define BUTTON_1_PIN    3
#define BUTTON_1_INT_NO (3UL) /* Base + pin = 0 + 3 */
#define BUTTON_1_INT_FLAG (1UL << BUTTON_1_INT_NO)


#define DEBOUNCE_TIME_MS  (75UL)
#define BUTTONS_DONE_US   (1500000UL) /* No presses for this time, then done */

#define BUTTONS_POLL_TIME_MS          (1005UL)
#define BUTTONS_ACTIVE_POLL_TIME_MS   (200UL)

#define BUTTON_0_INITIAL_PRESS_TIME_MS  (5000UL-1000UL)
#define BUTTON_0_CYCLE_TIME_MS          (2000UL)
#define BUTTON_1_INITIAL_PRESS_TIME_MS  (5000UL-1000UL)

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
#define FW_VERSION_LED_ON_OFF_TIME    (500UL)

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

typedef struct
{
  uint8_t edges;       // Copy of isr edges in Non-isr context
  uint8_t edges_isr;   // Incs in isr only
  uint32_t time_stamp; // Last change
  uint32_t time_stamp_isr; // Last change
  bool active;
}button_info_t;

sl_status_t hal_buttons_init(void);
bool get_button_presses(uint8_t *button_0, uint8_t *button_1);


#endif /* HAL_BUTTONS_H_ */
