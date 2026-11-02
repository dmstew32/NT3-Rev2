#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "fff.h"

/*
 * occ_sensor.c (Adaptiv part) + occ_group.c + nwk_multicast.c together:
 * the light's actions, the special cases and the bytes on the air.
 */

extern "C" {
#include "nt3_test_stubs.h"
#include "zap-id.h"
#include "../../Light/nwk_sensors/occ_sensor.h"
#include "occ_group.h"
void suspend_occ_sensor_control(uint16_t Dsecs);
bool is_occ_mid_or_lower(void);
}

DEFINE_FFF_GLOBALS

extern "C" {
FAKE_VALUE_FUNC(which_nwk_t, get_present_network);
FAKE_VALUE_FUNC(uint32_t, get_time_ms);
FAKE_VALUE_FUNC(bool, has_timed_out, uint32_t, uint32_t);
FAKE_VOID_FUNC_VARARG(sl_zigbee_app_debug_println, const char *, ...);
FAKE_VOID_FUNC_VARARG(sl_zigbee_app_debug_print, const char *, ...);
FAKE_VALUE_FUNC(uint8_t, sl_zigbee_af_next_sequence);
FAKE_VALUE_FUNC(uint8_t, get_max_light_level);
FAKE_VALUE_FUNC(uint8_t, get_mid_light_level);
FAKE_VALUE_FUNC(uint8_t, get_min_light_level);
FAKE_VALUE_FUNC(uint8_t, get_on_light_level);
FAKE_VALUE_FUNC(uint8_t, get_present_light_level);
FAKE_VOID_FUNC(move_to_level_onoff, uint8_t, uint16_t);
FAKE_VALUE_FUNC(sl_zigbee_af_zcl_request_status_t, sl_zigbee_af_on_off_cluster_off_cb);
FAKE_VALUE_FUNC(bool, is_app_flags_nonvol_set, uint32_t);
FAKE_VOID_FUNC(hal_init_occ_sensor);
FAKE_VOID_FUNC(hal_start_occ_sensor);
FAKE_VOID_FUNC(hal_stop_occ_sensor);
FAKE_VOID_FUNC(hal_get_occ_sensor_info, occ_info_t *);
FAKE_VALUE_FUNC(uint16_t, get_unoccupied_delay);
FAKE_VALUE_FUNC(uint16_t, get_unoccupied_delay_to_off);
FAKE_VOID_FUNC(restore_unoccupied_delay);
FAKE_VALUE_FUNC(uint16_t, read_on_off_attribute, uint16_t);
FAKE_VALUE_FUNC(uint16_t, read_light_level_attribute, sl_zigbee_af_attribute_id_t);
FAKE_VALUE_FUNC(bool, write_light_level_attribute, sl_zigbee_af_attribute_id_t, uint8_t *);
FAKE_VALUE_FUNC(bool, off_from_switch);
FAKE_VOID_FUNC(clear_off_from_switch);
FAKE_VALUE_FUNC(bool, on_from_switch);
FAKE_VOID_FUNC(clear_on_from_switch);
FAKE_VALUE_FUNC(bool, device_is_in_a_group);
FAKE_VALUE_FUNC(uint8_t, get_number_of_multicast_bindings);
FAKE_VOID_FUNC(get_multicast_info, uint8_t, device_binding_info_t *);
FAKE_VALUE_FUNC(uint32_t, get_random_number, uint32_t, uint32_t);
FAKE_VALUE_FUNC(bool, suspend_light_sensor_control, uint16_t);
FAKE_VALUE_FUNC(uint8_t, addTask, test_task_fn_t, taskParm_t, taskParm_t);
FAKE_VALUE_FUNC(uint8_t, removeTask, test_task_fn_t);
FAKE_VALUE_FUNC(sl_status_t, sl_token_get_data, uint32_t, uint32_t, void *, uint32_t);
FAKE_VALUE_FUNC(sl_zigbee_af_status_t, sl_zigbee_af_read_server_attribute, uint8_t, uint16_t, sl_zigbee_af_attribute_id_t, uint8_t *, uint8_t);
FAKE_VALUE_FUNC(sl_zigbee_af_status_t, sl_zigbee_af_write_server_attribute, uint8_t, uint16_t, sl_zigbee_af_attribute_id_t, uint8_t *, uint8_t);
FAKE_VALUE_FUNC(sl_802154_short_addr_t, sl_zigbee_get_node_id);
FAKE_VALUE_FUNC(sl_802154_short_addr_t, sl_zigbee_af_get_node_id);
FAKE_VALUE_FUNC(sl_zigbee_network_status_t, sl_zigbee_af_network_state);
FAKE_VALUE_FUNC(sl_status_t, sl_zigbee_send_multicast, sl_zigbee_aps_frame_t *, uint8_t, uint16_t, sl_802154_short_addr_t,
                uint8_t, uint16_t, uint8_t, const uint8_t *, uint8_t *);
FAKE_VALUE_FUNC(sl_status_t, sl_zigbee_af_send_broadcast, sl_802154_short_addr_t, sl_802154_short_addr_t, uint8_t,
                sl_zigbee_aps_frame_t *, uint16_t, uint8_t *);
}

