#include <gtest/gtest.h>
#include <map>
#include <string>
#include <vector>
#include "fff.h"

extern "C" {
#include "nt3_test_stubs.h"
#include "em_i2c.h"
#include "hal_light_sensor.h"
void start_reading_light_sensor(void);
void stop_reading_light_sensor(void);
}

DEFINE_FFF_GLOBALS

extern "C" {
I2C_TypeDef test_i2c0;
FAKE_VALUE_FUNC(I2C_TransferReturn_TypeDef, I2CSPM_Transfer, I2C_TypeDef *, I2C_TransferSeq_TypeDef *);
FAKE_VALUE_FUNC(bool, app_leds_green_off_for_lux);
FAKE_VOID_FUNC(app_leds_green_lux_done);
FAKE_VALUE_FUNC(bool, app_leds_green_is_lit);
FAKE_VOID_FUNC(execute_light_sensor_logic, light_sensor_info_t *);
FAKE_VALUE_FUNC(uint8_t, addTask, test_task_fn_t, taskParm_t, taskParm_t);
FAKE_VALUE_FUNC(uint8_t, removeTask, test_task_fn_t);
FAKE_VALUE_FUNC(uint32_t, get_time_ms);
FAKE_VALUE_FUNC(bool, has_timed_out, uint32_t, uint32_t);
FAKE_VOID_FUNC(sl_zigbee_af_event_init, sl_zigbee_af_event_t *, test_event_handler_t);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_active, sl_zigbee_af_event_t *);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_delay_ms, sl_zigbee_af_event_t *, uint32_t);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_inactive, sl_zigbee_af_event_t *);
FAKE_VALUE_FUNC(bool, sl_zigbee_af_event_is_scheduled, sl_zigbee_af_event_t *);
FAKE_VOID_FUNC_VARARG(sl_zigbee_app_debug_println, const char *, ...);
}

/* --------------------------------------------------------------- time, events, trace */
static uint32_t m_now;
static uint32_t now(void) { return m_now; }

struct Ev { void (*handler)(sl_zigbee_af_event_t *); bool scheduled; uint32_t due; };
static std::map<sl_zigbee_af_event_t *, Ev> m_ev;
static std::vector<sl_zigbee_af_event_t *> m_order;   // [0] sample period, [1] reading
static void ev_init(sl_zigbee_af_event_t *e, void (*h)(sl_zigbee_af_event_t *)) { m_ev[e] = { h, false, 0 }; m_order.push_back(e); }
static void ev_active(sl_zigbee_af_event_t *e) { m_ev[e].scheduled = true; m_ev[e].due = m_now; }
static void ev_delay(sl_zigbee_af_event_t *e, uint32_t ms) { m_ev[e].scheduled = true; m_ev[e].due = m_now + ms; }
static void ev_inactive(sl_zigbee_af_event_t *e) { m_ev[e].scheduled = false; }
static bool ev_scheduled(sl_zigbee_af_event_t *e) { return m_ev[e].scheduled; }

static std::vector<std::string> m_trace;
static bool m_green_free;
static bool m_green_held;
static uint32_t m_green_off_at;
static std::vector<uint32_t> m_green_off_ms;      // how long green was held off, per reading
static uint16_t m_lux_raw;
static test_task_fn_t m_task;
static std::vector<taskParm_t> m_task_parm1;

static bool green_off(void)
{
    if(!m_green_free) { m_trace.push_back("green busy"); return false; }
    m_trace.push_back("green off");
    m_green_held = true;
    m_green_off_at = m_now;
    return true;
}
static void green_done(void)
{
    m_trace.push_back("green restored");
    if(m_green_held) { m_green_off_ms.push_back(m_now - m_green_off_at); }
    m_green_held = false;
}
static I2C_TransferReturn_TypeDef i2c(I2C_TypeDef *, I2C_TransferSeq_TypeDef *seq)
{
    char buf[32];
    if(seq->flags & I2C_FLAG_READ)
    {
        seq->buf[0].data[0] = (uint8_t)(m_lux_raw >> 8);
        seq->buf[0].data[1] = (uint8_t)m_lux_raw;
        m_trace.push_back("read");
    }
    else
    {
        snprintf(buf, sizeof(buf), "cmd 0x%02X", seq->buf[0].data[0]);
        m_trace.push_back(buf);
    }
    return i2cTransferDone;
}
static uint8_t add_task(test_task_fn_t f, taskParm_t, taskParm_t p1) { m_task = f; m_task_parm1.push_back(p1); m_trace.push_back("sample to average"); return 1; }

