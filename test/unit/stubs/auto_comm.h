// Unit test stub: see nt3_test_stubs.h
#ifndef TEST_STUB_AUTO_COMM_H_
#define TEST_STUB_AUTO_COMM_H_
#include "nt3_test_stubs.h"
#ifdef __cplusplus
extern "C" {
#endif

// Global auto commissioning: occupancy and DH Level Adjust multicasts stop until the
// announced timeout ends (or a global announce with enabled = false).  Not saved.
#define GLOBAL_AC_DEFAULT_TIMEOUT_S  (5U * 60U)   // a timeout of 0
void global_ac_quiet_start(uint16_t timeout_s);
void global_ac_quiet_stop(void);
bool global_ac_quiet_active(void);
#ifdef __cplusplus
}
#endif
#endif
