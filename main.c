/***************************************************************************//**
 * @file main.c
 * @brief main() function.
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "sl_component_catalog.h"
#include "sl_main_init.h"
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif
#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "sl_main_kernel.h"
#else // SL_CATALOG_KERNEL_PRESENT
#include "sl_main_process_action.h"
#endif // SL_CATALOG_KERNEL_PRESENT

#ifdef SL_ZIGBEE_TEST
int nodeMain(void)
#else
#if defined(__ICCARM__)
#pragma diag_suppress=Pe111
#endif // defined(__ICCARM__)

#include "af.h"
#include "RehTOS.h"
#include "hal_leds.h"
#include "hal_pwm.h"
#include "hal_utils.h"
#include "hal_buttons.h"
#include "button_actions.h"

#include "hal_light_sensor.h"
#include "light_sensor.h"
#include "hal_occ_sensor.h"
#include "occ_sensor.h"
#include "light_on_off.h"
#include "sl_token_manager.h"
#include "nwk_utils.h"
#include "auto_comm.h"
#include "factory_reset.h"
#include "nwk_commission.h"
#include "adaptiv_config.h"
#include "nwk_groups.h"
#include "sl_zigbee_debug_print.h"

static void power_event_occurred(sl_power_manager_em_t from, sl_power_manager_em_t to);
void on_off_count_task(taskParm_t parm0, taskParm_t parm1);

static bool app_init_done_flag = false;

static sl_power_manager_em_transition_event_handle_t power_event_handle;
static sl_power_manager_em_transition_event_info_t power_event_info = {
  .event_mask =  (SL_POWER_MANAGER_EVENT_TRANSITION_ENTERING_EM0 | SL_POWER_MANAGER_EVENT_TRANSITION_LEAVING_EM0
      | SL_POWER_MANAGER_EVENT_TRANSITION_ENTERING_EM2 | SL_POWER_MANAGER_EVENT_TRANSITION_LEAVING_EM2),
  .on_event = power_event_occurred,
};


bool app_init_done(void)
{
  return(app_init_done_flag);
}

void app_init(void)
{
  initTasker();
  sl_zigbee_app_debug_println("App init {");
  hal_leds_init();
  hal_set_green_led(1);
  hal_set_red_led(1);
  hal_pwm_init();
  hal_test_pts_init();
  hal_timer1_init();
  hal_buttons_init();
  button_actions_on_startup();
  hal_watchdog_init(false);
  occ_sensor_init();
  light_sensor_init();
  power_on_set_light_level();
  inc_reset_counter();
  check_adaptiv_auto_commission();
  zcl_commissioning_init();
  write_firmware_version();
  write_date_version();
  update_bindingTable_address();
  sl_zigbee_set_radio_power(DEFAULT_RADIO_PWR);
  check_switch_network();
  init_sink_debug();
  sl_zigbee_app_debug_println("} App init done v%s   <%d>\n", APP_FIRMWARE_VERSION, get_time_ms());
  app_init_done_flag = true;

  // Start checking for on/off x5
  taskParm_t arg0, arg1;
  arg0.u32 = get_time_ms();
  arg1.u8 = 0;
  addTask(on_off_count_task, arg0, arg1 );

  //spew_app_info();
  hal_set_green_led(0);
  hal_set_red_led(0);

  // No sleep
  sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM0);
  sl_power_manager_subscribe_em_transition_event(&power_event_handle, &power_event_info);
}

void app_process_action(void)
{
  pet_the_dog();

  taskerTick();

#if 0
  if(is_light_level_active())
      {
      sl_zigbee_app_debug_println("level %d", get_time_ms());
      }
#endif
#if 0
  // Timer based debug
  if(true)
  {
    static uint32_t timeStamp = 0UL;
    static uint32_t count = 00UL;

    if(has_timed_out( timeStamp, 5000UL))
    {
        timeStamp = get_time_ms();
        count++;
        if(count & 0x01UL)
        {
            uint8_t data[2] = {0};
            data[0] = (uint8_t)(count & 0x0FUL);
            data[1] = 0;
            write_occupancy_sensor_attribute(ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_DELAY_ATTRIBUTE_ID, (uint8_t *)&data);
            sl_zigbee_app_debug_println("wrt %d",data[0]);
        }
        else
          {
            sl_zigbee_app_debug_println("rd %d",read_occupancy_sensor_attribute(ZCL_PIR_UNOCCUPIED_TO_OCCUPIED_DELAY_ATTRIBUTE_ID));
          }
        //assert(false);
    }
  }
#endif
}



int main(void)
{
#endif  // SL_ZIGBEE_TEST
  // Initialize Silicon Labs device, system, service(s) and protocol stack(s).
  // Note that if the kernel is present, the start task will be started and software component
  // configuration will take place there.
  sl_main_init();

#if defined(SL_CATALOG_KERNEL_PRESENT)
  // Start the kernel. The start task will be executed (Highest priority) to complete
  // the Simplicity SDK components initialization and the user app_init() hook function will be called.
  sl_main_kernel_start();
#else // SL_CATALOG_KERNEL_PRESENT

  // User provided code.
  app_init();

  while (1) {
    // Silicon Labs components process action routine
    // must be called from the super loop.
    sl_main_process_action();

    // User provided code. Application process.
    app_process_action();

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    // Let the CPU go to sleep if the system allows it.
    sl_power_manager_sleep();
#endif
  }
#endif // SL_CATALOG_KERNEL_PRESENT
}

// Determine if the power has been turned on/off 5 times in the
// required time period
void on_off_count_task(taskParm_t parm0, taskParm_t parm1)
{
  static bool meets_on_time = false;

  if(!meets_on_time && has_timed_out(parm0.u32, 2000UL))
  {
     meets_on_time = true;
     inc_on_off_counter();
  }
  else if(has_timed_out(parm0.u32, 8000UL))
  {
     reset_on_off_counter();
     removeTask(on_off_count_task);
  }
  else if(5 <= get_on_off_counter())
  {
     force_factory_reset(1000UL, eRESET_NORMAL);
     removeTask(on_off_count_task);
  }

  else
  {

  }
}

static void power_event_occurred(sl_power_manager_em_t from, sl_power_manager_em_t to)
{
  sl_zigbee_app_debug_println("\n\n########## POWER MODE  From %d to %d  ##########\n\n", from, to);
}

