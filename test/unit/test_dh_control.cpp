#include <gtest/gtest.h>
#include <cmath>
#include <vector>

/*
 * dh_control.c: the daylight harvesting master's logic, including a simulated
 * room for the control loop (lux = daylight + light from the group).
 */

extern "C" {
#include "../../Light/nwk_sensors/dh_control.h"
}

struct Sent { uint8_t current; uint8_t target; uint8_t duration; };
static std::vector<Sent> m_sent;
static int m_refuse;
static bool m_heavy;
static std::vector<uint32_t> m_retries;
static int m_claims;
static int m_refuse_claims;
static uint32_t m_now;

static bool send_level(void *, uint8_t current, uint8_t target, uint8_t duration)
{
    if(0 < m_refuse)
    {
        m_refuse--;
        return false;
    }
    m_sent.push_back({ current, target, duration });
    return true;
}
static bool send_claim(void *)
{
    if(0 < m_refuse_claims)
    {
        m_refuse_claims--;
        return false;
    }
    m_claims++;
    return true;
}
static bool heavy(void *) { return m_heavy; }
static void schedule_retry(void *, uint32_t delay_ms) { m_retries.push_back(delay_ms); }
static uint32_t random_ms(void *, uint32_t max_ms) { return max_ms / 2UL; }

static const dh_ops_t OPS = { nullptr, send_level, send_claim, heavy, schedule_retry, random_ms };

// Raw reading for this many lux, with the factor applied
static uint16_t raw(uint32_t lux) { return (uint16_t)std::lround((double)lux / CALIBRATED_LUX_FACTOR); }

class DhMaster : public ::testing::Test
{
protected:
    dh_master_t m;

    void SetUp() override
    {
        m_sent.clear();
        m_refuse = 0;
        m_heavy = false;
        m_retries.clear();
        m_claims = 0;
        m_refuse_claims = 0;
        m_now = 1000;
        dh_master_init(&m, &OPS);
    }

    // One reading, 2.5 s after the last
    void reading(uint8_t level, uint32_t lux, bool allowed = true, uint16_t target = 500)
    {
        m_now += 2500UL;
        dh_reading_t in = { allowed, m_now, raw(lux), level, target, 25, 204 };
        dh_master_on_reading(&m, &in);
    }

    void pair(uint8_t level, uint32_t lux, bool allowed = true, uint16_t target = 500)
    {
        reading(level, lux, allowed, target);
        reading(level, lux, allowed, target);
    }
};

/* --------------------------------------------------------------- the algorithm */
TEST(DhNextLevel, TheFactorMapsTheSensorReadingToLux)
{
    EXPECT_EQ(350u, dh_calibrated_lux(100));
    // 100 raw = 350 lux, target 500: desired 142, step 21
    EXPECT_EQ(121, dh_next_level(100, 500, 100, 25, 204));
}

TEST(DhNextLevel, StepIsHalfTheWayToTheDesiredLevelInBothDirections)
{
    EXPECT_EQ(112, dh_next_level(100, 500, raw(400), 25, 204));    // desired 125
    EXPECT_EQ(90, dh_next_level(100, 500, raw(625), 25, 204));     // desired 80
}

TEST(DhNextLevel, StepIsLimitedToThirtyPercentOfTheCurrentLevel)
{
    EXPECT_EQ(130, dh_next_level(100, 500, raw(100), 25, 204));
    EXPECT_EQ(70, dh_next_level(100, 300, raw(2000), 25, 204));
}

TEST(DhNextLevel, FivePercentOrLessIsNoChange)
{
    // desired 110, step 5: exactly 5 %
    EXPECT_EQ(100, dh_next_level(100, 500, raw(454), 25, 204));
    // desired 90, step -5
    EXPECT_EQ(100, dh_next_level(100, 500, raw(555), 25, 204));
    // desired 112, step 6: just over
    EXPECT_EQ(106, dh_next_level(100, 500, raw(445), 25, 204));
}

TEST(DhNextLevel, NeverAboveTheMaxTrimOrBelowTheFloor)
{
    EXPECT_EQ(204, dh_next_level(180, 800, raw(100), 25, 204));
    EXPECT_EQ(25, dh_next_level(32, 300, raw(3000), 25, 204));
    EXPECT_EQ(204, dh_next_level(204, 800, raw(100), 25, 204));
    EXPECT_EQ(25, dh_next_level(25, 300, raw(3000), 25, 204));
}

TEST(DhNextLevel, AChangeCutToFivePercentOrLessByTheLimitIsNoChange)
{
    EXPECT_EQ(200, dh_next_level(200, 800, raw(100), 25, 204));    // +30 % asked, only +2 % left
}

TEST(DhNextLevel, NoLuxAtAllDoesNotDivideByZero)
{
    EXPECT_EQ(130, dh_next_level(100, 500, 0, 25, 204));
}

/* --------------------------------------------------------------- sampling */
TEST_F(DhMaster, TwoReadingsAreAveragedThenTheAlgorithmRuns)
{
    reading(100, 200);
    EXPECT_TRUE(m_sent.empty());
    reading(100, 600);           // Average 400: desired 125, step 12
    ASSERT_EQ(1u, m_sent.size());
    EXPECT_EQ(112, m_sent[0].target);
}