/* --------------------------------------------------------------- light model */
static const sl_802154_short_addr_t MY_NODE = 0x1111, OTHER_NODE = 0x2222;
static uint32_t m_now;
static uint8_t m_on, m_level, m_on_level, m_max, m_mid, m_min;
static uint32_t m_flags;
static uint16_t m_delay_s, m_off_s;
static bool m_grouped, m_off_from_switch, m_on_from_switch;
static uint32_t m_random_pct;                  // backoff = max * pct / 100
static test_task_fn_t m_task;
static taskParm_t m_task_p0, m_task_p1;

struct Air { sl_zigbee_aps_frame_t aps; uint16_t bcast; uint8_t radius; std::vector<uint8_t> bytes; uint32_t at; };
static std::vector<Air> m_air;
struct Move { uint8_t level; uint32_t at; };
static std::vector<Move> m_moves;
static std::vector<uint32_t> m_offs;

static uint32_t now(void) { return m_now; }
static bool timed_out(uint32_t ts, uint32_t d) { return (m_now - ts) >= d; }
static void move(uint8_t level, uint16_t) { m_level = level; m_on = 1; m_moves.push_back({ level, m_now }); }
static sl_zigbee_af_zcl_request_status_t off_cb(void) { m_on = 0; m_offs.push_back(m_now); return 0; }
static uint16_t on_off(uint16_t) { return m_on; }
static uint16_t read_level(sl_zigbee_af_attribute_id_t id) { return ZCL_ON_LEVEL_ATTRIBUTE_ID == id ? m_on_level : m_level; }
static bool write_level(sl_zigbee_af_attribute_id_t id, uint8_t *d) { if(ZCL_ON_LEVEL_ATTRIBUTE_ID == id) { m_on_level = *d; } return true; }
static bool flag_set(uint32_t f) { return 0 != (m_flags & f); }
static uint32_t rnd(uint32_t max, uint32_t min) { return min + (uint32_t)((uint64_t)(max - min) * m_random_pct / 100); }
static uint8_t add_task(test_task_fn_t f, taskParm_t p0, taskParm_t p1) { m_task = f; m_task_p0 = p0; m_task_p1 = p1; return 1; }
static uint8_t remove_task(test_task_fn_t f) { if(m_task == f) { m_task = NULL; } return 1; }
static sl_status_t token(uint32_t, uint32_t, void *d, uint32_t) { memcpy(d, &m_delay_s, 2); return SL_STATUS_OK; }
static void multicast_info(uint8_t, device_binding_info_t *i) { i->group_address = 0x1234; i->dst_ep = 1; }
static sl_status_t send_multicast(sl_zigbee_aps_frame_t *aps, uint8_t radius, uint16_t bcast, sl_802154_short_addr_t, uint8_t,
                                  uint16_t, uint8_t len, const uint8_t *msg, uint8_t *)
{
    m_air.push_back({ *aps, bcast, radius, std::vector<uint8_t>(msg, msg + len), m_now });
    return SL_STATUS_OK;
}

