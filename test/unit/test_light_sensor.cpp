#include <gtest/gtest.h>
#include <map>
#include "fff.h"

/*
 * light_sensor.c + dh_control.c: what is around the control logic.  Upgrade from
 * the firmware with calibration, when a light follows the master, suspends in
 * milliseconds, and what the master feeds the logic.
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
bool suspend_light_sensor_control(uint32_t delay_ms);
bool suspend_light_sensor_control_dimming(uint32_t delay_ms);
bool allow_light_sensor_control(void);
bool dh_rx_level_command(sl_zigbee_af_cluster_command_t *cmd);
bool dh_rx_master_claim(sl_zigbee_af_cluster_command_t *cmd);
void dh_set_master(bool enable);
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
FAKE_VALUE_FUNC(bool, tx_dh_level, uint8_t, uint8_t);
FAKE_VALUE_FUNC(bool, tx_dh_master_claim);
FAKE_VALUE_FUNC(sl_802154_short_addr_t, sl_zigbee_get_node_id);
FAKE_VOID_FUNC(move_to_level_dhs, uint8_t, uint16_t);
FAKE_VALUE_FUNC(broadcast_traffic_t, broadcast_monitor_get_traffic);
FAKE_VALUE_FUNC(sl_status_t, sl_token_get_data, uint32_t, uint32_t, void *, uint32_t);
FAKE_VALUE_FUNC(sl_status_t, sl_token_set_data, uint32_t, uint32_t, void *, uint32_t);
FAKE_VALUE_FUNC(sl_zigbee_af_status_t, sl_zigbee_af_read_server_attribute, uint8_t, uint16_t, sl_zigbee_af_attribute_id_t, uint8_t *, uint8_t);
FAKE_VALUE_FUNC(sl_zigbee_af_status_t, sl_zigbee_af_write_server_attribute, uint8_t, uint16_t, sl_zigbee_af_attribute_id_t, uint8_t *, uint8_t);
FAKE_VALUE_FUNC(bool, dh_debug_send, uint16_t, uint16_t, uint8_t, uint8_t, uint8_t);
FAKE_VOID_FUNC_VARARG(sl_zigbee_app_debug_println, const char *, ...);
FAKE_VOID_FUNC_VARARG(sl_zigbee_app_debug_print, const char *, ...);
}

#define FAKES(F) \
  F(hal_light_sensor_init) F(hal_get_dim_pwm_255) F(get_time_ms) F(has_timed_out) F(addTask) F(removeTask) \
  F(get_present_network) F(read_on_off_attribute) F(read_light_level_attribute) F(device_is_in_a_group) \
  F(is_occ_mid_or_lower) F(set_app_flags_nonvol) F(is_app_flags_nonvol_set) \
  F(get_min_light_level) F(get_max_light_level) F(get_random_number) F(tx_dh_level) F(tx_dh_master_claim) F(sl_zigbee_get_node_id) F(move_to_level_dhs) \
  F(broadcast_monitor_get_traffic) F(sl_token_get_data) F(sl_token_set_data) \
  F(sl_zigbee_af_read_server_attribute) F(sl_zigbee_af_write_server_attribute) \
  F(dh_debug_send) F(sl_zigbee_app_debug_println) F(sl_zigbee_app_debug_print)

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
        tx_dh_level_fake.return_val = true;
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

    static void init_running(void)
    {
        light_sensor_init();
        past_suspends();
    }

    // Raw lux 3.5 x this in the logic
    static void average(uint16_t lux)
    {
        light_sensor_info_t info = {};
        info.mean_lux = (uint16_t)(lux * 2 / 7);
        execute_light_sensor_logic(&info);
    }

    static void rx_claim(sl_802154_short_addr_t from)
    {
        sl_zigbee_af_cluster_command_t cmd = {};
        cmd.source = from;
        EXPECT_TRUE(dh_rx_master_claim(&cmd));
    }

    static void rx(uint8_t level, uint8_t ramp, sl_802154_short_addr_t from = LOWER_NODE)
    {
        uint8_t bytes[7] = { 0, 0, 0, 0, 0, level, ramp };
        sl_zigbee_af_cluster_command_t cmd = {};
        cmd.source = from;
        cmd.buffer = bytes;
        cmd.bufLen = sizeof(bytes);
        cmd.payloadStartIndex = 5;
        dh_rx_level_command(&cmd);
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
    rx(120, 185);
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

/* --------------------------------------------------------------- suspends in ms */
TEST_F(LightSensor, PowerUpSuspendsDhForTwentySecondsNotHalfAnHour)
{
    light_sensor_init();
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
    rx(120, 185);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);

    m_now += 19999UL;
    run_tasks();
    rx(120, 185);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);

    m_now += 1UL;
    run_tasks();
    rx(120, 185);
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, ADimmingSuspendOfTwoHoursLastsTwoHours)
{
    init_running();
    suspend_light_sensor_control_dimming(2UL * 60UL * 60UL * 1000UL);

    m_now += (2UL * 60UL * 60UL * 1000UL) - 1UL;
    run_tasks();
    rx(120, 185);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);

    m_now += 1UL;
    run_tasks();
    rx(120, 185);
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, AShorterSuspendDoesNotCutALongerOneShort)
{
    init_running();
    suspend_light_sensor_control(120000UL);    // A level command
    m_now += 1000UL;
    suspend_light_sensor_control(2000UL);      // Then an occupancy move
    m_now += 3000UL;
    run_tasks();
    rx(120, 185);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);

    m_now += 116000UL;                         // 120 s after the first
    run_tasks();
    rx(120, 185);
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, ALongerSuspendExtendsAShorterOne)
{
    init_running();
    suspend_light_sensor_control(2000UL);
    suspend_light_sensor_control(120000UL);
    m_now += 3000UL;
    run_tasks();
    rx(120, 185);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
}

