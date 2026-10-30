// ============================================================================
//
// Copyright (c) Fabriq Ltd 2015-2016
// All rights reserved
//
// This computer program contains confidential and proprietary information,
// and  may NOT  be reproduced or transmitted, in whole or in part,  in any
// form,  or by any means electronic, mechanical, photo-optical, or  other-
// wise, and  may NOT  be translated  into  another  language  without  the
// express written permission from Fabriq Ltd.
//
// ============================================================================
// =      C O M P A N Y      P R O P R I E T A R Y      M A T E R I A L       =
// ============================================================================
//#ifndef __REHTOS_H__
#define __REHTOS_H__

// Tasker
typedef union {
        int32_t         s32;
        uint32_t        u32;
        int16_t         s16;
        uint16_t        u16;
        int8_t          s8;
        uint8_t         u8;
        int32_t *       s32Ptr;
        uint32_t *      u32Ptr;
        int16_t *       s16Ptr;
        uint16_t *      u16Ptr;
        int8_t *        s8Ptr;
        uint8_t *       u8Ptr;
} taskParm_t;  //!! oh... my ... god. Prepare the lube. LOTS OF LUBE. AN OCEAN OF LUBE!!!!!!!
#define servoTaskParm_t TaskParm_t

// Tasker
#define N_TASKS (8)     /* 2^N */
#define M_TASK  (0x07)

extern void initTasker( void );
extern void taskerTick( void );
extern uint8_t addTask( void (*funcPtr)( ), taskParm_t parm0, taskParm_t parm1 );
extern uint8_t removeTask( void (*funcPtr)( ) );
extern void spewRtosEntries( void );

//#endif  // __REHTOS_H__
