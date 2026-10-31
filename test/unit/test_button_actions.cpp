#include <gtest/gtest.h>
#include "fff.h"

extern "C" {
#include "nt3_test_stubs.h"
#include "app_button.h"
#include "app_leds.h"
#include "button_actions.h"
}

DEFINE_FFF_GLOBALS

extern "C" {
FAKE_VOID_FUNC(app_button_on_startup);
FAKE_VALUE_FUNC(sl_status_t, app_button_register_sequence_cb, app_button_sequence_cb_t);
FAKE_VALUE_FUNC(sl_status_t, app_button_register_hold, app_button_t, const app_button_hold_t *);
FAKE_VOID_FUNC(app_leds_show_presses, uint8_t);
FAKE_VOID_FUNC(app_leds_show_fail);
FAKE_VOID_FUNC(app_leds_show_dim_step);
FAKE_VALUE_FUNC(which_nwk_t, get_present_network);
FAKE_VALUE_FUNC(bool, device_is_in_a_group);
FAKE_VOID_FUNC(force_factory_reset, uint32_t, reset_reason_t);
FAKE_VOID_FUNC(toggle_50_percent_mode);
FAKE_VOID_FUNC(toggle_em_mode);
FAKE_VOID_FUNC(start_local_auto_commission);
FAKE_VOID_FUNC(start_display_fw_version);
FAKE_VOID_FUNC(set_occupancy_threshold, uint16_t);
FAKE_VOID_FUNC(save_adaptiv_unoccupied_delay, uint16_t);
FAKE_VOID_FUNC(save_adaptiv_unoccupied_delay_to_off, uint16_t);
FAKE_VALUE_FUNC(bool, tx_occ_sensor_new_delays, uint16_t, uint16_t);
FAKE_VALUE_FUNC(uint16_t, get_unoccupied_delay);
FAKE_VALUE_FUNC(uint16_t, get_unoccupied_delay_to_off);
FAKE_VOID_FUNC(save_max_light_level, uint8_t);
FAKE_VOID_FUNC(save_mid_light_level, uint8_t);
FAKE_VOID_FUNC(save_min_light_level, uint8_t);
FAKE_VALUE_FUNC(bool, tx_new_light_levels);
FAKE_VALUE_FUNC(uint16_t, get_light_color);
FAKE_VOID_FUNC(save_light_color, uint16_t);
FAKE_VALUE_FUNC(bool, tx_color_new_temp, uint16_t);
FAKE_VALUE_FUNC(uint8_t, get_present_light_level);
FAKE_VOID_FUNC(move_to_level_onoff, uint8_t, uint16_t);
FAKE_VALUE_FUNC(uint8_t, get_max_light_level);
FAKE_VOID_FUNC(place_in_manual_add_into_group_mode);
FAKE_VALUE_FUNC(uint32_t, get_time_ms);
FAKE_VOID_FUNC_VARARG(sl_zigbee_app_debug_println, const char *, ...);
FAKE_VOID_FUNC_VARARG(sl_zigbee_core_debug_println, const char *, ...);
}

#define FAKES(F) \
  F(app_button_on_startup) F(app_button_register_sequence_cb) F(app_button_register_hold) \
  F(app_leds_show_presses) F(app_leds_show_fail) F(app_leds_show_dim_step) F(get_present_network) \
  F(device_is_in_a_group) F(force_factory_reset) F(toggle_50_percent_mode) F(toggle_em_mode) \
  F(start_local_auto_commission) F(start_display_fw_version) F(set_occupancy_threshold) \
  F(save_adaptiv_unoccupied_delay) F(save_adaptiv_unoccupied_delay_to_off) F(tx_occ_sensor_new_delays) \
  F(get_unoccupied_delay) F(get_unoccupied_delay_to_off) F(save_max_light_level) F(save_mid_light_level) \
  F(save_min_light_level) F(tx_new_light_levels) F(get_light_color) F(save_light_color) F(tx_color_new_temp) \
  F(get_present_light_level) F(move_to_level_onoff) F(get_max_light_level) \
  F(place_in_manual_add_into_group_mode) F(get_time_ms) F(sl_zigbee_app_debug_println) \
  F(sl_zigbee_core_debug_println)

static app_button_sequence_cb_t m_sequence;
static app_button_hold_t m_hold[APP_BUTTON_COUNT];
static sl_status_t capture_sequence(app_button_sequence_cb_t cb) { m_sequence = cb; return SL_STATUS_OK; }
static sl_status_t capture_hold(app_button_t g, const app_button_hold_t *p) { m_hold[g] = *p; return SL_STATUS_OK; }

