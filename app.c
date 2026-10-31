/***************************************************************************//**
 * @file app.c
 * @brief Callbacks implementation and application specific code.
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#include "app/framework/include/af.h"
#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif

#ifdef SL_CATALOG_ZIGBEE_NETWORK_TEST_PRESENT
#include "network_test_config.h"
#endif //SL_CATALOG_ZIGBEE_NETWORK_TEST_PRESENT

#if (LARGE_NETWORK_TESTING == 0)
#ifndef EZSP_HOST

#include "zigbee_sleep_config.h"
#endif

//#include "network-creator.h"
//#include "network-creator-security.h"
#include "network-steering.h"
#include "find-and-bind-target.h"

#ifdef SL_CATALOG_ZIGBEE_ZLL_COMMISSIONING_COMMON_PRESENT
#include "zll-commissioning.h"
#endif //SL_CATALOG_ZIGBEE_ZLL_COMMISSIONING_COMMON_PRESENT

#include "RehTOS.h"
#include "factory_reset.h"
#include "hal_leds.h"
#include "hal_utils.h"
#include "hal_occ_sensor.h"
#include "nwk_utils.h"
#include "nwk_groups.h"
#include "nwk_commission.h"
#include "adaptiv_config.h"
#include "light_on_off.h"
#include "rx_zcl_handler.h"
#include "rx_zdo_handler.h"
#include "tx_queue.h"

#if defined(SL_CATALOG_LED0_PRESENT)
#include "sl_led.h"
#include "sl_simple_led_instances.h"
#define led_turn_on(led) sl_led_turn_on(led)
#define led_turn_off(led) sl_led_turn_off(led)
#define led_toggle(led) sl_led_toggle(led)
#define COMMISSIONING_STATUS_LED (&sl_led_led0)
#define ON_OFF_LIGHT_LED         (&sl_led_led0)
#else // !SL_CATALOG_LED0_PRESENT
#define led_turn_on(led)
#define led_turn_off(led)
#define led_toggle(led)
#endif // SL_CATALOG_LED0_PRESENT

#define LED_ON_TIME                500UL
#define LED_OFF_TIME               500UL
#define LED_CHECK_PERIOD_MS        (5000UL + LED_ON_TIME + LED_OFF_TIME)

#define ATTEMPT_TO_JOIN_PERIOD_MS  (10UL*1000UL)    // 10 secs
#define ATTEMPT_TO_JOIN_LONG_MS    (120UL*60UL*1000UL)  // 2 hrs

#define ATTEMPT_TO_JOIN_COUNT      (ATTEMPT_TO_JOIN_LONG_MS/ATTEMPT_TO_JOIN_PERIOD_MS)
#define JOINED_PERIOD_MS           (10UL*60UL*1000UL)

#define PRINT_NWK_PARAMS

#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT) && (SL_ZIGBEE_APP_FRAMEWORK_USE_BUTTON_TO_STAY_AWAKE == 0)
#include "sl_simple_button.h"
#include "sl_simple_button_instances.h"
#ifdef SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
#include "force-sleep-wakeup.h"
#endif //SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
#endif // SL_CATALOG_SIMPLE_BUTTON_PRESENT && SL_ZIGBEE_APP_FRAMEWORK_USE_BUTTON_TO_STAY_AWAKE == 0

static sl_zigbee_af_event_t commissioning_event;
static sl_zigbee_af_event_t which_nwk_we_on_event;
static sl_zigbee_af_event_t finding_and_binding_event;
static sl_zigbee_af_event_t red_led_event;

static void spew_nwkParams(sl_zigbee_network_parameters_t *nwkParams);

//---------------
// Event handlers

static void commissioning_event_handler(sl_zigbee_af_event_t *event)
{
  sl_zigbee_network_status_t nwk_state = sl_zigbee_af_network_state();
  (void)event;

  if(SL_ZIGBEE_JOINED_NETWORK == nwk_state)
  {
     sl_zigbee_app_debug_println("commissioning_event_handler JOINED   <%d>", get_time_ms());
     if(nwk_comms_timeout_long_daintree())
     {
        sl_zigbee_app_debug_println("No nwk communication");
        sl_zigbee_af_event_set_active(&which_nwk_we_on_event);
     }

     //sl_zigbee_af_find_ieee_address(0x0000, &adaptiv_disco_request_callback);

     set_app_flags_nonvol(B_SWITCH_NWK, 0);
  }
  else if(!is_app_flags_nonvol_set(B_SWITCH_NWK))
  {
     // NOT JOINED and use normal scan/join
     sl_zigbee_app_debug_println("\ncommissioning_event_handler NOT JOINED.....STARTING JOIN   <%d>", get_time_ms());
     sl_zigbee_af_network_steering_autostart();
     set_present_network(eNO_NWK);
  }
  else if(is_app_flags_nonvol_set(B_SWITCH_NWK))
  {
     // NOT JOINED and use a known nwk...Daintree crap
     bool failed_switch = false;
     sl_status_t status;
     commissioning_set_t m_active_commissioning_set = {0};
     sl_zigbee_app_debug_println("\ncommissioning_event_handler NOT JOINED.....USING COMMISSIONING JOIN   <%d>", get_time_ms());

     /*** Set security state ***/
     sl_zigbee_initial_security_state_t security_state;
     memset(&security_state, 0, sizeof(sl_zigbee_initial_security_state_t));

     security_state.bitmask = (SL_ZIGBEE_JOIN_PRECONFIG_KEY_BITMASK);

     status = commissioning_copy_nonvol_to_set(&m_active_commissioning_set);
     if(status != SL_ZIGBEE_ZCL_STATUS_SUCCESS)
     {
        failed_switch = true;
        sl_zigbee_app_debug_println("\nFAIL: commissioning_copy_nonvol_to_set");
        spew_commissioning_set(&m_active_commissioning_set);
     }

     memcpy(&security_state.preconfiguredKey, &m_active_commissioning_set.preconfig_link_key,
                           sizeof(m_active_commissioning_set.preconfig_link_key));

     status = sl_zigbee_set_initial_security_state(&security_state);
     if(status != SL_ZIGBEE_ZCL_STATUS_SUCCESS)
     {
        failed_switch = true;
        sl_zigbee_app_debug_println("\nFAIL: sl_zigbee_set_initial_security_state 0x%X", status);
        for(uint8_t i=0; i < SL_ZIGBEE_ENCRYPTION_KEY_SIZE; i++) {    //JGR  DEBUG
            sl_zigbee_app_debug_print("0x%X ", (uint8_t)security_state.preconfiguredKey.contents[i]);
        }
        sl_zigbee_app_debug_println(" ");
     }

     /*** Set nwk parameters. Loaded from non-vol; extPAN, PAN, Channel Mask ***/
     sl_zigbee_network_parameters_t nwkParams;
     memset(&nwkParams, 0, sizeof(sl_zigbee_network_parameters_t));
     if(!load_commissioning_nwk_parameters(&nwkParams))
     {
        failed_switch = true;
        sl_zigbee_app_debug_println("\nFAIL: load_commissioning_nwk_parameters");
        spew_nwkParams(&nwkParams);
     }

     // Fill in rest of nwkParams
     nwkParams.radioTxPower = DEFAULT_RADIO_PWR;
     nwkParams.joinMethod = SL_ZIGBEE_USE_NWK_REJOIN;
     nwkParams.nwkManagerId = 0x0000;
     nwkParams.nwkUpdateId = 0;

     // Get channel # from mask
     for(uint8_t channel = SL_ZIGBEE_MIN_802_15_4_CHANNEL_NUMBER;
         channel < SL_ZIGBEE_MAX_802_15_4_CHANNEL_NUMBER;
         channel++)
     {
        if(nwkParams.channels & (1UL << channel))
        {
           nwkParams.radioChannel = channel;
           break;
        }
     }

     sl_zigbee_app_debug_println("JOINING WITH");
     spew_nwkParams(&nwkParams);

     // Don't attempt to join if not ready
     if(!failed_switch)
     {
        status = sl_zigbee_join_network(SL_ZIGBEE_ROUTER, &nwkParams);
     }
     if(SL_ZIGBEE_ZCL_STATUS_SUCCESS != status)
     {
       failed_switch = true;
       sl_zigbee_app_debug_println("\nFAIL: Commission sl_zigbee_join_network 0x%X", status);
       spew_nwkParams(&nwkParams);
     }

     // Continue to try?
     if(failed_switch)
     {
        sl_zigbee_app_debug_println("\nFAIL: Commission switch");
        set_app_flags_nonvol(B_SWITCH_NWK, 0);
        force_factory_reset(1000UL, eRESET_NORMAL);
        return;
     }
  }
  else
  {
     // Where are we?
  }

  // Next attempt to join done in sl_zigbee_af_network_steering_complete_cb()
