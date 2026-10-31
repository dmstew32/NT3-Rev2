#include <gtest/gtest.h>
#include <vector>
#include "fff.h"

extern "C" {
#include "nt3_test_stubs.h"
#include "hal_buttons.h"
#include "hal_leds.h"
#include "hal_utils.h"
void GPIO_ODD_IRQHandler(void);
}

DEFINE_FFF_GLOBALS

extern "C" {
FAKE_VALUE_FUNC(uint32_t, get_time_ms);
FAKE_VALUE_FUNC(bool, has_timed_out, uint32_t, uint32_t);
FAKE_VALUE_FUNC(int8_t, set_one_shot_us_timer_isr, test_isr_fn_t, uint32_t);
FAKE_VALUE_FUNC(bool, disable_one_shot_us_timer_isr, int8_t);
FAKE_VOID_FUNC(hal_leds_off);
FAKE_VOID_FUNC(sl_zigbee_af_isr_event_init, sl_zigbee_af_event_t *, test_event_handler_t);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_active, sl_zigbee_af_event_t *);
FAKE_VOID_FUNC_VARARG(sl_zigbee_app_debug_println, const char *, ...);
FAKE_VOID_FUNC_VARARG(sl_zigbee_core_debug_print, const char *, ...);
FAKE_VOID_FUNC(GPIO_PinModeSet, int, unsigned int, int, unsigned int);
FAKE_VALUE_FUNC(int32_t, sl_hal_gpio_configure_external_interrupt, const sl_gpio_t *, int32_t, int);
FAKE_VOID_FUNC(sl_clock_manager_enable_bus_clock, int);
FAKE_VOID_FUNC(NVIC_EnableIRQ, int);

/* register model: writes to IF_CLR / IEN_SET take effect on the next GPIO access */
test_gpio_t test_gpio;
void test_gpio_sync(void)
{
    test_gpio.IF &= ~test_gpio.IF_CLR;
    test_gpio.IF_CLR = 0;
    test_gpio.IEN |= test_gpio.IEN_SET;
    test_gpio.IEN_SET = 0;
}
}

static uint32_t m_now;
static uint32_t now(void) { return m_now; }
static bool timed_out(uint32_t ts, uint32_t delay) { return (m_now - ts) >= delay; }
static int32_t configure_int(const sl_gpio_t *, int32_t int_no, int) { return int_no; }

static void (*m_one_shot)(void);
static uint32_t m_one_shot_us;
static int8_t m_next_slot;
static int8_t set_one_shot(void (*f)(void), uint32_t us) { m_one_shot = f; m_one_shot_us = us; return m_next_slot; }

static void (*m_done_handler)(sl_zigbee_af_event_t *);
static void capture_isr_event(sl_zigbee_af_event_t *, void (*h)(sl_zigbee_af_event_t *)) { m_done_handler = h; }

static std::vector<hal_buttons_sequence_t> m_sequences;
static void on_sequence(const hal_buttons_sequence_t *p) { m_sequences.push_back(*p); }

