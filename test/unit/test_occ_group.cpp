#include <gtest/gtest.h>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <map>
#include <random>
#include <string>
#include <vector>
#include <cstdlib>

extern "C" {
#include "occ_group.h"
}

/*
 * Part 1: one light, deterministic (random backoff = a value the test sets).
 * Part 2: a simulated group: every light runs its own occ_group_t on its own
 *         75 ms sensor clock; multicasts reach the others after a random
 *         latency and can be lost.
 */

static const uint32_t MIN_MS = 60UL * 1000UL;
static const uint8_t WAKE_LEVEL = 204, MID_LEVEL = 51, PRESENT_LEVEL = 180;

struct Msg
{
    bool keepalive;                  // else level change
    uint16_t time_s;
    occ_group_state_t state;
    occ_keepalive_reason_t reason;
    uint8_t level;
    int from;
    uint32_t at;
};

struct Light
{
    int id = 0;
    occ_group_t g = {};
    occ_group_ops_t ops = {};
    uint32_t dim_ms = 30 * MIN_MS, off_ms = 5 * MIN_MS;
    std::function<uint32_t(uint32_t)> random;          // max -> value
    std::function<void(const Msg &)> sent;              // network
    std::vector<std::pair<uint32_t, occ_group_state_t>> moves;   // go_to_state calls
    uint32_t *clock = nullptr;
    std::function<bool()> stack_accepts = [] { return true; };   // false: the stack refuses the send
    std::function<bool()> traffic_heavy = [] { return false; };  // broadcast traffic
    std::vector<uint32_t> refused;                               // times of refused sends
    bool loopback = true;                                        // the stack hands our own multicast back
    std::vector<Msg> looped;                                     // ... waiting to be delivered
};

static uint32_t l_dim(void *c) { return static_cast<Light *>(c)->dim_ms; }
static uint32_t l_off(void *c) { return static_cast<Light *>(c)->off_ms; }
static void l_go(void *c, occ_group_state_t s) { Light *l = static_cast<Light *>(c); l->moves.push_back({ *l->clock, s }); }
static uint8_t l_level_for(void *, occ_group_state_t s) { return OCC_GROUP_ON == s ? WAKE_LEVEL : (OCC_GROUP_MID == s ? MID_LEVEL : 0); }
static uint8_t l_current(void *) { return PRESENT_LEVEL; }
static bool l_send(Light *l, const Msg &m)
{
    if(!l->stack_accepts()) { l->refused.push_back(*l->clock); return false; }
    l->sent(m);
    if(l->loopback) { l->looped.push_back(m); }
    return true;
}
static bool l_keepalive(void *c, uint16_t t, occ_group_state_t s, occ_keepalive_reason_t r, uint8_t lvl)
{
    Light *l = static_cast<Light *>(c);
    return l_send(l, { true, t, s, r, lvl, l->id, *l->clock });
}
static bool l_level_change(void *c, occ_group_state_t s, uint8_t lvl)
{
    Light *l = static_cast<Light *>(c);
    return l_send(l, { false, 0, s, OCC_KEEPALIVE_WAKE, lvl, l->id, *l->clock });
}
static uint32_t l_random(void *c, uint32_t max) { return static_cast<Light *>(c)->random(max); }
static bool l_heavy(void *c) { return static_cast<Light *>(c)->traffic_heavy(); }

// Our own multicast, back from the stack
static void deliver_looped(Light &l, uint32_t now)
{
    std::vector<Msg> msgs;
    msgs.swap(l.looped);
    for(const Msg &m : msgs)
    {
        if(m.keepalive) { occ_group_rx_own_keepalive(&l.g, now, m.time_s, m.state, m.reason); }
    }
}

static void setup_light(Light &l, int id, uint32_t *clock)
{
    l.id = id;
    l.clock = clock;
    l.ops = { &l, l_dim, l_off, l_go, l_level_for, l_current, l_keepalive, l_level_change, l_random, l_heavy };
    occ_group_init(&l.g, &l.ops, *clock);
}

/* =============================================================== part 1: one light */
class OccGroup : public ::testing::Test
{
protected:
    uint32_t now = 1000;
    Light l;
    std::vector<Msg> out;
    uint32_t backoff_fraction = 0;     // random = max * fraction / 100

    void SetUp() override
    {
        l.random = [this](uint32_t max) { return (uint32_t)((uint64_t)max * backoff_fraction / 100); };
        l.sent = [this](const Msg &m) { out.push_back(m); };
        setup_light(l, 1, &now);
    }
    // run the sensor tick every 75 ms until 'until'
    void run_to(uint32_t until)
    {
        while((int32_t)(until - now) > 0)
        {
            now += std::min<uint32_t>(75, until - now);
            deliver_looped(l, now);                    // loopback arrives by the next sample
            occ_group_tick(&l.g, now);
        }
    }
    void run_for(uint32_t ms) { run_to(now + ms); }
};

TEST_F(OccGroup, StartsOnWithAFullTimerAndSendsNothing)
{
    EXPECT_EQ(OCC_GROUP_ON, occ_group_state(&l.g));
    EXPECT_EQ(30 * MIN_MS, occ_group_remaining_ms(&l.g, now));
    run_for(10 * MIN_MS);
    EXPECT_TRUE(out.empty());
    EXPECT_TRUE(l.moves.empty());
}

TEST_F(OccGroup, OccupancyWhileOnRestartsTheTimerOnly)
{
    run_for(10 * MIN_MS);
    occ_group_on_occupancy(&l.g, now);
    EXPECT_EQ(30 * MIN_MS, occ_group_remaining_ms(&l.g, now));
    run_for(MIN_MS);
    EXPECT_TRUE(out.empty());
    EXPECT_TRUE(l.moves.empty());
}

