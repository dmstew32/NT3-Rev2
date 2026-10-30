/***************************************************************************//**
 * @file sl_zigbee_dynamic_commissioning_baremetal_wrapper.c
 * @brief internal implementations for 'sl_zigbee_dynamic_commissioning' as a thin-wrapper
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
// automatically generated from sl_zigbee_dynamic_commissioning.h.  Do not manually edit
#include "stack/include/sl_zigbee_dynamic_commissioning.h"
#include "stack/internal/inc/sl_zigbee_dynamic_commissioning_internal_def.h"

#ifdef ADAPTIV_LIGHT
#include "sl_zigbee_debug_print.h"  //JGR

#endif // ADAPTIV_LIGHT
void sl_zigbee_device_interview_status_update(sl_zigbee_address_info *ids,
                                              sl_zigbee_dynamic_commissioning_event_t event)
{
#ifdef ADAPTIV_LIGHT
  sl_zigbee_app_debug_println("sl_zigbee_device_interview_status_update ");//HGR

#endif // ADAPTIV_LIGHT
  sli_zigbee_stack_device_interview_status_update(ids,
                                                  event);
}

bool sl_zigbee_dynamic_commissioning_is_open_for_interview(void)
{
#ifdef ADAPTIV_LIGHT
  sl_zigbee_app_debug_println("sl_zigbee_dynamic_commissioning_is_open_for_interview ");//JGR

#endif // ADAPTIV_LIGHT
  return sli_zigbee_stack_dynamic_commissioning_is_open_for_interview();
}

void sl_zigbee_dynamic_commissioning_set_open_for_interview(bool open_interview)
{
#ifdef ADAPTIV_LIGHT
  sl_zigbee_app_debug_println("sl_zigbee_dynamic_commissioning_set_open_for_interview ");//JGR

#endif // ADAPTIV_LIGHT
  sli_zigbee_stack_dynamic_commissioning_set_open_for_interview(open_interview);
}
