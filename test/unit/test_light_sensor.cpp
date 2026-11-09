#include <gtest/gtest.h>
#include <map>
#include "fff.h"

/*
 * light_sensor.c + dh_control.c: what is around the control logic.  Upgrade from
 * the firmware with calibration, when a light follows the master, when DH is
 * disabled and enabled, suspends in milliseconds, one master per group, and the
 * diagnostic packet.
 */

extern "C" {
#include "nt3_test_stubs.h"
#include "zap-id.h"
#include "broadcast_monitor.h"
#include "dh_debug.h"
#include "hal_light_sensor.h"
#include "../../Light/nwk_sensors/dh_control.h"
void light_sensor_init(void);
void execute_light_sensor_logic(light_sensor_info_t *light_sensor_info);
void light_sensor_on_reading(uint16_t reading);
bool suspend_light_sensor_control(uint32_t delay_ms);
bool allow_light_sensor_control(void);
bool dh_rx_level_adjust(sl_zigbee_af_cluster_command_t *cmd);
bool dh_rx_master_claim(sl_zigbee_af_cluster_command_t *cmd);
void dh_set_master(bool enable);
void dh_disable(void);
void dh_enable(void);
void save_light_lux_reference(uint16_t lux_ref);
void set_dhs_time_stamp(void);
}

DEFINE_FFF_GLOBALS

extern "C" {
FAKE_VALUE_FUNC(uint32_t, hal_light_sensor_init);
FAKE_VALUE_FUNC(uint8_t, hal_get_dim_pwm_255);
FAKE_VALUE_FUNC(uint32_t, get_time_ms);
FAKE_VALUE_FUNC(bool, has_timed_out, uint32_t, uint32_t);
FAKE_VALUE_FUNC(uint8_t, addTask, test_task_fn_t, taskParm_t, taskParm_t);
FAKE_VALUE_FUNC(uint8_t, removeTask, test_task_fn_t);
FAKE_VALUE_FUNC(which_nwk_t, get_present_network);
FAKE_VALUE_FUNC(uint16_t, read_on_off_attribute, uint16_t);
FAKE_VALUE_FUNC(uint16_t, read_light_level_attribute, sl_zigbee_af_attribute_id_t);
FAKE_VALUE_FUNC(bool, device_is_in_a_group);
FAKE_VALUE_FUNC(bool, is_occ_mid_or_lower);
FAKE_VALUE_FUNC(bool, set_app_flags_nonvol, uint32_t, bool);
FAKE_VALUE_FUNC(bool, is_app_flags_nonvol_set, uint32_t);
FAKE_VALUE_FUNC(uint8_t, get_min_light_level);
FAKE_VALUE_FUNC(uint8_t, get_max_light_level);
FAKE_VALUE_FUNC(uint32_t, get_random_number, uint32_t, uint32_t);
FAKE_VALUE_FUNC(bool, tx_dh_level_adjust, uint8_t, uint8_t, uint8_t);
FAKE_VALUE_FUNC(bool, tx_dh_master_claim);
FAKE_VALUE_FUNC(sl_802154_short_addr_t, sl_zigbee_get_node_id);
FAKE_VOID_FUNC(move_to_level_dhs, uint8_t, uint16_t);
FAKE_VALUE_FUNC(broadcast_traffic_t, broadcast_monitor_get_traffic);
FAKE_VALUE_FUNC(sl_status_t, sl_token_get_data, uint32_t, uint32_t, void *, uint32_t);
FAKE_VALUE_FUNC(sl_status_t, sl_token_set_data, uint32_t, uint32_t, void *, uint32_t);
FAKE_VALUE_FUNC(sl_zigbee_af_status_t, sl_zigbee_af_read_server_attribute, uint8_t, uint16_t, sl_zigbee_af_attribute_id_t, uint8_t *, uint8_t);
FAKE_VALUE_FUNC(sl_zigbee_af_status_t, sl_zigbee_af_write_server_attribute, uint8_t, uint16_t, sl_zigbee_af_attribute_id_t, uint8_t *, uint8_t);
FAKE_VALUE_FUNC(bool, dh_debug_send, uint16_t, uint16_t, uint8_t, uint8_t, uint8_t);
FAKE_VALUE_FUNC(bool, dh_lux_debug_send, uint16_t, uint16_t, uint32_t);
FAKE_VALUE_FUNC(bool, global_ac_quiet_active);
FAKE_VOID_FUNC_VARARG(sl_zigbee_app_debug_println, const char *, ...);
FAKE_VOID_FUNC_VARARG(sl_zigbee_app_debug_print, const char *, ...);
}

