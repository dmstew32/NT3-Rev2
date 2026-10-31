#include "af.h"
// SDK 2025.6.3: hal.h no longer pulls in the legacy token API, which
// provides the TOKEN_APP_* ids and types from sl_custom_token_header.h.
#include "sl_token_api.h"

#include "hal_utils.h"
#include "hal_leds.h"
#include "RehTOS.h"
#include "sl_event_handler.h"
#include "sl_token_manager.h"
#include "attribute-storage.h"
#include "nwk_utils.h"
#include "nwk_groups.h"
#include "factory_reset.h"
#include "light_on_off.h"

#include "sl_zigbee_debug_print.h"
#include "occ_sensor.h"     // needed by nwk_multicast.h
#include "nwk_multicast.h"

extern sl_zigbee_af_status_t manually_add_group(uint16_t new_group);

#define PRINT_AUTO

static uint8_t auto_tx_count = 0;
//static uint32_t auto_start_time = 0UL;

static adaptiv_commish_info_t get_commish_nv_data(void);

extern uint16_t get_unoccupied_delay(void);    // TEMP time out for manual mode

#ifdef PRINT_AUTO
void spew_adaptiv_mfg_cmd(sl_zigbee_af_cluster_command_t *cmd)
{
  uint8_t i = cmd->payloadStartIndex;

  sl_zigbee_app_debug_println("<Adaptive Mfg Specific Command>");
  sl_zigbee_app_debug_println("  Cluster:%04X cmd:0x%X data:[0x%02X %02X %02X %02X %02X]",
    cmd->apsFrame->clusterId, cmd->commandId, cmd->buffer[i], cmd->buffer[i+1],
    cmd->buffer[i+2], cmd->buffer[i+3],cmd->buffer[i+4]);
  sl_zigbee_app_debug_println("  srcId:0x%04X src-ep:%d %s", cmd->source, cmd->apsFrame->sourceEndpoint,
         (cmd->type==SL_ZIGBEE_INCOMING_UNICAST) ? "Unicast" : "Broadcast");
  sl_zigbee_app_debug_println("  Manual Mode is %s", is_app_flags_nonvol_set(B_MANUAL_ADD_MODE) ? "ON" : "OFF");
}
#else
void spew_adaptiv_mfg_cmd(sl_zigbee_af_cluster_command_t *cmd) { (void)cmd; }
#endif

/***********************************************************************/
/***********************    AUTO COMMISSIONING   ***********************/
/***********************************************************************/

#define MAX_RANDOM_AUTO_TIME  3000UL
#define MIN_RANDOM_AUTO_TIME  1000UL
static sl_zigbee_af_event_t auto_commission_event;

static uint32_t get_random_commish_delay(void)
{
  return(get_random_number(MAX_RANDOM_AUTO_TIME, MIN_RANDOM_AUTO_TIME));
}

static adaptiv_commish_info_t get_commish_nv_data(void)
{
  adaptiv_commish_info_t adaptiv_commish_info;
  sl_token_get_data(TOKEN_APP_ADAPTIV_COMMISH_INFO, 0,
                    &adaptiv_commish_info, sizeof(adaptiv_commish_info_t));
  return(adaptiv_commish_info);
}

static void stop_auto_commission(bool go_max_level)
{
  adaptiv_commish_info_t adaptiv_commish_info = {0};

  memset(&adaptiv_commish_info, 0, sizeof(adaptiv_commish_info));
  sl_token_set_data(TOKEN_APP_ADAPTIV_COMMISH_INFO, 0, &adaptiv_commish_info,
                                             sizeof(adaptiv_commish_info_t));

  sl_zigbee_af_event_set_inactive(&auto_commission_event);
  auto_tx_count = 0;
  if(go_max_level)
  {
     dim_lights_up_down(0, 0);
  }
  set_app_flags_nonvol(B_MANUAL_ADD_MODE, 0);

  sl_zigbee_app_debug_println("AUTO Commish STOP!!");
}

// Called when a group has been added or removed
void adaptiv_group_added(bool addSub)
{
  sl_zigbee_app_debug_println("NT-3 Group %s", addSub ? "ADDED" : "DELETED");
  update_bindingTable_address();
  if(addSub)
  {
     // Added to a group
     stop_auto_commission(true);
  }
}

