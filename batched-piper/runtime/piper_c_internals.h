/* piper_c_internals.h                 -*-C-*-
 *
 *************************************************************************
 *
 *  @copyright
 *  Copyright (C) 2013, Intel Corporation
 *  All rights reserved.
 *  
 *  @copyright
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *  
 *  @copyright
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *  AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 *  WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/

/**
 * @file piper_c_internals.h
 *
 * @brief Declaration of piper functions that are visible only the
 * runtime.
 *
 * This header contains only C because it is included from scheduler.c
 */

#ifndef INCLUDED_PIPER_C_INTERNALS_DOT_H
#define INCLUDED_PIPER_C_INTERNALS_DOT_H

#include <stdio.h>
#include <stdlib.h>

#include <cilk/common.h>
//#include <cilk/abi.h>
#include <internal/abi.h>
#include <cilk/piper_abi.h>

#include "bug.h"
#include "rts-common.h"
#include "full_frame.h"


// NOTE: This file is a C-header file, because it is being included in
// scheduler.c!

/**
 * @brief Create a full frame for the child iteration when the control
 * frame of a pipe_while loop is stolen.
 *
 * We should have (control_sf->flags & CILK_FRAME_PIPER).
 *
 * @param w             The thief worker that just stole control_sf
 * @param control_sf    The control stack frame that was just stolen.
 * @param control_ff    The corresponding full frame for the control frame.
 *
 * @return Returns pointer to the full frame allocated for the
 * child iteration frame that will remain on the victim.
 */
COMMON_PORTABLE
full_frame*
piper_create_iter_ff_for_stolen_control_frame(__cilkrts_worker *w,
                                              __cilkrts_stack_frame *control_sf,
                                              full_frame *control_ff);

/**
 * @brief Method called to install an iteration full frame on a worker
 * w.
 *
 * We should have (control_sf->flags & CILK_FRAME_PIPER).
 *
 * @param w             The thief worker that just stole control_sf
 * @param control_sf    The control stack frame that was just stolen.
 * @param base_iter_ff  The corresponding full frame for the iteration. 
 * @param victim        The victim worker we are installing into.
 *
 * @c (*base_iter_ff) should be of type pipe_iter_full_frame.
 */
COMMON_PORTABLE
void
piper_install_iteration_frame(__cilkrts_worker *w,
                              __cilkrts_stack_frame *control_sf,
                              full_frame *base_iter_ff,
                              __cilkrts_worker *victim);

/**
 * @brief Saves state from the worker into the control frame.
 */
COMMON_PORTABLE
void
piper_save_worker_state_for_suspend_control(__cilkrts_worker *w,
                                            __cilkrts_stack_frame *control_sf);

/**
 * @brief Restores state from control frame into the worker.
 *
 *  This method is the inverse of
 *  piper_save_worker_state_for_suspend_control.
 */
COMMON_PORTABLE
void
piper_restore_worker_state_from_suspend_control(__cilkrts_worker *w,
                                                __cilkrts_stack_frame *control_sf);

/**
 * Saves state from the worker into the appropriate pipeline iteration
 * frame.
 *
 * @param w                The currently executing worker.
 * @param iter_ff_uncast   The pipeline iteration frame to store data into.
 * @param suspending_ff    The currently full frame to suspend.
 *
 * @c iter_ff_uncast should be a structure of type @c
 * pipe_iter_full_frame.
 * 
 * This method expects @c suspending_ff to be a direct call descendant
 * of @c iter_ff_uncast, i.e., suspending_ff is either @c
 * iter_ff_uncast itself, or can be reached by following parent links
 * until is_call_child is false.
 *
 */
COMMON_PORTABLE
void piper_save_worker_state_for_suspend_iter(__cilkrts_worker *w,
                                              full_frame *iter_ff_uncast,
                                              full_frame *suspending_ff);