#if 0
  // Get here in all cases, joined or not joined
  // Determine how often to look for a network
  event_handler_count++;
  if(SL_ZIGBEE_JOINED_NETWORK == nwk_state)
  {
     sl_zigbee_af_event_set_delay_ms(&commissioning_event, JOINED_PERIOD_MS);
     set_app_flags_nonvol(B_SWITCH_NWK, 0);
  }
  else if(ATTEMPT_TO_JOIN_COUNT <= event_handler_count)
  {
     sl_zigbee_af_event_set_delay_ms(&commissioning_event, ATTEMPT_TO_JOIN_LONG_MS);
     event_handler_count = ATTEMPT_TO_JOIN_COUNT;
     set_app_flags_nonvol(B_SWITCH_NWK, 0);
  }
  else
  {
     sl_zigbee_af_event_set_delay_ms(&commissioning_event, ATTEMPT_TO_JOIN_PERIOD_MS);
  }
#endif
}

#ifdef PRINT_NWK_PARAMS
static void spew_nwkParams(sl_zigbee_network_parameters_t *nwkParams)
{
  sl_zigbee_app_debug_print("extPAN[");
  for(uint8_t i=0; i < EXTENDED_PAN_ID_SIZE; i++) {
      sl_zigbee_app_debug_print("0x%02X ", nwkParams->extendedPanId[i]); }
  sl_zigbee_app_debug_println("]");
  sl_zigbee_app_debug_println("PAN 0x%04X", nwkParams->panId);
  sl_zigbee_app_debug_println("Pwr %d dBm", nwkParams->radioTxPower);
  sl_zigbee_app_debug_println("Chan %d", nwkParams->radioChannel);
  sl_zigbee_app_debug_println("Chan mask 0x%08X", nwkParams->channels);
  sl_zigbee_app_debug_println("Join Method 0x%02X", nwkParams->joinMethod);
  return;
}
#else
static void spew_nwkParams(sl_zigbee_network_parameters_t *nwkParams) { (void)nwkParams; }
#endif