#define FAKES(F) \
  F(hal_light_sensor_init) F(hal_get_dim_pwm_255) F(get_time_ms) F(has_timed_out) F(addTask) F(removeTask) \
  F(get_present_network) F(read_on_off_attribute) F(read_light_level_attribute) F(device_is_in_a_group) \
  F(is_occ_mid_or_lower) F(set_app_flags_nonvol) F(is_app_flags_nonvol_set) \
  F(get_min_light_level) F(get_max_light_level) F(get_random_number) F(tx_dh_level_adjust) F(tx_dh_master_claim) \
  F(sl_zigbee_get_node_id) F(move_to_level_dhs) \
  F(broadcast_monitor_get_traffic) F(sl_token_get_data) F(sl_token_set_data) \
  F(sl_zigbee_af_read_server_attribute) F(sl_zigbee_af_write_server_attribute) \
  F(dh_debug_send) F(dh_lux_debug_send) F(global_ac_quiet_active) F(sl_zigbee_app_debug_println) F(sl_zigbee_app_debug_print)

static const sl_802154_short_addr_t MY_NODE = 0x5000, LOWER_NODE = 0x1000, HIGHER_NODE = 0x9000;
static uint32_t m_now;
static uint32_t m_flags;
static uint16_t m_token_lux;
static uint8_t m_token_stashed_max;
struct Task { taskParm_t p0; taskParm_t p1; };
static std::map<test_task_fn_t, Task> m_tasks;

static uint32_t now(void) { return m_now; }
static bool timed_out(uint32_t ts, uint32_t d) { return (m_now - ts) >= d; }
static bool flag_set(uint32_t f) { return 0 != (m_flags & f); }
static bool set_flags(uint32_t f, bool v) { if(v) { m_flags |= f; } else { m_flags &= ~f; } return true; }
static uint8_t add_task(test_task_fn_t fn, taskParm_t p0, taskParm_t p1) { m_tasks[fn] = { p0, p1 }; return 0; }
static uint8_t remove_task(test_task_fn_t fn) { m_tasks.erase(fn); return 0; }
static sl_status_t token_get(uint32_t t, uint32_t, void *d, uint32_t len)
{
    if(TOKEN_APP_LIGHT_SENSOR_LUX == t) { memcpy(d, &m_token_lux, len); }
    if(TOKEN_APP_LIGHT_LEVEL_MAX_STASHED == t) { memcpy(d, &m_token_stashed_max, len); }
    return 0;
}
static sl_status_t token_set(uint32_t t, uint32_t, void *d, uint32_t len)
{
    if(TOKEN_APP_LIGHT_SENSOR_LUX == t) { memcpy(&m_token_lux, d, len); }
    return 0;
}

