#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include "byte_utils.h"
#include "network_defines.h"
#include "zcl_header.h"

#define _MIN_ZCL_HEADER_SZ  3

static uint8_t m_zcl_tsn = 0;

static struct
{
    uint8_t transaction_seq_number;
    bool valid;
} m_next_zcl_sequence_number = {0};

void zcl_header_force_sequence_number(uint8_t sequence_number)
{
    // this sets the transaction sequence number of the next packet
    // definitely a HACK, but will call this good for now.
    m_next_zcl_sequence_number.transaction_seq_number = sequence_number;
    m_next_zcl_sequence_number.valid = true;
}

uint8_t zcl_header_from_data(uint8_t *p_msg, uint8_t length,
                             zcl_header_t *p_zcl_header)
{
    assert(p_msg != NULL);
    assert(p_zcl_header != NULL);
    if(length < _MIN_ZCL_HEADER_SZ)
    {
        return 0;
    }

    bytes_parser_t parser = BYTES_PARSER(p_msg, length);
    uint8_t frame_control = 0;
    bytes_parse_u8(&parser, &frame_control);

    p_zcl_header->cluster_specific = ((frame_control & 0x01) > 0);
    p_zcl_header->mfg_specific = ((frame_control & 0x04) > 0);
    p_zcl_header->direction_to_server = !(frame_control & 0x08);
    p_zcl_header->disable_def_rsp = ((frame_control & 0x10) > 0);
    if(p_zcl_header->mfg_specific == true)
    {
        bytes_parse_u16_le(&parser, &p_zcl_header->mfg_code);
    }
    else
    {
        p_zcl_header->mfg_code = INVALID_MFG_CODE;
    }
    bytes_parse_u8(&parser, &p_zcl_header->sequence_number);
    bool parsed = bytes_parse_u8(&parser, &p_zcl_header->cmd_id);
    return (parsed == true) ? parser.index : 0;
}

uint8_t zcl_header_to_data(uint8_t *p_out_msg, uint8_t out_msg_len,
                           zcl_header_t *p_zcl_header)
{
    assert(p_zcl_header != NULL);
    assert(p_out_msg != NULL);

    uint8_t frctrl = (p_zcl_header->cluster_specific == true) ? (1 << 0) : 0x00;
    frctrl |= (p_zcl_header->mfg_specific == true) ? (1 << 2) : 0;
    frctrl |= (p_zcl_header->direction_to_server == true) ? 0 : (1 << 3);
    frctrl |= (p_zcl_header->disable_def_rsp == true) ? (1 << 4) : 0;

    bytes_packer_t packer = BYTES_PACKER(p_out_msg, out_msg_len);
    bytes_pack_u8(&packer, frctrl);
    if(p_zcl_header->mfg_specific == true)
    {
        bytes_pack_u16_le(&packer, p_zcl_header->mfg_code);
    }
    uint8_t sequence_number = m_zcl_tsn;
    if(m_next_zcl_sequence_number.valid == true)
    {
        sequence_number = m_next_zcl_sequence_number.transaction_seq_number;
        m_next_zcl_sequence_number.valid = false;   // clear the valid flag
    }
    else
    {
        m_zcl_tsn++;
    }
    bytes_pack_u8(&packer, sequence_number);
    bool packed = bytes_pack_u8(&packer, p_zcl_header->cmd_id);
    return (packed == true) ? packer.index : 0;
}
