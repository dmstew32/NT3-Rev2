/*
 * Host (PC) simulation of everything the button code touches.  Force-included
 * (-include sim.h) into every translation unit of the button test; the real
 * project/SDK headers are replaced by the empty files in stubs/.
 */
#ifndef SIM_H_
#define SIM_H_

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- SDK basics ---------- */
typedef uint32_t sl_status_t;
#define SL_STATUS_OK                 ((sl_status_t)0x0000)
#define SL_STATUS_FULL               ((sl_status_t)0x0019)
#define SL_STATUS_INVALID_PARAMETER  ((sl_status_t)0x0021)

#define CORE_ATOMIC_SECTION(...)  { __VA_ARGS__ }   /* same shape as the SDK macro: a bare block */

/* ---------- GPIO ---------- */
typedef struct { volatile uint32_t DIN; } sim_gpio_port_t;
typedef struct
{
  sim_gpio_port_t P[4];
  uint32_t IEN, IF, IF_CLR, IEN_SET;
} sim_gpio_t;
extern sim_gpio_t sim_gpio;
void sim_gpio_sync(void);       // applies pending IF_CLR / IEN_SET writes
#define GPIO (sim_gpio_sync(), &sim_gpio)

enum { gpioPortA = 0, gpioPortB = 1, gpioPortC = 2, gpioPortD = 3 };
enum { gpioModeInputPullFilter = 3 };
typedef struct { uint8_t port; uint8_t pin; } sl_gpio_t;
#define SL_GPIO_INTERRUPT_RISING_FALLING_EDGE 3
#define SL_BUS_CLOCK_GPIO 0
#define GPIO_ODD_IRQn 1
void GPIO_PinModeSet(int port, unsigned pin, int mode, unsigned out);
int32_t sl_hal_gpio_configure_external_interrupt(const sl_gpio_t *gpio, int32_t int_no, int flags);
void sl_clock_manager_enable_bus_clock(int clock);
void NVIC_EnableIRQ(int irq);
void GPIO_ODD_IRQHandler(void);

/* ---------- events ---------- */
typedef struct sim_event
{
  void (*handler)(struct sim_event *);
  bool scheduled;
  uint32_t due;
} sl_zigbee_af_event_t;
void sl_zigbee_af_event_init(sl_zigbee_af_event_t *e, void (*handler)(sl_zigbee_af_event_t *));
void sl_zigbee_af_isr_event_init(sl_zigbee_af_event_t *e, void (*handler)(sl_zigbee_af_event_t *));
void sl_zigbee_af_event_set_active(sl_zigbee_af_event_t *e);
void sl_zigbee_af_event_set_delay_ms(sl_zigbee_af_event_t *e, uint32_t ms);
void sl_zigbee_af_event_set_inactive(sl_zigbee_af_event_t *e);
bool sl_zigbee_af_event_is_scheduled(sl_zigbee_af_event_t *e);

/* ---------- hal_utils ---------- */
uint32_t get_time_ms(void);
bool has_timed_out(uint32_t timeStamp, uint32_t delay);
int8_t set_one_shot_us_timer_isr(void (*func)(void), uint32_t delay_us);
bool disable_one_shot_us_timer_isr(int8_t int_num);

/* ---------- tasker (RehTOS) ---------- */
typedef union
{
  int32_t s32; uint32_t u32; int16_t s16; uint16_t u16; int8_t s8; uint8_t u8;
  int32_t *s32Ptr; uint32_t *u32Ptr; int16_t *s16Ptr; uint16_t *u16Ptr; int8_t *s8Ptr; uint8_t *u8Ptr;
} taskParm_t;
uint8_t addTask(void (*funcPtr)(), taskParm_t parm0, taskParm_t parm1);
uint8_t removeTask(void (*funcPtr)());

/* ---------- debug prints ---------- */
void sl_zigbee_app_debug_println(const char *fmt, ...);
void sl_zigbee_core_debug_print(const char *fmt, ...);
void sl_zigbee_core_debug_println(const char *fmt, ...);