class LightSensor : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FAKES(RESET_FAKE)
        FFF_RESET_HISTORY();
        m_now = 1000; m_flags = 0; m_token_lux = 500; m_token_stashed_max = 0;
        m_tasks.clear();
        get_time_ms_fake.custom_fake = now;
        has_timed_out_fake.custom_fake = timed_out;
        addTask_fake.custom_fake = add_task;
        removeTask_fake.custom_fake = remove_task;
        is_app_flags_nonvol_set_fake.custom_fake = flag_set;
        set_app_flags_nonvol_fake.custom_fake = set_flags;
        sl_token_get_data_fake.custom_fake = token_get;
        sl_token_set_data_fake.custom_fake = token_set;
        get_present_network_fake.return_val = eADAPTIV_NWK;
        read_on_off_attribute_fake.return_val = 1;
        device_is_in_a_group_fake.return_val = true;
        hal_get_dim_pwm_255_fake.return_val = 100;
        get_max_light_level_fake.return_val = 204;
        get_min_light_level_fake.return_val = 0;
        tx_dh_level_adjust_fake.return_val = true;
        tx_dh_master_claim_fake.return_val = true;
        sl_zigbee_get_node_id_fake.return_val = MY_NODE;
    }

    // Everything that was suspended has run out
    static void past_suspends(void)
    {
        m_now += 60000UL;
        run_tasks();
    }

    static void run_tasks(void)
    {
        std::map<test_task_fn_t, Task> tasks = m_tasks;
        for(auto &t : tasks)
        {
            t.first(t.second.p0, t.second.p1);
        }
    }

    // Started, nothing left over from the start, red not lit
    static void init_running(void)
    {
        light_sensor_init();
        past_suspends();
        light_sensor_on_reading(0xFFFF);        // Uses up the "changed" mark of the suspend
    }

    // One sensor reading of this many lux, 2.5 s after the last
    static void reading(uint32_t lux)
    {
        m_now += 2500UL;
        run_tasks();
        light_sensor_on_reading((uint16_t)(lux * 2 / 7));
    }

    static void pair(uint32_t lux)
    {
        reading(lux);
        reading(lux);
    }

    static void rx_claim(sl_802154_short_addr_t from)
    {
        sl_zigbee_af_cluster_command_t cmd = {};
        cmd.source = from;
        EXPECT_TRUE(dh_rx_master_claim(&cmd));
    }

    static void rx(uint8_t current, uint8_t target, uint8_t duration_sec, sl_802154_short_addr_t from = LOWER_NODE)
    {
        uint8_t bytes[8] = { 0, 0, 0, 0, 0, current, target, duration_sec };
        sl_zigbee_af_cluster_command_t cmd = {};
        cmd.source = from;
        cmd.buffer = bytes;
        cmd.bufLen = sizeof(bytes);
        cmd.payloadStartIndex = 5;
        dh_rx_level_adjust(&cmd);
    }
};

/* --------------------------------------------------------------- upgrade */
TEST_F(LightSensor, ACalibratedMasterLosesTheMasterSettingOnUpgrade)
{
    m_flags = B_DHS_MASTER_LIGHT | (1UL << 10);     // Master, calibrated
    light_sensor_init();
    EXPECT_FALSE(flag_set(B_DHS_MASTER_LIGHT));
    EXPECT_FALSE(flag_set(B_DHS_OLD_FLAGS));
}

TEST_F(LightSensor, AMasterSelectedWithTheButtonsStaysAMasterAcrossAReboot)
{
    m_flags = B_DHS_MASTER_LIGHT;
    light_sensor_init();
    EXPECT_TRUE(flag_set(B_DHS_MASTER_LIGHT));
}

TEST_F(LightSensor, ACalibrationCutShortNoLongerLeavesOccupancyDisabled)
{
    m_flags = B_DHS_OLD_CALIBRATING | B_DISABLE_OCC_SENSOR | B_DISABLE_LIGHT_SENSOR;
    light_sensor_init();
    EXPECT_EQ(0u, m_flags);
}

TEST_F(LightSensor, OccupancyDisabledForAnotherReasonIsLeftAlone)
{
    m_flags = B_DISABLE_OCC_SENSOR | (1UL << 10);
    light_sensor_init();
    EXPECT_TRUE(flag_set(B_DISABLE_OCC_SENSOR));
}

TEST_F(LightSensor, TheOldDisabledFlagNoLongerStopsAFollower)
{
    m_flags = B_DISABLE_LIGHT_SENSOR;
    init_running();
    rx(100, 120, 20);
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

/* --------------------------------------------------------------- suspends in ms */
TEST_F(LightSensor, PowerUpSuspendsDhForTwentySeconds)
{
    light_sensor_init();
    rx(100, 120, 20);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);

    m_now += 19999UL;
    run_tasks();
    rx(100, 120, 20);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);

    m_now += 1UL;
    run_tasks();
    rx(100, 120, 20);
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, AShorterSuspendDoesNotCutALongerOneShort)
{
    init_running();
    suspend_light_sensor_control(120000UL);
    m_now += 1000UL;
    suspend_light_sensor_control(2000UL);
    m_now += 3000UL;
    run_tasks();
    rx(100, 120, 20);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);

    m_now += 116000UL;                         // 120 s after the first
    run_tasks();
    rx(100, 120, 20);
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, ALongerSuspendExtendsAShorterOne)
{
    init_running();
    suspend_light_sensor_control(2000UL);
    suspend_light_sensor_control(120000UL);
    m_now += 3000UL;
    run_tasks();
    rx(100, 120, 20);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
}

