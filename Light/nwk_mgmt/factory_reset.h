#ifndef NWK_MGMT_FACTORY_RESET_H_
#define NWK_MGMT_FACTORY_RESET_H_

typedef enum
{
  eRESET_NORMAL = 0,
  eRESET_DAINTREE = 2,
} reset_reason_t;

void force_factory_reset(uint32_t delay_ms, reset_reason_t reason);
void nwk_down_clean_up(void);

void hal_watchdog_init(bool force_reset);
void pet_the_dog(void);

void inc_reset_counter(void);
void write_firmware_version(void);
void write_date_version(void);

uint8_t inc_on_off_counter(void);
void reset_on_off_counter(void);
uint8_t get_on_off_counter(void);

void set_light_configurations_to_default(reset_reason_t default_reason);


#endif /* NWK_MGMT_FACTORY_RESET_H_ */
