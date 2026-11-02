// Unit test stub: see nt3_test_stubs.h
#ifndef TEST_STUB_SL_I2CSPM_H_
#define TEST_STUB_SL_I2CSPM_H_
#include "nt3_test_stubs.h"
#include "em_i2c.h"
#ifdef __cplusplus
extern "C" {
#endif
I2C_TransferReturn_TypeDef I2CSPM_Transfer(I2C_TypeDef *i2c, I2C_TransferSeq_TypeDef *seq);
#ifdef __cplusplus
}
#endif
#endif