class OccSensor : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RESET_FAKE(get_present_network); RESET_FAKE(move_to_level_onoff); RESET_FAKE(sl_zigbee_af_on_off_cluster_off_cb);
        RESET_FAKE(clear_off_from_switch); RESET_FAKE(clear_on_from_switch); RESET_FAKE(restore_unoccupied_delay);
        RESET_FAKE(sl_zigbee_send_multicast); RESET_FAKE(hal_start_occ_sensor); RESET_FAKE(hal_stop_occ_sensor);
        FFF_RESET_HISTORY();
        get_present_network_fake.return_val = eADAPTIV_NWK;
        get_time_ms_fake.custom_fake = now;
        has_timed_out_fake.custom_fake = timed_out;
        get_max_light_level_fake.custom_fake = [] { return m_max; };
        get_mid_light_level_fake.custom_fake = [] { return m_mid; };
        get_min_light_level_fake.custom_fake = [] { return m_min; };
        get_on_light_level_fake.custom_fake = [] { return m_on_level; };
        get_present_light_level_fake.custom_fake = [] { return m_level; };
        move_to_level_onoff_fake.custom_fake = move;
        sl_zigbee_af_on_off_cluster_off_cb_fake.custom_fake = off_cb;
        is_app_flags_nonvol_set_fake.custom_fake = flag_set;
        get_unoccupied_delay_fake.custom_fake = [] { return m_delay_s; };
        get_unoccupied_delay_to_off_fake.custom_fake = [] { return m_off_s; };
        read_on_off_attribute_fake.custom_fake = on_off;
        read_light_level_attribute_fake.custom_fake = read_level;
        write_light_level_attribute_fake.custom_fake = write_level;
        off_from_switch_fake.custom_fake = [] { return m_off_from_switch; };
        clear_off_from_switch_fake.custom_fake = [] { m_off_from_switch = false; };
        on_from_switch_fake.custom_fake = [] { return m_on_from_switch; };
        clear_on_from_switch_fake.custom_fake = [] { m_on_from_switch = false; };
        device_is_in_a_group_fake.custom_fake = [] { return m_grouped; };
        get_number_of_multicast_bindings_fake.return_val = 1;
        get_multicast_info_fake.custom_fake = multicast_info;
        get_random_number_fake.custom_fake = rnd;
        addTask_fake.custom_fake = add_task;
        removeTask_fake.custom_fake = remove_task;
        sl_token_get_data_fake.custom_fake = token;
        sl_zigbee_get_node_id_fake.return_val = MY_NODE;
        sl_zigbee_af_network_state_fake.return_val = SL_ZIGBEE_JOINED_NETWORK;
        sl_zigbee_send_multicast_fake.custom_fake = send_multicast;

        m_now = 100000; m_on = 1; m_level = 204; m_on_level = 204; m_max = 204; m_mid = 51; m_min = 0;
        m_flags = 0; m_delay_s = 600; m_off_s = 300; m_grouped = true;
        m_off_from_switch = m_on_from_switch = false; m_random_pct = 0; m_task = NULL;
        m_air.clear(); m_moves.clear(); m_offs.clear();
        occ_sensor_init();
        run_task();
    }
    void TearDown() override { m_now += 100000; run_task(); }

    // unsuspend task (suspend_occ_sensor_control)
    static void run_task(void) { if(m_task) { m_task(m_task_p0, m_task_p1); } }

    // sensor samples every 75 ms for 'ms'; occupied(t) says what the sensor sees
    static void samples(uint32_t ms, bool (*occupied)(uint32_t) = nullptr)
    {
        for(uint32_t end = m_now + ms; (int32_t)(end - m_now) > 0;)
        {
            m_now += 75;
            occ_sensor_logic(occupied ? occupied(m_now) : false);
            if(m_task && (m_now - m_task_p0.u32) >= m_task_p1.u32) { run_task(); }
        }
    }
    static void rx(uint8_t command, std::vector<uint8_t> payload, sl_802154_short_addr_t from = OTHER_NODE)
    {
        std::vector<uint8_t> buf = { 0x15, 0x74, 0x15, 0x42, command };
        buf.insert(buf.end(), payload.begin(), payload.end());
        sl_zigbee_af_cluster_command_t cmd = {};
        cmd.source = from; cmd.buffer = buf.data(); cmd.bufLen = (uint16_t)buf.size();
        cmd.payloadStartIndex = 5; cmd.commandId = command; cmd.mfgSpecific = true; cmd.mfgCode = 0x1574;
        occ_sensor_rx_group_cmd(&cmd);
    }
    static void rx_off(void) { rx(ZCL_OCCUPANCY_SENSOR_LEVEL_CHANGE_COMMAND_ID, { OCC_GROUP_OFF, 0 }); }
    static void rx_wake(uint16_t s = 600) { rx(ZCL_OCCUPANCY_SENSOR_KEEPALIVE_COMMAND_ID, { (uint8_t)s, (uint8_t)(s >> 8), OCC_GROUP_ON, OCC_KEEPALIVE_WAKE, 204 }); }
    static std::vector<uint8_t> payload(const Air &a) { return std::vector<uint8_t>(a.bytes.begin() + 5, a.bytes.end()); }
};

