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

static std::vector<uint8_t> m_sent;
static std::vector<uint8_t> m_ramps;
static int m_refuse;
static bool m_heavy;
static std::vector<uint32_t> m_retries;

static bool send_level(void *, uint8_t level, uint8_t ramp_ds)
{
    if(0 < m_refuse)
    {
        m_refuse--;
        return false;
    }
    m_sent.push_back(level);
    m_ramps.push_back(ramp_ds);
    return true;
}
static bool heavy(void *) { return m_heavy; }
static void schedule_retry(void *, uint32_t delay_ms) { m_retries.push_back(delay_ms); }
static uint32_t random_ms(void *, uint32_t max_ms) { return max_ms / 2UL; }
static int m_claims;
static int m_refuse_claims;
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

static const dh_ops_t OPS = { nullptr, send_level, heavy, schedule_retry, random_ms, send_claim };

class DhMaster : public ::testing::Test
{
protected:
    dh_master_t m;

    void SetUp() override
    {
        m_sent.clear();
        m_ramps.clear();
        m_refuse = 0;
        m_heavy = false;
        m_retries.clear();
        m_claims = 0;
        m_refuse_claims = 0;
        dh_master_init(&m, &OPS);
        average(100, 500);       // The first average after power up is skipped
        EXPECT_TRUE(m_sent.empty());
    }

    void average(uint8_t level, uint16_t lux, bool allowed = true, uint16_t target = 500)
    {
        dh_average_t in = { allowed, level, lux, target, 25, 204 };
        dh_master_on_average(&m, &in);
    }
};

/* --------------------------------------------------------------- the next level */
TEST(DhNextLevel, MovesHalfWayToTheLevelThatReachesTheTarget)
{
    EXPECT_EQ(112, dh_next_level(100, 400, 500, 25, 204));    // desired 125
    EXPECT_EQ(90, dh_next_level(100, 625, 500, 25, 204));     // desired 80
}

TEST(DhNextLevel, MovesAtMostThirtyPercentOfTheLevel)
{
    EXPECT_EQ(130, dh_next_level(100, 100, 500, 25, 204));
    EXPECT_EQ(70, dh_next_level(100, 2000, 300, 25, 204));
}

TEST(DhNextLevel, StaysBetweenFloorAndCeiling)
{
    EXPECT_EQ(204, dh_next_level(200, 100, 800, 25, 204));
    EXPECT_EQ(25, dh_next_level(30, 3000, 300, 25, 204));
    EXPECT_EQ(204, dh_next_level(204, 100, 800, 25, 204));    // no change: nothing to send
    EXPECT_EQ(25, dh_next_level(25, 3000, 300, 25, 204));
}

TEST(DhNextLevel, NoLuxAtAllDoesNotDivideByZero)
{
    EXPECT_EQ(130, dh_next_level(100, 0, 500, 25, 204));
}

TEST(DhNextLevel, ASmallCorrectionStillMoves)
{
    EXPECT_EQ(27, dh_next_level(26, 460, 500, 25, 204));      // desired 28, half is 1
    EXPECT_EQ(26, dh_next_level(27, 551, 500, 25, 204));      // desired 24 (trim), still down
    EXPECT_EQ(100, dh_next_level(100, 498, 500, 25, 204));    // desired is the level itself: stays
}

/* --------------------------------------------------------------- deadband */
TEST_F(DhMaster, WithinFiftyLuxOfTheTargetNothingIsSent)
{
    average(100, 550);
    average(100, 450);
    average(100, 500);
    EXPECT_TRUE(m_sent.empty());
}

TEST_F(DhMaster, MoreThanFiftyLuxFromTheTargetSendsTheNewLevelWithTheRamp)
{
    average(100, 551);
    ASSERT_EQ(1u, m_sent.size());
    EXPECT_LT(m_sent[0], 100);
    EXPECT_EQ(185, m_ramps[0]);
}

/* --------------------------------------------------------------- one update per 60 s */
TEST_F(DhMaster, AfterASendTheNextTwoAveragesAreNotUsed)
{
    average(100, 200);
    ASSERT_EQ(1u, m_sent.size());
    average(130, 200);        // 20 s: during the ramp
    average(130, 200);        // 40 s
    EXPECT_EQ(1u, m_sent.size());
    average(130, 200);        // 60 s
    EXPECT_EQ(2u, m_sent.size());
}

