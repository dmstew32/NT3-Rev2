#include "sl_event_handler.h"

#include "sl_clock_manager.h"
#include "sl_hfxo_manager.h"
#include "pa_conversions_efr32.h"
#include "sl_rail_util_power_manager_init.h"
#include "sl_rail_util_pti.h"
#include "sl_rail_util_rf_path.h"
#include "sl_zigbee_system_common.h"
#include "btl_interface.h"
#include "sl_debug_swo.h"
#include "sl_gpio.h"
#include "sl_i2cspm_instances.h"
#include "sl_iostream_debug.h"
#include "sl_iostream_init_usart_instances.h"
#include "sl_mbedtls.h"
#include "sl_pwm_instances.h"
#include "eeprom.h"
#include "sl_cli_instances.h"
#include "psa/crypto.h"
#include "sl_se_manager.h"
#include "sl_iostream_init_instances.h"
#include "sl_token_manager_api.h"
#include "nvm3_default.h"
#include "sl_iostream_handles.h"

void sli_driver_permanent_allocation(void)
{
}

void sli_service_permanent_allocation(void)
{
}

void sli_stack_permanent_allocation(void)
{
  sli_legacy_buffer_manager_initialize_buffers();
}

void sli_internal_permanent_allocation(void)
{
}

void sl_platform_init(void)
{
  sl_clock_manager_runtime_init();
  sl_hfxo_manager_init_hardware();
  bootloader_init();
  nvm3_initDefault();
}

void sli_internal_init_early(void)
{
}

void sl_driver_init(void)
{
  sl_debug_swo_init();
  sl_gpio_init();
  sl_i2cspm_init_instances();
  sl_pwm_init_instances();
  sl_util_af_eeprom_init();
}

void sl_service_init(void)
{
  sl_hfxo_manager_init();
  sl_mbedtls_init();
  psa_crypto_init();
  sl_se_init();
  sl_iostream_init_instances_stage_1();
  sl_iostream_init_instances_stage_2();
  sl_cli_instances_init();
  sl_token_manager_init();
}

void sl_stack_init(void)
{
  sl_rail_util_pa_init();
  sl_rail_util_power_manager_init();
  sl_rail_util_pti_init();
  sl_rail_util_rf_path_init();
  sli_zigbee_stack_init_callback();
  sli_zigbee_app_framework_init_callback();
  sli_zigbee_stack_sleep_init();
  sli_zigbee_app_framework_sleep_init();
}

void sl_internal_app_init(void)
{
}

void sli_platform_process_action(void)
{
}

void sli_service_process_action(void)
{
  sl_cli_instances_tick();
}

void sli_stack_process_action(void)
{
  sli_zigbee_stack_tick_callback();
  sli_zigbee_app_framework_tick_callback();
}

void sli_internal_app_process_action(void)
{
}

void sl_iostream_init_instances_stage_1(void)
{
  sl_iostream_debug_init();
  sl_iostream_usart_init_instances();
}

void sl_iostream_init_instances_stage_2(void)
{
  sl_iostream_set_console_instance();
}