static bool always(uint32_t) { return true; }

/* --------------------------------------------------------------- on the air */
TEST_F(OccSensor, PowerUpStartsOnAndSendsNothing)
{
    samples(5 * 60 * 1000);
    EXPECT_TRUE(m_air.empty());
    EXPECT_TRUE(m_moves.empty());
    EXPECT_FALSE(is_occ_mid_or_lower());
}

TEST_F(OccSensor, KeepaliveBytes)
{
    rx_off();
    samples(1000);                                     // off settles 250 ms
    m_air.clear();
    samples(75, always);                               // wake: backoff 0
    samples(75);
    ASSERT_EQ(1u, m_air.size());
    const Air &a = m_air[0];
    EXPECT_EQ(ZCL_OCCUPANCY_SENSING_CLUSTER_ID, a.aps.clusterId);
    EXPECT_EQ(0x0104, a.aps.profileId);
    EXPECT_EQ(1, a.aps.sourceEndpoint);
    EXPECT_EQ(0xFF, a.aps.destinationEndpoint);
    EXPECT_EQ(0x1234, a.aps.groupId);
    EXPECT_EQ(3, a.radius);
    EXPECT_EQ(0xFFFD, a.bcast);
    ASSERT_EQ(10u, a.bytes.size());
    EXPECT_EQ(0x15, a.bytes[0]);                       // cluster specific, mfg specific, no default response
    EXPECT_EQ(0x74, a.bytes[1]);
    EXPECT_EQ(0x15, a.bytes[2]);
    EXPECT_EQ(0xDF, a.bytes[4]);
    uint16_t t = a.bytes[5] | (a.bytes[6] << 8);
    EXPECT_NEAR(600, t, 1);                            // time until expiration, seconds
    EXPECT_EQ(OCC_GROUP_ON, a.bytes[7]);
    EXPECT_EQ(OCC_KEEPALIVE_WAKE, a.bytes[8]);
    EXPECT_EQ(204, a.bytes[9]);                        // wake level
}

TEST_F(OccSensor, VacancyDimsToMidThenTurnsOffWithLevelChangeBytes)
{
    m_on_level = 51;                                   // e.g. a previous ramp saved mid
    samples(600 * 1000 + 150);                         // the refresh goes out in the last minute
    ASSERT_EQ(2u, m_air.size());
    EXPECT_EQ(0xDF, m_air[0].bytes[4]);
    EXPECT_EQ(OCC_KEEPALIVE_REFRESH, m_air[0].bytes[8]);
    EXPECT_EQ(204, m_air[0].bytes[9]);                 // current level
    EXPECT_EQ(0xE0, m_air[1].bytes[4]);
    EXPECT_EQ((std::vector<uint8_t>{ OCC_GROUP_MID, 51 }), payload(m_air[1]));
    ASSERT_EQ(1u, m_moves.size());
    EXPECT_EQ(51, m_moves[0].level);
    EXPECT_EQ(204, m_on_level);                        // an on command will restore the wake level
    EXPECT_TRUE(is_occ_mid_or_lower());

    samples(300 * 1000 + 150);
    ASSERT_EQ(3u, m_air.size());                       // no keepalive before off
    EXPECT_EQ((std::vector<uint8_t>{ OCC_GROUP_OFF, 0 }), payload(m_air[2]));
    EXPECT_EQ(1u, m_offs.size());
    samples(60 * 60 * 1000);
    EXPECT_EQ(3u, m_air.size());                       // off: nothing more
}

TEST_F(OccSensor, EmModeStaysAtMidInsteadOfOff)
{
    m_flags = B_EM_MODE;
    samples(900 * 1000 + 300);
    ASSERT_EQ(3u, m_air.size());
    EXPECT_EQ((std::vector<uint8_t>{ OCC_GROUP_OFF, 51 }), payload(m_air[2]));
    EXPECT_TRUE(m_offs.empty());
    EXPECT_EQ(51, m_level);
}

TEST_F(OccSensor, MinLevelAboveZeroAlsoStaysAtMid)
{
    m_min = 25;
    samples(900 * 1000 + 300);
    EXPECT_TRUE(m_offs.empty());
    samples(10000);
    EXPECT_TRUE(is_occ_mid_or_lower());                // still "off" (at mid), not flipped back on
}

