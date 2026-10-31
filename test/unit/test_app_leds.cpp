#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "fff.h"

extern "C" {
#include "nt3_test_stubs.h"
#include "app_leds.h"
#include "hal_leds.h"
void ref_network_status_start(void);   // reference/network_status_led_ref.c (original app.c code)
}

DEFINE_FFF_GLOBALS

extern "C" {
FAKE_VOID_FUNC(hal_leds_init);
FAKE_VOID_FUNC(hal_set_red_led, uint8_t);
FAKE_VOID_FUNC(hal_set_green_led, uint8_t);
FAKE_VOID_FUNC(hal_toggle_green_led);
FAKE_VALUE_FUNC(bool, hal_start_flashing_red_led, uint32_t, uint32_t, uint32_t);
FAKE_VALUE_FUNC(bool, hal_start_flashing_green_led, uint32_t, uint32_t, uint32_t);
FAKE_VALUE_FUNC(bool, is_red_led_free);
FAKE_VALUE_FUNC(sl_zigbee_network_status_t, sl_zigbee_af_network_state);
FAKE_VALUE_FUNC(bool, device_is_in_a_group);
FAKE_VALUE_FUNC(uint32_t, get_time_ms);
FAKE_VALUE_FUNC(bool, has_timed_out, uint32_t, uint32_t);
FAKE_VOID_FUNC(sl_zigbee_af_event_init, sl_zigbee_af_event_t *, test_event_handler_t);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_delay_ms, sl_zigbee_af_event_t *, uint32_t);
}

/* --------------------------------------------------------------- trace of LED driver calls */
static std::vector<std::string> m_trace;
static std::string fmt(const char *f, unsigned a, unsigned b = 0, unsigned c = 0)
{
    char buf[64];
    snprintf(buf, sizeof(buf), f, a, b, c);
    return buf;
}
static void t_init(void) { m_trace.push_back("init"); }
static void t_red(uint8_t on) { m_trace.push_back(fmt("red(%u)", on)); }
static void t_green(uint8_t on) { m_trace.push_back(fmt("green(%u)", on)); }
static void t_toggle_green(void) { m_trace.push_back("toggle_green"); }
static bool t_flash_red(uint32_t n, uint32_t on, uint32_t off) { m_trace.push_back(fmt("flash_red(%u,%u,%u)", n, on, off)); return true; }
static bool t_flash_green(uint32_t n, uint32_t on, uint32_t off) { m_trace.push_back(fmt("flash_green(%u,%u,%u)", n, on, off)); return true; }
static void t_delay(sl_zigbee_af_event_t *, uint32_t ms) { m_trace.push_back(fmt("next_check(%u)", ms)); }

static void (*m_handler)(sl_zigbee_af_event_t *);
static void t_event_init(sl_zigbee_af_event_t *, void (*h)(sl_zigbee_af_event_t *)) { m_handler = h; }

static uint32_t m_now;
static uint32_t now(void) { return m_now; }
static bool timed_out(uint32_t ts, uint32_t delay) { return (m_now - ts) >= delay; }

class AppLeds : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RESET_FAKE(hal_leds_init); RESET_FAKE(hal_set_red_led); RESET_FAKE(hal_set_green_led);
        RESET_FAKE(hal_toggle_green_led); RESET_FAKE(hal_start_flashing_red_led); RESET_FAKE(hal_start_flashing_green_led);
        RESET_FAKE(is_red_led_free); RESET_FAKE(sl_zigbee_af_network_state); RESET_FAKE(device_is_in_a_group);
        RESET_FAKE(get_time_ms); RESET_FAKE(has_timed_out); RESET_FAKE(sl_zigbee_af_event_init);
        RESET_FAKE(sl_zigbee_af_event_set_delay_ms);
        FFF_RESET_HISTORY();
        hal_leds_init_fake.custom_fake = t_init;
        hal_set_red_led_fake.custom_fake = t_red;
        hal_set_green_led_fake.custom_fake = t_green;
        hal_toggle_green_led_fake.custom_fake = t_toggle_green;
        hal_start_flashing_red_led_fake.custom_fake = t_flash_red;
        hal_start_flashing_green_led_fake.custom_fake = t_flash_green;
        sl_zigbee_af_event_set_delay_ms_fake.custom_fake = t_delay;
        sl_zigbee_af_event_init_fake.custom_fake = t_event_init;
        get_time_ms_fake.custom_fake = now;
        has_timed_out_fake.custom_fake = timed_out;
        m_trace.clear();
        m_handler = NULL;
        m_now = 100000;
    }

    static std::vector<std::string> trace(void)
    {
        std::vector<std::string> t = m_trace;
        m_trace.clear();
        return t;
    }
};

