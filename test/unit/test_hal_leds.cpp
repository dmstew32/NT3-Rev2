#include <gtest/gtest.h>
#include <map>
#include <string>
#include <vector>
#include "fff.h"

extern "C" {
#include "nt3_test_stubs.h"
#include "hal_leds.h"
}

DEFINE_FFF_GLOBALS

extern "C" {
FAKE_VOID_FUNC(GPIO_PinModeSet, int, unsigned int, int, unsigned int);
FAKE_VOID_FUNC(GPIO_PinOutSet, int, unsigned int);
FAKE_VOID_FUNC(GPIO_PinOutClear, int, unsigned int);
FAKE_VOID_FUNC(GPIO_PinOutToggle, int, unsigned int);
FAKE_VALUE_FUNC(unsigned int, GPIO_PinInGet, int, unsigned int);
FAKE_VOID_FUNC(sl_clock_manager_enable_bus_clock, int);
FAKE_VOID_FUNC(set_dhs_time_stamp);
FAKE_VOID_FUNC(sl_zigbee_af_event_init, sl_zigbee_af_event_t *, test_event_handler_t);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_active, sl_zigbee_af_event_t *);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_delay_ms, sl_zigbee_af_event_t *, uint32_t);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_inactive, sl_zigbee_af_event_t *);
FAKE_VALUE_FUNC(bool, sl_zigbee_af_event_is_scheduled, sl_zigbee_af_event_t *);
FAKE_VALUE_FUNC(uint32_t, get_time_ms);
FAKE_VOID_FUNC_VARARG(sl_zigbee_app_debug_println, const char *, ...);
}

#define RED_PORT   gpioPortD
#define RED_PIN    2
#define GREEN_PORT gpioPortD
#define GREEN_PIN  1

/* --------------------------------------------------------------- pin model (active low) */
static std::map<unsigned, unsigned> m_level;   // pin -> output level
static void pin_set(int, unsigned pin) { m_level[pin] = 1; }
static void pin_clear(int, unsigned pin) { m_level[pin] = 0; }
static void pin_toggle(int, unsigned pin) { m_level[pin] ^= 1; }
static unsigned pin_get(int, unsigned pin) { return m_level[pin]; }
static void mode_set(int, unsigned pin, int, unsigned out) { m_level[pin] = out; }
static bool red_on(void) { return 0 == m_level[RED_PIN]; }
static bool green_on(void) { return 0 == m_level[GREEN_PIN]; }

/* --------------------------------------------------------------- event model */
struct Ev { void (*handler)(sl_zigbee_af_event_t *); bool scheduled; uint32_t delay; };
static std::map<sl_zigbee_af_event_t *, Ev> m_ev;
static std::vector<sl_zigbee_af_event_t *> m_order;   // [0] red, [1] green (init order)
static void ev_init(sl_zigbee_af_event_t *e, void (*h)(sl_zigbee_af_event_t *)) { m_ev[e] = { h, false, 0 }; m_order.push_back(e); }
static void ev_active(sl_zigbee_af_event_t *e) { m_ev[e].scheduled = true; m_ev[e].delay = 0; }
static void ev_delay(sl_zigbee_af_event_t *e, uint32_t ms) { m_ev[e].scheduled = true; m_ev[e].delay = ms; }
static void ev_inactive(sl_zigbee_af_event_t *e) { m_ev[e].scheduled = false; }
static bool ev_scheduled(sl_zigbee_af_event_t *e) { return m_ev[e].scheduled; }

class HalLeds : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RESET_FAKE(GPIO_PinModeSet); RESET_FAKE(GPIO_PinOutSet); RESET_FAKE(GPIO_PinOutClear);
        RESET_FAKE(GPIO_PinOutToggle); RESET_FAKE(GPIO_PinInGet); RESET_FAKE(sl_clock_manager_enable_bus_clock);
        RESET_FAKE(set_dhs_time_stamp); RESET_FAKE(sl_zigbee_af_event_init); RESET_FAKE(sl_zigbee_af_event_set_active);
        RESET_FAKE(sl_zigbee_af_event_set_delay_ms); RESET_FAKE(sl_zigbee_af_event_set_inactive);
        RESET_FAKE(sl_zigbee_af_event_is_scheduled); RESET_FAKE(get_time_ms); RESET_FAKE(sl_zigbee_app_debug_println);
        FFF_RESET_HISTORY();
        GPIO_PinModeSet_fake.custom_fake = mode_set;
        GPIO_PinOutSet_fake.custom_fake = pin_set;
        GPIO_PinOutClear_fake.custom_fake = pin_clear;
        GPIO_PinOutToggle_fake.custom_fake = pin_toggle;
        GPIO_PinInGet_fake.custom_fake = pin_get;
        sl_zigbee_af_event_init_fake.custom_fake = ev_init;
        sl_zigbee_af_event_set_active_fake.custom_fake = ev_active;
        sl_zigbee_af_event_set_delay_ms_fake.custom_fake = ev_delay;
        sl_zigbee_af_event_set_inactive_fake.custom_fake = ev_inactive;
        sl_zigbee_af_event_is_scheduled_fake.custom_fake = ev_scheduled;
        m_level.clear(); m_ev.clear(); m_order.clear();
        hal_leds_init();
        ASSERT_EQ(2u, m_order.size());
    }

    static sl_zigbee_af_event_t *red_ev(void) { return m_order[0]; }
    static sl_zigbee_af_event_t *green_ev(void) { return m_order[1]; }

    // run an LED's flash event until it stops; returns "on/off@delay" for every step
    static std::vector<std::string> run(sl_zigbee_af_event_t *e, bool (*is_on)(void), int max_steps = 50)
    {
        std::vector<std::string> steps;
        while(m_ev[e].scheduled && max_steps--)
        {
            m_ev[e].scheduled = false;
            m_ev[e].handler(e);
            steps.push_back(std::string(is_on() ? "on" : "off") + (m_ev[e].scheduled ? "@" + std::to_string(m_ev[e].delay) : ""));
        }
        return steps;
    }
};

