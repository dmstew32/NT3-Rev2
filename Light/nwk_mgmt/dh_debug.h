#ifndef DH_DEBUG_H_
#define DH_DEBUG_H_

/*
 * Daylight harvesting debug: any light with a debug target sends one unicast to it,
 * normally a Twitch that prints it on its uart.  Checked every 20 s: sent if the
 * light's level changed since the last packet, otherwise every 120 s.
 *
 * Cluster ZCL_ADAPTIV_DEBUG_CLUSTER_ID (0xFCDB), command ZCL_ADAPTIV_DEBUG_COMMAND_ID
 * (0xDB), manufacturer 0x1574, endpoint 1 to endpoint 1.  All little endian:
 *
 *   0  u32  lower 4 bytes of this light's EUI64
 *   4  u32  uptime, seconds
 *   8  u16  current lux, the average of the last 2 readings
 *  10  u16  target lux
 *  12  u8   current level
 *  13  u8   mode flags, bit 0 = master, bit 1 = DH disabled
 *  14  u8   last level this light sent to the group as master, 0 = none since a restart
 *
 * Nothing is sent until a target is set.  The target is only one of the addresses
 * below 0xFFFC, and it is not saved: after a restart there is none.
 *
 * Setting the target: the same cluster and command, unicast or multicast to the
 * light, with the payload 0xAA (SINK_SHORT_ADDR), then the address, little endian.
 * Address 0xFFFE stops the debug messages.  The two payloads differ by length.
 */

#include <stdbool.h>
#include <stdint.h>

#define DH_DEBUG_TARGET_NONE      (0xFFFEU)
#define DH_DEBUG_FIRST_NON_UNICAST (0xFFFCU)
#define DH_DEBUG_PAYLOAD_LENGTH   (15U)
#define DH_DEBUG_SET_TARGET_LENGTH (3U)
#define DH_DEBUG_MODE_MASTER      (1U << 0)
#define DH_DEBUG_MODE_DH_DISABLED (1U << 1)
#define DH_DEBUG_CHECK_READINGS   (8U)    /* 8 x 2.5 s = 20 s */
#define DH_DEBUG_QUIET_CHECKS     (6U)    /* 6 x 20 s = 120 s with no level change */

void dh_debug_set_target(uint16_t node_id);
uint16_t dh_debug_get_target(void);

// True = handed to the stack
bool dh_debug_send(uint16_t lux, uint16_t target_lux, uint8_t level, uint8_t mode_flags, uint8_t new_level);

// Incoming ZCL_ADAPTIV_DEBUG_COMMAND_ID
bool dh_debug_rx(sl_zigbee_af_cluster_command_t* cmd);

#endif /* DH_DEBUG_H_ */
