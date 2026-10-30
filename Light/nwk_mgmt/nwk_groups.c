#include "af.h"
#include "af-main.h"

#include "sl_event_handler.h"
#include "sl_token_manager.h"
#include "attribute-storage.h"
#include "nwk_groups.h"
#include "sl_zigbee_debug_print.h"

#define PRINT_BINDING

static uint8_t _n_multicast_group_addr = 0;
static device_binding_info_t local_binding_table[SL_ZIGBEE_BINDING_TABLE_SIZE] = {0};

bool device_is_in_a_group(void)
{
  return(0 != _n_multicast_group_addr);
}
uint8_t get_number_of_multicast_bindings(void)
{
  return(_n_multicast_group_addr);
}

// Pass in pointer to uint16_t array
void get_multicast_info(uint8_t index, device_binding_info_t *info)
{
  memcpy(info, &local_binding_table[index], sizeof(device_binding_info_t));
}

// Updates number of binding addresses and local table.
// Called at por and when +/- binding entry
uint8_t update_bindingTable_address(void)
{
  sl_zigbee_binding_table_entry_t result;
  _n_multicast_group_addr = 0;
  memset(local_binding_table, 0, sizeof(local_binding_table));

  for (uint8_t i = 0; i < sl_zigbee_af_get_binding_table_size(); i++)
  {
     if(SL_STATUS_OK == sl_zigbee_get_binding(i, &result))
     {
        if(SL_ZIGBEE_UNUSED_BINDING == result.type)
        {
           break;
        }

        #ifdef PRINT_BINDING
        sl_zigbee_app_debug_print("Binding Tbl[%d] is", i);
        switch(result.type)
        {
           default:
           case SL_ZIGBEE_UNUSED_BINDING:
              sl_zigbee_app_debug_println(" UNUSED ");
              sl_zigbee_app_debug_println("");
              break;
           case SL_ZIGBEE_UNICAST_BINDING:
              sl_zigbee_app_debug_print(" UNCAST 64 ");
              break;
           case SL_ZIGBEE_MANY_TO_ONE_BINDING:
              sl_zigbee_app_debug_print(" MTO 64 ");
              break;
           case SL_ZIGBEE_MULTICAST_BINDING:
              sl_zigbee_app_debug_print(" MULTICAST ");
              break;
        }
        sl_zigbee_app_debug_println(" ");
        #endif  // PRINT_BINDING

        // Save multicast address
        if(SL_ZIGBEE_MULTICAST_BINDING == result.type)
        {
            _n_multicast_group_addr++;
           local_binding_table[i].group_address = GET_16_FROM_ARRAY(result.identifier, 0);
           local_binding_table[i].dst_ep = result.local;
           sl_zigbee_app_debug_println("Multicast-Tbl[%d] grp-id:0x%04X  ep:%d  n:%d",
            i, local_binding_table[i].group_address, local_binding_table[i].dst_ep,
            _n_multicast_group_addr);
        }
     }
  }
  return(_n_multicast_group_addr);
}
