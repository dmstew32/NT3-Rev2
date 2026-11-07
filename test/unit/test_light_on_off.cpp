#include <gtest/gtest.h>
#include <algorithm>
#include <vector>
#include "fff.h"

/*
 * light_on_off.c: how flashes end, and what a new max level (trim) does to the
 * light.  A flash runs through the fake event: run_flash() calls the handler
 * until nothing is scheduled.
 */

extern "C" {
#include "nt3_test_stubs.h"
#include "zap-id.h"
#include "light_levels.h"
void dim_lights_up_down_handler(void);
void dim_lights_up_down(uint16_t cycles, uint16_t ramp_Dsecs);
bool are_lights_dimming_up_dowm(void);
void save_max_light_level(uint8_t new_max_level);
void init_light_parameters(void);
void power_on_set_light_level(void);
uint8_t get_max_light_level(void);
void occ_sensor_set_switch_level(uint8_t level);
sl_zigbee_af_zcl_request_status_t sl_zigbee_af_on_off_cluster_off_cb(void);
void init_min_max_silab_levels(void);
}

DEFINE_FFF_GLOBALS

extern "C" {
FAKE_VALUE_FUNC(uint8_t, addTask, test_task_fn_t, taskParm_t, taskParm_t);
FAKE_VALUE_FUNC(uint8_t, removeTask, test_task_fn_t);
FAKE_VOID_FUNC(fill_in_apsFrame, sl_zigbee_af_cluster_command_t *, sl_zigbee_aps_frame_t *);
FAKE_VALUE_FUNC(uint8_t, get_present_light_level);
FAKE_VALUE_FUNC(which_nwk_t, get_present_network);
FAKE_VALUE_FUNC(uint32_t, get_time_ms);
FAKE_VALUE_FUNC(uint8_t, hal_get_dim_pwm_255);
FAKE_VALUE_FUNC(bool, has_timed_out, uint32_t, uint32_t);
FAKE_VOID_FUNC(init_min_max_silab_levels);
FAKE_VALUE_FUNC(bool, is_app_flags_nonvol_set, uint32_t);
FAKE_VALUE_FUNC(uint8_t, light_level_map, uint8_t, uint8_t, uint8_t);
FAKE_VOID_FUNC(move_to_level_onoff, uint8_t, uint16_t);
FAKE_VOID_FUNC(occ_sensor_set_switch_level, uint8_t);
FAKE_VALUE_FUNC(uint16_t, read_light_level_attribute, sl_zigbee_af_attribute_id_t);
FAKE_VALUE_FUNC(uint16_t, read_on_off_attribute, uint16_t);
FAKE_VOID_FUNC(save_adaptiv_unoccupied_delay, uint16_t);
FAKE_VOID_FUNC(save_adaptiv_unoccupied_delay_to_off, uint16_t);
FAKE_VALUE_FUNC(bool, set_app_flags_nonvol, uint32_t, bool);
FAKE_VALUE_FUNC(sl_status_t, sl_token_get_data, uint32_t, uint32_t, void *, uint32_t);
FAKE_VALUE_FUNC(sl_status_t, sl_token_set_data, uint32_t, uint32_t, void *, uint32_t);
typedef void (*event_handler_t)(sl_zigbee_af_event_t *);
FAKE_VOID_FUNC(sl_zigbee_af_event_init, sl_zigbee_af_event_t *, event_handler_t);
FAKE_VALUE_FUNC(bool, sl_zigbee_af_event_is_scheduled, sl_zigbee_af_event_t *);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_active, sl_zigbee_af_event_t *);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_delay_ms, sl_zigbee_af_event_t *, uint32_t);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_inactive, sl_zigbee_af_event_t *);
FAKE_VALUE_FUNC(sl_zigbee_af_zcl_request_status_t, sl_zigbee_af_on_off_cluster_off_cb);
FAKE_VALUE_FUNC(sl_status_t, sl_zigbee_af_send_unicast, sl_zigbee_outgoing_message_type_t, uint16_t,
                sl_zigbee_aps_frame_t *, uint16_t, uint8_t *);
FAKE_VALUE_FUNC(sl_zigbee_af_status_t, sl_zigbee_af_write_manufacturer_specific_server_attribute, uint8_t, uint16_t,
                sl_zigbee_af_attribute_id_t, uint16_t, uint8_t *, uint8_t);
