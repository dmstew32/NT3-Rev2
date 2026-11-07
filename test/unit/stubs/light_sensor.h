// Unit test stub: see nt3_test_stubs.h
#ifndef TEST_STUB_LIGHT_SENSOR_H_
#define TEST_STUB_LIGHT_SENSOR_H_
#include "nt3_test_stubs.h"
#include "hal_light_sensor.h"
#ifdef __cplusplus
extern "C" {
#endif
void execute_light_sensor_logic(light_sensor_info_t *light_sensor_info);
void dh_set_master(bool enable);
void dh_disable(void);
void dh_enable(void);
void light_sensor_on_reading(uint16_t reading);
#ifdef __cplusplus
}
#endif
#endif
