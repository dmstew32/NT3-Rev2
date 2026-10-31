#ifndef HAL_OCC_SENSOR_H_
#define HAL_OCC_SENSOR_H_


#define OCC_SENSOR_PORT   gpioPortB
#define OCC_SENSOR_PIN    0

#define OCC_SAMPLE_PERIOD_MS (75UL)
#define OCC_START_PULSE_US   (120UL)  /* Spec 110us - 200uS */

// Count for reading temperature, every 5 seconds
#define OCC_READ_TEMPERATURE_N (uint8_t)(5000UL/OCC_SAMPLE_PERIOD_MS)

#define N_OCC_ARRAY    16      /* Must be 2^N */
#define SH_OCC_ARRAY   (4)     /* Divide */
#define M_OCC_ARRAY    (0x0F)  /* Mask for index */


#define OCC_BAD_LEVEL_LOW    (0x01FF)
#define OCC_BAD_LEVEL_HIGH   (0x3E01)


typedef struct
{
  uint32_t time_occ_changed;
  uint16_t pir_data[N_OCC_ARRAY];
  uint16_t pir_temperature;
  uint16_t delta_pir_data;
  uint16_t running_average;
  uint16_t present_pir_data;
  uint16_t occ_threshold;
  uint8_t index;

  uint8_t b_occ_detected     : 1;
  uint8_t b_last_occ_detected : 1;
  uint8_t b_bad_occ_sensor   : 1;
  uint8_t b_long_delay       : 1;
  uint8_t b_force_occ_event  : 1;
  uint8_t unused             : 2;
} occ_info_t;


void hal_init_occ_sensor(void);
void hal_start_occ_sensor(void);
void hal_stop_occ_sensor(void);
void hal_get_occ_sensor_info(occ_info_t *occ_information);
void hal_set_occ_sensor_info(occ_info_t *occ_information);


void hal_reset_occ_sensor_state(void);
void hal_set_long_delay_bit(uint8_t new_long_delay);

void save_adaptiv_unoccupied_delay(uint16_t new_unoccupied_delay);
void save_adaptiv_unoccupied_delay_to_off(uint16_t new_unoccupied_delay_to_off);
uint16_t get_unoccupied_delay(void);
uint16_t get_unoccupied_delay_to_off(void);

void restore_unoccupied_delay(void);
void force_an_occupied_event(void);

void set_occupancy_threshold(uint16_t new_threshold);
uint16_t get_occupancy_threshold(void);

#endif /* HAL_OCC_SENSOR_H_ */