class HalLightSensor : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RESET_FAKE(I2CSPM_Transfer); RESET_FAKE(app_leds_green_off_for_lux); RESET_FAKE(app_leds_green_lux_done);
        RESET_FAKE(execute_light_sensor_logic); RESET_FAKE(addTask); RESET_FAKE(removeTask); RESET_FAKE(get_time_ms);
        RESET_FAKE(sl_zigbee_af_event_init); RESET_FAKE(sl_zigbee_af_event_set_active);
        RESET_FAKE(sl_zigbee_af_event_set_delay_ms); RESET_FAKE(sl_zigbee_af_event_set_inactive);
        RESET_FAKE(sl_zigbee_af_event_is_scheduled); RESET_FAKE(sl_zigbee_app_debug_println);
        RESET_FAKE(app_leds_green_is_lit);
        FFF_RESET_HISTORY();
        I2CSPM_Transfer_fake.custom_fake = i2c;
        app_leds_green_off_for_lux_fake.custom_fake = green_off;
        app_leds_green_lux_done_fake.custom_fake = green_done;
        addTask_fake.custom_fake = add_task;
        get_time_ms_fake.custom_fake = now;
        has_timed_out_fake.custom_fake = [](uint32_t ts, uint32_t d) { return (m_now - ts) >= d; };
        sl_zigbee_af_event_init_fake.custom_fake = ev_init;
        sl_zigbee_af_event_set_active_fake.custom_fake = ev_active;
        sl_zigbee_af_event_set_delay_ms_fake.custom_fake = ev_delay;
        sl_zigbee_af_event_set_inactive_fake.custom_fake = ev_inactive;
        sl_zigbee_af_event_is_scheduled_fake.custom_fake = ev_scheduled;
        m_now = 1000; m_ev.clear(); m_order.clear(); m_trace.clear();
        m_green_free = true; m_green_held = false; m_green_off_ms.clear();
        m_lux_raw = 0x0123; m_task = NULL; m_task_parm1.clear();

        hal_light_sensor_init();
        ASSERT_EQ(2u, m_order.size());
        EXPECT_EQ((std::vector<std::string>{ "cmd 0x01", "cmd 0x12" }), m_trace);   // power on, high resolution
        m_trace.clear();
    }
    void TearDown() override
    {
        stop_reading_light_sensor();      // the module keeps state between tests
    }

    static sl_zigbee_af_event_t *period_ev(void) { return m_order[0]; }
    static sl_zigbee_af_event_t *read_ev(void) { return m_order[1]; }

    // Run due events in time order until (and including) time 'until'
    static void run_until(uint32_t until)
    {
        for(;;)
        {
            sl_zigbee_af_event_t *next = NULL;
            for(sl_zigbee_af_event_t *e : m_order)
            {
                if(m_ev[e].scheduled && m_ev[e].due <= until && (!next || m_ev[e].due < m_ev[next].due)) { next = e; }
            }
            if(!next) { break; }
            m_now = m_ev[next].due;
            m_ev[next].scheduled = false;
            m_ev[next].handler(next);
        }
        m_now = until;
    }
};

typedef std::vector<std::string> Trace;

TEST_F(HalLightSensor, EveryReadingIsAFreshMeasurementWithGreenOff)
{
    run_until(m_now);                     // first sample period: reading starts
    EXPECT_EQ((Trace{ "green off", "cmd 0x12" }), m_trace);     // restart the measurement in the dark
    m_trace.clear();
    run_until(m_now + 179);
    EXPECT_TRUE(m_trace.empty());          // still measuring
    run_until(m_now + 1);
    EXPECT_EQ((Trace{ "read", "green restored", "sample to average" }), m_trace);
    ASSERT_EQ(1u, m_task_parm1.size());
    EXPECT_EQ(0x0123, m_task_parm1[0].u16);
}

TEST_F(HalLightSensor, GreenIsNeverOffMoreThan200ms)
{
    run_until(m_now + 60000 + 180);        // a reading every period, the last one finished
    ASSERT_EQ(60000 / LIGHT_SENSOR_HAL_SAMPLE_TIME_MS + 1, m_green_off_ms.size());
    for(uint32_t ms : m_green_off_ms)
    {
        EXPECT_LE(ms, 200u);
        EXPECT_GE(ms, 180u);               // the sensor's maximum measurement time
    }
}

TEST_F(HalLightSensor, ReadingsKeepTheSamplePeriod)
{
    uint32_t start = m_now;
    run_until(start + 3 * LIGHT_SENSOR_HAL_SAMPLE_TIME_MS);
    EXPECT_EQ(3u, m_task_parm1.size());    // started at 0, 1, 2 periods; the one at 3 is measuring
    EXPECT_EQ(start + 3 * LIGHT_SENSOR_HAL_SAMPLE_TIME_MS, m_ev[period_ev()].due - LIGHT_SENSOR_HAL_SAMPLE_TIME_MS);
}