FAKE_VOID_FUNC_VARARG(sl_zigbee_app_debug_println, const char *, ...);
FAKE_VOID_FUNC_VARARG(sl_zigbee_core_debug_println, const char *, ...);
FAKE_VALUE_FUNC(bool, write_level_config_attribute, sl_zigbee_af_attribute_id_t, uint8_t);
FAKE_VALUE_FUNC(bool, write_light_level_attribute, sl_zigbee_af_attribute_id_t, uint8_t *);
FAKE_VALUE_FUNC(bool, write_light_temperature_attribute, uint16_t);
FAKE_VALUE_FUNC(bool, write_occupancy_sensor_attribute, sl_zigbee_af_attribute_id_t, uint8_t *);
}

#define FAKES(F) \
  F(addTask) F(removeTask) F(fill_in_apsFrame) F(get_present_light_level) F(get_present_network) F(get_time_ms) \
  F(hal_get_dim_pwm_255) F(has_timed_out) F(init_min_max_silab_levels) F(is_app_flags_nonvol_set) F(light_level_map) \
  F(move_to_level_onoff) F(occ_sensor_set_switch_level) F(read_light_level_attribute) F(read_on_off_attribute) \
  F(save_adaptiv_unoccupied_delay) F(save_adaptiv_unoccupied_delay_to_off) F(set_app_flags_nonvol) F(sl_token_get_data) \
  F(sl_token_set_data) F(sl_zigbee_af_event_init) F(sl_zigbee_af_event_is_scheduled) F(sl_zigbee_af_event_set_active) \
  F(sl_zigbee_af_event_set_delay_ms) F(sl_zigbee_af_event_set_inactive) F(sl_zigbee_af_on_off_cluster_off_cb) \
  F(sl_zigbee_af_send_unicast) F(sl_zigbee_af_write_manufacturer_specific_server_attribute) \
  F(sl_zigbee_app_debug_println) F(sl_zigbee_core_debug_println) F(write_level_config_attribute) \
  F(write_light_level_attribute) F(write_light_temperature_attribute) F(write_occupancy_sensor_attribute)

static sl_status_t token_get(uint32_t t, uint32_t, void *d, uint32_t)
{
    if(TOKEN_APP_LIGHT_LEVEL_MAX == t) { *(uint8_t *)d = 204; }
    return 0;
}

static uint32_t m_now;
static uint8_t m_level;
static uint16_t m_on;
static std::vector<uint8_t> m_moves;
static bool m_scheduled;
static test_task_fn_t m_task;
static taskParm_t m_task_p0, m_task_p1;

static uint32_t now(void) { return m_now; }
static bool timed_out(uint32_t ts, uint32_t d) { return (m_now - ts) >= d; }
static void move(uint8_t level, uint16_t) { m_level = level; m_on = 1; m_moves.push_back(level); }
static uint16_t on_off(uint16_t) { return m_on; }
static sl_zigbee_af_zcl_request_status_t off_cb(void) { m_on = 0; return 0; }
static bool is_scheduled(sl_zigbee_af_event_t *) { return m_scheduled; }
static void set_active(sl_zigbee_af_event_t *) { m_scheduled = true; }
static void set_delay(sl_zigbee_af_event_t *, uint32_t) { m_scheduled = true; }
static void set_inactive(sl_zigbee_af_event_t *) { m_scheduled = false; }
static uint8_t add_task(test_task_fn_t fn, taskParm_t p0, taskParm_t p1) { m_task = fn; m_task_p0 = p0; m_task_p1 = p1; return 0; }
static uint8_t remove_task(test_task_fn_t fn) { if(m_task == fn) { m_task = nullptr; } return 0; }
static uint8_t present(void) { return m_level; }

