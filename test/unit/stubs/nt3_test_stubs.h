/*
 * Unit test stand-in for the Silicon Labs SDK and for the project modules that
 * are not under test.  Every stub header in this folder includes this file.
 * The functions declared here are faked (fff) by the tests that need them.
 */
#ifndef NT3_TEST_STUBS_H_
#define NT3_TEST_STUBS_H_

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------- status */
typedef uint32_t sl_status_t;
#define SL_STATUS_OK                      ((sl_status_t)0x0000)
#define SL_STATUS_FAIL                    ((sl_status_t)0x0001)
#define SL_STATUS_INVALID_STATE           ((sl_status_t)0x0002)
#define SL_STATUS_BUSY                    ((sl_status_t)0x0004)
#define SL_STATUS_NOT_INITIALIZED         ((sl_status_t)0x0011)
#define SL_STATUS_ALLOCATION_FAILED       ((sl_status_t)0x0019)
#define SL_STATUS_FULL                    ((sl_status_t)0x0019)
#define SL_STATUS_INVALID_PARAMETER       ((sl_status_t)0x0021)
#define SL_STATUS_NOT_FOUND               ((sl_status_t)0x002D)
#define SL_STATUS_MESSAGE_TOO_LONG        ((sl_status_t)0x0038)
#define SL_STATUS_ZIGBEE_MAX_MESSAGE_LIMIT_REACHED ((sl_status_t)0x0C03)

/* ---------------------------------------------------------------- basics */
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#define CORE_ATOMIC_SECTION(...)  { __VA_ARGS__ }

/* ---------------------------------------------------------------- zigbee */
typedef uint16_t sl_802154_short_addr_t;
typedef uint8_t sl_zigbee_af_status_t;
typedef uint8_t sl_zigbee_zdo_status_t;
typedef uint16_t sl_zigbee_aps_option_t;
typedef uint16_t sli_buffer_manager_buffer_t;

#define SL_ZIGBEE_ZCL_STATUS_SUCCESS   0x00
#define SL_ZIGBEE_ZDP_SUCCESS          0x00
#define SL_ZIGBEE_NULL_MESSAGE_BUFFER  ((sli_buffer_manager_buffer_t)0xFFFF)

enum
{
  SL_ZIGBEE_APS_OPTION_NONE                  = 0x0000,
  SL_ZIGBEE_APS_OPTION_RETRY                 = 0x0040,
  SL_ZIGBEE_APS_OPTION_ENABLE_ROUTE_DISCOVERY = 0x0100,
  SL_ZIGBEE_APS_OPTION_SOURCE_EUI64          = 0x0400,
};

typedef struct
{
  uint16_t profileId;
  uint16_t clusterId;
  uint8_t sourceEndpoint;
  uint8_t destinationEndpoint;
  sl_zigbee_aps_option_t options;
  uint16_t groupId;
  uint8_t sequence;
  uint8_t radius;
} sl_zigbee_aps_frame_t;

typedef uint16_t sl_zigbee_af_attribute_id_t;
typedef uint8_t sl_zigbee_af_attribute_type_t;
#define ZA_MAX_HOPS 12
typedef uint8_t sl_zigbee_af_zcl_request_status_t;
typedef uint8_t sl_zigbee_incoming_message_type_t;
#define SL_ZIGBEE_INCOMING_UNICAST  0u
typedef struct
{
  sl_zigbee_aps_frame_t *apsFrame;
  sl_zigbee_incoming_message_type_t type;
  sl_802154_short_addr_t source;
  uint8_t *buffer;
  uint16_t bufLen;
  bool clusterSpecific;
  bool mfgSpecific;
  uint16_t mfgCode;
  uint8_t seqNum;
  uint8_t commandId;
  uint8_t payloadStartIndex;
  uint8_t direction;
} sl_zigbee_af_cluster_command_t;

