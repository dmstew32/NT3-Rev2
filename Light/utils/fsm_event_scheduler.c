
#include "fsm_event_scheduler.h"

void fsm_event_scheduler_init_event(sl_zigbee_af_event_t *p_event, event_handler handler)
{
    sl_zigbee_af_event_init(p_event, handler);
}

void fsm_event_scheduler_init_isr_event(sl_zigbee_af_event_t *p_event, event_handler handler)
{
    sl_zigbee_af_isr_event_init(p_event, handler);
}

void fsm_event_scheduler_schedule_event(sl_zigbee_af_event_t *p_event, uint32_t timeout_ms)
{
    sl_zigbee_af_event_set_delay_ms(p_event, timeout_ms);
}

void fsm_event_scheduler_set_active(sl_zigbee_af_event_t *p_event)
{
    sl_zigbee_af_event_set_active(p_event);
}

void fsm_event_scheduler_stop_event(sl_zigbee_af_event_t *p_event)
{
    sl_zigbee_af_event_set_inactive(p_event);
}

bool fsm_event_scheduler_is_event_active(sl_zigbee_af_event_t *p_event)
{
    return sl_zigbee_af_event_is_scheduled(p_event);
}

uint32_t fsm_event_scheduler_ms_till_event(sl_zigbee_af_event_t *p_event)
{
    // returns -1 if not scheduled
    return sl_zigbee_af_event_get_remaining_ms(p_event);
}