TEST_F(OccGroup, VacantLightRefreshesThenDimsThenTurnsOff)
{
    backoff_fraction = 50;
    uint32_t start = now;
    run_to(start + 30 * MIN_MS - 60000 - 1);
    EXPECT_TRUE(out.empty());

    // refresh window = 60 s; sent 0-40 s into it (here 20 s), with the time left
    run_to(start + 30 * MIN_MS - 39000);
    ASSERT_EQ(1u, out.size());
    EXPECT_TRUE(out[0].keepalive);
    EXPECT_EQ(OCC_KEEPALIVE_REFRESH, out[0].reason);
    EXPECT_EQ(OCC_GROUP_ON, out[0].state);
    EXPECT_NEAR(40, out[0].time_s, 1);
    EXPECT_EQ(PRESENT_LEVEL, out[0].level);       // current level

    // expiry + 0-5 s backoff (2.5 s): dim to mid and tell the group
    run_to(start + 30 * MIN_MS + 2400);
    EXPECT_EQ(1u, out.size());
    run_to(start + 30 * MIN_MS + 2600);
    ASSERT_EQ(2u, out.size());
    EXPECT_FALSE(out[1].keepalive);
    EXPECT_EQ(OCC_GROUP_MID, out[1].state);
    EXPECT_EQ(MID_LEVEL, out[1].level);
    ASSERT_EQ(1u, l.moves.size());
    EXPECT_EQ(OCC_GROUP_MID, l.moves[0].second);

    // no keepalive before off; off after 5 min + backoff
    uint32_t mid_at = l.moves[0].first;
    run_to(mid_at + 5 * MIN_MS + 2600);
    ASSERT_EQ(3u, out.size());
    EXPECT_EQ(OCC_GROUP_OFF, out[2].state);
    EXPECT_EQ(OCC_GROUP_OFF, occ_group_state(&l.g));

    // off: no timer, nothing more
    run_for(5 * 60 * MIN_MS);
    EXPECT_EQ(3u, out.size());
}

TEST_F(OccGroup, ReceivedRefreshCancelsOursAndSyncsTheTimer)
{
    backoff_fraction = 90;
    run_for(30 * MIN_MS - 50000);                      // in the window, our refresh pending
    occ_group_rx_keepalive(&l.g, now, 45, OCC_GROUP_ON, OCC_KEEPALIVE_REFRESH);
    EXPECT_EQ(45000u, occ_group_remaining_ms(&l.g, now));
    run_for(44000);
    EXPECT_TRUE(out.empty());                          // ours was cancelled, no second refresh
    EXPECT_TRUE(l.moves.empty());                      // level untouched while ON
}

TEST_F(OccGroup, LongerTimerRepliesWithItsOwnTime)
{
    run_for(10 * MIN_MS);                              // 20 min left
    backoff_fraction = 100;
    occ_group_rx_keepalive(&l.g, now, 40, OCC_GROUP_ON, OCC_KEEPALIVE_REFRESH);
    EXPECT_EQ(20 * MIN_MS, occ_group_remaining_ms(&l.g, now));   // keeps its own
    run_for(5000);                                     // reply backoff max 5 s
    ASSERT_EQ(1u, out.size());
    EXPECT_EQ(OCC_KEEPALIVE_REFRESH, out[0].reason);
    EXPECT_NEAR(20 * 60 - 5, out[0].time_s, 1);
}

TEST_F(OccGroup, WithinTheMarginTheShorterTimeIsAdoptedWithoutReply)
{
    backoff_fraction = 100;                            // our refresh would go 40 s into the window
    run_for(30 * MIN_MS - 30000);                      // 30 s left
    occ_group_rx_keepalive(&l.g, now, 16, OCC_GROUP_ON, OCC_KEEPALIVE_REFRESH);   // 14 s shorter: < 15 s margin
    EXPECT_EQ(16000u, occ_group_remaining_ms(&l.g, now));
    run_for(15000);
    EXPECT_TRUE(out.empty());
}

TEST_F(OccGroup, AnotherReplyThatIsLongEnoughCancelsOurs)
{
    run_for(10 * MIN_MS);
    backoff_fraction = 100;
    occ_group_rx_keepalive(&l.g, now, 40, OCC_GROUP_ON, OCC_KEEPALIVE_REFRESH);    // we will reply
    occ_group_rx_keepalive(&l.g, now + 100, 20 * 60 - 5, OCC_GROUP_ON, OCC_KEEPALIVE_REFRESH);
    run_for(6000);
    EXPECT_TRUE(out.empty());
}

TEST_F(OccGroup, OccupancyAtMidWakesTheGroupAndThisLightComesOnWithIt)
{
    run_for(30 * MIN_MS + 5100);                       // MID now
    ASSERT_EQ(OCC_GROUP_MID, occ_group_state(&l.g));
    out.clear(); l.moves.clear();
    backoff_fraction = 100;
    occ_group_on_occupancy(&l.g, now);
    EXPECT_TRUE(l.moves.empty());                      // not on its own
    EXPECT_TRUE(out.empty());
    uint32_t seen = now;
    while(out.empty() && (now - seen) <= OCC_GROUP_WAKE_BACKOFF_MS + 75)
    {
        l.loopback = false;                            // hold the loopback back for a moment ...
        run_for(75);
    }
    ASSERT_EQ(1u, out.size());                         // the group: within the wake backoff
    EXPECT_EQ(OCC_KEEPALIVE_WAKE, out[0].reason);
    EXPECT_EQ(WAKE_LEVEL, out[0].level);
    EXPECT_EQ(30 * 60, out[0].time_s);                 // a full ON timer
    EXPECT_TRUE(l.moves.empty());                      // still waiting for its own wake
    occ_group_rx_own_keepalive(&l.g, now, out[0].time_s, out[0].state, out[0].reason);   // ... it comes back
    ASSERT_EQ(1u, l.moves.size());
    EXPECT_EQ(OCC_GROUP_ON, l.moves[0].second);
    EXPECT_EQ(OCC_GROUP_ON, occ_group_state(&l.g));
    EXPECT_NEAR(30 * MIN_MS, occ_group_remaining_ms(&l.g, now), 100);
    run_for(MIN_MS);
    EXPECT_EQ(1u, out.size());                         // one wake, no more
    EXPECT_EQ(1u, l.moves.size());
}

TEST_F(OccGroup, MoreOccupancyWhileWakingDoesNotRestartTheWake)
{
    run_for(30 * MIN_MS + 5100);
    out.clear(); l.moves.clear();
    backoff_fraction = 100;
    occ_group_on_occupancy(&l.g, now);
    for(int i = 0; i < 3; i++) { run_for(75); occ_group_on_occupancy(&l.g, now); }
    run_for(150);                                      // 300 ms after the first sample
    EXPECT_EQ(1u, out.size());
}

