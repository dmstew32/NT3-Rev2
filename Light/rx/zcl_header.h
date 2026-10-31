#ifndef _ZCL_HEADER_H_
#define _ZCL_HEADER_H_

#include <stdint.h>
#include <stdbool.h>

#define ZCL_HEADER_MIN_LENGTH  3

typedef struct
{
    bool cluster_specific;
    bool disable_def_rsp;
    bool direction_to_server;
    bool mfg_specific;
    uint8_t sequence_number;
    uint8_t cmd_id;
    uint16_t mfg_code;
} zcl_header_t;

void zcl_header_force_sequence_number(uint8_t sequence_number);

uint8_t zcl_header_from_data(uint8_t *p_data, uint8_t length,
                             zcl_header_t *p_zcl_header);
uint8_t zcl_header_to_data(uint8_t *p_out_msg, uint8_t out_msg_len,
                           zcl_header_t *p_zcl_header);

#endif  // #ifndef _ZCL_HEADER_H_
