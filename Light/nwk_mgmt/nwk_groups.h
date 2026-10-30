#ifndef _NWK_GROUPS_H
#define _NWK_GROUPS_H

typedef struct
{
  uint16_t address;
  uint8_t endPoint;
  sl_zigbee_binding_type_t bindingType;
} adaptiv_bind_info_t;

typedef struct
{
  uint16_t group_address;
  uint8_t dst_ep;
} device_binding_info_t;

uint8_t get_number_of_multicast_bindings(void);
bool device_is_in_a_group(void);
void get_multicast_info(uint8_t index, device_binding_info_t *info);
uint8_t update_bindingTable_address(void);

#endif // _NWK_GROUPS_H
