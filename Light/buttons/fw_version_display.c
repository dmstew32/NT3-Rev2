#include "app/framework/include/af.h"
#include "RehTOS.h"
#include "hal_utils.h"
#include "hal_leds.h"
#include "app_leds.h"
#include "hal_occ_sensor.h"
#include "hal_pwm.h"
#include "nwk_utils.h"
#include "fw_version_display.h"

#ifdef ALLOW_DISPLAY_VERSION

#define FW_VERSION_LED_ON_OFF_TIME    APP_LEDS_VERSION_BLINK_MS

static uint8_t original_light_level;

// Restore states
static void end_display_version_task(taskParm_t parm0, taskParm_t parm1)
{
  // Wait for leds to be done
  if(has_timed_out(parm0.u32, parm1.u32))
  {
     removeTask(end_display_version_task);
     hal_set_dim_pwm_255(original_light_level);
     hal_start_occ_sensor();  // Allows green led
     set_app_flags_nonvol((B_DISABLE_OCC_SENSOR | B_DISABLE_LIGHT_SENSOR), 0);
  }
}
// Minor version on red led
static void display_minor_version_task(taskParm_t parm0, taskParm_t parm1)
{
  // Wait for leds to be done
  if(has_timed_out(parm0.u32, parm1.u32))
  {
     uint32_t minor_fw_version = 0x00FFUL & APP_FIRMWARE_VERSION_U32;
     taskParm_t arg0, arg1;

     removeTask(display_minor_version_task);

     if(10 <= minor_fw_version)
     {
        minor_fw_version = 10UL;
     }

     app_leds_show_version_digit(APP_LED_RED, minor_fw_version);

     // Time to delay till next version
     arg0.u32 = get_time_ms();
     arg1.u32 = (minor_fw_version * FW_VERSION_LED_ON_OFF_TIME * 2UL) + 1000UL;

     addTask(end_display_version_task, arg0, arg1);
  }
}

// Middle version on green led
static void display_middle_version_task(taskParm_t parm0, taskParm_t parm1)
{
  // Wait for leds to be done
  if(has_timed_out(parm0.u32, parm1.u32))
  {
     uint32_t middle_fw_version = 0x00FFUL & (APP_FIRMWARE_VERSION_U32 >> 8);
     taskParm_t arg0, arg1;

     removeTask(display_middle_version_task);

     if(10 < middle_fw_version)
     {
        middle_fw_version = 10UL;
     }

     app_leds_show_version_digit(APP_LED_GREEN, middle_fw_version);

     // Time to delay till next version
     arg0.u32 = get_time_ms();
     arg1.u32 = (middle_fw_version * FW_VERSION_LED_ON_OFF_TIME * 2UL) + 750UL;
     addTask(display_minor_version_task, arg0, arg1);
  }
}

// Major version on red led
static void display_major_version_task(taskParm_t parm0, taskParm_t parm1)
{
  app_leds_off();
  hal_set_dim_pwm_255(0);

  // Wait for light to be off
  if(has_timed_out(parm0.u32, 1500UL))
  {
     uint32_t major_fw_version = 0x00FFUL & (APP_FIRMWARE_VERSION_U32 >> 16);
     taskParm_t arg0, arg1;

     removeTask(display_major_version_task);

     if(10 < major_fw_version)
     {
        major_fw_version = 10UL;
     }

     app_leds_show_version_digit(APP_LED_RED, major_fw_version);

     // Time to delay till next version
     arg0.u32 = get_time_ms();
     arg1.u32 = (major_fw_version * FW_VERSION_LED_ON_OFF_TIME * 2UL) + 750UL;
     addTask(display_middle_version_task, arg0, arg1);
  }
}

void start_display_fw_version(void)
{
  taskParm_t arg0, arg1;

  original_light_level = hal_get_dim_pwm_255();
  set_app_flags_nonvol((B_DISABLE_OCC_SENSOR | B_DISABLE_LIGHT_SENSOR), 1);
  hal_stop_occ_sensor();  // Keeps green led off
  app_leds_off();
  hal_set_dim_pwm_255(0);

  arg0.u32 = get_time_ms();
  arg1.u8 = 0;
  addTask(display_major_version_task, arg0, arg1);
}

#endif // ALLOW_DISPLAY_VERSION
