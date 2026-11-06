#ifndef DH_DEBUG_H_
#define DH_DEBUG_H_

/*
 * Daylight harvesting debug: each time the master sends a DH level, it also sends
 * one unicast to the debug target, normally a Twitch that prints it on its uart.
 *
 * Cluster ZCL_ADAPTIV_DEBUG_CLUSTER_ID (0xFCDB), command ZCL_ADAPTIV_DEBUG_COMMAND_ID
 * (0xDB), manufacturer 0x1574, endpoint 1 to endpoint 1.  All little endian:
 *
 *   0  u32  lower 4 bytes of this light's EUI64
 *   4  u32  uptime, seconds
 *   8  u16  current lux
 *  10  u16  target lux
 *  12  u8   current level (when the DH level is sent, so before that level's ramp)
 *  13  u8   mode flags, bit 0 = master
 *  14  u8   new level, the one the DH level message just sent to the group
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

void dh_debug_set_target(uint16_t node_id);
uint16_t dh_debug_get_target(void);

// True = handed to the stack
bool dh_debug_send(uint16_t lux, uint16_t target_lux, uint8_t level, uint8_t mode_flags, uint8_t new_level);

// Incoming ZCL_ADAPTIV_DEBUG_COMMAND_ID
bool dh_debug_rx(sl_zigbee_af_cluster_command_t* cmd);

#endif /* DH_DEBUG_H_ */