/* --------------------------------------------------------------- send twice */
TEST_F(DhMaster, ALevelIsRepeatedOnceAtTheNextUpdateThenSilence)
{
    average(100, 300);
    ASSERT_EQ(1u, m_sent.size());
    uint8_t level = m_sent[0];

    average(level, 500);
    average(level, 500);
    average(level, 500);      // 60 s, now in the deadband
    ASSERT_EQ(2u, m_sent.size());
    EXPECT_EQ(level, m_sent[1]);

    for(int i = 0; i < 12; i++)
    {
        average(level, 500);
    }
    EXPECT_EQ(2u, m_sent.size());
}

/* --------------------------------------------------------------- periodic refresh */
TEST_F(DhMaster, ThePresentLevelIsSentAgainAboutEveryTenMinutes)
{
    EXPECT_EQ(10u * 60u, (DH_SKIP_AFTER_SEND + DH_REFRESH_AVERAGES) * 20u);     // 20 s averages

    for(unsigned i = 0; i < DH_REFRESH_AVERAGES - 1U; i++)
    {
        average(100, 500);
    }
    EXPECT_TRUE(m_sent.empty());
    average(100, 500);
    ASSERT_EQ(1u, m_sent.size());
    EXPECT_EQ(100, m_sent[0]);

    // 30 x 20 s to the next: the 2 averages after a send + the quiet ones
    for(unsigned i = 0; i < 2U + DH_REFRESH_AVERAGES - 1U; i++)
    {
        average(100, 500);
    }
    EXPECT_EQ(1u, m_sent.size());
    average(100, 500);
    EXPECT_EQ(2u, m_sent.size());
}

TEST_F(DhMaster, ARefreshIsNotRepeated)
{
    for(unsigned i = 0; i < DH_REFRESH_AVERAGES; i++)
    {
        average(100, 500);
    }
    ASSERT_EQ(1u, m_sent.size());
    for(unsigned i = 0; i < 6U; i++)
    {
        average(100, 500);
    }
    EXPECT_EQ(1u, m_sent.size());
}

TEST_F(DhMaster, NoRefreshWhileNotAllowed)
{
    for(unsigned i = 0; i < 3U * DH_REFRESH_AVERAGES; i++)
    {
        average(100, 500, false);
    }
    EXPECT_TRUE(m_sent.empty());
}

TEST_F(DhMaster, ANewLevelRestartsTheRefreshTime)
{
    for(unsigned i = 0; i < DH_REFRESH_AVERAGES - 1U; i++)
    {
        average(100, 500);
    }
    average(100, 200);          // A real change just before the refresh was due
    ASSERT_EQ(1u, m_sent.size());
    uint8_t level = m_sent[0];
    average(level, 500);
    average(level, 500);
    average(level, 500);        // Its repeat
    EXPECT_EQ(2u, m_sent.size());
    average(level, 500);
    EXPECT_EQ(2u, m_sent.size());
}

TEST_F(DhMaster, ARefreshAlsoGoesOutAtTheCeilingOrFloor)
{
    for(unsigned i = 0; i < DH_REFRESH_AVERAGES; i++)
    {
        average(204, 100);
    }
    ASSERT_EQ(1u, m_sent.size());
    EXPECT_EQ(204, m_sent[0]);
}

TEST_F(DhMaster, ARefusedRefreshTriesAgainAtTheNextAverage)
{
    m_refuse = 5;
    for(unsigned i = 0; i < DH_REFRESH_AVERAGES; i++)
    {
        average(100, 500);
    }
    for(int i = 0; i < 5; i++)
    {
        dh_master_retry(&m, true);
    }
    EXPECT_TRUE(m_sent.empty());
    average(100, 500);
    EXPECT_EQ(1u, m_sent.size());
}

TEST_F(DhMaster, ANewLevelTakesThePlaceOfTheRepeat)
{
    average(100, 300);
    uint8_t first = m_sent[0];
    average(first, 300);
    average(first, 300);
    average(first, 300);      // Still far from the target: a new level, not the same one
    ASSERT_EQ(2u, m_sent.size());
    EXPECT_GT(m_sent[1], first);
}