#ifndef BIT
#define BIT(x) (1U << (x))
#endif
#define ZCL_CLUSTER_SPECIFIC_COMMAND          BIT(0)
#define ZCL_PROFILE_WIDE_COMMAND              0u
#define ZCL_GLOBAL_COMMAND                    (ZCL_PROFILE_WIDE_COMMAND)
#define ZCL_MANUFACTURER_SPECIFIC_MASK        BIT(2)
#define ZCL_DISABLE_DEFAULT_RESPONSE_MASK     BIT(4)
#define ZCL_FRAME_CONTROL_CLIENT_TO_SERVER    0u
#define SL_ZIGBEE_BROADCAST_ENDPOINT          0xFFu
#define SL_ZIGBEE_BROADCAST_ADDRESS           0xFFFCu

typedef enum
{
  SL_ZIGBEE_OUTGOING_DIRECT,
  SL_ZIGBEE_OUTGOING_VIA_ADDRESS_TABLE,
  SL_ZIGBEE_OUTGOING_VIA_BINDING,
  SL_ZIGBEE_OUTGOING_MULTICAST,
  SL_ZIGBEE_OUTGOING_BROADCAST,
} sl_zigbee_outgoing_message_type_t;

typedef enum
{
  SL_ZIGBEE_NO_NETWORK,
  SL_ZIGBEE_JOINING_NETWORK,
  SL_ZIGBEE_JOINED_NETWORK,
  SL_ZIGBEE_JOINED_NETWORK_NO_PARENT,
  SL_ZIGBEE_LEAVING_NETWORK,
} sl_zigbee_network_status_t;

#define SL_ZIGBEE_NULL_NODE_ID                       0xFFFFu
#define SL_ZIGBEE_ZDO_ENDPOINT                       0u
#define SL_ZIGBEE_ZDO_PROFILE_ID                     0x0000u
#define SL_ZIGBEE_RX_ON_WHEN_IDLE_BROADCAST_ADDRESS  0xFFFDu
#define SL_ZIGBEE_MIN_BROADCAST_ADDRESS              0xFFF8u
#define sl_zigbee_is_zigbee_broadcast_address(address) \
  (SL_ZIGBEE_MIN_BROADCAST_ADDRESS <= ((uint16_t)(address)))
#define HA_PROFILE_ID                                0x0104u

/* ZDO clusters (zigbee-device-stack.h) */
#define NETWORK_ADDRESS_REQUEST      0x0000
#define NETWORK_ADDRESS_RESPONSE     0x8000
#define IEEE_ADDRESS_REQUEST         0x0001
#define IEEE_ADDRESS_RESPONSE        0x8001
#define SIMPLE_DESCRIPTOR_REQUEST    0x0004
#define SIMPLE_DESCRIPTOR_RESPONSE   0x8004
#define MATCH_DESCRIPTORS_REQUEST    0x0006
#define MATCH_DESCRIPTORS_RESPONSE   0x8006
#define END_DEVICE_ANNOUNCE          0x0013
#define BIND_REQUEST                 0x0021
#define LEAVE_REQUEST                0x0034
#define PERMIT_JOINING_REQUEST       0x0036

/* ---------------------------------------------------------------- events */
typedef struct sl_zigbee_af_event
{
  void (*handler)(struct sl_zigbee_af_event *);
} sl_zigbee_af_event_t;

void sl_zigbee_af_event_init(sl_zigbee_af_event_t *event, void (*handler)(sl_zigbee_af_event_t *));
void sl_zigbee_af_isr_event_init(sl_zigbee_af_event_t *event, void (*handler)(sl_zigbee_af_event_t *));
void sl_zigbee_af_event_set_active(sl_zigbee_af_event_t *event);
void sl_zigbee_af_event_set_delay_ms(sl_zigbee_af_event_t *event, uint32_t delay_ms);
void sl_zigbee_af_event_set_inactive(sl_zigbee_af_event_t *event);
bool sl_zigbee_af_event_is_scheduled(sl_zigbee_af_event_t *event);

/* ---------------------------------------------------------------- stack */
sl_802154_short_addr_t sl_zigbee_get_node_id(void);
sl_zigbee_network_status_t sl_zigbee_af_network_state(void);
uint8_t sl_zigbee_af_get_data_size(uint8_t dataType);