typedef std::vector<std::string> Trace;

/* --------------------------------------------------------------- network status */
// Runs first: the "first check after boot" state only exists once per program.
TEST_F(AppLeds, NetworkStatusBehavesExactlyLikeTheOriginal)
{
    static const sl_zigbee_network_status_t states[] = {
        SL_ZIGBEE_NO_NETWORK, SL_ZIGBEE_JOINING_NETWORK, SL_ZIGBEE_JOINED_NETWORK,
        SL_ZIGBEE_JOINED_NETWORK_NO_PARENT, SL_ZIGBEE_LEAVING_NETWORK, SL_ZIGBEE_JOINED_NETWORK };

    Trace traces[2];
    for(int impl = 0; impl < 2; impl++)
    {
        m_trace.clear();
        if(0 == impl) { ref_network_status_start(); }
        else          { app_leds_network_status_start(); }
        ASSERT_NE(nullptr, m_handler);

        uint32_t rng = 12345;
        for(int i = 0; i < 500; i++)
        {
            rng = rng * 1103515245u + 12345u;
            sl_zigbee_af_network_state_fake.return_val = states[(rng >> 8) % 6];
            device_is_in_a_group_fake.return_val = ((rng >> 12) & 1) != 0;
            is_red_led_free_fake.return_val = ((rng >> 16) % 4) != 0;
            char step[48];
            snprintf(step, sizeof(step), "-- check %d: nwk=%d group=%d free=%d", i,
                     sl_zigbee_af_network_state_fake.return_val, device_is_in_a_group_fake.return_val,
                     is_red_led_free_fake.return_val);
            m_trace.push_back(step);
            m_handler(NULL);
        }
        traces[impl] = m_trace;
    }
    ASSERT_EQ(traces[0].size(), traces[1].size());
    for(size_t i = 0; i < traces[0].size(); i++)
    {
        ASSERT_EQ(traces[0][i], traces[1][i]) << "at trace line " << i;
    }
    // the first check after boot, not joined: one long blink
    EXPECT_EQ("next_check(750)", traces[1][0]);
}

TEST_F(AppLeds, NetworkStatusPatterns)
{
    app_leds_network_status_start();
    is_red_led_free_fake.return_val = true;
    sl_zigbee_af_network_state_fake.return_val = SL_ZIGBEE_NO_NETWORK;
    m_handler(NULL);       // uses up the one-time "first check after boot" (if not already used)
    trace();

    m_handler(NULL);
    EXPECT_EQ((Trace{ "flash_red(1,500,500)", "next_check(6000)" }), trace());

    sl_zigbee_af_network_state_fake.return_val = SL_ZIGBEE_JOINED_NETWORK;
    device_is_in_a_group_fake.return_val = false;
    m_handler(NULL);
    EXPECT_EQ((Trace{ "flash_red(2,500,500)", "next_check(6000)" }), trace());

    device_is_in_a_group_fake.return_val = true;
    m_handler(NULL);
    EXPECT_EQ((Trace{ "next_check(12000)", "red(0)" }), trace());
}

