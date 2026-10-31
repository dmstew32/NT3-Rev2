#include <gtest/gtest.h>

extern "C" {
#include "light_levels.h"
}

/* --------------------------------------------------------------- Daintree level mapping */
TEST(LightLevelMap, ZigbeeLevelsMapOntoMinToMax)
{
    // min 20, max 200
    EXPECT_EQ(200, light_level_map(254, 20, 200));
    EXPECT_EQ(110, light_level_map(128, 20, 200));    // 20 + 127 * 180 / 253
    EXPECT_EQ(20, light_level_map(1, 20, 200));
}

TEST(LightLevelMap, LevelZeroIsOff)
{
    EXPECT_EQ(0, light_level_map(0, 20, 200));
    EXPECT_EQ(0, light_level_map(0, 0, 254));
}

TEST(LightLevelMap, LevelOneNeverTurnsTheLightOff)
{
    EXPECT_EQ(1, light_level_map(1, 0, 254));         // Daintree default min is 0
    EXPECT_EQ(3, light_level_map(1, 3, 254));         // the values Daintree writes
}

TEST(LightLevelMap, DefaultRangeIsUnchanged)
{
    for(int level = 1; level <= 254; level++)
    {
        EXPECT_EQ(level, light_level_map((uint8_t)level, 0, 254)) << level;
        EXPECT_EQ(level, light_level_map((uint8_t)level, 1, 254)) << level;
    }
}

TEST(LightLevelMap, NeverOutsideMinMaxAndNeverGoesDown)
{
    for(int lo = 0; lo <= 254; lo += 7)
    {
        for(int hi = lo; hi <= 254; hi += 11)
        {
            uint8_t last = 0;
            for(int level = 1; level <= 255; level++)
            {
                uint8_t out = light_level_map((uint8_t)level, (uint8_t)lo, (uint8_t)hi);
                ASSERT_GE(out, lo < 1 ? 1 : lo) << lo << " " << hi << " " << level;
                ASSERT_LE(out, hi < 1 ? 1 : hi) << lo << " " << hi << " " << level;
                ASSERT_GE(out, last);
                last = out;
            }
            EXPECT_EQ(lo < 1 ? 1 : lo, light_level_map(1, (uint8_t)lo, (uint8_t)hi));
            EXPECT_EQ(hi < 1 ? 1 : hi, light_level_map(254, (uint8_t)lo, (uint8_t)hi));
        }
    }
}

TEST(LightLevelMap, MaxBelowMinUsesMin)
{
    EXPECT_EQ(100, light_level_map(1, 100, 50));
    EXPECT_EQ(100, light_level_map(254, 100, 50));
}

/* --------------------------------------------------------------- colour split */
TEST(LightPwmShare, SplitsByRatio)
{
    EXPECT_EQ(100, light_pwm_share(200, 0.5F));       // 4000 K
    EXPECT_EQ(200, light_pwm_share(200, 1.0F));       // 3000 / 5000 K: one channel
    EXPECT_EQ(0, light_pwm_share(200, 0.0F));         //                the other off
    EXPECT_EQ(7, light_pwm_share(15, 0.5F));
}

TEST(LightPwmShare, AChannelWithAShareStaysOnAtTheBottom)
{
    EXPECT_EQ(1, light_pwm_share(1, 0.5F));           // 0.5 would round to 0: light dark while on
    EXPECT_EQ(1, light_pwm_share(3, 0.25F));
    EXPECT_EQ(0, light_pwm_share(0, 0.5F));           // off is off
    EXPECT_EQ(0, light_pwm_share(5, 0.0F));           // no share, no light
}

/* --------------------------------------------------------------- PWM floor */
TEST(LightPwmFloor, OffStaysOffAndOnIsAtLeastTen)
{
    EXPECT_EQ(0, light_pwm_floor(0));
    for(int v = 1; v < 10; v++)
    {
        EXPECT_EQ(10, light_pwm_floor((uint8_t)v)) << v;
    }
    EXPECT_EQ(10, light_pwm_floor(10));
    EXPECT_EQ(11, light_pwm_floor(11));
    EXPECT_EQ(254, light_pwm_floor(254));
}

TEST(LightPwmFloor, BottomOfTheRangeAt4000K)
{
    // level 15 at 4000 K: 7 + 7 -> each channel 10
    EXPECT_EQ(10, light_pwm_floor(light_pwm_share(15, 0.5F)));
    // level 1 at 4000 K: still on
    EXPECT_EQ(10, light_pwm_floor(light_pwm_share(1, 0.5F)));
}