/* --------------------------------------------------------------- receiving */
TEST_F(OccSensor, ReceivedWakeTurnsTheLightOnToTheSwitchLevel)
{
    occ_sensor_set_switch_level(150);
    rx_off();
    EXPECT_EQ(1u, m_offs.size());
    rx_wake(500);
    ASSERT_EQ(1u, m_moves.size());
    EXPECT_EQ(150, m_moves[0].level);
    EXPECT_FALSE(is_occ_mid_or_lower());
    EXPECT_TRUE(m_air.empty());                        // receivers send nothing
}

TEST_F(OccSensor, SwitchLevelBelowMinimumIsIgnoredAndAboveMaxIsClamped)
{
    occ_sensor_set_switch_level(150);
    occ_sensor_set_switch_level(24);                   // dimmed to off by the switch
    rx_off(); rx_wake();
    EXPECT_EQ(150, m_moves.back().level);
    occ_sensor_set_switch_level(250);
    rx_off(); rx_wake();
    EXPECT_EQ(204, m_moves.back().level);              // trim max
}

TEST_F(OccSensor, ReceivedKeepaliveWhileOnLeavesTheLevelAlone)
{
    rx_wake();
    rx(ZCL_OCCUPANCY_SENSOR_KEEPALIVE_COMMAND_ID, { 30, 0, OCC_GROUP_ON, OCC_KEEPALIVE_REFRESH, 99 });
    EXPECT_TRUE(m_moves.empty());
}

TEST_F(OccSensor, ReceivedRefreshWithinTheMarginSyncsTheTimer)
{
    samples(575 * 1000);                               // 25 s left
    m_air.clear();
    rx(ZCL_OCCUPANCY_SENSOR_KEEPALIVE_COMMAND_ID, { 12, 0, OCC_GROUP_ON, OCC_KEEPALIVE_REFRESH, 204 });
    samples(11000);
    EXPECT_TRUE(m_moves.empty());
    samples(1200);
    ASSERT_EQ(1u, m_moves.size());                     // dims 12 s later with the sender, not 25 s
    EXPECT_EQ(51, m_moves[0].level);
}

TEST_F(OccSensor, LongerTimerRepliesToARefresh)
{
    samples(60 * 1000);                                // 540 s left
    rx(ZCL_OCCUPANCY_SENSOR_KEEPALIVE_COMMAND_ID, { 20, 0, OCC_GROUP_ON, OCC_KEEPALIVE_REFRESH, 204 });
    samples(150);
    ASSERT_EQ(1u, m_air.size());
    EXPECT_EQ(OCC_KEEPALIVE_REFRESH, m_air[0].bytes[8]);
    EXPECT_NEAR(540, m_air[0].bytes[5] | (m_air[0].bytes[6] << 8), 1);
    EXPECT_TRUE(m_moves.empty());
}

TEST_F(OccSensor, IgnoresItsOwnMulticast)
{
    rx(ZCL_OCCUPANCY_SENSOR_LEVEL_CHANGE_COMMAND_ID, { OCC_GROUP_OFF, 0 }, MY_NODE);
    EXPECT_TRUE(m_offs.empty());
}

TEST_F(OccSensor, IgnoresMessagesWhenOccupancyControlIsOff)
{
    m_flags = B_DISABLE_OCC_SENSOR;                    // DHS calibration, version display
    rx_off();
    m_flags = 0;
    m_delay_s = 0;                                     // occupancy sensor off
    rx_off();
    m_delay_s = 600;
    get_present_network_fake.return_val = eDAINTREE_NWK;
    rx_off();
    get_present_network_fake.return_val = eADAPTIV_NWK;
    EXPECT_TRUE(m_offs.empty());
}

TEST_F(OccSensor, IgnoresShortOrInvalidMessages)
{
    rx(ZCL_OCCUPANCY_SENSOR_LEVEL_CHANGE_COMMAND_ID, { OCC_GROUP_OFF });
    rx(ZCL_OCCUPANCY_SENSOR_LEVEL_CHANGE_COMMAND_ID, { 7, 0 });
    rx(ZCL_OCCUPANCY_SENSOR_KEEPALIVE_COMMAND_ID, { 10, 0, OCC_GROUP_ON, OCC_KEEPALIVE_WAKE });
    EXPECT_TRUE(m_offs.empty());
    rx_off();
    rx(ZCL_OCCUPANCY_SENSOR_KEEPALIVE_COMMAND_ID, { 10, 0, 9, OCC_KEEPALIVE_WAKE, 0 });
    EXPECT_TRUE(m_moves.empty());
}