class LightOnOff : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FAKES(RESET_FAKE)
        FFF_RESET_HISTORY();
        sl_token_get_data_fake.custom_fake = token_get;
        m_now = 1000; m_level = 100; m_on = 1; m_moves.clear(); m_scheduled = false; m_task = nullptr;
        get_time_ms_fake.custom_fake = now;
        has_timed_out_fake.custom_fake = timed_out;
        move_to_level_onoff_fake.custom_fake = move;
        read_on_off_attribute_fake.custom_fake = on_off;
        sl_zigbee_af_on_off_cluster_off_cb_fake.custom_fake = off_cb;
        sl_zigbee_af_event_is_scheduled_fake.custom_fake = is_scheduled;
        sl_zigbee_af_event_set_active_fake.custom_fake = set_active;
        sl_zigbee_af_event_set_delay_ms_fake.custom_fake = set_delay;
        sl_zigbee_af_event_set_inactive_fake.custom_fake = set_inactive;
        addTask_fake.custom_fake = add_task;
        removeTask_fake.custom_fake = remove_task;
        get_present_light_level_fake.custom_fake = present;
        get_present_network_fake.return_val = eADAPTIV_NWK;

        save_max_light_level(204);          // The module keeps its max between tests
        run_task();
        m_moves.clear();
        m_level = 100;
        RESET_FAKE(occ_sensor_set_switch_level);
    }

    static void run_flash(void)
    {
        for(int i = 0; (i < 100) && m_scheduled; i++)
        {
            m_scheduled = false;
            dim_lights_up_down_handler();
        }
        ASSERT_FALSE(m_scheduled);
    }

    static void run_task(void)
    {
        m_now += 2000UL;
        if(m_task)
        {
            m_task(m_task_p0, m_task_p1);
        }
    }
};

/* --------------------------------------------------------------- flashes */
TEST_F(LightOnOff, AFlashReturnsToThePriorLevel)
{
    dim_lights_up_down(3, DSECS_HALF_SECS);
    run_flash();
    ASSERT_FALSE(m_moves.empty());
    EXPECT_EQ(204, *std::max_element(m_moves.begin(), m_moves.end()));   // It did flash
    EXPECT_EQ(100, m_level);
    EXPECT_EQ(1, m_on);
}

TEST_F(LightOnOff, AFlashOnALightThatWasOffEndsOff)
{
    m_on = 0;
    dim_lights_up_down(1, DSECS_HALF_SECS);
    run_flash();
    EXPECT_EQ(1u, sl_zigbee_af_on_off_cluster_off_cb_fake.call_count);
    EXPECT_EQ(0, m_on);
}

TEST_F(LightOnOff, AFlashAtMidReturnsToMid)
{
    m_level = 51;
    dim_lights_up_down(1, DSECS_HALF_SECS);
    run_flash();
    EXPECT_EQ(51, m_level);
}

TEST_F(LightOnOff, StoppingCommissioningFlashesStillGoesToMax)
{
    dim_lights_up_down(0x7FFF, DSECS_HALF_SECS);
    dim_lights_up_down_handler();
    dim_lights_up_down(0, 0);
    EXPECT_EQ(204, m_level);
    EXPECT_FALSE(m_scheduled);
}

/* --------------------------------------------------------------- trim */
TEST_F(LightOnOff, RaisingTheMaxMovesToItAfterTheFlashEndsAndSetsTheWakeLevel)
{
    save_max_light_level(230);
    dim_lights_up_down(1, DSECS_HALF_SECS);

    run_task();                             // Still flashing: not yet
    EXPECT_NE(nullptr, m_task);

    run_flash();
    EXPECT_EQ(100, m_level);
    run_task();
    EXPECT_EQ(230, m_level);
    EXPECT_EQ(230, occ_sensor_set_switch_level_fake.arg0_val);
}

TEST_F(LightOnOff, RaisingTheMaxWithTheButtonsMovesToItWithoutAFlash)
{
    save_max_light_level(230);
    run_task();
    EXPECT_EQ(230, m_level);
}

TEST_F(LightOnOff, TheSameMaxDoesNotMoveTheLight)
{
    save_max_light_level(204);
    EXPECT_EQ(nullptr, m_task);
    EXPECT_TRUE(m_moves.empty());
}

TEST_F(LightOnOff, LoweringTheMaxBelowTheLevelLimitsTheLevel)
{
    save_max_light_level(80);
    EXPECT_EQ(80, m_level);
    EXPECT_EQ(nullptr, m_task);
}

TEST_F(LightOnOff, LoweringTheMaxAboveTheLevelLeavesTheLevel)
{
    save_max_light_level(150);
    EXPECT_EQ(100, m_level);
    EXPECT_TRUE(m_moves.empty());
    EXPECT_EQ(nullptr, m_task);
}

TEST_F(LightOnOff, OnDaintreeAnyNewMaxStillGoesToFullOn)
{
    get_present_network_fake.return_val = eDAINTREE_NWK;
    save_max_light_level(150);
    EXPECT_NE(nullptr, m_task);
}

TEST_F(LightOnOff, PowerUpStillGoesToMax)
{
    init_light_parameters();
    run_task();
    EXPECT_EQ(204, m_level);
}