static void finding_and_binding_event_handler(sl_zigbee_af_event_t *event)
{
  (void)event;
  if (sl_zigbee_af_network_state() == SL_ZIGBEE_JOINED_NETWORK) {
    sl_zigbee_af_event_set_inactive(&finding_and_binding_event);

    sl_zigbee_app_debug_println("\nFind and bind target start: 0x%02X",
                                sl_zigbee_af_find_and_bind_target_start(LIGHT_ENDPOINT));
  }
}

// Long 2 sec at por if not joined
// 1 quick if joined but not grouped
// 2 quick if not joined
static void red_led_event_handler(sl_zigbee_af_event_t *event)
{
  static bool por_not_joined_red_led_done = false;  // Reset only at por
  sl_zigbee_network_status_t nwk_status = sl_zigbee_af_network_state();

  (void)event;
  if((SL_ZIGBEE_JOINED_NETWORK == nwk_status) && device_is_in_a_group())
  {
     sl_zigbee_af_event_set_delay_ms(&red_led_event, 2UL*LED_CHECK_PERIOD_MS);
     if(is_red_led_free())
     {
        hal_set_red_led(0);
     }
  }
  else if((SL_ZIGBEE_JOINED_NETWORK == nwk_status) && !device_is_in_a_group())
  {
     if(is_red_led_free())
     {
        // Joined, not grouped, 2 blinks
        hal_start_flashing_red_led(2UL, LED_ON_TIME, LED_OFF_TIME);
     }
     sl_zigbee_af_event_set_delay_ms(&red_led_event, LED_CHECK_PERIOD_MS);
  }
  else if(por_not_joined_red_led_done)
  {
     if(is_red_led_free())
     {
        // Not joined and initial long por red led happened, 1 blink
        hal_start_flashing_red_led(1UL, LED_ON_TIME, LED_OFF_TIME);
     }
     sl_zigbee_af_event_set_delay_ms(&red_led_event, LED_CHECK_PERIOD_MS);
  }
  else
  {
     // Not joined and initial long por red led not happened
     por_not_joined_red_led_done = true;
     if(is_red_led_free())
     {
        hal_start_flashing_red_led(1UL, 2000UL, 5UL);
     }
     sl_zigbee_af_event_set_delay_ms(&red_led_event, LED_CHECK_PERIOD_MS + 2000UL);
  }
}