TEST_F(OccGroup, WakeSentButNotHeardBackStillTurnsTheLightOn)
{
    run_for(30 * MIN_MS + 5100);
    out.clear(); l.moves.clear();
    l.loopback = false;
    occ_group_on_occupancy(&l.g, now);
    run_for(75);
    ASSERT_EQ(1u, out.size());                         // the group has it
    uint32_t sent_at = out[0].at;
    run_to(sent_at + OCC_GROUP_OWN_WAKE_WAIT_MS - 75);
    EXPECT_TRUE(l.moves.empty());
    run_to(sent_at + OCC_GROUP_OWN_WAKE_WAIT_MS + 75);
    ASSERT_EQ(1u, l.moves.size());                     // on with the group anyway
    EXPECT_EQ(OCC_GROUP_ON, occ_group_state(&l.g));
    EXPECT_NEAR(30 * MIN_MS - OCC_GROUP_OWN_WAKE_WAIT_MS, occ_group_remaining_ms(&l.g, now), 150);  // the group's timer
}

TEST_F(OccGroup, LateOwnWakeAfterTheLightIsOnChangesNothing)
{
    run_for(30 * MIN_MS + 5100);
    l.moves.clear();
    l.loopback = false;
    occ_group_on_occupancy(&l.g, now);
    run_for(OCC_GROUP_OWN_WAKE_WAIT_MS + 150);         // on by the fallback
    ASSERT_EQ(1u, l.moves.size());
    uint32_t left = occ_group_remaining_ms(&l.g, now);
    occ_group_rx_own_keepalive(&l.g, now, 30 * 60, OCC_GROUP_ON, OCC_KEEPALIVE_WAKE);
    EXPECT_EQ(1u, l.moves.size());
    EXPECT_EQ(left, occ_group_remaining_ms(&l.g, now));
}

TEST_F(OccGroup, OwnRefreshOrLevelChangeComingBackChangesNothing)
{
    occ_group_rx_own_keepalive(&l.g, now, 20, OCC_GROUP_ON, OCC_KEEPALIVE_REFRESH);
    EXPECT_EQ(30 * MIN_MS, occ_group_remaining_ms(&l.g, now));
    occ_group_external_off(&l.g);
    occ_group_rx_own_keepalive(&l.g, now, 20, OCC_GROUP_ON, OCC_KEEPALIVE_REFRESH);
    EXPECT_EQ(OCC_GROUP_OFF, occ_group_state(&l.g));   // only our wake turns us on
    EXPECT_TRUE(l.moves.empty());
}

TEST_F(OccGroup, ExpiryWhileWakingWaitsForTheWake)
{
    // MID with 1 s of its off timer left when someone walks in; the stack is full
    run_for(30 * MIN_MS + 5100);
    ASSERT_EQ(OCC_GROUP_MID, occ_group_state(&l.g));
    while(occ_group_remaining_ms(&l.g, now) > 1000) { run_for(75); }
    ASSERT_EQ(OCC_GROUP_MID, occ_group_state(&l.g));
    out.clear(); l.moves.clear();
    l.stack_accepts = [] { return false; };
    occ_group_on_occupancy(&l.g, now);
    run_for(3000);                                     // the MID timer ran out meanwhile
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(OCC_GROUP_MID, occ_group_state(&l.g));   // the wake is still being tried
    run_for(10000);                                    // the wake gave up: the timer decides again
    EXPECT_EQ(OCC_GROUP_OFF, occ_group_state(&l.g));
}

TEST_F(OccGroup, AWakeFromAnotherLightCancelsOurs)
{
    run_for(30 * MIN_MS + 5100);
    out.clear();
    backoff_fraction = 100;
    occ_group_on_occupancy(&l.g, now);
    occ_group_rx_keepalive(&l.g, now + 50, 30 * 60, OCC_GROUP_ON, OCC_KEEPALIVE_WAKE);
    run_for(1000);
    EXPECT_TRUE(out.empty());
}

TEST_F(OccGroup, KeepaliveAtMidOrOffTurnsTheLightOnWithTheSendersTime)
{
    run_for(30 * MIN_MS + 5100);
    l.moves.clear();
    occ_group_rx_keepalive(&l.g, now, 600, OCC_GROUP_ON, OCC_KEEPALIVE_WAKE);
    EXPECT_EQ(OCC_GROUP_ON, occ_group_state(&l.g));
    ASSERT_EQ(1u, l.moves.size());
    EXPECT_EQ(OCC_GROUP_ON, l.moves[0].second);
    EXPECT_EQ(600000u, occ_group_remaining_ms(&l.g, now));

    occ_group_external_off(&l.g);
    occ_group_rx_keepalive(&l.g, now, 25, OCC_GROUP_ON, OCC_KEEPALIVE_REFRESH);   // outlasted us
    EXPECT_EQ(OCC_GROUP_ON, occ_group_state(&l.g));
}

TEST_F(OccGroup, KeepaliveWhileOnNeverMovesTheLevel)
{
    occ_group_rx_keepalive(&l.g, now, 30 * 60, OCC_GROUP_ON, OCC_KEEPALIVE_WAKE);
    occ_group_rx_keepalive(&l.g, now, 30, OCC_GROUP_ON, OCC_KEEPALIVE_REFRESH);
    EXPECT_TRUE(l.moves.empty());                      // daylight harvesting keeps its level
}

TEST_F(OccGroup, LevelChangeIsFollowedAndCancelsOurs)
{
    backoff_fraction = 100;
    run_for(30 * MIN_MS + 1000);                       // expired, our MID pending (5 s)
    size_t sent = out.size();
    occ_group_rx_level_change(&l.g, now, OCC_GROUP_MID);
    EXPECT_EQ(OCC_GROUP_MID, occ_group_state(&l.g));
    EXPECT_EQ(5 * MIN_MS, occ_group_remaining_ms(&l.g, now));
    run_for(10000);
    EXPECT_EQ(sent, out.size());                       // ours cancelled
}

TEST_F(OccGroup, MissedLevelChangeStillChangesOnItsOwnTimer)
{
    backoff_fraction = 100;
    run_for(30 * MIN_MS + 5100);
    EXPECT_EQ(OCC_GROUP_MID, occ_group_state(&l.g));
}

