#ifndef _NWK_COMMISSION_H
#define _NWK_COMMISSION_H

typedef enum
{
    STARTUP_CONTROL_REJOIN = 2,
    STARTUP_CONTROL_ASSOCIATION = 3
} startup_control_t;

typedef struct
{
    uint8_t preconfig_link_key[SL_ZIGBEE_ENCRYPTION_KEY_SIZE];
    uint8_t extended_pan_id[EXTENDED_PAN_ID_SIZE];
    uint32_t channel_mask;
    uint16_t panId;
    uint8_t stack_profile;
    uint8_t protocol_version;
    uint8_t startup_control;
} commissioning_set_t;

static const commissioning_set_t m_default_commissioning_set =
{
    .preconfig_link_key = {0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C,  0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39},
    .extended_pan_id = {0,0,0,0, 0,0,0,0},
    .channel_mask = SL_ZIGBEE_ALL_802_15_4_CHANNELS_MASK,
    .panId = 0xFFFF,
    .stack_profile = SL_ZIGBEE_STACK_PROFILE_ZIGBEE_PRO,
    .protocol_version = 2,
    .startup_control = STARTUP_CONTROL_ASSOCIATION
};

typedef struct
{
    uint8_t preconfig_link_key[SL_ZIGBEE_ENCRYPTION_KEY_SIZE];
    uint8_t extended_pan_id[EXTENDED_PAN_ID_SIZE];
    uint32_t channel_mask;
    uint16_t panId;
    uint8_t  startup_control;  // use u8 type instead of startup_control_mode_t to ensure size
} commissioning_join_config_t;

void zcl_commissioning_init(void);

bool check_switch_network(void);
uint32_t adaptiv_commissioning_cluster_handler(sl_service_opcode_t opcode, sl_service_function_context_t *context);

bool load_commissioning_nwk_parameters(sl_zigbee_network_parameters_t *nwkParams);
bool load_commissioning_preconfiguredKey(sl_zigbee_key_data_t *key);
void spew_commissioning_set(commissioning_set_t *p_set);
sl_zigbee_af_status_t commissioning_copy_nonvol_to_set(commissioning_set_t *p_set);

#endif // _NWK_COMMISSION_H
