// Startup of the button code under test, as main.c does it.
#ifdef SIM_REFERENCE
sl_status_t hal_buttons_init(void);

void sim_variant_init(void)
{
  hal_buttons_init();
}
#else
#include "hal_buttons.h"
#include "button_actions.h"

void sim_variant_init(void)
{
  hal_buttons_init();
  button_actions_on_startup();
}
#endif