TEST_F(OccGroup, LevelChangeWhileOccupiedKeepsTheLightOnAndWakesTheGroup)
{
    run_for(29 * MIN_MS);
    occ_group_on_occupancy(&l.g, now);
    run_for(1000);
    backoff_fraction = 0;
    occ_group_rx_level_change(&l.g, now, OCC_GROUP_MID);
    EXPECT_EQ(OCC_GROUP_ON, occ_group_state(&l.g));
    EXPECT_TRUE(l.moves.empty());
    run_for(100);
    ASSERT_EQ(1u, out.size());
    EXPECT_EQ(OCC_KEEPALIVE_WAKE, out[0].reason);
}

TEST_F(OccGroup, DuplicateOrLateLevelChangesAreHarmless)
{
    run_for(30 * MIN_MS + 5100);                       // MID on its own
    l.moves.clear();
    run_for(MIN_MS);
    occ_group_rx_level_change(&l.g, now, OCC_GROUP_MID);
    EXPECT_TRUE(l.moves.empty());
    EXPECT_EQ(5 * MIN_MS, occ_group_remaining_ms(&l.g, now));   // resynced
    occ_group_rx_level_change(&l.g, now, OCC_GROUP_OFF);
    EXPECT_EQ(OCC_GROUP_OFF, occ_group_state(&l.g));
    l.moves.clear();
    occ_group_rx_level_change(&l.g, now, OCC_GROUP_MID);       // late MID: stay off
    EXPECT_EQ(OCC_GROUP_OFF, occ_group_state(&l.g));
    EXPECT_TRUE(l.moves.empty());
}

TEST_F(OccGroup, ExternalOnAndOffSendNothingAndMoveNothing)
{
    occ_group_external_off(&l.g);
    EXPECT_EQ(OCC_GROUP_OFF, occ_group_state(&l.g));
    run_for(60 * MIN_MS);
    occ_group_external_on(&l.g, now);
    EXPECT_EQ(OCC_GROUP_ON, occ_group_state(&l.g));
    EXPECT_EQ(30 * MIN_MS, occ_group_remaining_ms(&l.g, now));
    EXPECT_TRUE(out.empty());
    EXPECT_TRUE(l.moves.empty());
}

TEST_F(OccGroup, ShortTimeoutsScaleTheWindow)
{
    l.dim_ms = 30000; l.off_ms = 30000;               // D x 8 test setting
    occ_group_init(&l.g, &l.ops, now);
    backoff_fraction = 100;
    uint32_t start = now;
    run_to(start + 30000 - 7500 - 1);                  // window = 30 s / 4
    EXPECT_TRUE(out.empty());
    run_to(start + 30000);
    ASSERT_EQ(1u, out.size());                         // one refresh, 5 s into the window
    EXPECT_NEAR(3, out[0].time_s, 1);
}

TEST_F(OccGroup, WorksAcrossTheMillisecondClockWrap)
{
    now = 0xFFFFFFFFu - 10 * MIN_MS;
    occ_group_init(&l.g, &l.ops, now);
    run_for(30 * MIN_MS + 5100);
    EXPECT_EQ(OCC_GROUP_MID, occ_group_state(&l.g));
    ASSERT_EQ(2u, out.size());
}

/* --------------------------------------------------------------- send retries */
// MID, then occupancy: a wake goes out after the backoff (backoff_fraction 50: 150 ms,
// retries exactly 1 s apart)
class OccGroupRetry : public OccGroup
{
protected:
    uint32_t t0 = 0;
    int refuse = 0;                    // refuse this many sends, then accept (-1: always)

    void SetUp() override
    {
        OccGroup::SetUp();
        l.stack_accepts = [this] { if(0 == refuse) { return true; } if(refuse > 0) { refuse--; } return false; };
        run_for(30 * MIN_MS + 5100);
        out.clear(); l.moves.clear();
        backoff_fraction = 50;
    }
    void walk_in() { t0 = now; occ_group_on_occupancy(&l.g, now); }
};

TEST_F(OccGroupRetry, RefusedSendIsTriedAgainAboutOnceASecondWithAFreshTime)
{
    refuse = 2;
    walk_in();
    run_for(5000);
    ASSERT_EQ(2u, l.refused.size());
    EXPECT_NEAR(t0 + 150, l.refused[0], 75);
    EXPECT_NEAR(1000, l.refused[1] - l.refused[0], 75);
    ASSERT_EQ(1u, out.size());
    EXPECT_NEAR(1000, out[0].at - l.refused[1], 75);
    EXPECT_EQ(OCC_KEEPALIVE_WAKE, out[0].reason);
    EXPECT_EQ(30 * 60, out[0].time_s);                 // the light is not on yet: a full timer
    EXPECT_TRUE(l.moves.size() == 1u && OCC_GROUP_ON == l.moves[0].second);   // on once it went out
    EXPECT_GE(l.moves[0].first, out[0].at);
}

TEST_F(OccGroupRetry, RetriesAreSpreadByTheJitter)
{
    refuse = 1;
    walk_in();
    backoff_fraction = 0;                              // retry at 1000 - 250 ms
    run_for(3000);
    ASSERT_EQ(1u, out.size());
    EXPECT_NEAR(750, out[0].at - l.refused[0], 75);
}

TEST_F(OccGroupRetry, GivesUpAfterFiveAttemptsInAboutFourSeconds)
{
    refuse = -1;
    walk_in();
    run_for(10000);
    ASSERT_EQ(5u, l.refused.size());
    EXPECT_NEAR(4000 + 150, l.refused[4] - l.refused[0], 150);   // each attempt up to one 75 ms tick late
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(OCC_GROUP_MID, occ_group_state(&l.g));   // the group never heard: this light stays with it
    EXPECT_TRUE(l.moves.empty());
}

TEST_F(OccGroupRetry, AnotherLightsWakeDuringTheRetryCancelsIt)
{
    refuse = -1;
    walk_in();
    run_for(500);
    ASSERT_EQ(1u, l.refused.size());
    EXPECT_TRUE(l.moves.empty());
    occ_group_rx_keepalive(&l.g, now, 30 * 60, OCC_GROUP_ON, OCC_KEEPALIVE_WAKE);
    ASSERT_EQ(1u, l.moves.size());                     // on with the group
    run_for(10000);
    EXPECT_EQ(1u, l.refused.size());                   // no more attempts
}

