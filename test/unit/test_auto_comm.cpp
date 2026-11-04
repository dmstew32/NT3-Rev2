#include <gtest/gtest.h>
#include "fff.h"

/*
 * auto_comm.c: what happens to the light when a group is added.
 *
 * - adaptiv_group_added(true): Adaptiv commissioning put this light in its group
 *   (manually_add_group()).  Commissioning stops and the light goes to max.
 * - adaptiv_group_added_by_command(): an Add Group command was received (e.g. a
 *   Daintree gateway).  Commissioning state is cleared the same way, but the
 *   light level is left alone.
 */

extern "C" {
#include "nt3_test_stubs.h"
void adaptiv_group_added(bool addSub);
void adaptiv_group_added_by_command(void);
}

DEFINE_FFF_GLOBALS

extern "C" {
FAKE_VOID_FUNC_VARARG(sl_zigbee_app_debug_println, const char *, ...);
FAKE_VOID_FUNC(update_bindingTable_address);
FAKE_VOID_FUNC(dim_lights_up_down, uint16_t, uint16_t);
FAKE_VALUE_FUNC(bool, set_app_flags_nonvol, uint32_t, bool);
FAKE_VALUE_FUNC(sl_status_t, sl_token_set_data, uint32_t, uint32_t, void *, uint32_t);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_inactive, sl_zigbee_af_event_t *);
// used elsewhere in auto_comm.c
FAKE_VALUE_FUNC(uint8_t, addTask, test_task_fn_t, taskParm_t, taskParm_t);
FAKE_VALUE_FUNC(uint8_t, removeTask, test_task_fn_t);
FAKE_VALUE_FUNC(bool, are_lights_dimming_up_dowm);
FAKE_VALUE_FUNC(sl_status_t, broadcast_autocommission_announce);
FAKE_VOID_FUNC(get_multicast_info, uint8_t, device_binding_info_t *);
FAKE_VALUE_FUNC(uint8_t, get_number_of_multicast_bindings);
FAKE_VALUE_FUNC(uint32_t, get_random_number, uint32_t, uint32_t);
FAKE_VALUE_FUNC(uint32_t, get_time_ms);
FAKE_VALUE_FUNC(uint8_t, halGetResetInfo);
FAKE_VALUE_FUNC(bool, has_timed_out, uint32_t, uint32_t);
FAKE_VALUE_FUNC(bool, is_app_flags_nonvol_set, uint32_t);
FAKE_VALUE_FUNC(sl_zigbee_af_status_t, manually_add_group, uint16_t);
FAKE_VALUE_FUNC(sl_status_t, sl_token_get_data, uint32_t, uint32_t, void *, uint32_t);
FAKE_VOID_FUNC(sl_zigbee_af_event_init, sl_zigbee_af_event_t *, test_event_handler_t);
FAKE_VOID_FUNC(sl_zigbee_af_event_set_delay_ms, sl_zigbee_af_event_t *, uint32_t);
FAKE_VALUE_FUNC(sl_zigbee_network_status_t, sl_zigbee_af_network_state);
}

static adaptiv_commish_info_t m_saved_commish;
static bool m_token_written;
static sl_status_t save_token(uint32_t token, uint32_t, void *data, uint32_t len)
{
    if(TOKEN_APP_ADAPTIV_COMMISH_INFO == token && sizeof(m_saved_commish) == len)
    {
        memcpy(&m_saved_commish, data, len);
        m_token_written = true;
    }
    return SL_STATUS_OK;
}

class AutoCommGroupAdded : public ::testing::Test
{
protected:
    void SetUp() override
    {
        RESET_FAKE(update_bindingTable_address); RESET_FAKE(dim_lights_up_down);
        RESET_FAKE(set_app_flags_nonvol); RESET_FAKE(sl_token_set_data);
        RESET_FAKE(sl_zigbee_af_event_set_inactive);
        FFF_RESET_HISTORY();
        sl_token_set_data_fake.custom_fake = save_token;
        memset(&m_saved_commish, 0xA5, sizeof(m_saved_commish));
        m_token_written = false;
    }
    // commissioning was stopped: token cleared, event stopped, manual add mode off
    static void expect_commissioning_stopped(void)
    {
        EXPECT_TRUE(m_token_written);
        adaptiv_commish_info_t zero;
        memset(&zero, 0, sizeof(zero));
        EXPECT_EQ(0, memcmp(&zero, &m_saved_commish, sizeof(zero)));
        EXPECT_EQ(1u, sl_zigbee_af_event_set_inactive_fake.call_count);
        ASSERT_EQ(1u, set_app_flags_nonvol_fake.call_count);
        EXPECT_EQ(B_MANUAL_ADD_MODE, set_app_flags_nonvol_fake.arg0_val);
        EXPECT_FALSE(set_app_flags_nonvol_fake.arg1_val);
    }
};

TEST_F(AutoCommGroupAdded, ReceivedAddGroupLeavesTheLightLevelAlone)
{
    adaptiv_group_added_by_command();
    EXPECT_EQ(0u, dim_lights_up_down_fake.call_count);    // no jump to max
    EXPECT_EQ(1u, update_bindingTable_address_fake.call_count);
    expect_commissioning_stopped();
}

TEST_F(AutoCommGroupAdded, AdaptivCommissioningGroupStillGoesToMax)
{
    adaptiv_group_added(true);                         // from manually_add_group()
    ASSERT_EQ(1u, dim_lights_up_down_fake.call_count);
    EXPECT_EQ(0, dim_lights_up_down_fake.arg0_val);    // 0 cycles: stop flashing, go to max
    EXPECT_EQ(0, dim_lights_up_down_fake.arg1_val);
    EXPECT_EQ(1u, update_bindingTable_address_fake.call_count);
    expect_commissioning_stopped();
}

TEST_F(AutoCommGroupAdded, RemovedGroupOnlyUpdatesTheBindings)
{
    adaptiv_group_added(false);
    EXPECT_EQ(0u, dim_lights_up_down_fake.call_count);
    EXPECT_EQ(1u, update_bindingTable_address_fake.call_count);
    EXPECT_FALSE(m_token_written);
}