/* --------------------------------------------------------------- following the master */
TEST_F(LightSensor, ALightOnAtTheOnLevelFollowsTheTargetLevelAndDuration)
{
    init_running();
    rx(100, 120, 20);
    ASSERT_EQ(1u, move_to_level_dhs_fake.call_count);
    EXPECT_EQ(120, move_to_level_dhs_fake.arg0_val);
    EXPECT_EQ(200, move_to_level_dhs_fake.arg1_val);       // Deciseconds
}

TEST_F(LightSensor, TheDurationIsTakenFromTheMessage)
{
    init_running();
    rx(100, 120, 255);
    EXPECT_EQ(2550, move_to_level_dhs_fake.arg1_val);
}

TEST_F(LightSensor, TheMastersCurrentLevelIsNotUsedByAReceiver)
{
    init_running();
    rx(1, 120, 20);
    EXPECT_EQ(120, move_to_level_dhs_fake.arg0_val);
}

TEST_F(LightSensor, LevelZeroIsIgnored)
{
    init_running();
    rx(100, 0, 20);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, ALightFollowsWithoutBeingTheMasterOrHavingATarget)
{
    m_token_lux = 0;
    init_running();
    rx(100, 120, 20);
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, ALightThatIsOffStaysOff)
{
    init_running();
    read_on_off_attribute_fake.return_val = 0;
    rx(100, 120, 20);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, ALightAtMidStaysAtMid)
{
    init_running();
    is_occ_mid_or_lower_fake.return_val = true;
    rx(100, 120, 20);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, ALightJustChangedStaysAsItIs)
{
    init_running();
    suspend_light_sensor_control(3000UL);
    rx(100, 120, 20);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, AShortMessageIsIgnored)
{
    init_running();
    uint8_t bytes[7] = { 0, 0, 0, 0, 0, 100, 120 };
    sl_zigbee_af_cluster_command_t cmd = {};
    cmd.source = LOWER_NODE;
    cmd.buffer = bytes;
    cmd.bufLen = sizeof(bytes);
    cmd.payloadStartIndex = 5;
    EXPECT_TRUE(dh_rx_level_adjust(&cmd));
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
}

/* --------------------------------------------------------------- DH disabled */
TEST_F(LightSensor, ADisabledFollowerIgnoresALevel)
{
    init_running();
    dh_disable();
    rx(100, 120, 20);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, ADisabledMasterSendsNothing)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    dh_disable();
    for(int i = 0; i < 40; i++)
    {
        reading(100);
    }
    EXPECT_EQ(0u, tx_dh_level_adjust_fake.call_count);
}

TEST_F(LightSensor, DhStaysDisabledUntilEnabledHoweverLongItIs)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    dh_disable();
    m_now += 24UL * 60UL * 60UL * 1000UL;
    run_tasks();
    pair(100);
    EXPECT_EQ(0u, tx_dh_level_adjust_fake.call_count);

    dh_enable();
    pair(100);
    EXPECT_EQ(1u, tx_dh_level_adjust_fake.call_count);
}

TEST_F(LightSensor, ARestartEnablesDh)
{
    init_running();
    dh_disable();
    init_running();
    rx(100, 120, 20);
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, WritingATargetEnablesDh)
{
    init_running();
    dh_disable();
    save_light_lux_reference(800);
    rx(100, 120, 20);
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, BeingSelectedAsMasterEnablesDh)
{
    init_running();
    dh_disable();
    dh_set_master(true);
    rx(100, 120, 20, MY_NODE);
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, BeingDeselectedDoesNotEnableDh)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    dh_disable();
    dh_set_master(false);
    rx(100, 120, 20);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
}

/* --------------------------------------------------------------- global auto commissioning */
TEST_F(LightSensor, DuringGlobalAutoCommissioningTheMasterSendsNoLevel)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    global_ac_quiet_active_fake.return_val = true;
    for(int i = 0; i < 20; i++)
    {
        reading(100);
    }
    EXPECT_EQ(0u, tx_dh_level_adjust_fake.call_count);

    global_ac_quiet_active_fake.return_val = false;
    pair(100);
    EXPECT_EQ(1u, tx_dh_level_adjust_fake.call_count);
}

