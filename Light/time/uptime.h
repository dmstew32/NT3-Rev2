#ifndef _UPTIME_H_
#define _UPTIME_H_

#include <stdint.h>

uint32_t uptime_get_seconds(void);
uint64_t uptime_get_ms64(void);
uint32_t uptime_get_ms32(void);


#endif // #ifndef _UPTIME_H_