TEST_F(OccGroupRetry, HeavyTrafficHoldsTheFirstThreeAttemptsThenSends)
{
    l.traffic_heavy = [] { return true; };
    walk_in();
    run_to(t0 + 150 + 3000 - 75);
    EXPECT_TRUE(out.empty());
    run_for(400);                                      // attempts land on the 75 ms sensor ticks
    ASSERT_EQ(1u, out.size());                         // 4th attempt: sent anyway
    EXPECT_NEAR(t0 + 150 + 3000 + 150, out[0].at, 150);
    EXPECT_EQ(30 * 60, out[0].time_s);
    EXPECT_TRUE(l.moves.empty() || l.moves[0].first >= out[0].at);   // dark until the group is woken
}

TEST_F(OccGroupRetry, SendsAtTheNextAttemptOnceTrafficIsLighter)
{
    l.traffic_heavy = [this] { return (now - t0) < 1500; };
    walk_in();
    run_for(5000);
    ASSERT_EQ(1u, out.size());
    EXPECT_NEAR(t0 + 2150 + 75, out[0].at, 150);       // held at 150 and 1150 ms
}

TEST_F(OccGroupRetry, HeavyTrafficAndAFullStackGivesUpAfterTheLastTwoAttempts)
{
    l.traffic_heavy = [] { return true; };
    refuse = -1;
    walk_in();
    run_for(10000);
    EXPECT_EQ(2u, l.refused.size());                   // attempts 4 and 5 reached the stack
    EXPECT_TRUE(out.empty());
}

TEST_F(OccGroup, RefusedLevelChangeMovesTheLightOnceAndIsRetried)
{
    backoff_fraction = 50;
    uint32_t start = now;
    run_to(start + 30 * MIN_MS - 1000);                // the refresh has gone out
    out.clear();
    int refuse = 1;
    l.stack_accepts = [&refuse] { if(refuse > 0) { refuse--; return false; } return true; };
    run_to(start + 30 * MIN_MS + 2600);                // expiry backoff 2.5 s
    ASSERT_EQ(1u, l.refused.size());
    ASSERT_EQ(1u, l.moves.size());
    EXPECT_EQ(OCC_GROUP_MID, l.moves[0].second);
    run_for(1100);
    ASSERT_EQ(1u, out.size());
    EXPECT_FALSE(out[0].keepalive);
    EXPECT_EQ(OCC_GROUP_MID, out[0].state);
    EXPECT_EQ(1u, l.moves.size());                     // not moved again
}

TEST_F(OccGroup, HeldLevelChangeMovesTheLightOnlyWhenSent)
{
    backoff_fraction = 50;
    uint32_t start = now;
    run_to(start + 30 * MIN_MS - 1000);
    out.clear();
    l.traffic_heavy = [] { return true; };
    run_to(start + 30 * MIN_MS + 2500 + 3000 - 75);
    EXPECT_TRUE(l.moves.empty());
    EXPECT_EQ(OCC_GROUP_ON, occ_group_state(&l.g));
    run_for(400);                                      // attempts land on the 75 ms sensor ticks
    ASSERT_EQ(1u, l.moves.size());
    ASSERT_EQ(1u, out.size());
    EXPECT_EQ(OCC_GROUP_MID, out[0].state);
}

TEST_F(OccGroup, HeldLevelChangeFollowsALevelChangeFromAnotherLight)
{
    backoff_fraction = 50;
    uint32_t start = now;
    run_to(start + 30 * MIN_MS - 1000);
    out.clear();
    l.traffic_heavy = [] { return true; };
    run_to(start + 30 * MIN_MS + 3000);                // ours is held
    occ_group_rx_level_change(&l.g, now, OCC_GROUP_MID);
    ASSERT_EQ(1u, l.moves.size());
    run_for(10000);
    EXPECT_TRUE(out.empty());                          // ours was cancelled
    EXPECT_EQ(1u, l.moves.size());
}

/* =============================================================== part 2: group simulation */
struct Sim
{
    uint32_t now = 1000;
    std::mt19937 rng;
    std::vector<Light> lights;
    std::vector<uint32_t> phase;          // each light's sensor sample offset
    std::vector<Msg> in_flight;           // (at = delivery time, from)
    std::vector<int> in_flight_to;
    std::vector<Msg> all_sent;
    double loss = 0.0;
    std::function<bool(uint32_t)> heavy;       // broadcast traffic heavy at time t
    std::function<bool(uint32_t)> stack_full;  // the stack refuses every send at time t
    int refused = 0;