class ButtonActions : public ::testing::Test
{
protected:
    void SetUp() override
    {
        FAKES(RESET_FAKE)
        FFF_RESET_HISTORY();
        app_button_register_sequence_cb_fake.custom_fake = capture_sequence;
        app_button_register_hold_fake.custom_fake = capture_hold;
        memset(m_hold, 0, sizeof(m_hold));
        m_sequence = NULL;
        network(eNO_NWK, false);
        button_actions_on_startup();
        ASSERT_NE(nullptr, m_sequence);
    }

    static void network(which_nwk_t nwk, bool in_group)
    {
        get_present_network_fake.return_val = nwk;
        device_is_in_a_group_fake.return_val = in_group;
    }

    static void presses(uint8_t t, uint8_t d) { m_sequence(t, d); }
    static void hold(app_button_t g) { m_hold[g].cb(g); }

    static bool only_fail_shown(void)
    {
        return (1u == app_leds_show_fail_fake.call_count) && (0u == app_leds_show_presses_fake.call_count);
    }
};

/* --------------------------------------------------------------- registration */
TEST_F(ButtonActions, RegistersTheNt3Gestures)
{
    EXPECT_EQ(1u, app_button_on_startup_fake.call_count);
    EXPECT_EQ(4000u, m_hold[APP_BUTTON_T].first_ms);
    EXPECT_EQ(2500u, m_hold[APP_BUTTON_T].repeat_ms);
    EXPECT_EQ(4000u, m_hold[APP_BUTTON_D].first_ms);
    EXPECT_EQ(APP_BUTTON_REPEAT_EVERY_POLL, m_hold[APP_BUTTON_D].repeat_ms);
    EXPECT_EQ(4000u, m_hold[APP_BUTTON_BOTH].first_ms);
    EXPECT_TRUE(m_hold[APP_BUTTON_BOTH].rearm);
    ASSERT_NE(nullptr, m_hold[APP_BUTTON_BOTH].enabled);
}

/* --------------------------------------------------------------- both buttons, any network */
TEST_F(ButtonActions, FactoryResetAcceptsTenPlusOrMinusOneOnEachButton)
{
    for(uint8_t t = 8; t <= 12; t++)
    {
        for(uint8_t d = 8; d <= 12; d++)
        {
            RESET_FAKE(force_factory_reset);
            presses(t, d);
            bool expected = (t >= 9) && (t <= 11) && (d >= 9) && (d <= 11);
            EXPECT_EQ(expected ? 1u : 0u, force_factory_reset_fake.call_count) << (int)t << "/" << (int)d;
            if(expected)
            {
                EXPECT_EQ(250u, force_factory_reset_fake.arg0_val);
                EXPECT_EQ(eRESET_NORMAL, force_factory_reset_fake.arg1_val);
            }
        }
    }
}

TEST_F(ButtonActions, FactoryResetIsAllowedOnAnyNetwork)
{
    network(eDAINTREE_NWK, false);
    presses(10, 10);
    EXPECT_EQ(1u, force_factory_reset_fake.call_count);
}

TEST_F(ButtonActions, TwoAndTwoToggles50PercentMode)
{
    network(eDAINTREE_NWK, false);
    presses(2, 2);
    EXPECT_EQ(1u, toggle_50_percent_mode_fake.call_count);
    EXPECT_EQ(2, app_leds_show_presses_fake.arg0_val);
}

TEST_F(ButtonActions, FourAndFourTogglesEmMode)
{
    presses(4, 4);
    EXPECT_EQ(1u, toggle_em_mode_fake.call_count);
    EXPECT_EQ(4, app_leds_show_presses_fake.arg0_val);
}

TEST_F(ButtonActions, SixAndSixStartsLocalAutoCommissionOnlyOnAdaptivWithoutAGroup)
{
    network(eADAPTIV_NWK, false);
    presses(6, 6);
    EXPECT_EQ(1u, start_local_auto_commission_fake.call_count);
    EXPECT_EQ(6, app_leds_show_presses_fake.arg0_val);

    RESET_FAKE(start_local_auto_commission);
    RESET_FAKE(app_leds_show_presses);
    network(eADAPTIV_NWK, true);
    presses(6, 6);
    EXPECT_EQ(0u, start_local_auto_commission_fake.call_count);
    EXPECT_TRUE(only_fail_shown());
}

TEST_F(ButtonActions, EightAndEightShowsTheFirmwareVersion)
{
    network(eDAINTREE_NWK, false);
    presses(8, 8);
    EXPECT_EQ(1u, start_display_fw_version_fake.call_count);
    EXPECT_EQ(0u, app_leds_show_presses_fake.call_count);
}

