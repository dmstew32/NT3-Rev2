// ============================================================================
//
// Copyright (c) Fabriq Ltd 2026-2027
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
/*----------------------------------------------------------------------------
 Include files:
 ---------------------------------------------------------------------------*/
#include "app/framework/include/af.h"
#include "hal_utils.h"

#include "RehTOS.h"

// Local variables
static void (* TaskerStatePtr[N_TASKS] )( taskParm_t parm0, taskParm_t parm1 );
static taskParm_t TaskParm0[N_TASKS];
static taskParm_t TaskParm1[N_TASKS];
static bool tasker_is_initialized = false;

/** @brief Initialize state machine
 *
 * This does as it says, nothing
 */
static void NopTaskerState( taskParm_t arg0, taskParm_t arg1 )
{
    (void)arg0.u32;
    (void)arg1.u32;
}

/** @brief Initialize tasker machine
 *
 * Fill tasker queue with nop's
 *
 */
void initTasker( void )
{
    uint8_t i;

    // Nop out states
    for( i = 0; i < N_TASKS; i++ )
    {
        TaskerStatePtr[i] = NopTaskerState;
        TaskParm0[i].u32 = 0UL;
        TaskParm1[i].u32 = 0UL;
    }
    tasker_is_initialized = true;
}

/** @brief Round robin application state machine
 *
 * This is called from the main while loop.  It presently will execute
 * one task per call in a sequential/round-robin manner.
 * The tasker supports N_TASKS 'threads'. A state/thread will
 * continuously be called until removeTask() terminates it.  To add a
 * state/thread call addTask().  The tasks are user defined and will
 * accept 2 arguments of any type.
 * TODO: Optimize by not calling NOP states.
 * jgr
 */
void taskerTick( void )
{
  static uint8_t idx = 0;

  uint8_t i = 0;
  CORE_ATOMIC_SECTION(
     for(i = 0; i < N_TASKS; i++)  // Search for next active task
     {
        if(TaskerStatePtr[idx] != NopTaskerState)
        {
            break;
        }
        idx++;
        idx &= M_TASK;
     }
  )
  (*TaskerStatePtr[idx])(TaskParm0[idx], TaskParm1[idx]);
  idx++;
  idx &= M_TASK;
}
/** @brief Will place on tasker a state/task
 *
 * Support functions for the  application state machines.
 * These functions will add/delete to the N_TASKS deep function
 * pointers.  This acts as starting/deleting a thread.  If no
 * functions are in the queue then they must be populated with
 * the NopAppState.
 *
 */
uint8_t addTask( void (*funcPtr)( ), taskParm_t parm0, taskParm_t parm1 )
{
  uint8_t i = 0;

  removeTask(funcPtr);
  if(false == tasker_is_initialized)
  {
     return(0xFF);
  }

  CORE_ATOMIC_SECTION(
     for(i = 0; i < N_TASKS; i++)  // Search for open task slot
     {
        if(TaskerStatePtr[i] == NopTaskerState)
        {
            break;
        }
     }
     TaskerStatePtr[i] = funcPtr;
     TaskParm0[i] = parm0;
     TaskParm1[i] = parm1;
  )
  assert(N_TASKS > i);
  return(i);
}
/** @brief Remove task/state from tasker queue.
 *
 *  Pass in function/state name and this will terminate the task.
 *  Returns the task's index, 0 - N_TASKS, if successful.
 *  If can't find task in queue then returns 0xFF (-1), no harm.
 *
 */
uint8_t removeTask( void (*funcPtr)( ) )
{
  uint8_t i = 0;
  taskParm_t zero;
  zero.u32 = 0UL;

  do
  {
     if( TaskerStatePtr[i] == funcPtr )
     {
        TaskerStatePtr[i] = NopTaskerState;
        TaskParm0[i].u32 = zero.u32;
        TaskParm1[i].u32 = zero.u32;
        return( i );
     }
  } while( ++i < N_TASKS );
  // No harm if not in task stack
  return( 0xFF );
}

#if 0
// Debug, note only one or none of the printf's are active
void spewRtosEntries( void )
{
   uint8_t i;

   sl_zigbee_app_debug_println("\r\n");
   for( i = 0; i < N_TASKS; i++ )
   {
      if( TaskerStatePtr[i] != NopTaskerState )
      {
#ifdef FABRIQ_ULT_X1_DEBUG
         fabriqDebugPrintf16Hex( (((uint32_t)TaskerStatePtr[i] >> 16) & 0x0000FFFF), FALSE );
         fabriqDebugPrintf16Hex( ((uint32_t)TaskerStatePtr[i] & 0x0000FFFF), TRUE );
#endif

         sl_zigbee_app_debug_println("%d 0x%4x",i,(uint32_t)TaskerStatePtr[i]);
      }
   }
}
#else
void spewRtosEntries( void ) { }
#endif

#ifdef REHTOS_UNIT_TEST
// Example: Main() kicks off the start, via calling xxx which sets up parms
// for someState0 and adds it to the tasker.  It eventually gets called
// via the taskerTick.
void someState2( taskParm_t parm0, taskParm_t parm1 )
{
    // parm0 is the passed timeStamp, parm1 is the delay time
    if( has_timed_out( parm0.u32, parm1.u32) )
    {
        sl_zigbee_app_debug_println("\r\nState 2 done %d %d   <%d>", parm0.u32, parm1.u32, get_time_ms());
        removeTask( someState2 );
    }
}
void someState1( taskParm_t parm0, taskParm_t parm1 )
{
    // parm0 is the passed timeStamp, parm1 is the delay time
    if( has_timed_out( parm0.u32, parm1.u32) )
    {
        sl_zigbee_app_debug_println("\r\nState 1 done %d %d   <%d>", parm0.u32, parm1.u32, get_time_ms());
        removeTask( someState1 );
    }
}
void someState0( taskParm_t parm0, taskParm_t parm1 )
{
    taskParm_t arg0, arg1;

    arg0.u32 = get_time_ms();
    arg1.u32 = 5000UL;

    sl_zigbee_app_debug_println("\r\nState 0: %d %d   <%d>",parm0.u16, parm1.s16, get_time_ms());
    removeTask( someState0 );
    addTask( someState1, arg0, arg1 );
}
// Call this to start the ball rolling
void tasker_unit_test( void )
{
    taskParm_t arg0, arg1;

    addTask( someState0, arg0, arg1 );
    arg0.u32 = get_time_ms();
    arg1.u32 = 7500UL;
    addTask( someState2, arg0, arg1 );
}
#endif