TEST_F(AppLeds, NetworkStatusLeavesABusyRedLedAlone)
{
    app_leds_network_status_start();
    trace();
    is_red_led_free_fake.return_val = false;
    for(sl_zigbee_network_status_t s : { SL_ZIGBEE_NO_NETWORK, SL_ZIGBEE_JOINED_NETWORK })
    {
        for(bool group : { false, true })
        {
            sl_zigbee_af_network_state_fake.return_val = s;
            device_is_in_a_group_fake.return_val = group;
            m_handler(NULL);
            Trace t = trace();
            ASSERT_EQ(1u, t.size());
            EXPECT_EQ(0u, t[0].rfind("next_check(", 0));
        }
    }
}

/* --------------------------------------------------------------- startup */
TEST_F(AppLeds, StartupTurnsBothOnThenOff)
{
    app_leds_on_startup();
    EXPECT_EQ((Trace{ "init", "green(1)", "red(1)" }), trace());
    app_leds_on_startup_done();
    EXPECT_EQ((Trace{ "green(0)", "red(0)" }), trace());
}

/* --------------------------------------------------------------- buttons */
TEST_F(AppLeds, ButtonFeedback)
{
    app_leds_show_presses(4);
    app_leds_show_fail();
    app_leds_show_dim_step();
    EXPECT_EQ((Trace{ "flash_red(4,250,250)", "flash_red(1,1500,50)", "flash_red(1,500,50)" }), trace());
}

/* --------------------------------------------------------------- factory reset */
TEST_F(AppLeds, FactoryResetHoldsRedOn)
{
    app_leds_factory_reset(true);
    app_leds_factory_reset(false);
    EXPECT_EQ((Trace{ "red(1)", "red(0)" }), trace());
}

/* --------------------------------------------------------------- firmware version */
TEST_F(AppLeds, FirmwareVersionDigits)
{
    app_leds_off();
    app_leds_show_version_digit(APP_LED_RED, 9);
    app_leds_show_version_digit(APP_LED_GREEN, 1);
    EXPECT_EQ((Trace{ "green(0)", "red(0)", "flash_red(9,500,500)", "flash_green(1,500,500)" }), trace());
}

/* --------------------------------------------------------------- occupancy */
TEST_F(AppLeds, OccupancyLedOnWhileDetectedThenOff1250msAfterTheLastDetection)
{
    app_leds_on_occupancy_sample(true);
    m_now += 75;
    app_leds_on_occupancy_sample(true);          // re-asserted on every detected sample
    EXPECT_EQ((Trace{ "green(1)", "green(1)" }), trace());

    uint32_t last = m_now;
    for(m_now = last + 75; m_now < last + APP_LEDS_OCCUPANCY_HOLD_MS; m_now += 75)
    {
        app_leds_on_occupancy_sample(false);
    }
    EXPECT_TRUE(trace().empty());                // still on
    m_now = last + APP_LEDS_OCCUPANCY_HOLD_MS;
    app_leds_on_occupancy_sample(false);
    EXPECT_EQ((Trace{ "green(0)" }), trace());
}

TEST_F(AppLeds, OccupancyLedStaysOnAsLongAsMotionContinues)
{
    for(int i = 0; i < 1000; i++)                // 75 s of continuous motion
    {
        app_leds_on_occupancy_sample(true);
        m_now += 75;
    }
    Trace t = trace();
    for(const std::string &s : t)
    {
        EXPECT_EQ("green(1)", s);
    }
}

TEST_F(AppLeds, BadOccupancySensorFlickersGreen)
{
    app_leds_on_bad_occ_sensor();
    app_leds_on_bad_occ_sensor();
    EXPECT_EQ((Trace{ "toggle_green", "toggle_green" }), trace());
}

TEST_F(AppLeds, OccupancySensorStoppedTurnsGreenOff)
{
    app_leds_on_occ_sensor_stopped();
    EXPECT_EQ((Trace{ "green(0)" }), trace());
}