/* --------------------------------------------------------------- policy */
TEST_F(ButtonActions, SettingsBlockedOnDaintreeUnknownAndAdaptivWithoutAGroup)
{
    for(which_nwk_t nwk : { eDAINTREE_NWK, eUNKNOWN_NWK })
    {
        for(bool group : { false, true })
        {
            RESET_FAKE(app_leds_show_fail); RESET_FAKE(save_adaptiv_unoccupied_delay); RESET_FAKE(save_max_light_level);
            network(nwk, group);
            presses(0, 1);
            presses(1, 0);
            EXPECT_EQ(2u, app_leds_show_fail_fake.call_count);
            EXPECT_EQ(0u, save_adaptiv_unoccupied_delay_fake.call_count);
            EXPECT_EQ(0u, save_max_light_level_fake.call_count);
        }
    }
    RESET_FAKE(app_leds_show_fail);
    network(eADAPTIV_NWK, false);
    presses(0, 1);
    EXPECT_EQ(1u, app_leds_show_fail_fake.call_count);
}

TEST_F(ButtonActions, SettingsAllowedWithoutNetworkStandAloneOrInAnAdaptivGroup)
{
    struct { which_nwk_t nwk; bool group; } allowed[] = { { eNO_NWK, false }, { eSTAND_ALONE, false }, { eADAPTIV_NWK, true } };
    for(auto a : allowed)
    {
        RESET_FAKE(save_adaptiv_unoccupied_delay);
        network(a.nwk, a.group);
        presses(0, 1);
        EXPECT_EQ(1u, save_adaptiv_unoccupied_delay_fake.call_count) << a.nwk;
    }
}

/* --------------------------------------------------------------- occupancy (D presses) */
TEST_F(ButtonActions, OccupancyDelayTable)
{
    struct { uint8_t presses; uint16_t delay; uint16_t to_off; } table[] = {
        { 1, 600, 300 }, { 2, 1200, 300 }, { 3, 1800, 300 }, { 4, 2700, 300 }, { 5, 3600, 300 },
        { 8, 30, 30 }, { 9, 0, 31 }, { 10, 7200, 300 },
    };
    get_unoccupied_delay_fake.return_val = 111;
    get_unoccupied_delay_to_off_fake.return_val = 222;
    for(auto e : table)
    {
        RESET_FAKE(save_adaptiv_unoccupied_delay); RESET_FAKE(save_adaptiv_unoccupied_delay_to_off);
        RESET_FAKE(tx_occ_sensor_new_delays); RESET_FAKE(app_leds_show_presses);
        presses(0, e.presses);
        EXPECT_EQ(e.delay, save_adaptiv_unoccupied_delay_fake.arg0_val) << (int)e.presses;
        EXPECT_EQ(e.to_off, save_adaptiv_unoccupied_delay_to_off_fake.arg0_val) << (int)e.presses;
        EXPECT_EQ(1u, tx_occ_sensor_new_delays_fake.call_count);
        EXPECT_EQ(111, tx_occ_sensor_new_delays_fake.arg0_val);
        EXPECT_EQ(222, tx_occ_sensor_new_delays_fake.arg1_val);
        EXPECT_EQ(e.presses, app_leds_show_presses_fake.arg0_val);
    }
    EXPECT_EQ(0u, set_occupancy_threshold_fake.call_count);
}

TEST_F(ButtonActions, ElevenAndTwelveSetOnlyTheOccupancyThreshold)
{
    presses(0, 11);
    presses(0, 12);
    ASSERT_EQ(2u, set_occupancy_threshold_fake.call_count);
    EXPECT_EQ(DEFAULT_OCC_SENSOR_THRESHOLD, set_occupancy_threshold_fake.arg0_history[0]);
    EXPECT_EQ(DEFAULT_OCC_SENSOR_THRESHOLD_LOW, set_occupancy_threshold_fake.arg0_history[1]);
    EXPECT_EQ(0u, save_adaptiv_unoccupied_delay_fake.call_count);
    EXPECT_EQ(2u, tx_occ_sensor_new_delays_fake.call_count);   // still sent to the group
    EXPECT_EQ(12, app_leds_show_presses_fake.arg0_val);
}

TEST_F(ButtonActions, SixAndSevenDPressesAreRejected)
{
    for(uint8_t n : { 6, 7 })
    {
        RESET_FAKE(app_leds_show_fail); RESET_FAKE(app_leds_show_presses);
        presses(0, n);
        EXPECT_TRUE(only_fail_shown()) << (int)n;
    }
    EXPECT_EQ(0u, tx_occ_sensor_new_delays_fake.call_count);
    EXPECT_EQ(2u, sl_zigbee_core_debug_println_fake.call_count);   // "Occ button press %d"
}

