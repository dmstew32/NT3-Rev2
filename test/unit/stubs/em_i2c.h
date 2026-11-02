// Unit test stub: see nt3_test_stubs.h
#ifndef TEST_STUB_EM_I2C_H_
#define TEST_STUB_EM_I2C_H_
#include "nt3_test_stubs.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { i2cTransferDone = 0, i2cTransferNack = -1 } I2C_TransferReturn_TypeDef;
typedef struct
{
  uint16_t addr;
  uint16_t flags;
  struct { uint8_t *data; uint16_t len; } buf[2];
} I2C_TransferSeq_TypeDef;
#define I2C_FLAG_WRITE  0x0001
#define I2C_FLAG_READ   0x0002
typedef struct { int unused; } I2C_TypeDef;
extern I2C_TypeDef test_i2c0;
#define I2C0 (&test_i2c0)
#ifdef __cplusplus
}
#endif
#endif