    Sim(int n, uint32_t seed, uint32_t dim_ms = 30 * MIN_MS, uint32_t off_ms = 5 * MIN_MS) : rng(seed), lights(n)
    {
        for(int i = 0; i < n; i++)
        {
            Light &l = lights[i];
            l.dim_ms = dim_ms; l.off_ms = off_ms;
            l.random = [this](uint32_t max) { return max ? (uint32_t)(rng() % (max + 1)) : 0; };
            l.sent = [this](const Msg &m) { send(m); };
            l.traffic_heavy = [this] { return heavy && heavy(now); };
            l.stack_accepts = [this] { if(stack_full && stack_full(now)) { refused++; return false; } return true; };
            l.loopback = false;                                     // send() models it
            setup_light(l, i, &now);
            phase.push_back((rng() % 15) * 5);                      // on the 5 ms simulation step
        }
    }
    void send(const Msg &m)
    {
        all_sent.push_back(m);
        for(size_t j = 0; j < lights.size(); j++)
        {
            Msg d = m;
            if((int)j == m.from)
            {
                d.at = now + 5;                                     // own message: the stack's loopback
            }
            else
            {
                if(std::uniform_real_distribution<>(0, 1)(rng) < loss) { continue; }
                d.at = now + 20 + rng() % 280;                      // 20-300 ms over 3 hops
            }
            in_flight.push_back(d);
            in_flight_to.push_back((int)j);
        }
    }
    // occupancy(i, t) -> does light i see occupancy at sample time t
    void run_to(uint32_t until, const std::function<bool(int, uint32_t)> &occupied = nullptr)
    {
        while((int32_t)(until - now) > 0)
        {
            now += 5;
            for(size_t k = 0; k < in_flight.size();)
            {
                if((int32_t)(now - in_flight[k].at) >= 0)
                {
                    Msg m = in_flight[k];
                    int to = in_flight_to[k];
                    occ_group_t *g = &lights[to].g;
                    in_flight.erase(in_flight.begin() + k);
                    in_flight_to.erase(in_flight_to.begin() + k);
                    if(getenv("OCC_TRACE_RX") && to == atoi(getenv("OCC_TRACE_RX")))
                    {
                        printf("  rx t=%u light %d: %s time=%u  own state=%d left=%u pending=%d\n", now, to,
                               m.keepalive ? "keepalive" : "level", m.time_s, g->state, occ_group_remaining_ms(g, now), g->pending);
                    }
                    if(to == m.from)
                    {
                        if(m.keepalive) { occ_group_rx_own_keepalive(g, now, m.time_s, m.state, m.reason); }
                    }
                    else if(m.keepalive) { occ_group_rx_keepalive(g, now, m.time_s, m.state, m.reason); }
                    else                 { occ_group_rx_level_change(g, now, m.state); }
                }
                else { k++; }
            }
            for(size_t i = 0; i < lights.size(); i++)
            {
                if((now + phase[i]) % 75 != 0) { continue; }
                if(occupied && occupied((int)i, now)) { occ_group_on_occupancy(&lights[i].g, now); }
                occ_group_tick(&lights[i].g, now);
            }
        }
    }
    int count(bool keepalive, int reason_or_state) const
    {
        int c = 0;
        for(const Msg &m : all_sent)
        {
            if(m.keepalive != keepalive) { continue; }
            if(keepalive ? (int)m.reason == reason_or_state : (int)m.state == reason_or_state) { c++; }
        }
        return c;
    }
    bool all_in(occ_group_state_t s) const
    {
        for(const Light &l : lights) { if(occ_group_state(&l.g) != s) { return false; } }
        return true;
    }
    // first time each light moved to state s (after 'since')
    std::vector<uint32_t> first_move(occ_group_state_t s, uint32_t since = 0) const
    {
        std::vector<uint32_t> t;
        for(const Light &l : lights)
        {
            for(auto &mv : l.moves) { if(mv.second == s && mv.first >= since) { t.push_back(mv.first); break; } }
        }
        return t;
    }
};

static uint32_t spread(const std::vector<uint32_t> &t)
{
    return t.empty() ? 0 : *std::max_element(t.begin(), t.end()) - *std::min_element(t.begin(), t.end());
}

TEST(OccGroupSim, VacantGroupDimsAndTurnsOffTogetherWithFewMessages)
{
    int refresh = 0, mid = 0, off = 0, runs = 20;
    for(int seed = 1; seed <= runs; seed++)
    {
        Sim s(20, seed);
        uint32_t start = s.now;
        s.run_to(start + 30 * MIN_MS + 6000);
        ASSERT_TRUE(s.all_in(OCC_GROUP_MID)) << "seed " << seed;
        std::vector<uint32_t> t = s.first_move(OCC_GROUP_MID);
        ASSERT_EQ(20u, t.size());
        EXPECT_LE(spread(t), 400u) << "seed " << seed;             // the command reaches everyone
        s.run_to(start + 35 * MIN_MS + 12000);
        ASSERT_TRUE(s.all_in(OCC_GROUP_OFF)) << "seed " << seed;
        EXPECT_LE(spread(s.first_move(OCC_GROUP_OFF)), 400u);
        refresh += s.count(true, OCC_KEEPALIVE_REFRESH);
        mid += s.count(false, OCC_GROUP_MID);
        off += s.count(false, OCC_GROUP_OFF);
        EXPECT_EQ(0, s.count(true, OCC_KEEPALIVE_WAKE));
    }
    printf("      20 lights, vacant, %d runs: refresh %.2f  mid %.2f  off %.2f multicasts per run\n",
           runs, (double)refresh / runs, (double)mid / runs, (double)off / runs);
    EXPECT_LE(refresh, runs * 3 / 2);
    EXPECT_LE(mid, runs * 3 / 2);
    EXPECT_LE(off, runs * 3 / 2);
}

TEST(OccGroupSim, OneOccupiedLightKeepsTheWholeGroupOn)
{
    int runs = 10, total = 0;
    for(int seed = 1; seed <= runs; seed++)
    {
        Sim s(20, 100 + seed);
        uint32_t start = s.now;
        // light 7 sees someone every 20 s; nobody else does
        auto occ = [&](int i, uint32_t t) { return 7 == i && ((t - start) % 20000) < 75; };
        s.run_to(start + 3 * 60 * MIN_MS, occ);
        if(getenv("OCC_TRACE") && seed == atoi(getenv("OCC_TRACE")))
        {
            for(const Msg &m : s.all_sent)
            {
                printf("  t=%9.1f s  light %2d  %s %s time=%u\n", (m.at - start) / 1000.0, m.from,
                       m.keepalive ? "KEEPALIVE" : "LEVEL", m.keepalive ? (m.reason ? "refresh" : "wake") : (m.state == OCC_GROUP_MID ? "MID" : "OFF"), m.time_s);
            }
        }
        for(const Light &l : s.lights)
        {
            EXPECT_TRUE(l.moves.empty()) << "light " << l.id << " moved, seed " << seed;   // never dimmed
        }
        EXPECT_TRUE(s.all_in(OCC_GROUP_ON));
        total += (int)s.all_sent.size();
    }
    double per_period = (double)total / runs / 6.0;    // 3 h = 6 dim periods
    printf("      20 lights, one occupied, 3 h: %.2f multicasts per 30 min dim period\n", per_period);
    EXPECT_LE(per_period, 3.0);
}