/* --------------------------------------------------------------- limits */
TEST_F(DhMaster, AtTheCeilingNothingIsSentEvenFarFromTheTarget)
{
    for(int i = 0; i < 10; i++)
    {
        average(204, 100);
    }
    EXPECT_TRUE(m_sent.empty());
}

TEST_F(DhMaster, AtTheFloorNothingIsSentEvenFarFromTheTarget)
{
    for(int i = 0; i < 10; i++)
    {
        average(25, 3000);
    }
    EXPECT_TRUE(m_sent.empty());
}

/* --------------------------------------------------------------- not allowed */
TEST_F(DhMaster, NothingIsSentWhileNotAllowedAndOneAverageIsSkippedAfter)
{
    average(100, 200, false);
    EXPECT_TRUE(m_sent.empty());
    average(100, 200);        // May still include whatever stopped DH
    EXPECT_TRUE(m_sent.empty());
    average(100, 200);
    EXPECT_EQ(1u, m_sent.size());
}

TEST_F(DhMaster, NotAllowedCancelsTheRepeat)
{
    average(100, 300);
    uint8_t level = m_sent[0];
    average(level, 500);
    average(level, 500, false);
    for(int i = 0; i < 6; i++)
    {
        average(level, 500);
    }
    EXPECT_EQ(1u, m_sent.size());
}

TEST_F(DhMaster, NotAllowedDoesNotShortenTheWaitAfterASend)
{
    average(100, 200);
    average(130, 200, false);
    average(130, 200);
    EXPECT_EQ(1u, m_sent.size());
}

/* --------------------------------------------------------------- refused sends */
TEST_F(DhMaster, ARefusedSendIsTriedAgainAboutOnceASecond)
{
    m_refuse = 2;
    average(100, 200);
    EXPECT_TRUE(m_sent.empty());
    ASSERT_EQ(1u, m_retries.size());
    EXPECT_EQ(1000u, m_retries[0]);

    dh_master_retry(&m, true);
    EXPECT_TRUE(m_sent.empty());
    dh_master_retry(&m, true);
    EXPECT_EQ(1u, m_sent.size());
    EXPECT_EQ(2u, m_retries.size());
}

TEST_F(DhMaster, AfterFiveAttemptsItGivesUpAndWorksItOutAgainAtTheNextAverage)
{
    m_refuse = 5;
    average(100, 200);
    for(int i = 0; i < 6; i++)
    {
        dh_master_retry(&m, true);
    }
    EXPECT_TRUE(m_sent.empty());
    EXPECT_EQ(4u, m_retries.size());

    average(100, 200);        // Nothing went out, so no 60 s wait
    EXPECT_EQ(1u, m_sent.size());
}

TEST_F(DhMaster, HeavyTrafficHoldsTheFirstThreeAttempts)
{
    m_heavy = true;
    average(100, 200);
    dh_master_retry(&m, true);
    dh_master_retry(&m, true);
    EXPECT_TRUE(m_sent.empty());
    dh_master_retry(&m, true);      // Sent anyway, the stack refuses it if it has no room
    EXPECT_EQ(1u, m_sent.size());
}

TEST_F(DhMaster, ARetryIsDroppedWhenDhIsNoLongerAllowed)
{
    m_refuse = 1;
    average(100, 200);
    dh_master_retry(&m, false);
    dh_master_retry(&m, true);
    EXPECT_TRUE(m_sent.empty());
}

TEST_F(DhMaster, AnAverageDuringRetriesDoesNotStartAnotherSend)
{
    m_refuse = 1;
    average(100, 200);
    size_t retries = m_retries.size();
    average(100, 300);
    EXPECT_EQ(retries, m_retries.size());
    dh_master_retry(&m, true);
    EXPECT_EQ(1u, m_sent.size());
}

/* --------------------------------------------------------------- the claim */
TEST_F(DhMaster, AClaimGoesOutAtOnce)
{
    dh_master_claim(&m);
    EXPECT_EQ(1, m_claims);
    EXPECT_TRUE(m_retries.empty());
}

TEST_F(DhMaster, ARefusedClaimIsRetriedEvenWhileDhIsNotAllowed)
{
    m_refuse_claims = 2;
    dh_master_claim(&m);
    EXPECT_EQ(0, m_claims);
    average(100, 500, false);       // The selection flashes the light
    dh_master_retry(&m, false);
    dh_master_retry(&m, false);
    EXPECT_EQ(1, m_claims);
}