class HalButtons : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RESET_FAKE(get_time_ms); RESET_FAKE(has_timed_out); RESET_FAKE(set_one_shot_us_timer_isr);
        RESET_FAKE(disable_one_shot_us_timer_isr); RESET_FAKE(hal_leds_off); RESET_FAKE(sl_zigbee_af_isr_event_init);
        RESET_FAKE(sl_zigbee_af_event_set_active); RESET_FAKE(sl_zigbee_app_debug_println);
        RESET_FAKE(sl_zigbee_core_debug_print); RESET_FAKE(GPIO_PinModeSet);
        RESET_FAKE(sl_hal_gpio_configure_external_interrupt); RESET_FAKE(sl_clock_manager_enable_bus_clock);
        RESET_FAKE(NVIC_EnableIRQ);
        FFF_RESET_HISTORY();

        get_time_ms_fake.custom_fake = now;
        has_timed_out_fake.custom_fake = timed_out;
        set_one_shot_us_timer_isr_fake.custom_fake = set_one_shot;
        sl_hal_gpio_configure_external_interrupt_fake.custom_fake = configure_int;
        sl_zigbee_af_isr_event_init_fake.custom_fake = capture_isr_event;

        memset(&test_gpio, 0, sizeof(test_gpio));
        test_gpio.P[gpioPortB].DIN = 0xFFFF;   // pull-ups: released
        test_gpio.P[gpioPortD].DIN = 0xFFFF;
        m_now = 10000;
        m_one_shot = NULL;
        m_next_slot = 1;
        m_done_handler = NULL;
        m_sequences.clear();

        ASSERT_EQ(SL_STATUS_OK, hal_buttons_init());
        hal_buttons_register_sequence_cb(on_sequence);
        // flush any sequence left over from the previous test (module state is static):
        // an interrupt with both buttons released starts the end timer, then let it run
        test_gpio.IF |= BUTTON_0_INT_FLAG | BUTTON_1_INT_FLAG;
        GPIO_ODD_IRQHandler();
        finish_sequence();
        m_sequences.clear();
        RESET_FAKE(disable_one_shot_us_timer_isr);
        RESET_FAKE(hal_leds_off);
        RESET_FAKE(sl_zigbee_af_event_set_active);
        RESET_FAKE(sl_zigbee_core_debug_print);
    }

    static void set_pin(hal_button_t b, bool pressed)
    {
        uint8_t port = (HAL_BUTTON_T == b) ? BUTTON_0_PORT : BUTTON_1_PORT;
        uint8_t pin = (HAL_BUTTON_T == b) ? BUTTON_0_PIN : BUTTON_1_PIN;
        uint32_t flag = (HAL_BUTTON_T == b) ? BUTTON_0_INT_FLAG : BUTTON_1_INT_FLAG;
        if(pressed) { test_gpio.P[port].DIN &= ~(1u << pin); }
        else        { test_gpio.P[port].DIN |= (1u << pin); }
        test_gpio_sync();
        test_gpio.IF |= flag;
        GPIO_ODD_IRQHandler();
    }

    static void press(hal_button_t b, uint32_t down_ms, uint32_t gap_ms = 200)
    {
        set_pin(b, true);
        m_now += down_ms;
        set_pin(b, false);
        m_now += gap_ms;
    }

    // the 1.5 s timer fires, then the main loop runs the event
    static void finish_sequence(void)
    {
        if(m_one_shot)
        {
            void (*f)(void) = m_one_shot;
            m_one_shot = NULL;
            f();
        }
        if(m_done_handler)
        {
            m_done_handler(NULL);
        }
    }
};

TEST_F(HalButtons, InitConfiguresBothPinsWithPullUpsAndInterrupts)
{
    RESET_FAKE(GPIO_PinModeSet);
    RESET_FAKE(sl_hal_gpio_configure_external_interrupt);
    sl_hal_gpio_configure_external_interrupt_fake.custom_fake = configure_int;
    hal_buttons_init();
    ASSERT_EQ(2u, GPIO_PinModeSet_fake.call_count);
    EXPECT_EQ(BUTTON_0_PORT, GPIO_PinModeSet_fake.arg0_history[0]);
    EXPECT_EQ((unsigned)BUTTON_0_PIN, GPIO_PinModeSet_fake.arg1_history[0]);
    EXPECT_EQ(gpioModeInputPullFilter, GPIO_PinModeSet_fake.arg2_history[0]);
    EXPECT_EQ(BUTTON_1_PORT, GPIO_PinModeSet_fake.arg0_history[1]);
    EXPECT_EQ(2u, sl_hal_gpio_configure_external_interrupt_fake.call_count);
    EXPECT_EQ(BUTTON_0_INT_FLAG | BUTTON_1_INT_FLAG, test_gpio.IEN & (BUTTON_0_INT_FLAG | BUTTON_1_INT_FLAG));
}