TEST_F(LightSensor, DuringGlobalAutoCommissioningAMasterCanStillBeSelected)
{
    init_running();
    global_ac_quiet_active_fake.return_val = true;
    dh_set_master(true);
    EXPECT_EQ(1u, tx_dh_master_claim_fake.call_count);
}

/* --------------------------------------------------------------- the target */
TEST_F(LightSensor, WritingATargetOnlyChangesTheTarget)
{
    m_flags = B_DHS_MASTER_LIGHT | B_50_PERCENT_MODE;
    init_running();
    save_light_lux_reference(800);
    EXPECT_EQ(800, m_token_lux);
    EXPECT_EQ(B_DHS_MASTER_LIGHT | B_50_PERCENT_MODE, m_flags);
}

TEST_F(LightSensor, TargetZeroStopsTheMaster)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    save_light_lux_reference(0);
    EXPECT_FALSE(allow_light_sensor_control());
    for(int i = 0; i < 20; i++)
    {
        reading(100);
    }
    EXPECT_EQ(0u, tx_dh_level_adjust_fake.call_count);
}

/* --------------------------------------------------------------- the master */
TEST_F(LightSensor, TheMasterSendsAfterTwoReadingsAndALightThatIsNotDoesNot)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    reading(350);
    EXPECT_EQ(0u, tx_dh_level_adjust_fake.call_count);
    reading(350);
    ASSERT_EQ(1u, tx_dh_level_adjust_fake.call_count);
    EXPECT_EQ(100, tx_dh_level_adjust_fake.arg0_val);
    EXPECT_GT(tx_dh_level_adjust_fake.arg1_val, 100);
    EXPECT_EQ(DEFAULT_DH_RAMP_DURATION_SEC, tx_dh_level_adjust_fake.arg2_val);

    RESET_FAKE(tx_dh_level_adjust);
    m_flags = 0;
    m_now += 30000UL;
    for(int i = 0; i < 10; i++)
    {
        reading(350);
    }
    EXPECT_EQ(0u, tx_dh_level_adjust_fake.call_count);
}

TEST_F(LightSensor, TheMasterDoesNotMoveItselfOnlyTheLoopbackDoes)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    pair(350);
    ASSERT_EQ(1u, tx_dh_level_adjust_fake.call_count);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);

    rx(100, tx_dh_level_adjust_fake.arg1_val, 20, MY_NODE);
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, AReadingAfterALightChangeIsNotUsed)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    reading(350);
    suspend_light_sensor_control(1000UL);      // Over before the next reading ...
    reading(350);                              // ... but it was in that reading
    EXPECT_EQ(0u, tx_dh_level_adjust_fake.call_count);
    pair(350);
    EXPECT_EQ(1u, tx_dh_level_adjust_fake.call_count);
}

TEST_F(LightSensor, ARedFlashAlsoSpoilsTheReading)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    reading(350);
    set_dhs_time_stamp();
    reading(350);
    EXPECT_EQ(0u, tx_dh_level_adjust_fake.call_count);
    pair(350);
    EXPECT_EQ(1u, tx_dh_level_adjust_fake.call_count);
}

TEST_F(LightSensor, TheLevelNeverGoesBelowTenPercentWithMinLevelZero)
{
    m_flags = B_DHS_MASTER_LIGHT;
    hal_get_dim_pwm_255_fake.return_val = 30;
    init_running();
    pair(3000);
    ASSERT_EQ(1u, tx_dh_level_adjust_fake.call_count);
    EXPECT_EQ(MIN_ALLOWABLE_LIGHT_LEVEL, tx_dh_level_adjust_fake.arg1_val);
}

TEST_F(LightSensor, ARefusedLevelIsSentAgainAtTheNextAverage)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    tx_dh_level_adjust_fake.return_val = false;
    pair(350);
    EXPECT_EQ(1u, tx_dh_level_adjust_fake.call_count);

    tx_dh_level_adjust_fake.return_val = true;
    pair(350);
    EXPECT_EQ(2u, tx_dh_level_adjust_fake.call_count);
}

/* --------------------------------------------------------------- diagnostic packet */
class LightSensorDebug : public LightSensor
{
protected:
    void SetUp() override
    {
        LightSensor::SetUp();
        dh_debug_send_fake.return_val = true;
        init_running();
    }

