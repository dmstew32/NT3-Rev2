#ifndef _FSM_EVENT_SCHEDULER_H_
#define _FSM_EVENT_SCHEDULER_H_

#include "af.h"

typedef void(*event_handler)(sl_zigbee_af_event_t *p_event);

void fsm_event_scheduler_init_event(sl_zigbee_af_event_t *p_event, event_handler handler);
void fsm_event_scheduler_init_isr_event(sl_zigbee_af_event_t *p_event, event_handler handler);

void fsm_event_scheduler_schedule_event(sl_zigbee_af_event_t *p_event, uint32_t timeout_ms);
void fsm_event_scheduler_set_active(sl_zigbee_af_event_t *p_event);
void fsm_event_scheduler_stop_event(sl_zigbee_af_event_t *p_event);
bool fsm_event_scheduler_is_event_active(sl_zigbee_af_event_t *p_event);
uint32_t fsm_event_scheduler_ms_till_event(sl_zigbee_af_event_t *p_event);

#endif // #ifndef _FSM_EVENT_SCHEDULER_H_
