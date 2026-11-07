#ifndef NWK_AUTO_COMMISSION_H_
#define NWK_AUTO_COMMISSION_H_

void check_adaptiv_auto_commission(void);
void adaptiv_group_added(bool addSub);
void adaptiv_group_added_by_command(void);   // received Add Group: no level change
bool auto_commission_incoming_cmd(sl_zigbee_af_cluster_command_t *cmd);

void place_in_manual_add_into_group_mode(void);
void start_local_auto_commission(void);

// Global auto commissioning: occupancy and DH Level Adjust multicasts stop until the
// announced timeout ends (or a global announce with enabled = false).  Not saved.
#define GLOBAL_AC_DEFAULT_TIMEOUT_S  (5U * 60U)   // a timeout of 0
void global_ac_quiet_start(uint16_t timeout_s);
void global_ac_quiet_stop(void);
bool global_ac_quiet_active(void);


#endif /* NWK_AUTO_COMMISSION_H_ */
