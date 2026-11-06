#include "af.h"
#include "ota_diag.h"
#include "broadcast_monitor.h"
#include "nwk_utils.h"

#if (OTA_DIAG_ENABLED == 1)

#define OTA_DIAG_CMD_ID        0xD5
#define OTA_DIAG_RADIUS        2       // for a sniffer near the light, not the whole network
#define OTA_DIAG_ZCL_HDR_LEN   5       // frame control, mfg code (2), sequence, command

static uint16_t m_counters_start[SL_ZIGBEE_COUNTER_TYPE_COUNT];
static uint8_t m_bootload_state;

void ota_diag_tx_download_start(void)
{
  ota_diag_download_start(halCommonGetInt32uMillisecondTick());
  sl_zigbee_read_counters(m_counters_start, SL_ZIGBEE_COUNTER_TYPE_COUNT);
}

void ota_diag_tx_set_bootload_state(uint8_t state)
{
  m_bootload_state = state;
}

bool ota_diag_tx_send(void)
{
  uint16_t now_counters[SL_ZIGBEE_COUNTER_TYPE_COUNT] = { 0 };
  sl_zigbee_read_counters(now_counters, SL_ZIGBEE_COUNTER_TYPE_COUNT);
  #define DELTA(type_) ((uint16_t)(now_counters[SL_ZIGBEE_COUNTER_##type_] - m_counters_start[SL_ZIGBEE_COUNTER_##type_]))
  ota_diag_counters_t counters =
  {
    .mac_retry   = DELTA(MAC_TX_UNICAST_RETRY),
    .mac_fail    = DELTA(MAC_TX_UNICAST_FAILED),
    .cca_fail    = DELTA(PHY_CCA_FAIL_COUNT),
    .aps_retry   = DELTA(APS_DATA_TX_UNICAST_RETRY),
    .buffer_fail = DELTA(ALLOCATE_PACKET_BUFFER_FAILURE),
    .bcast_rx    = DELTA(MAC_RX_BROADCAST),
  };
  #undef DELTA

  uint8_t message[OTA_DIAG_ZCL_HDR_LEN + OTA_DIAG_PACKET_LEN];
  uint8_t sequence = sl_zigbee_af_next_sequence();

  // Server to client: lights ignore it, it is only for the sniffer.
  message[0] = (ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_MANUFACTURER_SPECIFIC_MASK
                | ZCL_FRAME_CONTROL_SERVER_TO_CLIENT | ZCL_DISABLE_DEFAULT_RESPONSE_MASK);
  message[1] = (uint8_t)(ADAPTIV_ZIGBEE_MFG_ID & 0xFFU);
  message[2] = (uint8_t)(ADAPTIV_ZIGBEE_MFG_ID >> 8);
  message[3] = sequence;
  message[4] = OTA_DIAG_CMD_ID;

  uint8_t length = ota_diag_pack(&message[OTA_DIAG_ZCL_HDR_LEN],
                                 halCommonGetInt32uMillisecondTick(),
                                 m_bootload_state,
                                 (uint8_t)broadcast_monitor_get_traffic(),
                                 &counters);

  sl_zigbee_aps_frame_t apsFrame =
  {
    .profileId = HA_PROFILE_ID,
    .clusterId = ZCL_ADAPTIV_CONFIGURATION_CLUSTER_ID,
    .sourceEndpoint = LIGHT_ENDPOINT,
    .destinationEndpoint = SL_ZIGBEE_BROADCAST_ENDPOINT,
    .options = SL_ZIGBEE_APS_OPTION_NONE,
    .groupId = 0x0000,
    .radius = OTA_DIAG_RADIUS,
  };

  return(SL_STATUS_OK == sl_zigbee_af_send_broadcast(SL_ZIGBEE_NULL_NODE_ID,
                                                     SL_ZIGBEE_BROADCAST_ADDRESS,
                                                     sequence,
                                                     &apsFrame,
                                                     OTA_DIAG_ZCL_HDR_LEN + length,
                                                     message));
}

// Framework callback for every message sent: for a unicast with APS retry it
// comes when the APS ack arrives (or delivery has failed).
bool sl_zigbee_af_message_sent_cb(sl_zigbee_outgoing_message_type_t type,
                                  uint16_t indexOrDestination,
                                  sl_zigbee_aps_frame_t *apsFrame,
                                  uint16_t msgLen,
                                  uint8_t *message,
                                  sl_status_t status)
{
  (void)type;
  (void)indexOrDestination;

  if((NULL != apsFrame) && (NULL != message) && (2U < msgLen)
     && (ZCL_OTA_BOOTLOAD_CLUSTER_ID == apsFrame->clusterId)
     && (ZCL_IMAGE_BLOCK_REQUEST_COMMAND_ID == message[2]))
  {
     ota_diag_request_acked(halCommonGetInt32uMillisecondTick(), (SL_STATUS_OK == status));
  }
  return(false);
}

#endif // OTA_DIAG_ENABLED