// parm0 = time stamp, parm1 = delay
static void manual_add_mode_timeout_task(taskParm_t parm0, taskParm_t parm1)
{
  if(has_timed_out(parm0.u32, parm1.u32))
  {
     stop_auto_commission(true);
     removeTask(manual_add_mode_timeout_task);
     sl_zigbee_app_debug_println("MANUAL Mode timed out   <%d>", get_time_ms());
  }
}

/*
 * Called from nt-3 buttons or twitch cmd
 * Set manual bit, starts flashing light forever, set up time out.
 * Tx back to the coordinator starts after auto commish cmd.
 */
void place_in_manual_add_into_group_mode(void)
{
  taskParm_t arg0, arg1;
  adaptiv_commish_info_t adaptiv_commish_info = get_commish_nv_data();

  if(0 == adaptiv_commish_info.auto_time_out)
  {
     adaptiv_commish_info.auto_time_out = (5UL*60UL*1000UL);
  }
  else
  {
     adaptiv_commish_info.auto_time_out *= 1000UL;
  }

  set_app_flags_nonvol(B_MANUAL_ADD_MODE, 1);
  dim_lights_up_down(0x7FFF, DSECS_HALF_SECS);

  arg0.u32 = get_time_ms();
  arg1.u32 = adaptiv_commish_info.auto_time_out;
  addTask(manual_add_mode_timeout_task, arg0, arg1);
  sl_zigbee_app_debug_println("MANUAL Group Mode Started for %d secs   <%d>",
                                                    arg1.u32/1000, arg0.u32);
}

// After initial delay, start normal commissioning event for manual mode.
static void start_manual_add_group_task(taskParm_t parm0, taskParm_t parm1)
{
  // Start the normal process
  uint32_t initial_delay = get_random_commish_delay();

  if(has_timed_out(parm0.u32, 2000UL))
  {
     sl_zigbee_af_event_set_delay_ms(&auto_commission_event, initial_delay);
     auto_tx_count = 0;
     removeTask(start_manual_add_group_task);
     sl_zigbee_app_debug_println("AUTO Manual Grouping starting in %d   <%d>",
                                                   initial_delay, get_time_ms());
  }
}

// This is initiated after power cycle or delay from manual add, both start this event.
// Will read saved commish info and place this device into the group.
// This is one and done.
static void auto_commission_handler(sl_zigbee_af_event_t *event)
{
  adaptiv_commish_info_t adaptiv_commish_info;

  adaptiv_commish_info = get_commish_nv_data();

  sl_zigbee_app_debug_println("AUTO handler: enable:%d cmd:%d grpId:0x%X   <%d>",
                              adaptiv_commish_info.enable_byte,
                              adaptiv_commish_info.options_byte,
                              adaptiv_commish_info.group_id,
                              get_time_ms());

  if(ZCL_ADAPTIV_COMMISH_CMD_ANNOUNCE_GROUP == adaptiv_commish_info.options_byte)
  {
     // Manual group added will stop auto commish...just join group
     manually_add_group(adaptiv_commish_info.group_id);
     set_app_flags_nonvol(B_MANUAL_ADD_MODE, 0);
  }
  else
  {
     // Auto commissioning not enabled
     sl_zigbee_app_debug_println("AUTO event Check done Joined:%d POR:%d Auto enb:%d",
                    (SL_ZIGBEE_JOINED_NETWORK == sl_zigbee_af_network_state()),
                                                  (0x04 == halGetResetInfo()),
                                 (0x01 == adaptiv_commish_info.enable_byte));
     stop_auto_commission(false);
  }
}

// We are in auto commish mode, waiting for power cycle.  If no power cycle
// this will trip
static void announce_group_time_out_task(taskParm_t parm0, taskParm_t parm1)
{
  if(has_timed_out(parm0.u32, parm1.u32))
  {
     removeTask(announce_group_time_out_task);
     sl_zigbee_app_debug_println("AUTO Group Announce TIMEOUT   <%d>", get_time_ms());
     stop_auto_commission(false);
  }
}

// Called after every reset/power cycle, within app_init().
// If reset from power cycle, start another event, to check if do auto
void check_adaptiv_auto_commission(void)
{
  sl_zigbee_af_event_init(&auto_commission_event, auto_commission_handler);

  if(0x04 == halGetResetInfo())
  {
     uint32_t initial_delay = get_random_commish_delay();

     sl_zigbee_af_event_set_delay_ms(&auto_commission_event, initial_delay);
     auto_tx_count = 0;
     sl_zigbee_app_debug_println("AUTO Got reset from POR check auto in %d   <%d>",
                                                 initial_delay, get_time_ms());
  }
}