/* --------------------------------------------------------------- light levels (T presses) */
TEST_F(ButtonActions, LightLevelTable)
{
    struct { uint8_t presses, max, mid; } table[] = {
        { 1, 255, 51 }, { 2, 204, 51 }, { 3, 153, 51 }, { 4, 102, 51 }, { 5, 51, 25 }, { 6, 25, 25 },
    };
    for(auto e : table)
    {
        RESET_FAKE(save_max_light_level); RESET_FAKE(save_mid_light_level); RESET_FAKE(save_min_light_level);
        RESET_FAKE(tx_new_light_levels); RESET_FAKE(app_leds_show_presses);
        presses(e.presses, 0);
        EXPECT_EQ(e.max, save_max_light_level_fake.arg0_val) << (int)e.presses;
        EXPECT_EQ(e.mid, save_mid_light_level_fake.arg0_val) << (int)e.presses;
        EXPECT_EQ(0, save_min_light_level_fake.arg0_val);
        EXPECT_EQ(1u, tx_new_light_levels_fake.call_count);
        EXPECT_EQ(e.presses, app_leds_show_presses_fake.arg0_val);
    }
}

TEST_F(ButtonActions, OtherCombinationsAreRejected)
{
    uint8_t combos[][2] = { { 0, 0 }, { 1, 1 }, { 7, 0 }, { 0, 13 }, { 3, 2 }, { 2, 4 } };
    for(auto c : combos)
    {
        RESET_FAKE(app_leds_show_fail); RESET_FAKE(app_leds_show_presses);
        presses(c[0], c[1]);
        EXPECT_TRUE(only_fail_shown()) << (int)c[0] << "/" << (int)c[1];
    }
}

/* --------------------------------------------------------------- holds */
TEST_F(ButtonActions, HoldTStepsColorAndWraps)
{
    network(eNO_NWK, false);
    uint16_t colors[][2] = { { 3000, 3500 }, { 4500, 5000 }, { 5000, 3000 } };
    for(auto c : colors)
    {
        RESET_FAKE(save_light_color); RESET_FAKE(tx_color_new_temp);
        get_light_color_fake.return_val = c[0];
        hold(APP_BUTTON_T);
        EXPECT_EQ(c[1], save_light_color_fake.arg0_val);
        EXPECT_EQ(c[1], tx_color_new_temp_fake.arg0_val);
    }
}

TEST_F(ButtonActions, ColorOnlyChangesInAGroupOrWithoutNetwork)
{
    get_light_color_fake.return_val = 4000;
    network(eADAPTIV_NWK, false);
    hold(APP_BUTTON_T);
    network(eDAINTREE_NWK, false);
    hold(APP_BUTTON_T);
    EXPECT_EQ(0u, save_light_color_fake.call_count);
    network(eDAINTREE_NWK, true);
    hold(APP_BUTTON_T);
    EXPECT_EQ(1u, save_light_color_fake.call_count);
}

TEST_F(ButtonActions, HoldDDimsDownThenBackToMax)
{
    get_max_light_level_fake.return_val = 230;
    get_present_light_level_fake.return_val = 100;
    hold(APP_BUTTON_D);
    EXPECT_EQ(94, move_to_level_onoff_fake.arg0_val);     // 100 - 25/4
    EXPECT_EQ(1u, app_leds_show_dim_step_fake.call_count);

    get_present_light_level_fake.return_val = 30;         // 24 < minimum 25
    hold(APP_BUTTON_D);
    EXPECT_EQ(230, move_to_level_onoff_fake.arg0_val);
}

TEST_F(ButtonActions, HoldDBelowSixWrapsAroundLikeTheOriginal)
{
    // uint8_t arithmetic, kept from the original code: 3 - 6 = 253
    get_present_light_level_fake.return_val = 3;
    hold(APP_BUTTON_D);
    EXPECT_EQ(253, move_to_level_onoff_fake.arg0_val);
}

TEST_F(ButtonActions, HoldBothStartsManualAddOnlyOnAdaptivWithoutAGroup)
{
    network(eADAPTIV_NWK, false);
    EXPECT_TRUE(m_hold[APP_BUTTON_BOTH].enabled());
    hold(APP_BUTTON_BOTH);
    EXPECT_EQ(1u, place_in_manual_add_into_group_mode_fake.call_count);

    network(eADAPTIV_NWK, true);
    EXPECT_FALSE(m_hold[APP_BUTTON_BOTH].enabled());
    network(eNO_NWK, false);
    EXPECT_FALSE(m_hold[APP_BUTTON_BOTH].enabled());
}