TEST_F(DhMaster, EachAverageUsesTwoNewReadings)
{
    pair(100, 500);              // In range
    reading(100, 100);           // Only this one in the next average
    EXPECT_TRUE(m_sent.empty());
    reading(100, 100);
    EXPECT_EQ(1u, m_sent.size());
}

TEST_F(DhMaster, TheCommandCarriesTheCurrentLevelTargetLevelAndDuration)
{
    pair(100, 100);
    ASSERT_EQ(1u, m_sent.size());
    EXPECT_EQ(100, m_sent[0].current);
    EXPECT_EQ(130, m_sent[0].target);
    EXPECT_EQ(20, m_sent[0].duration);
}

TEST_F(DhMaster, NothingIsSentWhenTheChangeIsFivePercentOrLess)
{
    for(int i = 0; i < 200; i++)
    {
        pair(100, 500);
    }
    EXPECT_TRUE(m_sent.empty());
}

TEST_F(DhMaster, NoRepeatOrRefreshInSteadyLight)
{
    pair(100, 100);
    ASSERT_EQ(1u, m_sent.size());
    uint8_t level = m_sent[0].target;
    for(int i = 0; i < 1000; i++)     // Over 40 minutes
    {
        reading(level, 500);
    }
    EXPECT_EQ(1u, m_sent.size());
}

/* --------------------------------------------------------------- the ramp timer */
TEST_F(DhMaster, ReadingsDuringTheRampAreDiscardedAndTheNextTwoAreTheAverage)
{
    pair(100, 100);              // Sent at t
    ASSERT_EQ(1u, m_sent.size());
    uint32_t sent_at = m_now;

    while(m_now + 2500UL < sent_at + 20000UL)
    {
        reading(130, 100);       // Discarded
    }
    EXPECT_EQ(1u, m_sent.size());

    reading(130, 100);           // Timer over: first of the next two
    EXPECT_EQ(1u, m_sent.size());
    reading(130, 100);
    EXPECT_EQ(2u, m_sent.size());
    EXPECT_GE(m_now - sent_at, 20000u);
}

TEST_F(DhMaster, AReadingBeforeTheRampEndIsNotAveragedWithOneAfter)
{
    reading(100, 500);           // First of a pair
    reading(100, 100);           // Average 300: sends
    ASSERT_EQ(1u, m_sent.size());
    uint32_t sent_at = m_now;
    while(m_now + 2500UL < sent_at + 20000UL)
    {
        reading(130, 5000);      // Discarded
    }
    reading(130, 100);           // Only one reading since the ramp ended
    EXPECT_EQ(1u, m_sent.size());
}

TEST_F(DhMaster, WithoutTheLoopbackTheMasterHasNotMovedAndSendsAgainAfterTheTimer)
{
    pair(100, 100);
    uint32_t sent_at = m_now;
    while(m_now < sent_at + 20000UL)
    {
        reading(100, 100);       // Its own level never changed
    }
    while(m_sent.size() < 2u)
    {
        reading(100, 100);
    }
    EXPECT_EQ(m_sent[0].target, m_sent[1].target);
    EXPECT_LE(m_now - sent_at, 27500u);
}

TEST_F(DhMaster, ACommandGoesOutNoMoreThanOncePerRampTime)
{
    for(int i = 0; i < 240; i++)      // 10 minutes of readings in a dark room
    {
        reading(100, 100);
    }
    EXPECT_LE(m_sent.size(), 30u);
    EXPECT_GE(m_sent.size(), 20u);
}

/* --------------------------------------------------------------- limits */
TEST_F(DhMaster, AtTheCeilingNothingIsSentEvenFarFromTheTarget)
{
    for(int i = 0; i < 40; i++)
    {
        reading(204, 100);
    }
    EXPECT_TRUE(m_sent.empty());
}

TEST_F(DhMaster, AtTheFloorNothingIsSentEvenFarFromTheTarget)
{
    for(int i = 0; i < 40; i++)
    {
        reading(25, 3000);
    }
    EXPECT_TRUE(m_sent.empty());
}

/* --------------------------------------------------------------- not allowed */
TEST_F(DhMaster, ReadingsWhileNotAllowedAreDiscarded)
{
    for(int i = 0; i < 20; i++)
    {
        reading(100, 100, false);
    }
    EXPECT_TRUE(m_sent.empty());
}

TEST_F(DhMaster, ANotAllowedReadingAlsoDiscardsTheOneBeforeIt)
{
    reading(100, 100);
    reading(100, 100, false);
    reading(100, 100);
    EXPECT_TRUE(m_sent.empty());     // Only one good reading since
    reading(100, 100);
    EXPECT_EQ(1u, m_sent.size());
}

/* --------------------------------------------------------------- refused */
TEST_F(DhMaster, ARefusedLevelIsNotRetriedTheNextAverageSendsIt)
{
    m_refuse = 1;
    pair(100, 100);
    EXPECT_TRUE(m_sent.empty());
    EXPECT_TRUE(m_retries.empty());
    pair(100, 100);                  // 5 s later, no ramp timer started
    EXPECT_EQ(1u, m_sent.size());
}