TEST(OccGroupSim, OccupancyWhileOffWakesEveryoneWithinASecond)
{
    for(int seed = 1; seed <= 20; seed++)
    {
        Sim s(20, 200 + seed);
        uint32_t start = s.now;
        s.run_to(start + 36 * MIN_MS);
        ASSERT_TRUE(s.all_in(OCC_GROUP_OFF));
        size_t before = s.all_sent.size();
        uint32_t walk_in = s.now + 1000;
        // three lights near the door see the person within 100 ms of each other
        auto occ = [&](int i, uint32_t t) { return i < 3 && t >= walk_in + 50 * i && t < walk_in + 50 * i + 2000; };
        s.run_to(walk_in + 3000, occ);
        ASSERT_TRUE(s.all_in(OCC_GROUP_ON)) << "seed " << seed;
        std::vector<uint32_t> t = s.first_move(OCC_GROUP_ON, walk_in);
        ASSERT_EQ(20u, t.size());
        EXPECT_LE(*std::max_element(t.begin(), t.end()) - walk_in, 800u) << "seed " << seed;
        EXPECT_LE(s.all_sent.size() - before, 3u);     // wakes: at most one per detecting light
    }
}

TEST(OccGroupSim, OccupancyAtMidWakesEveryone)
{
    Sim s(10, 300);
    uint32_t start = s.now;
    s.run_to(start + 31 * MIN_MS);
    ASSERT_TRUE(s.all_in(OCC_GROUP_MID));
    uint32_t t0 = s.now;
    s.run_to(t0 + 2000, [&](int i, uint32_t t) { return 4 == i && t < t0 + 200; });
    EXPECT_TRUE(s.all_in(OCC_GROUP_ON));
}

TEST(OccGroupSim, WithTenPercentLossEveryLightStillEndsOffAndOccupancyIsFollowed)
{
    for(int seed = 1; seed <= 20; seed++)
    {
        Sim s(20, 400 + seed);
        s.loss = 0.10;
        uint32_t start = s.now;
        std::mt19937 occ_rng(seed);
        std::vector<std::pair<int, uint32_t>> visits;      // light, time: someone walks by
        for(int k = 0; k < 30; k++) { visits.push_back({ (int)(occ_rng() % 20), start + occ_rng() % (90 * MIN_MS) }); }
        auto occ = [&](int i, uint32_t t)
        {
            for(auto &v : visits) { if(v.first == i && t >= v.second && t < v.second + 3000) { return true; } }
            return false;
        };
        uint32_t last_visit = 0;
        for(auto &v : visits) { last_visit = std::max(last_visit, v.second); }
        s.run_to(last_visit + 3000, occ);
        // just after the last visit the whole group is on (or that light's wake reached it)
        int on = 0;
        for(const Light &l : s.lights) { on += OCC_GROUP_ON == occ_group_state(&l.g); }
        EXPECT_GE(on, 18) << "seed " << seed;
        // nobody around any more: everyone ends off within the off delay + backoffs
        s.run_to(last_visit + 36 * MIN_MS + 30000);
        EXPECT_TRUE(s.all_in(OCC_GROUP_OFF)) << "seed " << seed;
    }
}

TEST(OccGroupSim, LargeGroupStillSendsAboutOneRefreshPerPeriod)
{
    int refresh = 0, runs = 20;
    for(int seed = 1; seed <= runs; seed++)
    {
        Sim s(50, 500 + seed);
        s.run_to(s.now + 30 * MIN_MS);
        refresh += s.count(true, OCC_KEEPALIVE_REFRESH);
    }
    printf("      50 lights: %.2f refresh multicasts per period\n", (double)refresh / runs);
    EXPECT_LE(refresh, runs * 2);
}

TEST(OccGroupSim, ShortTestTimeoutsDoNotRefreshContinuously)
{
    Sim s(10, 600, 30000, 30000);                      // D x 8: 30 s / 30 s
    uint32_t start = s.now;
    s.run_to(start + 10 * MIN_MS, [&](int i, uint32_t t) { return 0 == i && ((t - start) % 10000) < 75; });
    EXPECT_TRUE(s.all_in(OCC_GROUP_ON));
    printf("      30 s timeout, one occupied light, 10 min: %zu multicasts\n", s.all_sent.size());
    // A cycle is only 20-30 s here, so refresh + reply repeat about every 25 s
    EXPECT_LE(s.all_sent.size(), 100u);
}

/* --------------------------------------------------------------- busy network */
// The case a send queue would make worse: nobody's message goes out at once, so
// every light has one pending.  Held/retried messages stay cancellable here, so
// the first one that goes out still stops the rest.
TEST(OccGroupSim, HeavyTrafficVacantGroupStillDimsTogetherWithFewMessages)
{
    int refresh = 0, mid = 0, off = 0, runs = 20;
    uint32_t worst_spread = 0;
    for(int seed = 1; seed <= runs; seed++)
    {
        Sim s(10, 700 + seed);
        s.heavy = [](uint32_t) { return true; };
        uint32_t start = s.now;
        s.run_to(start + 30 * MIN_MS + 10000);
        ASSERT_TRUE(s.all_in(OCC_GROUP_MID)) << "seed " << seed;
        uint32_t sp = spread(s.first_move(OCC_GROUP_MID));
        worst_spread = std::max(worst_spread, sp);
        EXPECT_LE(sp, 1500u) << "seed " << seed;
        s.run_to(start + 35 * MIN_MS + 20000);
        ASSERT_TRUE(s.all_in(OCC_GROUP_OFF)) << "seed " << seed;
        refresh += s.count(true, OCC_KEEPALIVE_REFRESH);
        mid += s.count(false, OCC_GROUP_MID);
        off += s.count(false, OCC_GROUP_OFF);
    }
    printf("      10 lights, heavy traffic, %d runs: refresh %.2f  mid %.2f  off %.2f multicasts per run,"
           " worst MID spread %u ms\n", runs, (double)refresh / runs, (double)mid / runs, (double)off / runs, worst_spread);
    EXPECT_LE(refresh, runs * 2);
    EXPECT_LE(mid, runs * 2);
    EXPECT_LE(off, runs * 2);
}