TEST_F(HalLightSensor, ReadingWaitsWhileAGreenFlashRuns)
{
    m_green_free = false;
    run_until(m_now + 120);
    EXPECT_EQ((Trace{ "green busy", "green busy", "green busy" }), m_trace);   // retried every 50 ms
    EXPECT_EQ(0u, m_task_parm1.size());
    m_trace.clear();
    m_green_free = true;                   // flash done
    run_until(m_now + 30);
    EXPECT_EQ((Trace{ "green off", "cmd 0x12" }), m_trace);
    run_until(m_now + 180);
    EXPECT_EQ(1u, m_task_parm1.size());
}

TEST_F(HalLightSensor, SamplePeriodDoesNotRestartAReadingInProgress)
{
    run_until(m_now);                      // reading started
    uint32_t read_due = m_ev[read_ev()].due;
    m_ev[period_ev()].due = m_now + 10;    // period tick lands during the measurement
    run_until(m_now + 10);
    EXPECT_EQ(read_due, m_ev[read_ev()].due);
    EXPECT_EQ(1u, app_leds_green_off_for_lux_fake.call_count);
}

TEST_F(HalLightSensor, StoppingDuringAReadingRestoresGreen)
{
    run_until(m_now);                      // green held off
    stop_reading_light_sensor();
    EXPECT_EQ(1u, app_leds_green_lux_done_fake.call_count);
    EXPECT_FALSE(m_ev[read_ev()].scheduled);
    EXPECT_FALSE(m_ev[period_ev()].scheduled);
    stop_reading_light_sensor();           // not restored twice
    EXPECT_EQ(1u, app_leds_green_lux_done_fake.call_count);
}


TEST_F(HalLightSensor, AverageGoesToTheLogicOnceTheBufferIsFull)
{
    static const uint16_t raw[N_LIGHT_SAMPLES] = { 100, 110, 120, 130, 140, 150, 160, 170 };
    for(int i = 0; i < N_LIGHT_SAMPLES; i++)
    {
        m_lux_raw = raw[i];
        run_until(m_now + (i ? LIGHT_SENSOR_HAL_SAMPLE_TIME_MS : 0));
        run_until(m_now + 180);
        ASSERT_NE(nullptr, m_task);
        taskParm_t p0 = {}, p1 = m_task_parm1.back();
        m_task(p0, p1);                    // the task runs outside the event
        EXPECT_EQ(i == N_LIGHT_SAMPLES - 1 ? 1u : 0u, execute_light_sensor_logic_fake.call_count);
    }
    EXPECT_EQ(135, execute_light_sensor_logic_fake.arg0_val->mean_lux);
    EXPECT_EQ(170, hal_light_sensor_read_latest());
}

/* --------------------------------------------------------------- green lit (occupancy) */
TEST_F(HalLightSensor, WhileGreenIsLitItBlinksOffOncePerAverageNotEverySample)
{
    app_leds_green_is_lit_fake.return_val = true;
    uint32_t start = m_now;
    run_until(start + 60000 + 180);
    // a sample every 2.5 s, but green held off only every 20 s
    EXPECT_EQ(60000 / LIGHT_SENSOR_HAL_SAMPLE_TIME_MS + 1, m_task_parm1.size());
    ASSERT_EQ(60000 / LIGHT_SENSOR_UPDATE_AVERAGE_MS + 1, m_green_off_ms.size());
    for(uint32_t ms : m_green_off_ms)
    {
        EXPECT_LE(ms, 200u);
    }
}

TEST_F(HalLightSensor, RepeatedSamplesUseTheLastRealReading)
{
    app_leds_green_is_lit_fake.return_val = true;
    m_lux_raw = 300;
    run_until(m_now + 180);                     // first: a real reading
    m_lux_raw = 999;                            // the sensor would read this now
    run_until(m_now + 3 * LIGHT_SENSOR_HAL_SAMPLE_TIME_MS);
    ASSERT_EQ(4u, m_task_parm1.size());
    for(const taskParm_t &p : m_task_parm1)
    {
        EXPECT_EQ(300, p.u16);
    }
    EXPECT_EQ(1u, app_leds_green_off_for_lux_fake.call_count);
}

TEST_F(HalLightSensor, GreenOffMeansEverySampleIsARealReading)
{
    app_leds_green_is_lit_fake.return_val = true;
    run_until(m_now + 180);
    app_leds_green_is_lit_fake.return_val = false;   // nobody there: nobody sees the hold
    run_until(m_now + 2 * LIGHT_SENSOR_HAL_SAMPLE_TIME_MS + 180);
    EXPECT_EQ(3u, app_leds_green_off_for_lux_fake.call_count);
}

