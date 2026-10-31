#ifndef _SYS_DEFINES_H_
#define _SYS_DEFINES_H_

#include <assert.h>
#include <stddef.h>
#include "af.h"     // MIN()

#define ASSERT(a_)            assert(a_)
#define ASSERT_NOT_NULL(a_)   assert(a_ != NULL)
#define ARRAY_SIZE(arr_)    (sizeof(arr_) / (sizeof(arr_[0])))
//#define MIN(a_,b_)   (a_ < b_) ? a_ : b_
#define MAX(a_,b_)   (a_ > b_) ? a_ : b_

#define MAX_U8  (0xFF)
#define MAX_U16 (0xFFFF)
#define MAX_U32 (0xFFFFFFFF)

#define LARGEST_PACKET_PAYLOAD_SZ 80

#endif // #ifndef _SYS_DEFINES_H_