TEST_F(DhMaster, AClaimGivesUpAfterFiveAttempts)
{
    m_refuse_claims = 5;
    dh_master_claim(&m);
    for(int i = 0; i < 6; i++)
    {
        dh_master_retry(&m, true);
    }
    EXPECT_EQ(0, m_claims);
    EXPECT_EQ(4u, m_retries.size());
}

TEST_F(DhMaster, HeavyTrafficHoldsTheClaimToo)
{
    m_heavy = true;
    dh_master_claim(&m);
    dh_master_retry(&m, true);
    dh_master_retry(&m, true);
    EXPECT_EQ(0, m_claims);
    dh_master_retry(&m, true);
    EXPECT_EQ(1, m_claims);
}

TEST_F(DhMaster, AClaimAndALevelRetryDoNotDisturbEachOther)
{
    m_refuse = 1;
    m_refuse_claims = 1;
    average(100, 200);
    dh_master_claim(&m);
    dh_master_retry(&m, true);
    EXPECT_EQ(1, m_claims);
    EXPECT_EQ(1u, m_sent.size());
}

TEST_F(DhMaster, StoppingDropsWhateverWasGoingOut)
{
    m_refuse = 1;
    m_refuse_claims = 1;
    average(100, 200);
    dh_master_claim(&m);
    dh_master_stop(&m);
    dh_master_retry(&m, true);
    EXPECT_EQ(0, m_claims);
    EXPECT_TRUE(m_sent.empty());
}

TEST_F(DhMaster, StoppingDropsTheRepeat)
{
    average(100, 300);
    uint8_t level = m_sent[0];
    average(level, 500);
    dh_master_stop(&m);
    for(int i = 0; i < 6; i++)
    {
        average(level, 500);
    }
    EXPECT_EQ(1u, m_sent.size());
}

/* --------------------------------------------------------------- closed loop */
struct Room { double lux_per_level; double curve; double daylight; uint16_t target; };

static double room_lux(const Room &r, uint8_t level)
{
    return r.daylight + r.lux_per_level * 254.0 * std::pow(level / 254.0, r.curve);
}

class DhLoop : public ::testing::TestWithParam<Room> {};

// No calibration: the same code settles in rooms it knows nothing about
TEST_P(DhLoop, SettlesInTheDeadbandThenGoesQuiet)
{
    const Room r = GetParam();
    dh_master_t m;
    uint8_t level = 204;
    int reversals = 0;
    int last_direction = 0;

    m_sent.clear();
    m_ramps.clear();
    m_refuse = 0;
    m_heavy = false;
    dh_master_init(&m, &OPS);

    for(int i = 0; i < 180; i++)     // One hour of 20 s averages
    {
        dh_average_t in = { true, level, (uint16_t)room_lux(r, level), r.target, 25, 204 };
        size_t before = m_sent.size();

        dh_master_on_average(&m, &in);
        if(m_sent.size() != before)
        {
            int direction = (m_sent.back() > level) ? 1 : ((m_sent.back() < level) ? -1 : 0);
            if((0 != direction) && (0 != last_direction) && (direction != last_direction))
            {
                reversals++;
            }
            if(0 != direction)
            {
                last_direction = direction;
            }
            level = m_sent.back();      // The master moves with its own message
        }
    }

    double lux = room_lux(r, level);
    bool at_limit = ((lux > r.target) && (25 == level)) || ((lux < r.target) && (204 == level));

    EXPECT_TRUE(at_limit || (std::fabs(lux - r.target) <= DH_LUX_DEADBAND + 1)) << "lux " << lux << " level " << (int)level;
    EXPECT_LE(reversals, 1);                 // No hunting
    EXPECT_LE(m_sent.size(), 22u);           // Nothing like one every update (6 of these are refreshes)

    size_t settled = m_sent.size();
    for(int i = 0; i < 90; i++)              // 30 minutes
    {
        dh_average_t in = { true, level, (uint16_t)room_lux(r, level), r.target, 25, 204 };
        dh_master_on_average(&m, &in);
    }
    EXPECT_LE(m_sent.size() - settled, 3u);  // Steady daylight: only the refreshes
    for(size_t i = settled; i < m_sent.size(); i++)
    {
        EXPECT_EQ(level, m_sent[i]);
    }
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
