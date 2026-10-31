
#include "uptime.h"
#include "hal/hal.h"

uint32_t uptime_get_seconds()
{
    return (uint32_t)(uptime_get_ms64() / 1000);
}

uint64_t uptime_get_ms64()
{
    return halCommonGetInt64uMillisecondTick();
}

uint32_t uptime_get_ms32()
{
    return halCommonGetInt32uMillisecondTick();
}