// This is called from sl_zigbee_af_stack_status_cb() when we get on a nwk,
// so to get ieee number.  Also called if no ota comms after tbd time.
void which_nwk_we_on_handler(void)
{
  static bool daintree_no_comms = false;

  switch((uint8_t)get_present_network())
  {
     case eNO_NWK:
     case eSTAND_ALONE:
        // Keep doing this until on a known nwk
        sl_zigbee_af_find_ieee_address(0x0000, &adaptiv_disco_request_callback);
        sl_zigbee_af_event_set_delay_ms(&which_nwk_we_on_event, DISCO_POLL_TIME);
        set_app_flags_nonvol(B_SWITCH_NWK, 0);
        break;
     case eADAPTIV_NWK:
     case eUNKNOWN_NWK:
        set_app_flags_nonvol(B_SWITCH_NWK, 0);
        break;
     case eDAINTREE_NWK:
        if(nwk_comms_timeout_long_daintree())
        {
          // No radio activity for long time??
        }
        else if(nwk_comms_timeout_short_daintree())
        {
           if(false == daintree_no_comms)
           {
              move_to_level_onoff(get_max_light_level(), 10);
              daintree_no_comms = true;
           }
           sl_zigbee_af_event_set_delay_ms(&which_nwk_we_on_event, DISCO_POLL_TIME_DAINTREE);
        }
        else
        {
           daintree_no_comms = false;
           sl_zigbee_af_event_set_delay_ms(&which_nwk_we_on_event, DISCO_POLL_TIME_DAINTREE);
        }
        break;
     default:
        assert(false);
        break;
  }
}

//----------------------
// Implemented Callbacks

/** @brief Stack Status
 *
 * This function is called by the application framework from the stack status
 * handler.  This callbacks provides applications an opportunity to be notified
 * of changes to the stack status and take appropriate action. The framework
 * will always process the stack status after the callback returns.
 */
