#ifndef _NETWORK_DEFINES_H_
#define _NETWORK_DEFINES_H_

#include "sl_zigbee_types.h"

#define INVALID_ADDR16          SL_ZIGBEE_NULL_NODE_ID
#define INVALID_CLUSTER_ID      0xFFFF
#define INVALID_DEVICE_ID       0xFFFF
#define INVALID_ENDPOINT_ID     0xFF
#define INVALID_GROUP_ID        0xFFFF

#define INVALID_MFG_CODE        0xFFFF

#define APP_ENDPOINT            0x01

#define DEFAULT_REMOTE_ENDPOINT  0x01   // At some point, this will need to be discovered, not assumed

#define ADAPTIV_NETWORK_PING_CLUSTER_ID 0xF01D
#define ADAPTIV_NETWORK_PING_PROFILE_ID 0xC3D0

#endif  // #ifndef _NETWORK_DEFINES_H_