/* --------------------------------------------------------------- special cases */
static uint32_t s_visit_start;
static bool every_30s(uint32_t t) { return ((t - s_visit_start) % 30000) < 75; }

TEST_F(OccSensor, OffFromASwitchHoldsOffForFiveMinutesAfterTheLastOccupancy)
{
    m_on = 0; m_off_from_switch = true;               // the switch turned the group off
    samples(1000);
    EXPECT_TRUE(is_occ_mid_or_lower());
    s_visit_start = m_now;
    samples(4 * 60 * 1000, every_30s);                 // people still there: stays off
    EXPECT_TRUE(m_moves.empty());
    EXPECT_TRUE(m_air.empty());
    samples(4 * 60 * 1000);                            // 4 min quiet: still holding
    samples(75, always);
    EXPECT_TRUE(m_moves.empty());
    samples(5 * 60 * 1000 + 100);                      // 5 min after that occupancy
    EXPECT_FALSE(m_off_from_switch);                   // hold over
    samples(75, always);
    samples(75);
    ASSERT_EQ(1u, m_moves.size());
    ASSERT_EQ(1u, m_air.size());
    EXPECT_EQ(OCC_KEEPALIVE_WAKE, m_air[0].bytes[8]);
}

TEST_F(OccSensor, ManualOnModeNeverTurnsLightsOnFromOff)
{
    m_flags = B_MANUAL_ON_MODE;
    rx_off();
    samples(1000);
    samples(10000, always);
    EXPECT_TRUE(m_moves.empty());
    EXPECT_TRUE(m_air.empty());
}

TEST_F(OccSensor, ManualOnModeStillWakesFromMid)
{
    m_flags = B_MANUAL_ON_MODE;
    rx(ZCL_OCCUPANCY_SENSOR_LEVEL_CHANGE_COMMAND_ID, { OCC_GROUP_MID, 51 });
    samples(3000);
    m_moves.clear();
    samples(75, always);
    ASSERT_EQ(1u, m_moves.size());
    EXPECT_EQ(204, m_moves[0].level);
}

TEST_F(OccSensor, ExternalOffAndOnSendNothing)
{
    m_on = 0;                                          // switch off
    samples(1000);
    EXPECT_TRUE(is_occ_mid_or_lower());
    m_on = 1;                                          // switch on
    samples(1000);
    EXPECT_FALSE(is_occ_mid_or_lower());
    EXPECT_TRUE(m_air.empty());
    EXPECT_TRUE(m_moves.empty());
}

TEST_F(OccSensor, ThreeHoursOffResetsTheWakeLevelToMax)
{
    occ_sensor_set_switch_level(120);
    rx_off();
    samples(3 * 60 * 60 * 1000 + 1000);
    EXPECT_EQ(204, m_on_level);
    samples(75, always);
    ASSERT_EQ(1u, m_moves.size());
    EXPECT_EQ(204, m_moves[0].level);
}

TEST_F(OccSensor, NotGroupedDoesNothing)
{
    m_grouped = false;
    samples(30 * 60 * 1000, always);
    samples(30 * 60 * 1000);
    EXPECT_TRUE(m_air.empty());
    EXPECT_TRUE(m_moves.empty());
}

TEST_F(OccSensor, OccupancyIsIgnoredWhileSuspendedAndTheTimerRestartsAfter)
{
    samples(500 * 1000);                               // 100 s left
    suspend_occ_sensor_control(20);                    // e.g. a switch command (2 s)
    samples(1000, always);                             // ignored
    samples(1200);                                     // unsuspended: timer restarted (600 s)
    samples(200 * 1000);
    EXPECT_TRUE(m_moves.empty());                      // would have dimmed at 600 s
    samples(400 * 1000 + 5000);
    ASSERT_EQ(1u, m_moves.size());
    EXPECT_EQ(51, m_moves[0].level);
}

TEST_F(OccSensor, OccupancyWhileOnSendsNothing)
{
    samples(30 * 60 * 1000, always);
    EXPECT_TRUE(m_air.empty());
    EXPECT_TRUE(m_moves.empty());
}
