#ifndef _BROADCAST_MONITOR_H_
#define _BROADCAST_MONITOR_H_

typedef enum
{
    BROADCASTING_UNKNOWN,
    BROADCASTING_LIGHT,
    BROADCASTING_MEDIUM,
    BROADCASTING_HEAVY,
} broadcast_traffic_t;

broadcast_traffic_t broadcast_monitor_get_traffic(void);
void broadcast_monitor_print(void);

#endif  // #ifndef _BROADCAST_MONITOR_H_
