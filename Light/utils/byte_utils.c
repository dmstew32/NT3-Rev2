#include <assert.h>
#include <string.h>
#include "byte_utils.h"


bool bytes_parse_u8(bytes_parser_t *p_parser, uint8_t *p_u8_out)
{
    if((p_parser->index + sizeof(uint8_t)) <= p_parser->length)
    {
        *p_u8_out = p_parser->p_bytes_in[p_parser->index];
        p_parser->index += sizeof(uint8_t);
        return true;
    }
    return false;
}

bool bytes_parse_u16_le(bytes_parser_t *p_parser, uint16_t *p_u16_out)
{
    if((p_parser->index + sizeof(uint16_t)) <= p_parser->length)
    {
        *p_u16_out = BYTES_LE_TO_U16(p_parser->p_bytes_in[p_parser->index], p_parser->p_bytes_in[p_parser->index+1]);
        p_parser->index += sizeof(uint16_t);
        return true;
    }
    return false;
}

bool bytes_parse_u32_le(bytes_parser_t *p_parser, uint32_t *p_u32_out)
{
    if((p_parser->index + sizeof(uint32_t)) <= p_parser->length)
    {
        *p_u32_out = BYTES_LE_TO_U32(p_parser->p_bytes_in[p_parser->index], \
                                     p_parser->p_bytes_in[p_parser->index+1], \
                                     p_parser->p_bytes_in[p_parser->index+2], \
                                     p_parser->p_bytes_in[p_parser->index+3]);
        p_parser->index += sizeof(uint32_t);
        return true;
    }
    return false;
}

bool bytes_parse_array(bytes_parser_t *p_parser, uint8_t *p_array_out, uint8_t size)
{
    if((p_parser->index + size) <= p_parser->length)
    {
        memcpy(p_array_out, &p_parser->p_bytes_in[p_parser->index], size);
        p_parser->index += size;
        return true;
    }
    return false;
}


bool bytes_pack_u8(bytes_packer_t *p_packer, uint8_t data)
{
    if((p_packer->index + sizeof(uint8_t)) <= p_packer->length)
    {
        p_packer->p_bytes_out[p_packer->index] = data;
        p_packer->index += sizeof(uint8_t);
        return true;
    }
    return false;
}

bool bytes_pack_u16_le(bytes_packer_t *p_packer, uint16_t data)
{
    if((p_packer->index + sizeof(uint16_t)) <= p_packer->length)
    {
        p_packer->p_bytes_out[p_packer->index] = U16_LSB_TO_U8(data);
        p_packer->p_bytes_out[p_packer->index+1] = U16_MSB_TO_U8(data);
        p_packer->index += sizeof(uint16_t);
        return true;
    }
    return false;
}

bool bytes_pack_u32_le(bytes_packer_t *p_packer, uint32_t data)
{
    if((p_packer->index + sizeof(uint32_t)) <= p_packer->length)
    {
        p_packer->p_bytes_out[p_packer->index] = U32_B0_LSB_TO_U8(data);
        p_packer->p_bytes_out[p_packer->index+1] = U32_B1_TO_U8(data);
        p_packer->p_bytes_out[p_packer->index+2] = U32_B2_TO_U8(data);
        p_packer->p_bytes_out[p_packer->index+3] = U32_B3_MSB_TO_U8(data);
        p_packer->index += sizeof(uint32_t);
        return true;
    }
    return false;
}

bool bytes_pack_array(bytes_packer_t *p_packer, uint8_t *p_data, uint16_t length)
{
    if((p_packer->index + length) <= p_packer->length)
    {
        memcpy(&p_packer->p_bytes_out[p_packer->index], p_data, length);
        p_packer->index += length;
        return true;
    }
    return false;
}

void double_to_float(double dbl, int *p_int_part, int *p_float_part)
{
    assert(p_int_part != NULL);
    assert(p_float_part != NULL);
    *p_int_part = (int)dbl;
    *p_float_part = (int)(dbl * 1000) - (*p_int_part * 1000);
}
