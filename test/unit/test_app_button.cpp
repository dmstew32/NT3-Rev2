#include <gtest/gtest.h>
#include <utility>
#include <vector>
#include "fff.h"

extern "C" {
#include "nt3_test_stubs.h"
#include "app_button.h"
#include "hal_buttons.h"
}

DEFINE_FFF_GLOBALS

extern "C" {
FAKE_VALUE_FUNC(bool, hal_button_is_pressed, hal_button_t);
FAKE_VOID_FUNC(hal_buttons_register_sequence_cb, hal_buttons_sequence_cb_t);
FAKE_VALUE_FUNC(uint32_t, get_time_ms);
FAKE_VALUE_FUNC(bool, has_timed_out, uint32_t, uint32_t);
FAKE_VOID_FUNC(sl_zigbee_af_event_init, sl_zigbee_af_event_t *, test_event_handler_t);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_active, sl_zigbee_af_event_t *);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_delay_ms, sl_zigbee_af_event_t *, uint32_t);
FAKE_VOID_FUNC_VARARG(sl_zigbee_core_debug_print, const char *, ...);
}

static bool m_pressed[HAL_BUTTON_COUNT];
static bool is_pressed(hal_button_t b) { return (b < HAL_BUTTON_COUNT) && m_pressed[b]; }
static uint32_t m_now;
static uint32_t now(void) { return m_now; }
static bool timed_out(uint32_t ts, uint32_t delay) { return (m_now - ts) >= delay; }

static hal_buttons_sequence_cb_t m_hal_cb;
static void capture_hal_cb(hal_buttons_sequence_cb_t cb) { m_hal_cb = cb; }
static void (*m_poll)(sl_zigbee_af_event_t *);
static void capture_poll(sl_zigbee_af_event_t *, void (*h)(sl_zigbee_af_event_t *)) { m_poll = h; }

static std::vector<std::pair<uint8_t, uint8_t>> m_sequences;
static void seq_cb(uint8_t t, uint8_t d) { m_sequences.push_back({ t, d }); }
static int m_seq2_calls;
static void seq_cb2(uint8_t, uint8_t) { m_seq2_calls++; }
static void seq_cb3(uint8_t, uint8_t) {}

static std::vector<std::pair<app_button_t, uint32_t>> m_holds;   // gesture, time
static void hold_cb(app_button_t g) { m_holds.push_back({ g, m_now }); }
static bool m_both_enabled;
static bool both_enabled(void) { return m_both_enabled; }

class AppButton : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RESET_FAKE(hal_button_is_pressed); RESET_FAKE(hal_buttons_register_sequence_cb); RESET_FAKE(get_time_ms);
        RESET_FAKE(has_timed_out); RESET_FAKE(sl_zigbee_af_event_init); RESET_FAKE(sl_zigbee_af_event_set_active);
        RESET_FAKE(sl_zigbee_af_event_set_delay_ms); RESET_FAKE(sl_zigbee_core_debug_print);
        FFF_RESET_HISTORY();
        hal_button_is_pressed_fake.custom_fake = is_pressed;
        get_time_ms_fake.custom_fake = now;
        has_timed_out_fake.custom_fake = timed_out;
        hal_buttons_register_sequence_cb_fake.custom_fake = capture_hal_cb;
        sl_zigbee_af_event_init_fake.custom_fake = capture_poll;

        m_pressed[0] = m_pressed[1] = false;
        m_now = 50000;
        m_sequences.clear(); m_seq2_calls = 0; m_holds.clear(); m_both_enabled = true;
        app_button_on_startup();
        ASSERT_NE(nullptr, m_hal_cb);
        ASSERT_NE(nullptr, m_poll);
    }

    static void register_nt3_holds(void)
    {
        app_button_hold_t t = {}; t.first_ms = 4000; t.repeat_ms = 2500; t.cb = hold_cb;
        app_button_hold_t d = {}; d.first_ms = 4000; d.repeat_ms = APP_BUTTON_REPEAT_EVERY_POLL; d.cb = hold_cb;
        app_button_hold_t b = {}; b.first_ms = 4000; b.rearm = true; b.enabled = both_enabled; b.cb = hold_cb;
        ASSERT_EQ(SL_STATUS_OK, app_button_register_hold(APP_BUTTON_T, &t));
        ASSERT_EQ(SL_STATUS_OK, app_button_register_hold(APP_BUTTON_D, &d));
        ASSERT_EQ(SL_STATUS_OK, app_button_register_hold(APP_BUTTON_BOTH, &b));
    }

    static uint32_t poll(void)
    {
        m_poll(NULL);
        return sl_zigbee_af_event_set_delay_ms_fake.arg1_val;
    }

    // poll for duration_ms, every 200 ms (the rate while a button is down)
    static void hold_for(uint32_t duration_ms)
    {
        for(uint32_t t = 0; t < duration_ms; t += 200)
        {
            poll();
            m_now += 200;
        }
    }

    static void sequence(uint8_t t, uint8_t d, uint32_t t_age_ms, uint32_t d_age_ms)
    {
        hal_buttons_sequence_t s = {};
        s.presses[HAL_BUTTON_T] = t; s.presses[HAL_BUTTON_D] = d;
        s.last_press_ms[HAL_BUTTON_T] = m_now - t_age_ms;
        s.last_press_ms[HAL_BUTTON_D] = m_now - d_age_ms;
        m_hal_cb(&s);
    }
};