    // Readings until the next 20 s check has gone by, starting just after a check
    static void one_check(uint32_t lux = 350)
    {
        for(unsigned i = 0; i < DH_DEBUG_CHECK_READINGS; i++)
        {
            reading(lux);
        }
    }

    // Line up on the checks, and the level in the last packet
    static void settle(void)
    {
        for(unsigned i = 0; i < (DH_DEBUG_CHECK_READINGS * (DH_DEBUG_QUIET_CHECKS + 1U)); i++)
        {
            reading(350);
            if(0u != dh_debug_send_fake.call_count)
            {
                break;
            }
        }
        RESET_FAKE(dh_debug_send);
        dh_debug_send_fake.return_val = true;
    }
};

TEST_F(LightSensorDebug, NoLevelChangeSendsEvery120Seconds)
{
    settle();
    for(unsigned i = 0; i < DH_DEBUG_QUIET_CHECKS - 1U; i++)
    {
        one_check();
    }
    EXPECT_EQ(0u, dh_debug_send_fake.call_count);
    one_check();
    EXPECT_EQ(1u, dh_debug_send_fake.call_count);
    EXPECT_EQ(120000u, DH_DEBUG_QUIET_CHECKS * DH_DEBUG_CHECK_READINGS * 2500u);
}

TEST_F(LightSensorDebug, ALevelChangeSendsAtTheNext20SecondCheck)
{
    settle();
    hal_get_dim_pwm_255_fake.return_val = 110;
    one_check();
    ASSERT_EQ(1u, dh_debug_send_fake.call_count);
    EXPECT_EQ(110, dh_debug_send_fake.arg2_val);
    EXPECT_EQ(20000u, DH_DEBUG_CHECK_READINGS * 2500u);

    hal_get_dim_pwm_255_fake.return_val = 120;
    one_check();
    EXPECT_EQ(2u, dh_debug_send_fake.call_count);

    one_check();                                   // No change: back to the slow rate
    EXPECT_EQ(2u, dh_debug_send_fake.call_count);
}

TEST_F(LightSensorDebug, ALevelChangeRestartsThe120Seconds)
{
    settle();
    one_check();
    one_check();
    hal_get_dim_pwm_255_fake.return_val = 110;
    one_check();
    ASSERT_EQ(1u, dh_debug_send_fake.call_count);
    for(unsigned i = 0; i < DH_DEBUG_QUIET_CHECKS - 1U; i++)
    {
        one_check();
    }
    EXPECT_EQ(1u, dh_debug_send_fake.call_count);
    one_check();
    EXPECT_EQ(2u, dh_debug_send_fake.call_count);
}

TEST_F(LightSensorDebug, APacketThatIsNotSentIsTriedAgainAtTheNextCheck)
{
    settle();
    dh_debug_send_fake.return_val = false;         // No target yet, or refused
    for(unsigned i = 0; i < DH_DEBUG_QUIET_CHECKS; i++)
    {
        one_check();
    }
    EXPECT_EQ(1u, dh_debug_send_fake.call_count);
    one_check();
    EXPECT_EQ(2u, dh_debug_send_fake.call_count);

    dh_debug_send_fake.return_val = true;
    one_check();
    EXPECT_EQ(3u, dh_debug_send_fake.call_count);
    one_check();                                   // Sent: back to the slow rate
    EXPECT_EQ(3u, dh_debug_send_fake.call_count);
}

TEST_F(LightSensorDebug, ThePacketHasTheLuxTargetLevelAndFlags)
{
    settle();
    dh_disable();
    for(unsigned i = 0; i < DH_DEBUG_QUIET_CHECKS; i++)
    {
        one_check(700);
    }
    ASSERT_EQ(1u, dh_debug_send_fake.call_count);
    EXPECT_EQ(700, dh_debug_send_fake.arg0_val);   // The last 2 readings
    EXPECT_EQ(500, dh_debug_send_fake.arg1_val);
    EXPECT_EQ(100, dh_debug_send_fake.arg2_val);
    EXPECT_EQ(DH_DEBUG_MODE_DH_DISABLED, dh_debug_send_fake.arg3_val);
    EXPECT_EQ(0, dh_debug_send_fake.arg4_val);     // Has not sent a level
}