/*
 * Called from sl_zigbee_af_pre_command_received_cb().  To get here means:
 * Mfg specific cmd, Adaptiv Mfg id, Client->server,  Adaptiv Auto Commish Cluster
 *
 * NOTE: Moved to new scheme in v11,
 *       Also option_byte will hold cmd op-code
 */
bool auto_commission_incoming_cmd(sl_zigbee_af_cluster_command_t *cmd)
{
  adaptiv_commish_info_t adaptiv_commish_info = {0};
  uint8_t base_idx = cmd->payloadStartIndex;
  bool return_status = false;
  taskParm_t arg0, arg1;
  uint8_t n_resets = 0;

  //spew_adaptiv_mfg_cmd(cmd);

  switch(cmd->commandId)
  {
     case ZCL_ADAPTIV_COMMISH_CMD_ANNOUNCE_GROUP:
     {
        // Auto commish cmd, save info in non-vol, enable_byte set, wait for por
        adaptiv_commish_info.enable_byte = cmd->buffer[base_idx];
        adaptiv_commish_info.options_byte = ZCL_ADAPTIV_COMMISH_CMD_ANNOUNCE_GROUP;
        // NOTE: Union ep = # resets
        n_resets = cmd->buffer[base_idx+2];
        adaptiv_commish_info.session_id = GET_16_FROM_ARRAY(cmd->buffer, base_idx+3);
        adaptiv_commish_info.group_id = GET_16_FROM_ARRAY(cmd->buffer, base_idx+5);
        adaptiv_commish_info.auto_time_out = (uint32_t)GET_16_FROM_ARRAY(cmd->buffer, base_idx+7);
        adaptiv_commish_info.src_ep =  cmd->buffer[base_idx+11];

        //for(uint8_t i = 0; i < 12; i++) { sl_zigbee_app_debug_print("0x%X ", cmd->buffer[base_idx+i]); }
        //sl_zigbee_app_debug_println(" "); //JGR
        // Check if already in this group
        for(uint8_t i = 0; i < get_number_of_multicast_bindings(); i++)
        {
           device_binding_info_t device_binding_info = {0};

           get_multicast_info(i, &device_binding_info);
           if(LIGHT_ENDPOINT != device_binding_info.dst_ep)
           {
              continue;
           }

           if(adaptiv_commish_info.group_id == device_binding_info.group_address)
           {
              sl_zigbee_app_debug_println("AUTO: Already in this group 0x%X DONE   <%d>",
                                  device_binding_info.group_address, get_time_ms());
              return(true);
           }
        }

        // Commit info to non-vol
        sl_token_set_data(TOKEN_APP_ADAPTIV_COMMISH_INFO, 0, &adaptiv_commish_info,
                                                   sizeof(adaptiv_commish_info_t));

        memset(&adaptiv_commish_info, 0, sizeof(adaptiv_commish_info_t));
        sl_token_get_data(TOKEN_APP_ADAPTIV_COMMISH_INFO, 0, &adaptiv_commish_info,
                                                   sizeof(adaptiv_commish_info_t));

        sl_zigbee_app_debug_println("AUTO: enable:%d options:0x%X resets:%d group_id:0x%04X timeout:%d   <%d>",
                                                  adaptiv_commish_info.enable_byte,
                                                  adaptiv_commish_info.options_byte,
                                                  n_resets,
                                                  adaptiv_commish_info.group_id,
                                                  adaptiv_commish_info.auto_time_out,
                                                  get_time_ms());

        // Are we in manual group mode?  If so use group id and we're done.
        if(is_app_flags_nonvol_set(B_MANUAL_ADD_MODE) && (0 != adaptiv_commish_info.enable_byte))
        {
           // Start event after delay
           arg0.u32 = get_time_ms();
           addTask(start_manual_add_group_task, arg0, arg1);
        }
        else
        {
           // Normal auto commish mode, set time out here.  Wait for power
           // cycle, which  calls check_adaptiv_auto_commission(), to start
           // event and clear time out task
           arg0.u32 = get_time_ms();
           arg1.u32 = adaptiv_commish_info.auto_time_out * 1000UL;
           addTask(announce_group_time_out_task, arg0, arg1);
        }
        return_status = true;
        break;
     }

     case ZCL_ADAPTIV_COMMISH_CMD_SET_MANUAL_ADD_MODE:
        adaptiv_commish_info.enable_byte = cmd->buffer[base_idx];
        adaptiv_commish_info.options_byte = ZCL_ADAPTIV_COMMISH_CMD_SET_MANUAL_ADD_MODE;
        adaptiv_commish_info.auto_time_out = (uint32_t)GET_16_FROM_ARRAY(cmd->buffer, base_idx+1);

        sl_token_set_data(TOKEN_APP_ADAPTIV_COMMISH_INFO, 0, &adaptiv_commish_info,
                                                           sizeof(adaptiv_commish_info_t));

        memset(&adaptiv_commish_info, 0, sizeof(adaptiv_commish_info_t));
        sl_token_get_data(TOKEN_APP_ADAPTIV_COMMISH_INFO, 0, &adaptiv_commish_info,
                                                    sizeof(adaptiv_commish_info_t));

        sl_zigbee_app_debug_println("AUTO: Manual Add enable:%d options:0x%X timeout:%d   <%d>",
                                                          adaptiv_commish_info.enable_byte,
                                                          adaptiv_commish_info.options_byte,
                                                          adaptiv_commish_info.auto_time_out,
                                                          get_time_ms());
        // Start flashing lights, wait for ZCL_ADAPTIV_COMMISH_CMD_ANNOUNCE_GROUP
        place_in_manual_add_into_group_mode();
        return_status = true;
        break;

     case ZCL_ADAPTIV_COMMISH_CMD_ID_FIRMWARE_VER:
        uint16_t seq_num = GET_16_FROM_ARRAY(cmd->buffer, base_idx+0);
        uint16_t mfg_id = GET_16_FROM_ARRAY(cmd->buffer, base_idx+2);
        uint16_t image_type = GET_16_FROM_ARRAY(cmd->buffer, base_idx+4);
        uint32_t fw_ver = GET_32_FROM_ARRAY(cmd->buffer, base_idx+6);
        bool flash_on_match = GET_8_FROM_ARRAY(cmd->buffer, base_idx+10);
        uint16_t flash_secs = GET_16_FROM_ARRAY(cmd->buffer, base_idx+11);

        (void)seq_num;
        (void)mfg_id;
        return_status = true;
        //sl_zigbee_app_debug_println("seq:%d mfg:%X img:%d fw 0x%X flash:%d secs:%d",
        //   seq_num, mfg_id, image_type, fw_ver, flash_on_match,flash_secs  );

        if(2 != image_type)
        {
           break;
        }

        if(!are_lights_dimming_up_dowm())
        {
           if((APP_FIRMWARE_VERSION_U32 == fw_ver) && (0 != flash_on_match))
           {
              // Flash on match
              dim_lights_up_down(flash_secs, DSECS_HALF_SECS);
              sl_zigbee_app_debug_println("FLASH ==");
           }
           else if((APP_FIRMWARE_VERSION_U32 != fw_ver) && (0 == flash_on_match))
           {
              // Flash on mis-match
              dim_lights_up_down(flash_secs, DSECS_HALF_SECS);
              sl_zigbee_app_debug_println("FLASH !=");
           }
           else
           {

           }
        }
        break;

     default:
        sl_zigbee_app_debug_println("AUTO NO WHERE 0x%X", cmd->commandId );
        return_status = false;
        break;
  }
  return(return_status);
}


/***********************  LOCAL AUTO COMMISSION  *********************/
// Selected with the buttons, see Light/buttons/button_actions.c

// Output auto commish announce cmd 3 times at 3 seconds apart
static void bx_auto_announce_task(taskParm_t parm0, taskParm_t parm1)
{
  if(has_timed_out(parm0.u32, 3000UL))
  {
     parm1.u8++;
     if(3 >= parm1.u8)
     {
        taskParm_t arg0, arg1;

        arg0.u32 = get_time_ms();
        arg1.u8 = parm1.u8;
        addTask(bx_auto_announce_task, arg0, arg1);

        broadcast_autocommission_announce();
     }
     else
     {
        // Done
        removeTask(bx_auto_announce_task);
     }
  }
}

void start_local_auto_commission(void)
{
  taskParm_t arg0, arg1;

  // Make sure this light is in manual add mode
  if(!is_app_flags_nonvol_set(B_MANUAL_ADD_MODE))
  {
     place_in_manual_add_into_group_mode();
  }
  arg0.u32 = get_time_ms();
  arg1.u8 = 0;    // Counter
  addTask(bx_auto_announce_task, arg0, arg1);

}
