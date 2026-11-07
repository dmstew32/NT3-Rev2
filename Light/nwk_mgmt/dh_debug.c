#include "af.h"
#include "hal_utils.h"
#include "dh_debug.h"

#include "sl_zigbee_debug_print.h"

static uint16_t debug_target = DH_DEBUG_TARGET_NONE;

void dh_debug_set_target(uint16_t node_id)
{
  debug_target = node_id;
  sl_zigbee_app_debug_println("DHS debug target 0x%04X   <%d>", node_id, get_time_ms());
}

uint16_t dh_debug_get_target(void)
{
  return(debug_target);
}

bool dh_debug_send(uint16_t lux, uint16_t target_lux, uint8_t level, uint8_t mode_flags, uint8_t new_level)
{
  uint8_t message[5U + DH_DEBUG_PAYLOAD_LENGTH] = {0};
  sl_802154_long_addr_t eui64;
  uint32_t uptime_s = get_time_ms() / 1000UL;
  sl_zigbee_aps_frame_t apsFrame =
  {
     .profileId = HA_PROFILE_ID,
     .clusterId = ZCL_ADAPTIV_DEBUG_CLUSTER_ID,
     .sourceEndpoint = LIGHT_ENDPOINT,
     .destinationEndpoint = LIGHT_ENDPOINT,
     .options = (sl_zigbee_aps_option_t)(SL_ZIGBEE_APS_OPTION_RETRY | SL_ZIGBEE_APS_OPTION_ENABLE_ROUTE_DISCOVERY),
     .radius = ZA_MAX_HOPS,
     .groupId = 0,
  };

  // Includes 0xFFFE (no target) and the broadcast addresses
  if(DH_DEBUG_FIRST_NON_UNICAST <= debug_target)
  {
     return(false);
  }

  sl_zigbee_af_get_eui64(eui64);

  message[0] = (ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER |
                ZCL_MANUFACTURER_SPECIFIC_MASK | ZCL_DISABLE_DEFAULT_RESPONSE_MASK);
  message[1] = GET_LSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[2] = GET_MSB_FROM_16(ADAPTIV_ZIGBEE_MFG_ID);
  message[3] = sl_zigbee_af_next_sequence();
  message[4] = ZCL_ADAPTIV_DEBUG_COMMAND_ID;
  // Payload.  The EUI64 is stored low byte first.
  message[5] = eui64[0];
  message[6] = eui64[1];
  message[7] = eui64[2];
  message[8] = eui64[3];
  message[9] = (uint8_t)(uptime_s & 0xFFUL);
  message[10] = (uint8_t)((uptime_s >> 8) & 0xFFUL);
  message[11] = (uint8_t)((uptime_s >> 16) & 0xFFUL);
  message[12] = (uint8_t)((uptime_s >> 24) & 0xFFUL);
  message[13] = GET_LSB_FROM_16(lux);
  message[14] = GET_MSB_FROM_16(lux);
  message[15] = GET_LSB_FROM_16(target_lux);
  message[16] = GET_MSB_FROM_16(target_lux);
  message[17] = level;
  message[18] = mode_flags;
  message[19] = new_level;

  sl_status_t status = sl_zigbee_af_send_unicast(SL_ZIGBEE_OUTGOING_DIRECT,
                                                 debug_target,
                                                 &apsFrame,
                                                 (uint16_t)sizeof(message),
                                                 message);
  if(SL_STATUS_OK != status)
  {
     sl_zigbee_app_debug_println("ERR DHS debug 0x%X", status);
     return(false);
  }
  return(true);
}

bool dh_debug_rx(sl_zigbee_af_cluster_command_t* cmd)
{
  uint16_t base_idx = cmd->payloadStartIndex;
  uint16_t length = (cmd->bufLen > base_idx) ? (uint16_t)(cmd->bufLen - base_idx) : (uint16_t)0;
  const uint8_t *p = &cmd->buffer[base_idx];

  if(DH_DEBUG_PAYLOAD_LENGTH == length)
  {
     // Another light's debug data, this light is the target
     sl_zigbee_app_debug_print("[NT3 0x%02X%02X%02X%02X, up:%d, ", p[3], p[2], p[1], p[0],
                               (uint32_t)p[4] | ((uint32_t)p[5] << 8) | ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24));
     sl_zigbee_app_debug_println("lux:%d, target:%d, level:%d, mode:%d, new:%d]",
                                 (uint16_t)p[8] | ((uint16_t)p[9] << 8),
                                 (uint16_t)p[10] | ((uint16_t)p[11] << 8), p[12], p[13], p[14]);
  }
  else if((DH_DEBUG_SET_TARGET_LENGTH == length) && (SINK_SHORT_ADDR == p[0]))
  {
     dh_debug_set_target((uint16_t)p[1] | ((uint16_t)p[2] << 8));
  }
  return(true);
}