TEST_F(LightSensorDebug, TheMastersPacketHasTheLastLevelItSent)
{
    settle();
    m_flags = B_DHS_MASTER_LIGHT;       // After settle(), so its readings send no level
    pair(100);
    ASSERT_EQ(1u, tx_dh_level_adjust_fake.call_count);
    uint8_t sent = tx_dh_level_adjust_fake.arg1_val;
    RESET_FAKE(dh_debug_send);
    dh_debug_send_fake.return_val = true;
    hal_get_dim_pwm_255_fake.return_val = sent;
    one_check();
    ASSERT_EQ(1u, dh_debug_send_fake.call_count);
    EXPECT_EQ(DH_DEBUG_MODE_MASTER, dh_debug_send_fake.arg3_val);
    EXPECT_EQ(sent, dh_debug_send_fake.arg4_val);
}

TEST_F(LightSensorDebug, SendingALevelDoesNotItselfSendAPacket)
{
    settle();
    m_flags = B_DHS_MASTER_LIGHT;       // After settle(), so its readings send no level
    pair(350);
    ASSERT_EQ(1u, tx_dh_level_adjust_fake.call_count);
    EXPECT_EQ(0u, dh_debug_send_fake.call_count);
}

/* --------------------------------------------------------------- lux packet */
TEST_F(LightSensor, ALuxPacketIsSentEvery2Readings)
{
    light_sensor_init();
    past_suspends();
    for(unsigned i = 0; i < 8u; i++)
    {
        reading(350);
    }
    EXPECT_EQ(4u, dh_lux_debug_send_fake.call_count);   // 20 s: every 5 s
}

TEST_F(LightSensor, TheLuxPacketHasTheRawReadingsAndTheCalibratedLux)
{
    light_sensor_init();
    past_suspends();
    light_sensor_on_reading(100);
    EXPECT_EQ(0u, dh_lux_debug_send_fake.call_count);
    light_sensor_on_reading(201);
    ASSERT_EQ(1u, dh_lux_debug_send_fake.call_count);
    EXPECT_EQ(201, dh_lux_debug_send_fake.arg0_val);    // The last reading
    EXPECT_EQ(150, dh_lux_debug_send_fake.arg1_val);    // The average, before the factor
    EXPECT_EQ(dh_calibrated_lux(150), dh_lux_debug_send_fake.arg2_val);
}

TEST_F(LightSensor, TheLuxPacketIsSentEvenWhenDhIsNotRunning)
{
    get_present_network_fake.return_val = eDAINTREE_NWK;
    read_on_off_attribute_fake.return_val = 0;
    light_sensor_init();                                // Suspended
    light_sensor_on_reading(100);
    light_sensor_on_reading(100);
    EXPECT_EQ(1u, dh_lux_debug_send_fake.call_count);
}

TEST_F(LightSensor, ACalibratedLuxAbove16BitsIsNotCut)
{
    light_sensor_init();
    light_sensor_on_reading(0xFFFF);
    light_sensor_on_reading(0xFFFF);
    ASSERT_EQ(1u, dh_lux_debug_send_fake.call_count);
    EXPECT_LT(0xFFFFu, dh_lux_debug_send_fake.arg2_val);
}

/* --------------------------------------------------------------- illuminance attribute */
static uint16_t m_illum;
static sl_zigbee_af_status_t write_attr(uint8_t, uint16_t, sl_zigbee_af_attribute_id_t id, uint8_t *d, uint8_t)
{
    if(ZCL_ILLUM_MEASURED_VALUE_ATTRIBUTE_ID == id) { m_illum = (uint16_t)(d[0] | (d[1] << 8)); }
    return SL_ZIGBEE_ZCL_STATUS_SUCCESS;
}

static uint16_t illum_attribute(uint16_t mean_raw)
{
    light_sensor_info_t info = {};
    info.mean_lux = mean_raw;
    m_illum = 0x5555;
    sl_zigbee_af_write_server_attribute_fake.custom_fake = write_attr;
    execute_light_sensor_logic(&info);
    return m_illum;
}

TEST_F(LightSensor, NoLuxIsReportedAsTooLowToMeasure)
{
    EXPECT_EQ(0u, illum_attribute(0));
    get_present_network_fake.return_val = eDAINTREE_NWK;
    EXPECT_EQ(0u, illum_attribute(2));                  // Daintree: below 3 is 0 lux
}