sli_buffer_manager_buffer_t sl_legacy_buffer_manager_fill_linked_buffers(const uint8_t *contents, uint8_t length);
uint16_t sl_legacy_buffer_manager_message_buffer_length(sli_buffer_manager_buffer_t buffer);
void sli_legacy_buffer_manager_copy_from_linked_buffers(uint8_t *contents, sli_buffer_manager_buffer_t buffer, uint8_t length);
void sli_legacy_buffer_manager_mark_buffer(sli_buffer_manager_buffer_t *root);

sl_status_t sl_zigbee_send_unicast(sl_zigbee_outgoing_message_type_t type, uint16_t indexOrDestination,
                                   sl_zigbee_aps_frame_t *apsFrame, uint16_t messageTag,
                                   uint8_t messageLength, const uint8_t *message, uint8_t *apsSequence);
sl_status_t sl_zigbee_send_broadcast(sl_802154_short_addr_t alias, sl_802154_short_addr_t destination,
                                     uint8_t nwkSequence, sl_zigbee_aps_frame_t *apsFrame, uint8_t radius,
                                     uint16_t messageTag, uint8_t messageLength, const uint8_t *message,
                                     uint8_t *apsSequence);
sl_status_t sl_zigbee_send_multicast(sl_zigbee_aps_frame_t *apsFrame, uint8_t radius, uint16_t broadcastAddr,
                                     sl_802154_short_addr_t alias, uint8_t nwkSequence, uint16_t messageTag,
                                     uint8_t messageLength, const uint8_t *message, uint8_t *apsSequence);
sl_status_t sl_zigbee_af_send_unicast(sl_zigbee_outgoing_message_type_t type, uint16_t indexOrDestination,
                                      sl_zigbee_aps_frame_t *apsFrame, uint16_t messageLength, uint8_t *message);

uint8_t sl_zigbee_af_next_sequence(void);
sl_802154_short_addr_t sl_zigbee_af_get_node_id(void);
sl_status_t sl_zigbee_af_send_broadcast(sl_802154_short_addr_t alias, sl_802154_short_addr_t destination,
                                        uint8_t sequence, sl_zigbee_aps_frame_t *apsFrame,
                                        uint16_t messageLength, uint8_t *message);
sl_zigbee_af_status_t sl_zigbee_af_read_server_attribute(uint8_t endpoint, uint16_t cluster,
                                                         sl_zigbee_af_attribute_id_t attributeID,
                                                         uint8_t *dataPtr, uint8_t readLength);
sl_zigbee_af_status_t sl_zigbee_af_write_server_attribute(uint8_t endpoint, uint16_t cluster,
                                                          sl_zigbee_af_attribute_id_t attributeID,
                                                          uint8_t *dataPtr, uint8_t dataType);
sl_status_t sl_token_get_data(uint32_t token, uint32_t index, void *data, uint32_t length);
sl_status_t sl_token_set_data(uint32_t token, uint32_t index, void *data, uint32_t length);
#define TOKEN_APP_UNOCCUPIED_DELAY  0x1001u
#define TOKEN_APP_ADAPTIV_COMMISH_INFO  0x1002u
#define TOKEN_APP_LIGHT_SENSOR_LUX  0x1003u
#define TOKEN_APP_LIGHT_LEVEL_MAX_STASHED  0x1004u
#define TOKEN_APP_LIGHT_LEVEL_MID_STASHED  0x1005u
#define TOKEN_APP_LIGHT_LEVEL_MIN_STASHED  0x1006u
#define TOKEN_APP_LIGHT_LEVEL_MAX  0x1007u
#define TOKEN_APP_LIGHT_LEVEL_MID  0x1008u
#define TOKEN_APP_LIGHT_LEVEL_MIN  0x1009u
#define TOKEN_APP_ALLOW_LIGHT_OFF  0x100Au
#define TOKEN_APP_COLOR_TEMP_K  0x100Bu
#define TOKEN_APP_UNOCCUPIED_DELAY_TO_OFF  0x100Cu
typedef struct
{
  uint8_t enable_byte;
  uint8_t options_byte;
  uint8_t src_ep;
  uint16_t src_short_addr;
  uint16_t session_id;
  uint16_t group_id;
  uint32_t auto_time_out;
} adaptiv_commish_info_t;     // config/sl_custom_token_header.h