/* ---------- NT_3_app.h ---------- */
#define APP_FIRMWARE_VERSION_U32 0x00090104
#define ALLOW_DISPLAY_VERSION
#define DEFAULT_MIN_LIGHT_LEVEL_DT (25)
#define MIN_ALLOWABLE_LIGHT_LEVEL  (25)
#define DEFAULT_OCC_SENSOR_XXL_DELAY    (2UL*60UL*60UL)
#define DEFAULT_OCC_SENSOR_THRESHOLD    (80)
#define DEFAULT_OCC_SENSOR_THRESHOLD_LOW (60)
#define B_50_PERCENT_MODE       (1UL << 0UL)
#define B_DISABLE_OCC_SENSOR    (1UL << 2UL)
#define B_DISABLE_LIGHT_SENSOR  (1UL << 3UL)
#define B_MANUAL_ADD_MODE       (1UL << 5UL)
#define B_EM_MODE               (1UL << 7UL)
typedef enum { eNO_NWK, eADAPTIV_NWK, eDAINTREE_NWK, eUNKNOWN_NWK, eSTAND_ALONE } which_nwk_t;
typedef enum { eRESET_NORMAL } reset_reason_t;
#define ZCL_CURRENT_LEVEL_ATTRIBUTE_ID 0x0000
enum { TOKEN_APP_LIGHT_LEVEL_MAX_STASHED, TOKEN_APP_LIGHT_LEVEL_MID_STASHED, TOKEN_APP_LIGHT_LEVEL_MIN_STASHED, SIM_N_TOKENS };

/* ---------- application functions the button code calls ---------- */
void force_factory_reset(uint32_t delay_ms, reset_reason_t reason);
bool hal_start_flashing_red_led(uint32_t n, uint32_t on_ms, uint32_t off_ms);
bool hal_start_flashing_green_led(uint32_t n, uint32_t on_ms, uint32_t off_ms);
void hal_set_red_led(uint8_t on);
void hal_set_green_led(uint8_t on);
void hal_toggle_green_led(void);
void hal_leds_init(void);
void hal_leds_off(void);
bool is_red_led_free(void);
typedef enum { SL_ZIGBEE_NO_NETWORK, SL_ZIGBEE_JOINED_NETWORK = 2 } sl_zigbee_network_status_t;
sl_zigbee_network_status_t sl_zigbee_af_network_state(void);
void save_min_light_level(uint8_t v);
void save_mid_light_level(uint8_t v);
void save_max_light_level(uint8_t v);
uint8_t get_min_light_level(void);
uint8_t get_mid_light_level(void);
uint8_t get_max_light_level(void);
uint16_t read_light_level_attribute(uint16_t id);
bool is_app_flags_nonvol_set(uint32_t mask);
void set_app_flags_nonvol(uint32_t mask, uint8_t value);
void sl_token_get_data(uint32_t token, uint32_t index, void *data, uint32_t length);
void sl_token_set_data(uint32_t token, uint32_t index, void *data, uint32_t length);
void move_to_level_onoff(uint8_t level, uint16_t transitionTimeDs);
uint8_t get_present_light_level(void);
uint16_t get_light_color(void);
void save_light_color(uint16_t k);
bool tx_color_new_temp(uint16_t k);
bool tx_new_light_levels(void);
bool tx_occ_sensor_new_delays(uint16_t d, uint16_t d_off);
void save_adaptiv_unoccupied_delay(uint16_t d);
void save_adaptiv_unoccupied_delay_to_off(uint16_t d);
void set_occupancy_threshold(uint16_t t);
uint16_t get_unoccupied_delay(void);
uint16_t get_unoccupied_delay_to_off(void);
void place_in_manual_add_into_group_mode(void);
void broadcast_autocommission_announce(void);
which_nwk_t get_present_network(void);
bool device_is_in_a_group(void);
uint8_t hal_get_dim_pwm_255(void);
void hal_set_dim_pwm_255(uint8_t v);
void hal_start_occ_sensor(void);
void hal_stop_occ_sensor(void);

/* moved feature functions (new layout; static inside the reference hal_buttons.c) */
#ifndef SIM_REFERENCE
void toggle_50_percent_mode(void);
void toggle_em_mode(void);
void start_local_auto_commission(void);
#endif

/* ---------- variant hook: the code under test's init ---------- */
void sim_variant_init(void);

#endif /* SIM_H_ */