TEST_F(LightSensor, TheAttributeIs10000Log10LuxPlus1)
{
    EXPECT_EQ(25441u, illum_attribute(100));            // 100 x 3.5 = 350 lux, log10 = 2.5441
}

/* --------------------------------------------------------------- one master per group */
TEST_F(LightSensor, BeingSelectedSetsTheFlagAndTellsTheGroup)
{
    init_running();
    dh_set_master(true);
    EXPECT_TRUE(flag_set(B_DHS_MASTER_LIGHT));
    EXPECT_EQ(1u, tx_dh_master_claim_fake.call_count);
}

TEST_F(LightSensor, BeingDeselectedClearsTheFlagAndSendsNothing)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    dh_set_master(false);
    EXPECT_FALSE(flag_set(B_DHS_MASTER_LIGHT));
    EXPECT_EQ(0u, tx_dh_master_claim_fake.call_count);
}

TEST_F(LightSensor, AMasterStandsDownWhenAnotherLightIsSelected)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    rx_claim(HIGHER_NODE);                     // Selected last wins, whatever its node ID
    EXPECT_FALSE(flag_set(B_DHS_MASTER_LIGHT));

    m_flags = B_DHS_MASTER_LIGHT;
    rx_claim(LOWER_NODE);
    EXPECT_FALSE(flag_set(B_DHS_MASTER_LIGHT));
}

TEST_F(LightSensor, ASecondMasterStopsSending)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    rx_claim(LOWER_NODE);
    for(int i = 0; i < 20; i++)
    {
        reading(350);
    }
    EXPECT_EQ(0u, tx_dh_level_adjust_fake.call_count);
}

TEST_F(LightSensor, AMasterKeepsTheSettingOnHearingItsOwnClaim)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    rx_claim(MY_NODE);
    EXPECT_TRUE(flag_set(B_DHS_MASTER_LIGHT));
}

TEST_F(LightSensor, AClaimDoesNothingToALightThatIsNotAMaster)
{
    init_running();
    rx_claim(HIGHER_NODE);
    EXPECT_EQ(0u, set_app_flags_nonvol_fake.call_count);
}

TEST_F(LightSensor, TwoMastersTheHigherNodeIdStandsDownOnHearingALevel)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    rx(100, 120, 20, LOWER_NODE);
    EXPECT_FALSE(flag_set(B_DHS_MASTER_LIGHT));
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);     // and it follows like any other light
}

TEST_F(LightSensor, TwoMastersTheLowerNodeIdStaysTheMaster)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    rx(100, 120, 20, HIGHER_NODE);
    EXPECT_TRUE(flag_set(B_DHS_MASTER_LIGHT));
}

TEST_F(LightSensor, AMasterHearingItsOwnLevelStaysTheMasterAndMoves)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    rx(100, 120, 20, MY_NODE);
    EXPECT_TRUE(flag_set(B_DHS_MASTER_LIGHT));
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, ALevelFromAnotherMasterStandsDownEvenWhileThisLightIgnoresTheLevel)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    is_occ_mid_or_lower_fake.return_val = true;
    rx(100, 120, 20, LOWER_NODE);
    EXPECT_FALSE(flag_set(B_DHS_MASTER_LIGHT));
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
}

/* --------------------------------------------------------------- master in 50% mode */
TEST_F(LightSensor, AMasterHeldAtItsFiftyPercentLimitStillRaisesTheRestOfTheGroup)
{
    m_flags = B_DHS_MASTER_LIGHT | B_50_PERCENT_MODE;
    m_token_lux = 800;
    m_token_stashed_max = 204;
    get_max_light_level_fake.return_val = 102;
    hal_get_dim_pwm_255_fake.return_val = 102;
    init_running();
    pair(100);
    ASSERT_EQ(1u, tx_dh_level_adjust_fake.call_count);
    uint8_t first = tx_dh_level_adjust_fake.arg1_val;
    EXPECT_GT(first, 102);                     // Above its own limit: the group's level

    while(1u == tx_dh_level_adjust_fake.call_count)
    {
        reading(100);                          // This light is still at 102
    }
    EXPECT_GT(tx_dh_level_adjust_fake.arg1_val, first);
}