// auto commissioning (light_on_off.h, nwk_groups.h, micro.h, nwk_multicast.h)
void dim_lights_up_down(uint16_t cycles, uint16_t ramp_Dsecs);
bool are_lights_dimming_up_dowm(void);
void update_bindingTable_address(void);
uint8_t halGetResetInfo(void);
sl_status_t broadcast_autocommission_announce(void);

/* ---------------------------------------------------------------- prints */
void sl_zigbee_af_core_println(const char *format, ...);
void sl_zigbee_af_core_print(const char *format, ...);
void sl_zigbee_app_debug_println(const char *format, ...);
void sl_zigbee_app_debug_print(const char *format, ...);
void sl_zigbee_core_debug_print(const char *format, ...);
void sl_zigbee_core_debug_println(const char *format, ...);

/* ---------------------------------------------------------------- GPIO */
typedef struct { uint32_t DIN; } test_gpio_port_t;
typedef struct
{
  test_gpio_port_t P[4];
  uint32_t IEN, IF, IF_CLR, IEN_SET;
} test_gpio_t;
extern test_gpio_t test_gpio;
void test_gpio_sync(void);   // applies pending IF_CLR / IEN_SET register writes
#define GPIO (test_gpio_sync(), &test_gpio)

enum { gpioPortA = 0, gpioPortB = 1, gpioPortC = 2, gpioPortD = 3 };
enum { gpioModeInputPullFilter = 3, gpioModeWiredAndPullUp = 9 };
typedef struct { uint8_t port; uint8_t pin; } sl_gpio_t;
#define SL_GPIO_INTERRUPT_RISING_FALLING_EDGE 3
#define SL_BUS_CLOCK_GPIO 0
#define GPIO_ODD_IRQn 1
void GPIO_PinModeSet(int port, unsigned int pin, int mode, unsigned int out);
void GPIO_PinOutSet(int port, unsigned int pin);
void GPIO_PinOutClear(int port, unsigned int pin);
void GPIO_PinOutToggle(int port, unsigned int pin);
unsigned int GPIO_PinInGet(int port, unsigned int pin);
int32_t sl_hal_gpio_configure_external_interrupt(const sl_gpio_t *gpio, int32_t int_no, int flags);
void sl_clock_manager_enable_bus_clock(int clock);
void NVIC_EnableIRQ(int irq);

/* ---------------------------------------------------------------- tasker */
typedef union
{
  int32_t s32; uint32_t u32; int16_t s16; uint16_t u16; int8_t s8; uint8_t u8;
  int32_t *s32Ptr; uint32_t *u32Ptr; int16_t *s16Ptr; uint16_t *u16Ptr; int8_t *s8Ptr; uint8_t *u8Ptr;
} taskParm_t;
typedef void (*test_task_fn_t)(taskParm_t, taskParm_t);
uint8_t addTask(test_task_fn_t funcPtr, taskParm_t parm0, taskParm_t parm1);   // RehTOS.h
uint8_t removeTask(test_task_fn_t funcPtr);

/* ---------------------------------------------------------------- application */
typedef enum
{
  eRESET_NORMAL = 0,
  eRESET_DAINTREE = 2,
} reset_reason_t;               // factory_reset.h

// function pointer types (fff cannot take them as macro arguments)
typedef void (*test_isr_fn_t)(void);
typedef void (*test_event_handler_t)(sl_zigbee_af_event_t *);

uint32_t get_time_ms(void);
bool has_timed_out(uint32_t timeStamp, uint32_t delay);
int8_t set_one_shot_us_timer_isr(void (*func)(void), uint32_t delay_us);
bool disable_one_shot_us_timer_isr(int8_t int_num);

