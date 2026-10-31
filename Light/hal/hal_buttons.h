#ifndef HAL_BUTTONS_H_
#define HAL_BUTTONS_H_

/*
 * Button hardware layer: pins, interrupt, debounce and press counting.
 *
 * BUTTON 0 also known as T
 * BUTTON 1 also known as D, closest to sensor
 *
 * A press counts when the button is released after being low for at least
 * DEBOUNCE_TIME_MS.  A press sequence ends when both buttons have been
 * released for BUTTONS_DONE_US; the registered sequence callback then gets
 * the press counts (main context).
 *
 * Gestures (holds, press-count actions) are in Light/buttons/.
 */

#include <stdbool.h>
#include <stdint.h>
#include "sl_status.h"

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

// 0 = no debug   1 = some
#ifndef BUTTON_PRINT
#define BUTTON_PRINT 1//0    /* Default to 0 */
#elif (BUTTON_PRINT != 0)
#warning "BUTTON_PRINT active"
#endif

typedef enum
{
  HAL_BUTTON_T = 0,   // BUTTON 0
  HAL_BUTTON_D = 1,   // BUTTON 1
  HAL_BUTTON_COUNT
} hal_button_t;

typedef struct
{
  uint8_t presses[HAL_BUTTON_COUNT];        // valid presses in the sequence
  uint32_t last_press_ms[HAL_BUTTON_COUNT]; // get_time_ms() of the last press (falling edge)
} hal_buttons_sequence_t;

typedef void (*hal_buttons_sequence_cb_t)(const hal_buttons_sequence_t *p_sequence);

sl_status_t hal_buttons_init(void);
void hal_buttons_register_sequence_cb(hal_buttons_sequence_cb_t cb);
bool hal_button_is_pressed(hal_button_t button);

#endif /* HAL_BUTTONS_H_ */