/* --------------------------------------------------------------- following the master */
TEST_F(LightSensor, ALightOnAtTheOnLevelFollowsTheLevelAndRamp)
{
    init_running();
    rx(120, 185);
    ASSERT_EQ(1u, move_to_level_dhs_fake.call_count);
    EXPECT_EQ(120, move_to_level_dhs_fake.arg0_val);
    EXPECT_EQ(185, move_to_level_dhs_fake.arg1_val);
}

TEST_F(LightSensor, ALightFollowsWithoutBeingTheMasterOrHavingATarget)
{
    m_token_lux = 0;
    init_running();
    rx(120, 185);
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, ALightThatIsOffStaysOff)
{
    init_running();
    read_on_off_attribute_fake.return_val = 0;
    rx(120, 185);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, ALightAtMidStaysAtMid)
{
    init_running();
    is_occ_mid_or_lower_fake.return_val = true;
    rx(120, 185);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, ALightJustChangedOrDimmedByASwitchStaysAsItIs)
{
    init_running();
    suspend_light_sensor_control(3000UL);
    rx(120, 185);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
    past_suspends();

    suspend_light_sensor_control_dimming(3000UL);
    rx(120, 185);
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, AShortMessageIsIgnored)
{
    init_running();
    uint8_t bytes[6] = { 0, 0, 0, 0, 0, 120 };
    sl_zigbee_af_cluster_command_t cmd = {};
    cmd.source = LOWER_NODE;
    cmd.buffer = bytes;
    cmd.bufLen = sizeof(bytes);
    cmd.payloadStartIndex = 5;
    EXPECT_TRUE(dh_rx_level_command(&cmd));
    EXPECT_EQ(0u, move_to_level_dhs_fake.call_count);
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
    for(int i = 0; i < 5; i++)
    {
        average(100);
    }
    EXPECT_EQ(0u, tx_dh_level_fake.call_count);
}

/* --------------------------------------------------------------- the master */
TEST_F(LightSensor, TheMasterSendsWhenFarFromTheTargetAndALightThatIsNotDoesNot)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    average(350);    // Skipped: the power up suspend was in this average
    average(350);
    EXPECT_EQ(0u, tx_dh_level_fake.call_count);
    average(350);
    ASSERT_EQ(1u, tx_dh_level_fake.call_count);
    EXPECT_EQ(DH_RAMP_DS, tx_dh_level_fake.arg1_val);
    EXPECT_GT(tx_dh_level_fake.arg0_val, 100);

    RESET_FAKE(tx_dh_level);
    m_flags = 0;
    for(int i = 0; i < 5; i++)
    {
        average(350);
    }
    EXPECT_EQ(0u, tx_dh_level_fake.call_count);
}

TEST_F(LightSensor, AnAverageThatALightChangeFellIntoIsNotUsed)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    average(350);
    average(350);
    suspend_light_sensor_control(1000UL);      // Over before the average ends ...
    past_suspends();
    average(350);                              // ... but it is in this one ...
    average(350);                              // ... and could still be in this one
    EXPECT_EQ(0u, tx_dh_level_fake.call_count);
    average(350);
    EXPECT_EQ(1u, tx_dh_level_fake.call_count);
}

TEST_F(LightSensor, ARedFlashAlsoSpoilsTheAverage)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    average(350);
    average(350);
    set_dhs_time_stamp();
    average(350);
    average(350);
    EXPECT_EQ(0u, tx_dh_level_fake.call_count);
    average(350);
    EXPECT_EQ(1u, tx_dh_level_fake.call_count);
}