TEST_F(HalLeds, InitLeavesBothLedsOff)
{
    EXPECT_EQ(2u, GPIO_PinModeSet_fake.call_count);
    EXPECT_EQ(gpioModeWiredAndPullUp, GPIO_PinModeSet_fake.arg2_history[0]);
    EXPECT_FALSE(red_on());
    EXPECT_FALSE(green_on());
}

TEST_F(HalLeds, OnOffAreActiveLowAndOnTellsTheLightSensor)
{
    hal_set_red_led(1);
    EXPECT_TRUE(red_on());
    EXPECT_EQ(1u, set_dhs_time_stamp_fake.call_count);
    hal_set_green_led(1);
    EXPECT_TRUE(green_on());
    EXPECT_EQ(2u, set_dhs_time_stamp_fake.call_count);
    hal_set_red_led(0);
    hal_set_green_led(0);
    EXPECT_FALSE(red_on());
    EXPECT_FALSE(green_on());
    EXPECT_EQ(2u, set_dhs_time_stamp_fake.call_count);    // only "on" sets it
}

TEST_F(HalLeds, EveryOnCallTellsTheLightSensorEvenIfAlreadyOn)
{
    // (this is why a steady occupancy LED keeps the DHS timestamp fresh)
    hal_set_green_led(1);
    hal_set_green_led(1);
    hal_set_green_led(1);
    EXPECT_EQ(3u, set_dhs_time_stamp_fake.call_count);
}

TEST_F(HalLeds, LedsOffTurnsBothOff)
{
    hal_set_red_led(1);
    hal_set_green_led(1);
    hal_leds_off();
    EXPECT_FALSE(red_on());
    EXPECT_FALSE(green_on());
}

TEST_F(HalLeds, ToggleBothLeds)
{
    hal_toggle_red_led();
    hal_toggle_green_led();
    EXPECT_TRUE(red_on());
    EXPECT_TRUE(green_on());
}

TEST_F(HalLeds, RedFlashPattern)
{
    hal_set_red_led(1);
    EXPECT_TRUE(hal_start_flashing_red_led(2, 250, 100));
    EXPECT_FALSE(red_on());                     // always starts off
    EXPECT_FALSE(is_red_led_free());
    std::vector<std::string> expected = { "on@250", "off@100", "on@250", "off" };
    EXPECT_EQ(expected, run(red_ev(), red_on));
    EXPECT_TRUE(is_red_led_free());
}

TEST_F(HalLeds, RedFlashIsRefusedWhileOneIsRunning)
{
    EXPECT_TRUE(hal_start_flashing_red_led(1, 2000, 5));
    EXPECT_FALSE(hal_start_flashing_red_led(3, 250, 250));
    std::vector<std::string> expected = { "on@2000", "off" };   // first pattern unchanged
    EXPECT_EQ(expected, run(red_ev(), red_on));
}

TEST_F(HalLeds, ZeroFlashesIsRefused)
{
    EXPECT_FALSE(hal_start_flashing_red_led(0, 250, 250));
    EXPECT_FALSE(hal_start_flashing_green_led(0, 250, 250));
    EXPECT_TRUE(is_red_led_free());
}

TEST_F(HalLeds, StopRedFlash)
{
    hal_start_flashing_red_led(5, 250, 250);
    run(red_ev(), red_on, 1);                   // one step: on
    hal_stop_flashing_red_led();
    EXPECT_FALSE(red_on());
    EXPECT_TRUE(is_red_led_free());
}

TEST_F(HalLeds, GreenFlashPattern)
{
    EXPECT_TRUE(hal_start_flashing_green_led(2, 500, 300));
    EXPECT_FALSE(green_on());
    std::vector<std::string> expected = { "on@500", "off@300", "on@500", "off" };
    EXPECT_EQ(expected, run(green_ev(), green_on));
}

TEST_F(HalLeds, GreenFlashReplacesARunningOne)
{
    EXPECT_TRUE(hal_start_flashing_green_led(5, 500, 500));
    run(green_ev(), green_on, 1);
    EXPECT_TRUE(hal_start_flashing_green_led(1, 100, 100));
    std::vector<std::string> expected = { "on@100", "off" };
    EXPECT_EQ(expected, run(green_ev(), green_on));
}

TEST_F(HalLeds, StopGreenFlash)
{
    hal_start_flashing_green_led(5, 250, 250);
    run(green_ev(), green_on, 1);
    hal_stop_flashing_green_led();
    EXPECT_FALSE(green_on());
    EXPECT_FALSE(m_ev[green_ev()].scheduled);
}

TEST_F(HalLeds, ForcingRedOffMidFlashStillEndsOff)
{
    // what hal_leds_off() from the button interrupt does to a running red flash
    hal_start_flashing_red_led(2, 250, 250);
    run(red_ev(), red_on, 1);                   // on
    hal_leds_off();                             // forced off
    std::vector<std::string> steps = run(red_ev(), red_on);
    ASSERT_FALSE(steps.empty());
    EXPECT_EQ("off", steps.back());
}