void sl_zigbee_af_stack_status_cb(sl_status_t status)
{
  // Note, the ZLL state is automatically updated by the stack and the plugin.
  if (status == SL_STATUS_NETWORK_DOWN)
  {
     sl_zigbee_app_debug_println("sl_zigbee_af_stack_status_cb  SL_STATUS_NETWORK_DOWN");
     // Set up to periodically look for nwk   JGR
     sl_zigbee_af_event_set_inactive(&commissioning_event);
     sl_zigbee_af_event_set_delay_ms(&commissioning_event, get_random_number(8000UL, 300UL));
     sl_zigbee_af_event_set_inactive(&finding_and_binding_event);
     nwk_down_clean_up();
     set_present_network(eNO_NWK);
  }
  else if (status == SL_STATUS_NETWORK_UP)
  {
     sl_zigbee_app_debug_println("sl_zigbee_af_stack_status_cb  SL_STATUS_NETWORK_UP");
     if((eNO_NWK == get_present_network()) || (eSTAND_ALONE == get_present_network()))
     {
        // Just joined a nwk, set to normal defaults
        sl_zigbee_app_debug_println("Setting all light parameters to DEFAULTS");
        set_light_configurations_to_default(eRESET_NORMAL);
     }
     sl_zigbee_af_event_set_active(&finding_and_binding_event);
     // Determine whose nwk we joined
     sl_zigbee_af_event_set_active(&which_nwk_we_on_event);
  }
  else if (status == SL_STATUS_ZIGBEE_NETWORK_OPENED)
  {
     sl_zigbee_app_debug_println("sl_zigbee_af_stack_status_cb  SL_STATUS_NETWORK_OPEN");
  }
  else
  {
     sl_zigbee_app_debug_println("sl_zigbee_af_stack_status_cb STATUS 0x%X", status);
  }
}

/** @brief Init
 * Application init function
 * This is called from the main(), within sl_main_init().  It is not in an isr
 * but none of the app init's have been called.
 *
 */
void sl_zigbee_af_main_init_cb(void)
{
  sl_zigbee_app_debug_println("\n\n\nsl_zigbee_af_main_init_cb   <%d>", get_time_ms());

  sl_zigbee_af_isr_event_init(&finding_and_binding_event, finding_and_binding_event_handler);

#if 1 // TEST STANDALONE
  // Start the commissioning LED event after network is up, and communicated to app framework task in the main loop
  sl_zigbee_af_event_init(&commissioning_event, commissioning_event_handler);
  sl_zigbee_af_event_set_delay_ms(&commissioning_event, ATTEMPT_TO_JOIN_PERIOD_MS);

  // Init only here, activated when up/joined
  sl_zigbee_af_event_init(&which_nwk_we_on_event, which_nwk_we_on_handler);
#endif
  sl_zigbee_af_event_init(&red_led_event, red_led_event_handler);
  sl_zigbee_af_event_set_delay_ms(&red_led_event, 750UL);

  // tx / rx management modules (Light/tx, Light/rx)
  rx_zcl_handler_on_startup();
  rx_zdo_on_startup();
  tx_queue_on_startup();
}

/** @brief Mark buffers
 * Called by the stack during buffer cleanup.  Messages waiting in the tx queue
 * hold stack buffers that must be marked here or the stack reclaims them.
 */
void sl_zigbee_af_mark_buffers_cb(void)
{
  tx_queue_on_mark_buffers_callback();
}

/** @brief Incoming message
 * Called by the framework after it has processed the message (ZCL handling,
 * sl_zigbee_af_pre_command_received_cb(), ...), so this only adds to the existing
 * processing.  Hands a copy of each message to the rx modules, which notify
 * any registered callbacks.
 */
void sl_zigbee_af_incoming_message_cb(sl_zigbee_incoming_message_type_t type,
                                      sl_zigbee_aps_frame_t *apsFrame,
                                      sl_zigbee_rx_packet_info_t *packetInfo,
                                      uint8_t messageLength,
                                      uint8_t *message)
{
  (void)type;

  if ((apsFrame == NULL) || (packetInfo == NULL) || (message == NULL)) {
    return;
  }
  uint16_t sender = packetInfo->sender_short_id;
  if (sender == sl_zigbee_get_node_id()) {
    return;
  }

  if ((apsFrame->profileId == SL_ZIGBEE_ZDO_PROFILE_ID)
      && (apsFrame->destinationEndpoint == SL_ZIGBEE_ZDO_ENDPOINT)) {
    rx_zdo_rx_handler(sender, apsFrame, message, messageLength);
  } else {
    rx_zcl_handler_on_rx(sender, apsFrame, message, messageLength);
  }
}