/* --------------------------------------------------------------- startup / polling */
TEST_F(AppButton, StartupStartsThePoll)
{
    EXPECT_EQ(1u, sl_zigbee_af_event_set_active_fake.call_count);
}

TEST_F(AppButton, PollsSlowlyWhenIdleAndFastWhileAnyButtonIsDown)
{
    EXPECT_EQ(BUTTONS_POLL_TIME_MS, poll());
    m_pressed[HAL_BUTTON_D] = true;
    EXPECT_EQ(BUTTONS_ACTIVE_POLL_TIME_MS, poll());
    m_pressed[HAL_BUTTON_D] = false;
    m_pressed[HAL_BUTTON_T] = true;          // line 481 fix: T alone polls fast too
    EXPECT_EQ(BUTTONS_ACTIVE_POLL_TIME_MS, poll());
    m_pressed[HAL_BUTTON_D] = true;
    EXPECT_EQ(BUTTONS_ACTIVE_POLL_TIME_MS, poll());
}

/* --------------------------------------------------------------- sequences */
TEST_F(AppButton, SequencePassesPressCounts)
{
    app_button_register_sequence_cb(seq_cb);
    sequence(2, 3, 1600, 1600);
    ASSERT_EQ(1u, m_sequences.size());
    EXPECT_EQ(2, m_sequences[0].first);
    EXPECT_EQ(3, m_sequences[0].second);
}

TEST_F(AppButton, SequenceWhoseLastPressWasAHoldIsIgnored)
{
    app_button_register_sequence_cb(seq_cb);
    sequence(1, 0, 2001, 99999);    // T pressed 2.001 s ago (held ~0.5 s): ignored
    sequence(1, 0, 2000, 99999);    // exactly 2 s: accepted
    sequence(0, 2, 99999, 1700);    // old T time does not matter, T was not pressed
    ASSERT_EQ(2u, m_sequences.size());
    EXPECT_EQ(1u, sl_zigbee_core_debug_print_fake.call_count);   // "Time out"
}

TEST_F(AppButton, TwoSequenceCallbacks)
{
    EXPECT_EQ(SL_STATUS_OK, app_button_register_sequence_cb(seq_cb));
    EXPECT_EQ(SL_STATUS_OK, app_button_register_sequence_cb(seq_cb));    // same one again: no new slot
    EXPECT_EQ(SL_STATUS_OK, app_button_register_sequence_cb(seq_cb2));
    EXPECT_EQ(SL_STATUS_FULL, app_button_register_sequence_cb(seq_cb3));
    EXPECT_EQ(SL_STATUS_INVALID_PARAMETER, app_button_register_sequence_cb(NULL));
    sequence(1, 1, 100, 100);
    EXPECT_EQ(1u, m_sequences.size());
    EXPECT_EQ(1, m_seq2_calls);
}

/* --------------------------------------------------------------- holds */
TEST_F(AppButton, HoldRegistrationIsChecked)
{
    app_button_hold_t h = {};
    EXPECT_EQ(SL_STATUS_INVALID_PARAMETER, app_button_register_hold(APP_BUTTON_T, &h));      // no callback
    h.cb = hold_cb;
    EXPECT_EQ(SL_STATUS_INVALID_PARAMETER, app_button_register_hold(APP_BUTTON_COUNT, &h));
    EXPECT_EQ(SL_STATUS_INVALID_PARAMETER, app_button_register_hold(APP_BUTTON_T, NULL));
}