which_nwk_t get_present_network(void);
bool device_is_in_a_group(void);
void force_factory_reset(uint32_t delay_ms, reset_reason_t reason);
void save_min_light_level(uint8_t new_min_level);
void save_mid_light_level(uint8_t new_mid_level);
void save_max_light_level(uint8_t new_max_level);
uint8_t get_max_light_level(void);
void move_to_level_onoff(uint8_t level, uint16_t transitionTimeDs);
uint8_t get_present_light_level(void);
uint16_t get_light_color(void);
void save_light_color(uint16_t temperature_K);
bool tx_color_new_temp(uint16_t new_color);
bool tx_new_light_levels(void);
bool tx_occ_sensor_new_delays(uint16_t delay_unoccupied, uint16_t delay_to_off);
void save_adaptiv_unoccupied_delay(uint16_t new_unoccupied_delay);
void save_adaptiv_unoccupied_delay_to_off(uint16_t new_unoccupied_delay_to_off);
void set_occupancy_threshold(uint16_t new_threshold);
uint16_t get_unoccupied_delay(void);
uint16_t get_unoccupied_delay_to_off(void);
void place_in_manual_add_into_group_mode(void);
void start_local_auto_commission(void);
void toggle_50_percent_mode(void);
void toggle_em_mode(void);
void start_display_fw_version(void);
void set_dhs_time_stamp(void);

// occupancy (light_on_off.h, nwk_utils.h, nwk_groups.h, nwk_multicast.h, light_sensor.h)
uint8_t get_min_light_level(void);
uint8_t get_mid_light_level(void);
uint8_t get_on_light_level(void);
uint16_t read_on_off_attribute(uint16_t attributeID);
uint16_t read_light_level_attribute(sl_zigbee_af_attribute_id_t attributeID);
bool write_light_level_attribute(sl_zigbee_af_attribute_id_t attributeID, uint8_t *data);
bool off_from_switch(void);
void clear_off_from_switch(void);
bool on_from_switch(void);
void clear_on_from_switch(void);
bool set_app_flags_nonvol(uint32_t bit, bool value);
bool is_app_flags_nonvol_set(uint32_t flag);
uint32_t get_random_number(uint32_t max, uint32_t min);
typedef struct
{
  uint16_t group_address;
  uint8_t dst_ep;
} device_binding_info_t;
uint8_t get_number_of_multicast_bindings(void);
void get_multicast_info(uint8_t index, device_binding_info_t *info);
bool tx_occ_keepalive(uint16_t time_until_expiration_sec, uint8_t state, uint8_t reason, uint8_t level);
bool tx_occ_level_change(uint8_t state, uint8_t level);
bool tx_dh_level(uint8_t level, uint8_t ramp_ds);
bool tx_dh_master_claim(void);

// daylight harvesting (light_on_off.h, nwk_utils.h, hal_pwm.h, occ_sensor.h)
void move_to_level_dhs(uint8_t level, uint16_t transitionTimeDs);
uint8_t hal_get_dim_pwm_255(void);

// on/off, flashes, max level (nwk_utils.h, adaptiv_config.h, occ_sensor.h)
#ifndef SL_ZIGBEE_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE
#define SL_ZIGBEE_ZCL_STATUS_UNSUPPORTED_ATTRIBUTE 0x86u
#endif
#ifndef ZCL_FRAME_CONTROL_SERVER_TO_CLIENT
#define ZCL_FRAME_CONTROL_SERVER_TO_CLIENT 0x08u
#endif
void fill_in_apsFrame(sl_zigbee_af_cluster_command_t *cmd, sl_zigbee_aps_frame_t *apsFrame);
void occ_sensor_set_switch_level(uint8_t level);
sl_zigbee_af_status_t sl_zigbee_af_write_manufacturer_specific_server_attribute(uint8_t endpoint, uint16_t cluster,
                                                                                sl_zigbee_af_attribute_id_t attributeID,
                                                                                uint16_t manufacturerCode, uint8_t *dataPtr,
                                                                                uint8_t dataType);
bool write_level_config_attribute(sl_zigbee_af_attribute_id_t attribute, uint8_t data);
bool write_light_temperature_attribute(uint16_t temperature);
bool write_occupancy_sensor_attribute(sl_zigbee_af_attribute_id_t attribute, uint8_t *data);
typedef uint8_t sl_802154_long_addr_t[8];
void sl_zigbee_af_get_eui64(uint8_t *eui64);

#ifdef __cplusplus
}
#endif

#endif /* NT3_TEST_STUBS_H_ */