/** @brief Complete network steering.
 *
 * This callback is fired when the Network Steering plugin is complete.
 *
 * @param status On success this will be set to SL_STATUS_OK to indicate a
 * network was joined successfully. On failure this will be the status code of
 * the last join or scan attempt.
 *
 * @param totalBeacons The total number of 802.15.4 beacons that were heard,
 * including beacons from different devices with the same PAN ID.
 *
 * @param joinAttempts The number of join attempts that were made to get onto
 * an open Zigbee network.
 *
 * @param finalState The finishing state of the network steering process. From
 * this, one is able to tell on which channel mask and with which key the
 * process was complete.
 */
void sl_zigbee_af_network_steering_complete_cb(sl_status_t status,
                                               uint8_t totalBeacons,
                                               uint8_t joinAttempts,
                                               uint8_t finalState)
{
  (void)totalBeacons;
  (void)joinAttempts;
  (void)finalState;
  uint32_t next_time = get_random_number(8000UL, 300UL);

  if(true)
  {
     char error[16] = {0};
     switch((uint32_t)status)
     {
        case SL_STATUS_OK:
           sprintf(error, "JOIN GOOD");
           next_time = JOINED_PERIOD_MS;
           set_app_flags_nonvol(B_SWITCH_NWK, 0);
           break;
        case SL_STATUS_NETWORK_UP:
           sprintf(error, "NETWORK_UP");
           next_time = JOINED_PERIOD_MS;
           set_app_flags_nonvol(B_SWITCH_NWK, 0);
           break;
        case SL_STATUS_NETWORK_DOWN:
           sprintf(error, "NWK_DOWN");
           break;
        case SL_STATUS_NOT_JOINED:
           sprintf(error, "NOT JOINED");
           break;
        case SL_STATUS_NO_BEACONS:
           sprintf(error, "NO BEACON");
           break;
        default:
           sprintf(error, "UNKNOWN");
           break;
     }
     sl_zigbee_app_debug_println("sl_zigbee_af_network_steering_complete_cb: %s  [0x%02X]   <%d>", error, status, get_time_ms() );
  }
  //sl_zigbee_app_debug_println("Delay %d", next_time);

 sl_zigbee_af_event_set_delay_ms(&commissioning_event, next_time);

}

/** @brief Complete the network creation process.
 *
 * This callback notifies the user that the network creation process has
 * completed successfully.
 *
 * @param network The network that the network creator plugin successfully
 * formed.
 *
 * @param usedSecondaryChannels Whether or not the network creator wants to
 * form a network on the secondary channels.
 */
void sl_zigbee_af_network_creator_complete_cb(const sl_zigbee_network_parameters_t *network,
                                              bool usedSecondaryChannels)
{
  (void)network;
  (void)usedSecondaryChannels;

  sl_zigbee_app_debug_println("Form Network Complete: 0x%02X",
                              SL_STATUS_OK);
}

/** @brief Post Attribute Change
 *
 * This function is called by the application framework after it changes an
 * attribute value. The value passed into this callback is the value to which
 * the attribute was set by the framework.
 */