/**
 * Restores state from appropriate frame into the worker from a
 * pipeline iteration frame.
 *
 * This method is the inverse of piper_save_worker_state_for_suspend.
 *
 * @param  w               The currently executing worker.
 * @param  iter_ff_uncast  The pipeline iteration frame to restore data from.
 * @return                 The full frame that was suspended.
 *
 * @c iter_ff_uncast should be a structure of type @c
 * pipe_iter_full_frame.
 *
 * (Return value will be @c suspending_ff from the matching call to
 *  @c piper_save_worker_state_for_suspend_iter)
 */
COMMON_PORTABLE
full_frame*
piper_restore_worker_state_from_suspend_iter(__cilkrts_worker *w,
                                             full_frame *iter_ff_uncast);

/**
 *@brief Tries to suspend the control frame due to throttling.
 *
 * Returns nonzero if suspend was successful, and 0 otherwise.  If it
 * returns 0, then it is safe to continue execution, i.e., we have
 * space in the buffer for the next iteration.
 */
COMMON_PORTABLE
int
piper_try_suspend_control_frame_at_throttle(__cilkrts_worker *w,
                                            full_frame *control_ff,
                                            __cilkrts_stack_frame *control_sf);

/**
 * @brief Try to suspend currently executing stage.
 *
 * This method then checks the next iteration and the parent (on
 * implicit resume) to see if we should continue execution with one of
 * those full frames.  It may also return @c iter_ff, if it notices
 * that we don't need to suspend after all.
 *
 * @c iter_ff_uncast should be a structure of type @c
 * pipe_iter_full_frame.
 *
 * @param w               Currently executing worker.
 * @param iter_ff_uncast  Full frame for currently executing iteration. 
 * @return                Next full frame to execute, if we picked up one.
 */
COMMON_PORTABLE
full_frame*
piper_try_suspend_at_stage_wait(__cilkrts_worker *w,
                                full_frame *iter_ff_uncast);

/**
 * @brief Finish iteration and try to resume a full frame after this
 * one.
 *
 * @c iter_ff_uncast should be a structure of type @c
 * pipe_iter_full_frame.
 *
 * @param w               Currently executing worker.
 * @param iter_ff_uncast  Full frame for currently executing iteration. 
 * @return                Next full frame to execute, if we picked one up.
 */
COMMON_PORTABLE
full_frame*
piper_finish_iter_and_get_next(__cilkrts_worker *w,
                               full_frame *iter_ff_uncast);

/**
 * @brief Try to steal implicit pipeline iterations from victim. 
 *
 * This method should be called on a worker @c only when w's currently
 * executing full frame is a direct descendant of a pipeline
 * iteration.
 * 
 * If victim's parent_resume flags is set, we will resume the parent
 * (control) frame of the pipeline.
 *
 * Otherwise, we will try to resume the next iteration, if it has been
 * suspended.
 *
 * @c iter_ff_uncast should be a structure of type @c
 * pipe_iter_full_frame.
 *
 * @param w               Currently executing worker.
 * @param victim          Victim worker we are trying to steal from.
 * @param iter_ff_uncast  Full frame for currently executing iteration on @c w.
 * @return                Next full frame to execute, if we picked one up.
 */
COMMON_PORTABLE
full_frame*
piper_try_implicit_steals(__cilkrts_worker *w,
                          __cilkrts_worker *victim,
                          full_frame *iter_ff_uncast);

/**
 * @brief Cleanup for an iteration when it is returning after a
 * simulated steal of the parent.
 */ 
COMMON_PORTABLE
void
piper_leave_iter_frame_for_simulated_steal(__cilkrts_stack_frame *control_sf);

/**
 * @brief Get the iteration data from a full frame.
 * For debugging only. */
COMMON_PORTABLE
__cilkrts_pipe_iter_data*
piper_get_iter_data_for_ff(full_frame* iter_ff);

#endif // !defined(INCLUDED_PIPER_C_INTERNALS_DOT_H)