TEST_F(AppButton, THoldFiresAfterFirstTimeThenRepeats)
{
    register_nt3_holds();
    m_pressed[HAL_BUTTON_T] = true;
    uint32_t start = m_now;            // first poll that sees T down arms the hold
    hold_for(10000);
    ASSERT_EQ(3u, m_holds.size());
    EXPECT_EQ(APP_BUTTON_T, m_holds[0].first);
    EXPECT_EQ(start + 4000, m_holds[0].second);
    EXPECT_EQ(start + 4000 + 2600, m_holds[1].second);   // 2.5 s, on the next 200 ms poll
    EXPECT_EQ(start + 4000 + 5200, m_holds[2].second);
}

TEST_F(AppButton, DHoldFiresOnEveryPollAfterFirstTime)
{
    register_nt3_holds();
    m_pressed[HAL_BUTTON_D] = true;
    uint32_t start = m_now;
    hold_for(5000);
    ASSERT_EQ(5u, m_holds.size());     // at 4.0, 4.2, 4.4, 4.6, 4.8 s
    EXPECT_EQ(APP_BUTTON_D, m_holds[0].first);
    EXPECT_EQ(start + 4000, m_holds[0].second);
    EXPECT_EQ(start + 4800, m_holds[4].second);
}

TEST_F(AppButton, ShortHoldDoesNothing)
{
    register_nt3_holds();
    m_pressed[HAL_BUTTON_T] = true;
    hold_for(3800);
    m_pressed[HAL_BUTTON_T] = false;
    poll();
    m_pressed[HAL_BUTTON_T] = true;    // pressed again: timing starts over
    hold_for(3800);
    EXPECT_TRUE(m_holds.empty());
}

TEST_F(AppButton, BothHoldFiresOnceThenRearms)
{
    register_nt3_holds();
    m_pressed[HAL_BUTTON_T] = m_pressed[HAL_BUTTON_D] = true;
    uint32_t start = m_now;
    hold_for(9000);
    ASSERT_EQ(2u, m_holds.size());
    EXPECT_EQ(APP_BUTTON_BOTH, m_holds[0].first);
    EXPECT_EQ(start + 4000, m_holds[0].second);
    EXPECT_EQ(start + 4000 + 200 + 4000, m_holds[1].second);   // re-armed on the next poll
}

TEST_F(AppButton, BothHoldOnlyWhileEnabled)
{
    register_nt3_holds();
    m_both_enabled = false;
    m_pressed[HAL_BUTTON_T] = m_pressed[HAL_BUTTON_D] = true;
    hold_for(9000);
    EXPECT_TRUE(m_holds.empty());
}

TEST_F(AppButton, SingleHoldsDoNotFireWhileTheOtherButtonIsDown)
{
    register_nt3_holds();
    m_both_enabled = false;
    m_pressed[HAL_BUTTON_T] = true;
    hold_for(1000);                    // T armed
    m_pressed[HAL_BUTTON_D] = true;
    hold_for(8000);
    EXPECT_TRUE(m_holds.empty());
}

TEST_F(AppButton, PressingTheSecondButtonLateDoesNotStartTheBothHold)
{
    // same as the original code: both must first be seen down on the same poll
    register_nt3_holds();
    m_pressed[HAL_BUTTON_T] = true;
    hold_for(400);
    m_pressed[HAL_BUTTON_D] = true;
    hold_for(9000);
    EXPECT_TRUE(m_holds.empty());
}

/* --------------------------------------------------------------- gestures */
TEST_F(AppButton, IsPressedPerGesture)
{
    m_pressed[HAL_BUTTON_T] = true;
    EXPECT_TRUE(app_button_is_pressed(APP_BUTTON_T));
    EXPECT_FALSE(app_button_is_pressed(APP_BUTTON_D));
    EXPECT_FALSE(app_button_is_pressed(APP_BUTTON_BOTH));
    m_pressed[HAL_BUTTON_D] = true;
    EXPECT_FALSE(app_button_is_pressed(APP_BUTTON_T));
    EXPECT_TRUE(app_button_is_pressed(APP_BUTTON_BOTH));
    EXPECT_FALSE(app_button_is_pressed(APP_BUTTON_COUNT));
}