TEST_F(HalButtons, IsPressedReadsActiveLowPins)
{
    EXPECT_FALSE(hal_button_is_pressed(HAL_BUTTON_T));
    test_gpio.P[BUTTON_0_PORT].DIN &= ~(1u << BUTTON_0_PIN);
    EXPECT_TRUE(hal_button_is_pressed(HAL_BUTTON_T));
    EXPECT_FALSE(hal_button_is_pressed(HAL_BUTTON_D));
    test_gpio.P[BUTTON_1_PORT].DIN &= ~(1u << BUTTON_1_PIN);
    EXPECT_TRUE(hal_button_is_pressed(HAL_BUTTON_D));
    EXPECT_FALSE(hal_button_is_pressed(HAL_BUTTON_COUNT));
}

TEST_F(HalButtons, PressesAreCountedPerButton)
{
    press(HAL_BUTTON_T, 100);
    press(HAL_BUTTON_T, 100);
    press(HAL_BUTTON_D, 100);
    finish_sequence();
    ASSERT_EQ(1u, m_sequences.size());
    EXPECT_EQ(2, m_sequences[0].presses[HAL_BUTTON_T]);
    EXPECT_EQ(1, m_sequences[0].presses[HAL_BUTTON_D]);
}

TEST_F(HalButtons, DebounceNeedsTheButtonDownFor75ms)
{
    press(HAL_BUTTON_T, 74);   // too short
    press(HAL_BUTTON_T, 75);   // counts
    finish_sequence();
    ASSERT_EQ(1u, m_sequences.size());
    EXPECT_EQ(1, m_sequences[0].presses[HAL_BUTTON_T]);
}

TEST_F(HalButtons, LastPressTimeIsTheLastFallingEdge)
{
    press(HAL_BUTTON_T, 100);
    uint32_t last = m_now;
    press(HAL_BUTTON_T, 100);
    finish_sequence();
    EXPECT_EQ(last, m_sequences[0].last_press_ms[HAL_BUTTON_T]);
}

TEST_F(HalButtons, SequenceEndsWhenBothReleasedFor1500ms)
{
    set_pin(HAL_BUTTON_T, true);
    EXPECT_EQ(nullptr, m_one_shot);            // no timer while a button is down
    m_now += 100;
    set_pin(HAL_BUTTON_T, false);
    ASSERT_NE(nullptr, m_one_shot);
    EXPECT_EQ(BUTTONS_DONE_US, m_one_shot_us);

    // timer interrupt: LEDs off, event to main context
    m_one_shot();
    EXPECT_EQ(1u, hal_leds_off_fake.call_count);
    EXPECT_EQ(1u, sl_zigbee_af_event_set_active_fake.call_count);
}

TEST_F(HalButtons, NewEdgeCancelsThePendingTimer)
{
    press(HAL_BUTTON_T, 100);                   // timer in slot 1
    set_pin(HAL_BUTTON_T, true);
    ASSERT_GE(disable_one_shot_us_timer_isr_fake.call_count, 1u);
    EXPECT_EQ(1, disable_one_shot_us_timer_isr_fake.arg0_val);
}

TEST_F(HalButtons, FiredTimerSlotIsNotCancelledLater)
{
    // regression: after the timer fired, its slot can belong to another module
    press(HAL_BUTTON_T, 100);
    finish_sequence();
    RESET_FAKE(disable_one_shot_us_timer_isr);
    set_pin(HAL_BUTTON_T, true);
    for(unsigned i = 0; i < disable_one_shot_us_timer_isr_fake.call_count; i++)
    {
        EXPECT_EQ(-1, disable_one_shot_us_timer_isr_fake.arg0_history[i]);
    }
}

TEST_F(HalButtons, SequenceIgnoredIfANewOneStartedBeforeTheEventRan)
{
    press(HAL_BUTTON_T, 100);
    m_one_shot();                               // timer fired
    m_one_shot = NULL;
    set_pin(HAL_BUTTON_D, true);                // new press before the main loop ran
    m_done_handler(NULL);
    EXPECT_TRUE(m_sequences.empty());
    EXPECT_EQ(1u, sl_zigbee_core_debug_print_fake.call_count);   // "Buttons still active"
}

TEST_F(HalButtons, NoCallbackRegisteredIsSafe)
{
    hal_buttons_register_sequence_cb(NULL);
    press(HAL_BUTTON_T, 100);
    finish_sequence();
    EXPECT_TRUE(m_sequences.empty());
}