TEST_F(LightSensor, TheLevelNeverGoesBelowTenPercentWithMinLevelZero)
{
    m_flags = B_DHS_MASTER_LIGHT;
    hal_get_dim_pwm_255_fake.return_val = 30;
    init_running();
    average(3000);
    average(3000);
    average(3000);
    ASSERT_EQ(1u, tx_dh_level_fake.call_count);
    EXPECT_EQ(MIN_ALLOWABLE_LIGHT_LEVEL, tx_dh_level_fake.arg0_val);
}

TEST_F(LightSensor, ARefusedSendRetriesThroughTheTasker)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    average(350);
    average(350);
    tx_dh_level_fake.return_val = false;
    average(350);
    EXPECT_EQ(1u, tx_dh_level_fake.call_count);

    tx_dh_level_fake.return_val = true;
    m_now += 2000UL;
    run_tasks();
    EXPECT_EQ(2u, tx_dh_level_fake.call_count);
}

/* --------------------------------------------------------------- debug unicast */
TEST_F(LightSensor, EachLevelSentAlsoSendsTheDebugData)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    average(350);
    average(350);
    EXPECT_EQ(0u, dh_debug_send_fake.call_count);
    average(350);
    ASSERT_EQ(1u, tx_dh_level_fake.call_count);
    ASSERT_EQ(1u, dh_debug_send_fake.call_count);
    EXPECT_EQ(350, dh_debug_send_fake.arg0_val);
    EXPECT_EQ(500, dh_debug_send_fake.arg1_val);
    EXPECT_EQ(100, dh_debug_send_fake.arg2_val);
    EXPECT_EQ(DH_DEBUG_MODE_MASTER, dh_debug_send_fake.arg3_val);
    EXPECT_EQ(tx_dh_level_fake.arg0_val, dh_debug_send_fake.arg4_val);
}

TEST_F(LightSensor, ARefusedLevelSendsNoDebugDataUntilItGoesOut)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    average(350);
    average(350);
    tx_dh_level_fake.return_val = false;
    average(350);
    EXPECT_EQ(0u, dh_debug_send_fake.call_count);

    tx_dh_level_fake.return_val = true;
    m_now += 2000UL;
    run_tasks();
    EXPECT_EQ(1u, dh_debug_send_fake.call_count);
}

TEST_F(LightSensor, TheRepeatOfALevelAlsoSendsTheDebugData)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    average(350);
    average(350);
    average(350);
    average(500);
    average(500);
    average(500);      // In the deadband: the repeat
    EXPECT_EQ(2u, tx_dh_level_fake.call_count);
    EXPECT_EQ(2u, dh_debug_send_fake.call_count);
}

TEST_F(LightSensor, NoDebugDataOnAnAverageWithoutALevel)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    for(int i = 0; i < 6; i++)
    {
        average(500);
    }
    EXPECT_EQ(0u, dh_debug_send_fake.call_count);
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
    for(int i = 0; i < 6; i++)
    {
        average(350);
    }
    EXPECT_EQ(0u, tx_dh_level_fake.call_count);
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
    rx(120, 185, LOWER_NODE);
    EXPECT_FALSE(flag_set(B_DHS_MASTER_LIGHT));
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);     // and it follows like any other light
}

TEST_F(LightSensor, TwoMastersTheLowerNodeIdStaysTheMaster)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    rx(120, 185, HIGHER_NODE);
    EXPECT_TRUE(flag_set(B_DHS_MASTER_LIGHT));
}

TEST_F(LightSensor, AMasterHearingItsOwnLevelStaysTheMasterAndMoves)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    rx(120, 185, MY_NODE);
    EXPECT_TRUE(flag_set(B_DHS_MASTER_LIGHT));
    EXPECT_EQ(1u, move_to_level_dhs_fake.call_count);
}

TEST_F(LightSensor, ALevelFromAnotherMasterStandsDownEvenWhileThisLightIgnoresTheLevel)
{
    m_flags = B_DHS_MASTER_LIGHT;
    init_running();
    is_occ_mid_or_lower_fake.return_val = true;
    rx(120, 185, LOWER_NODE);
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
    average(100);
    average(100);
    average(100);
    ASSERT_EQ(1u, tx_dh_level_fake.call_count);
    uint8_t first = tx_dh_level_fake.arg0_val;
    EXPECT_GT(first, 102);                     // Above its own limit: the group's level

    average(100);
    average(100);
    average(100);                              // This light is still at 102
    ASSERT_EQ(2u, tx_dh_level_fake.call_count);
    EXPECT_GT(tx_dh_level_fake.arg0_val, first);
}