void sl_zigbee_af_post_attribute_change_cb(uint8_t endpoint,
                                           sl_zigbee_af_cluster_id_t clusterId,
                                           sl_zigbee_af_attribute_id_t attributeId,
                                           uint8_t mask,
                                           uint16_t manufacturerCode,
                                           uint8_t type,
                                           uint8_t size,
                                           uint8_t* value)
{
  (void)type;
  (void)size;
  (void)value;

     //sl_zigbee_app_debug_println("%d %X %X %x %d %d",endpoint,clusterId,attributeId,manufacturerCode,delay,get_time_ms());

  #if 0
  //if (((clusterId == ZCL_ON_OFF_CLUSTER_ID) || (clusterId == ZCL_LEVEL_CONTROL_CLUSTER_ID))
  if ( (clusterId == ZCL_ON_OFF_CLUSTER_ID)
      && attributeId == ZCL_ON_OFF_ATTRIBUTE_ID /* Both 0 */
      && mask == CLUSTER_MASK_SERVER)
  {
    bool onOff;
    sl_zigbee_app_debug_println("IN cid 0x%04X  attr 0x%X  value %d",clusterId,attributeId,*value);

    if (sl_zigbee_af_read_server_attribute(endpoint,
                                           ZCL_ON_OFF_CLUSTER_ID,
                                           ZCL_ON_OFF_ATTRIBUTE_ID,
                                           (uint8_t *)&onOff,
                                           sizeof(onOff))
        == SL_ZIGBEE_ZCL_STATUS_SUCCESS)
    {
        if(onOff)
        {
           led_turn_on(ON_OFF_LIGHT_LED);
           sl_zigbee_app_debug_println("TURN ON");
        }
        else
        {
           led_turn_off(ON_OFF_LIGHT_LED);
           sl_zigbee_app_debug_println("TURN OFF");
        }
    }
  }
#endif
 // sl_zigbee_app_debug_println("IN cid 0x%04X  attr 0x%X  value %d",clusterId,attributeId,*value);

}

/** @brief On/off Cluster Server Post Init
 *
 * Following resolution of the On/Off state at startup for this endpoint, perform any
 * additional initialization needed; e.g., synchronize hardware state.
 *
 * @param endpoint Endpoint that is being initialized
 */
void sl_zigbee_af_on_off_cluster_server_post_init_cb(uint8_t endpoint)
{
  // At startup, trigger a read of the attribute and possibly a toggle of the
  // LED to make sure they are always in sync.
  sl_zigbee_af_post_attribute_change_cb(endpoint,
                                        ZCL_ON_OFF_CLUSTER_ID,
                                        ZCL_ON_OFF_ATTRIBUTE_ID,
                                        CLUSTER_MASK_SERVER,
                                        0,
                                        0,
                                        0,
                                        NULL);
}

/** @brief
 *
 * Application framework equivalent of ::sl_zigbee_radio_needs_calibrating_handler
 */
void sl_zigbee_af_radio_needs_calibrating_cb(void)
{
  #ifndef EZSP_HOST
  sl_mac_calibrate_current_channel();
  #endif
}

#if defined(SL_CATALOG_SIMPLE_BUTTON_PRESENT) && (SL_ZIGBEE_APP_FRAMEWORK_USE_BUTTON_TO_STAY_AWAKE == 0)
/***************************************************************************//**
 * A callback called in interrupt context whenever a button changes its state.
 *
 * @remark Can be implemented by the application if required. This function
 * can contain the functionality to be executed in response to changes of state
 * in each of the buttons, or callbacks to appropriate functionality.
 *
 * @note The button state should not be updated in this function, it is updated
 * by specific button driver prior to arriving here
 *
   @param[out] handle             Pointer to button instance
 ******************************************************************************/
void sl_button_on_change(const sl_button_t *handle)
{
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_RELEASED) {
    sl_zigbee_af_event_set_active(&finding_and_binding_event);
    #ifdef SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
    sl_zigbee_app_framework_force_wakeup();
    #endif //SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
  }
}
#endif // SL_CATALOG_SIMPLE_BUTTON_PRESENT && SL_ZIGBEE_APP_FRAMEWORK_USE_BUTTON_TO_STAY_AWAKE == 0

//Internal testing stuff
#if defined(SL_ZIGBEE_TEST)
void sl_zigbee_af_hal_button_isr_cb(uint8_t button, uint8_t state)
{
  if (state == BUTTON_RELEASED) {
    sl_zigbee_af_event_set_active(&finding_and_binding_event);
  }
}
#endif // SL_ZIGBEE_TEST

#ifdef SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
void sli_zigbee_app_framework_force_sleep_callback(void)
{
  // Do other things like turn off LEDs etc
  sl_led_turn_off(&sl_led_led0);
}
#endif // SL_CATALOG_ZIGBEE_FORCE_SLEEP_AND_WAKEUP_PRESENT
#endif //#if (LARGE_NETWORK_TESTING == 0)
