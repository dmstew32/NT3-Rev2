#ifndef _BYTE_UTILS_H_
#define _BYTE_UTILS_H_

#include <stdint.h>
#include <stdbool.h>

#define U16_LSB_TO_U8(u16_)  (uint8_t)(u16_ & 0xFF)
#define U16_MSB_TO_U8(u16_)  (uint8_t)(u16_ >> 8)

#define U32_B0_LSB_TO_U8(u32_)  (uint8_t)(u32_ & 0xFF)
#define U32_B1_TO_U8(u32_)  (uint8_t)((u32_ >> 8) & 0xFF)
#define U32_B2_TO_U8(u32_)  (uint8_t)((u32_ >> 16) & 0xFF)
#define U32_B3_MSB_TO_U8(u32_)  (uint8_t)((u32_ >> 24) & 0xFF)

#define BYTES_LE_TO_U32(lsb_, b1_, b2_, msb_)  ((((uint32_t)msb_) << 24) + (((uint32_t)b2_) << 16) + \
                                                (((uint16_t)b1_) << 8) + lsb_)
#define BYTES_LE_TO_U16(lsb_, msb_)  ((((uint32_t)msb_) << 8) +  lsb_)

#define U32_TO_BYTES(u32_, msb_, b2_, b1_, lsb_)  {msb_ = U32_B3_MSB_TO_U8(u32_);  \
                                                   b2_ = U32_B2_TO_U8(u32_); \
                                                   b1_ = U32_B1_TO_U8(u32_); \
                                                   lsb_ = U32_B0_LSB_TO_U8(u32_); }
#define U32_REVERSE(u32_)  (((u32_ >> 24) & 0xFF) + ((u32_ >> 8) & 0xFF00)  + \
                            ((u32_ << 8) & 0xFF0000) + ((u32_ << 24) & 0xFF000000))

typedef struct
{
    uint8_t *p_bytes_in;
    uint8_t length;
    uint8_t index;
} bytes_parser_t;

typedef struct
{
    uint8_t *p_bytes_out;
    uint8_t length;
    uint8_t index;
} bytes_packer_t;

#define BYTES_PARSER(array_, length_) \
        {.p_bytes_in = array_,  .length = length_,  .index = 0}

#define BYTES_PACKER(array_, length_) \
        {.p_bytes_out = array_,  .length = length_,  .index = 0}

bool bytes_parse_u8(bytes_parser_t *p_parser, uint8_t *p_u8_out);
bool bytes_parse_u16_le(bytes_parser_t *p_parser, uint16_t *p_u16_out);
bool bytes_parse_u32_le(bytes_parser_t *p_parser, uint32_t *p_u32_out);
bool bytes_parse_array(bytes_parser_t *p_parser, uint8_t *p_array_out, uint8_t size);

bool bytes_pack_u8(bytes_packer_t *p_packer, uint8_t data);
bool bytes_pack_u16_le(bytes_packer_t *p_packer, uint16_t data);
bool bytes_pack_u32_le(bytes_packer_t *p_packer, uint32_t data);
bool bytes_pack_array(bytes_packer_t *p_packer, uint8_t *p_data, uint16_t length);

void double_to_float(double dbl, int *p_int_part, int *p_float_part);

#endif  // #ifndef _BYTE_UTILS_H_