TEST(OccGroupSim, HeavyTrafficWakeStillReachesEveryoneWithinFourSeconds)
{
    for(int seed = 1; seed <= 20; seed++)
    {
        Sim s(10, 800 + seed);
        uint32_t start = s.now;
        s.run_to(start + 36 * MIN_MS);
        ASSERT_TRUE(s.all_in(OCC_GROUP_OFF));
        s.heavy = [](uint32_t) { return true; };
        size_t before = s.all_sent.size();
        uint32_t walk_in = s.now + 1000;
        auto occ = [&](int i, uint32_t t) { return i < 3 && t >= walk_in + 50 * i && t < walk_in + 50 * i + 2000; };
        s.run_to(walk_in + 5000, occ);
        ASSERT_TRUE(s.all_in(OCC_GROUP_ON)) << "seed " << seed;
        std::vector<uint32_t> t = s.first_move(OCC_GROUP_ON, walk_in);
        EXPECT_LE(*std::max_element(t.begin(), t.end()) - walk_in, 4000u) << "seed " << seed;
        EXPECT_LE(s.all_sent.size() - before, 3u);     // still at most one per detecting light
    }
}

TEST(OccGroupSim, StackFullForTwoSecondsTheWakeIsRetriedAndOnlyOneGoesOut)
{
    int wakes = 0, runs = 20;
    for(int seed = 1; seed <= runs; seed++)
    {
        Sim s(10, 900 + seed);
        uint32_t start = s.now;
        s.run_to(start + 36 * MIN_MS);
        ASSERT_TRUE(s.all_in(OCC_GROUP_OFF));
        size_t before = s.all_sent.size();
        uint32_t walk_in = s.now + 1000;
        s.stack_full = [walk_in](uint32_t t) { return t < walk_in + 2000; };
        s.run_to(walk_in + 6000, [&](int i, uint32_t t) { return 0 == i && t >= walk_in && t < walk_in + 200; });
        ASSERT_TRUE(s.all_in(OCC_GROUP_ON)) << "seed " << seed;
        EXPECT_EQ(1u, s.all_sent.size() - before) << "seed " << seed;
        EXPECT_GE(s.refused, 2);
        wakes += (int)(s.all_sent.size() - before);
    }
    EXPECT_EQ(runs, wakes);
}

/* --------------------------------------------------------------- waking together */
// The light that sees someone goes on when its own wake comes back from the
// stack, so it is not ahead of the group by the wake backoff any more.
TEST(OccGroupSim, TheDetectingLightComesOnWithTheGroupNotBeforeIt)
{
    uint32_t worst_lead = 0, worst_on = 0;
    for(int seed = 1; seed <= 20; seed++)
    {
        Sim s(10, 1000 + seed);
        uint32_t start = s.now;
        s.run_to(start + 36 * MIN_MS);
        ASSERT_TRUE(s.all_in(OCC_GROUP_OFF));
        uint32_t walk_in = s.now + 1000;
        s.run_to(walk_in + 3000, [&](int i, uint32_t t) { return 0 == i && t >= walk_in && t < walk_in + 200; });
        ASSERT_TRUE(s.all_in(OCC_GROUP_ON)) << "seed " << seed;
        std::vector<uint32_t> t = s.first_move(OCC_GROUP_ON, walk_in);
        ASSERT_EQ(10u, t.size());
        uint32_t detector = t[0];
        uint32_t first_other = *std::min_element(t.begin() + 1, t.end());
        uint32_t wake_at = 0;
        for(const Msg &m : s.all_sent) { if(m.keepalive && OCC_KEEPALIVE_WAKE == m.reason && m.at >= walk_in) { wake_at = m.at; break; } }
        ASSERT_NE(0u, wake_at);
        EXPECT_GE(detector, wake_at) << "seed " << seed;          // not before the wake went out
        EXPECT_LE(detector - wake_at, 75u) << "seed " << seed;    // on at its next sample after the loopback
        worst_lead = std::max(worst_lead, (first_other > detector) ? first_other - detector : 0u);
        worst_on = std::max(worst_on, *std::max_element(t.begin(), t.end()) - walk_in);
    }
    printf("      walk-in: detector ahead of the first other light by at most %u ms (radio time only); "
           "all on within %u ms\n", worst_lead, worst_on);
}

TEST(OccGroupSim, IfTheWakeCannotBeSentNoLightComesOn)
{
    Sim s(10, 1100);
    uint32_t start = s.now;
    s.run_to(start + 36 * MIN_MS);
    ASSERT_TRUE(s.all_in(OCC_GROUP_OFF));
    uint32_t walk_in = s.now + 1000;
    s.stack_full = [](uint32_t) { return true; };
    s.run_to(walk_in + 10000, [&](int i, uint32_t t) { return 0 == i && t >= walk_in && t < walk_in + 200; });
    EXPECT_TRUE(s.all_in(OCC_GROUP_OFF));              // the detecting light stays with the group
    EXPECT_TRUE(s.first_move(OCC_GROUP_ON, walk_in).empty());
    EXPECT_EQ(5, s.refused);                           // tried 5 times, then gave up
}

// Wake backoff trade-off: a person walks in under 3 lights whose sensors see
// them up to 100 ms apart.  Fewer wakes with a longer backoff, sooner light
// with a shorter one.  Prints the numbers; the limits only catch a regression.
TEST(OccGroupSim, WakeBackoffTradeoff)
{
    int runs = 50, wakes = 0;
    uint32_t total_on = 0, worst_on = 0;
    for(int seed = 1; seed <= runs; seed++)
    {
        Sim s(10, 1200 + seed);
        uint32_t start = s.now;
        s.run_to(start + 36 * MIN_MS);
        size_t before = s.all_sent.size();
        uint32_t walk_in = s.now + 1000;
        uint32_t offset[3] = { 0, (uint32_t)(s.rng() % 101), (uint32_t)(s.rng() % 101) };
        s.run_to(walk_in + 3000, [&](int i, uint32_t t) { return i < 3 && t >= walk_in + offset[i] && t < walk_in + offset[i] + 2000; });
        ASSERT_TRUE(s.all_in(OCC_GROUP_ON));
        std::vector<uint32_t> t = s.first_move(OCC_GROUP_ON, walk_in);
        uint32_t all_on = *std::max_element(t.begin(), t.end()) - walk_in;
        total_on += all_on;
        worst_on = std::max(worst_on, all_on);
        wakes += (int)(s.all_sent.size() - before);
    }
    printf("      wake backoff %lu ms: %.2f wakes per walk-in (3 lights see it), all lights on after %u ms avg, %u ms worst\n",
           (unsigned long)OCC_GROUP_WAKE_BACKOFF_MS, (double)wakes / runs, total_on / runs, worst_on);
    EXPECT_LE(wakes, runs * 3);
}