TEST_F(DhMaster, HeavyTrafficDoesNotHoldALevel)
{
    m_heavy = true;
    pair(100, 100);
    EXPECT_EQ(1u, m_sent.size());
}

/* --------------------------------------------------------------- the claim */
TEST_F(DhMaster, AClaimGoesOutAtOnce)
{
    dh_master_claim(&m);
    EXPECT_EQ(1, m_claims);
    EXPECT_TRUE(m_retries.empty());
}

TEST_F(DhMaster, ARefusedClaimIsRetried)
{
    m_refuse_claims = 2;
    dh_master_claim(&m);
    EXPECT_EQ(0, m_claims);
    ASSERT_EQ(1u, m_retries.size());
    EXPECT_EQ(1000u, m_retries[0]);
    dh_master_retry(&m);
    dh_master_retry(&m);
    EXPECT_EQ(1, m_claims);
}

TEST_F(DhMaster, AClaimGivesUpAfterFiveAttempts)
{
    m_refuse_claims = 5;
    dh_master_claim(&m);
    for(int i = 0; i < 6; i++)
    {
        dh_master_retry(&m);
    }
    EXPECT_EQ(0, m_claims);
    EXPECT_EQ(4u, m_retries.size());
}

TEST_F(DhMaster, HeavyTrafficHoldsTheClaim)
{
    m_heavy = true;
    dh_master_claim(&m);
    dh_master_retry(&m);
    dh_master_retry(&m);
    EXPECT_EQ(0, m_claims);
    dh_master_retry(&m);
    EXPECT_EQ(1, m_claims);
}

TEST_F(DhMaster, StoppingDropsAPendingClaimAndHalfAnAverage)
{
    m_refuse_claims = 1;
    dh_master_claim(&m);
    reading(100, 100);
    dh_master_stop(&m);
    dh_master_retry(&m);
    EXPECT_EQ(0, m_claims);
    reading(100, 100);
    EXPECT_TRUE(m_sent.empty());
}

/* --------------------------------------------------------------- closed loop */
struct Room { double lux_per_level; double curve; double daylight; uint16_t target; };

static double room_lux(const Room &r, uint8_t level)
{
    return r.daylight + r.lux_per_level * 254.0 * std::pow(level / 254.0, r.curve);
}

class DhLoop : public ::testing::TestWithParam<Room> {};

// No calibration: the same code settles in rooms it knows nothing about
TEST_P(DhLoop, SettlesWhereFivePercentIsNotWorthSendingThenGoesQuiet)
{
    const Room r = GetParam();
    dh_master_t m;
    uint8_t level = 204;
    int reversals = 0;
    int last_direction = 0;
    uint32_t now = 1000;

    m_sent.clear();
    m_refuse = 0;
    m_heavy = false;
    dh_master_init(&m, &OPS);

    for(int i = 0; i < 1440; i++)     // One hour of 2.5 s readings
    {
        now += 2500UL;
        dh_reading_t in = { true, now, raw((uint32_t)room_lux(r, level)), level, r.target, 25, 204 };
        size_t before = m_sent.size();

        dh_master_on_reading(&m, &in);
        if(m_sent.size() != before)
        {
            int direction = (m_sent.back().target > level) ? 1 : -1;
            if((0 != last_direction) && (direction != last_direction))
            {
                reversals++;
            }
            last_direction = direction;
            level = m_sent.back().target;      // The master moves with its own message
        }
    }

    EXPECT_EQ(level, dh_next_level(level, r.target, raw((uint32_t)room_lux(r, level)), 25, 204)) << (int)level;
    EXPECT_LE(reversals, 1);                 // No hunting
    EXPECT_LE(m_sent.size(), 16u);           // Nothing like one every update

    size_t settled = m_sent.size();
    for(int i = 0; i < 720; i++)             // 30 more minutes
    {
        now += 2500UL;
        dh_reading_t in = { true, now, raw((uint32_t)room_lux(r, level)), level, r.target, 25, 204 };
        dh_master_on_reading(&m, &in);
    }
    EXPECT_EQ(settled, m_sent.size());       // Steady daylight: silence
}

INSTANTIATE_TEST_SUITE_P(Rooms, DhLoop, ::testing::Values(
    Room{ 1.0, 1.0,    0.0, 300 },   // Only the lights
    Room{ 3.0, 1.0,    0.0, 300 },
    Room{ 8.0, 1.0,    0.0, 800 },
    Room{ 8.0, 1.0,    0.0, 300 },
    Room{ 3.0, 1.0,  200.0, 500 },   // Some daylight
    Room{ 5.0, 1.0,  400.0, 800 },
    Room{ 3.0, 1.5,  200.0, 500 },   // Light output not linear with the level
    Room{ 3.0, 2.0,    0.0, 300 },
    Room{ 8.0, 2.0,  100.0, 800 },
    Room{ 3.0, 1.0, 3000.0, 300 },   // Bright day: floor
    Room{ 0.5, 1.0,    0.0, 800 }    // Lights can't reach the target: ceiling
));
