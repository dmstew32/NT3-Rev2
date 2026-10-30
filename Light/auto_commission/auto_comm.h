#ifndef NWK_AUTO_COMMISSION_H_
#define NWK_AUTO_COMMISSION_H_

void check_adaptiv_auto_commission(void);
void adaptiv_group_added(bool addSub);
bool auto_commission_incoming_cmd(sl_zigbee_af_cluster_command_t *cmd);

void place_in_manual_add_into_group_mode(void);


#endif /* NWK_AUTO_COMMISSION_H_ */
