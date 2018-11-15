/* piper_sched.cpp                  -*-C++-*-
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

/* Implementation of functions for pipeline parallelism. 
 */


#include <stdio.h>
#include <stdlib.h>

#include <cilk/common.h>
//#include <cilk/abi.h>
#include <internal/abi.h>
#include <cilk/piper_abi.h>
#include <cilk/piper_fake_macros.h>

#include "bug.h"
#include "os.h"
#include "rts-common.h"
#include "sched_lock_macros.h"

// This file has a cyclic dependency with scheduler.c.  But this file
// can't be merged because it uses a small amount of C++.
#include "scheduler.h"  
#include "sysdep.h"
#include "frame_malloc.h"
#include "piper_runtime_structs.h"
#include "local_state.h"


// Tiny bit of local C++ code.

/**
 *@brief Casts a @c __cilkrts_stack_frame that is a control frame into
 * a @c pipe_control frame_t.
 *
 * This cast works because the __cilkrts_stack_frame is the first
 * member of __cilkrts_pipe_control_frame.
 */ 
static __cilkrts_pipe_control_frame*
piper_cast_pipe_control_frame(__cilkrts_stack_frame *control_sf)
{
    __cilkrts_pipe_control_frame* pcf =
        reinterpret_cast<__cilkrts_pipe_control_frame*>(control_sf);
    CILK_ASSERT(control_sf->flags & CILK_FRAME_PIPER);
    return pcf;
}



__CILKRTS_BEGIN_EXTERN_C
#include "piper_c_internals.h"

/********************************************************************/
/**
 * Protocol for an iteration data block's cleanup counter.
 *
 * The cleanup counter starts with initial value of
 *   2 * CLEANUP_FINISH_ITER_VAL + CLEANUP_THROTTLE_VAL.
 *
 * 3 updates to the cleanup counter for a data block (i % K). 
 * 
 *   1. When an iteration i finishes, it decrements its own counter (i % K) by
 *      CLEANUP_FINISH_ITER_VAL.
 *
 *   2. When an iteration i finishes, it decrements the previous
 *      block's counter ((i-1) % K) by CLEANUP_FINISH_ITER_VAL.
 *
 *   3. When the continuation of the spawn of iteration i reaches the
 *      throttle point (__cilkrts_piper_throttle), it decrements the
 *      cleanup counter by CLEANUP_THROTTLE_VAL.
 *
 *  When the counter value <= CLEANUP_THROTTLE_VAL, the block is ready
 *  to be reused.  Thus, the status flags marking the block as ready
 *  for reuse can happen at this point.
 *
 *  When the counter value reaches 0, execution can continue past the
 *  __cilkrts_piper_throttle call.
 */ 

/// Cleanup counter value for finishing an iteration.  Can be any
/// value larger >= 2.
const static int CLEANUP_FINISH_ITER_VAL = 1 << 4;

/// Cleanup counter value for reaching a throttling point.
const static int CLEANUP_THROTTLE_VAL = 1;

const static int CLEANUP_INITIAL_VAL = 2*CLEANUP_FINISH_ITER_VAL + CLEANUP_THROTTLE_VAL;

static int cleanup_counter_ready_for_reuse(pipe_iter_full_frame* iter_ff) {
    CILK_ASSERT(iter_ff->cleanup_counter >= 0);
    return (iter_ff->cleanup_counter <= CLEANUP_THROTTLE_VAL);
}

static int cleanup_counter_ready_to_resume_after_throttle(pipe_iter_full_frame* iter_ff) {
    CILK_ASSERT(iter_ff->cleanup_counter >= 0);
    return (iter_ff->cleanup_counter <= 0);
}

static int cleanup_counter_is_reset(pipe_iter_full_frame* iter_ff) 
{
    return (CLEANUP_INITIAL_VAL == iter_ff->cleanup_counter);
}

static int cleanup_counter_finish_iter(pipe_iter_full_frame* iter_ff)
{
    iter_ff->cleanup_counter -= CLEANUP_FINISH_ITER_VAL;
    CILK_ASSERT(iter_ff->cleanup_counter >= 0);
    return iter_ff->cleanup_counter;
}

static int cleanup_counter_start_throttle_point(pipe_iter_full_frame* iter_ff)
{
    iter_ff->cleanup_counter -= CLEANUP_THROTTLE_VAL;
    CILK_ASSERT(iter_ff->cleanup_counter >= 0);
    return iter_ff->cleanup_counter;
}

static void cleanup_counter_init_buffer(pipe_iter_full_frame* ff_buffer,
                                        int buffer_size)
{
    CILK_ASSERT(buffer_size >= 2);

    // Block 0 represents user iteration "-1".  Its own iteration has
    // completed, but the next iteration has not happened yet, and
    // iteration (K-1) has not tried to use this block for the next
    // iteration yet, where K == buffer_size.
    ff_buffer[0].cleanup_counter = CLEANUP_THROTTLE_VAL + CLEANUP_FINISH_ITER_VAL;

    // Block 1 represents user iteration 0. It starts as "full", it is
    // not done, and its next iteration is not done.
    ff_buffer[1].cleanup_counter = CLEANUP_INITIAL_VAL;

    // Blocks 2 through K-1 represent iterations (1-K), (2-K), ... -2.
    // These blocks are all ready for reuse, but haven't reached the
    // throttle point yet.
    for (int i = 2; i < buffer_size; ++i) {
        ff_buffer[i].cleanup_counter = CLEANUP_THROTTLE_VAL;
    }
}

static void cleanup_counter_reset_for_new_iter(pipe_iter_full_frame* iter_ff)
{
    CILK_ASSERT(0 == iter_ff->cleanup_counter);
    iter_ff->cleanup_counter = CLEANUP_INITIAL_VAL;
}


static void piper_mark_frame_as_done_helper(pipe_iter_full_frame *iter_ff) 
{
    // When we finish, the data block status should not be suspended
    // or finished.
    CILK_ASSERT((ITER_PROMOTED_ACTIVE == iter_ff->iter_status) ||
                (ITER_STANDARD_EXEC == iter_ff->iter_status));
    iter_ff->iter_status = ITER_FINISHED;
    
    // Mark the previous iteration block as available to be reused.
    // After this point, it is unsafe to access the main fields
    // of the previous iteration.
    CILK_ASSERT(BLOCK_AVAILABLE != iter_ff->block_status);
    iter_ff->block_status = BLOCK_AVAILABLE;
}

/**
 * @brief Decrements the cleanup count on the current block
 * (represented by iter_data), and mark the worker's parent_resume
 * flag if the cleanup count hits 0.
 *
 * The worker parameter @c w should be non-NULL if it is possible that
 * the freeing of this iteration can enable a new iteration that is
 * suspended by throttling.
 *
 * Said differently, pass in w == NULL if any future iteration that
 * might be enabled via a throttling edge by completing this iteration
 * has not started yet.
 */ 
static
int
piper_try_free_iter_data_block_common(__cilkrts_pipe_iter_data* iter_data,
                                      __cilkrts_worker *w,
                                      bool need_lock_acquire)
{
    __cilkrts_pipe_iter_data* prev_iter_data = iter_data->left;
    CILK_ASSERT(prev_iter_data);
    int freed = 0;
    
    // Lock the previous iteration (i-1).  Since we are just finishing
    // this iteration (i), the previous iteration should not be
    // available.
    BEGIN_WITH_PIPE_DATA_LOCK_CONDITIONAL(iter_data, need_lock_acquire)
    {

        pipe_iter_full_frame *iter_ff = iter_data->iter_ff_link;

        // The current iteration should at least be past its final
        // user stage.
        CILK_ASSERT(iter_data->stage >=
                    CILK_STAGE_USER_TO_SYS(CILK_PIPE_POST_ITER_CLEANUP_STAGE));
        CILK_ASSERT(BLOCK_AVAILABLE != iter_ff->block_status);

        cleanup_counter_finish_iter(iter_ff);
        PIPER_DBG_PRINTF(1,
                         "W=%d: cleanup on iter_data=%p, m_iter_num =%ld. got count = %d\n",
                         w ? w->self : -1,
                         iter_data,
                         iter_data->iter_num,
                         iter_ff->cleanup_counter);
        
        if (w) {
            // This function is being called from a path where this
            // iteration has been stolen from.
            //
            // If this iteration block was suspended at a throttle, we
            // want to mark the parent frame so it can be stolen
            // again.
            //
            // This path is already holding the worker lock, so it is
            // safe to manipulate w's deque flags.

            if (cleanup_counter_ready_to_resume_after_throttle(iter_ff)) {
                PIPER_DBG_PRINTF(1,
                                 "W=%d: cleanup on iter_data = %p, iter_num = %ld. got count == 0, marking parent resume = %d (throttle!)\n",
                                 w->self,
                                 iter_data,
                                 iter_data->iter_num,
                                 CILK_PIPE_PARENT_THROTTLE_SUSPEND);
                w->l->implicit_deque_flags.parent_resume = CILK_PIPE_PARENT_THROTTLE_SUSPEND;
            }
        }
        else {
            // If this function is being called in a path where this
            // iteration not been stolen from, then when we finish
            // this iteration, the parent frame can not be suspended
            // at a throttling edge that is waiting on this block.
            //
            // For the parent (control) frame to be suspended waiting
            // on this block, we would need to have started some
            // future iteration (i + K) before finishing this
            // iteration (i).  That could only happen if a steal
            // occurred!
            CILK_ASSERT(!cleanup_counter_ready_to_resume_after_throttle(iter_ff));
        }

        // Check if all the current users have finished with the
        // frame.  If so, mark the block as ready for reuse.
        if (cleanup_counter_ready_for_reuse(iter_ff)) {
            piper_mark_frame_as_done_helper(iter_ff);
            freed = 1;
        }
    } END_WITH_PIPE_DATA_LOCK_CONDITIONAL(iter_data, need_lock_acquire);

    return freed;
}


/********************************************************************/

CILK_ABI_PIPE_DATA_PTR
__cilkrts_piper_get_current_iter_data(__cilkrts_pipe_control_frame* pcf)
{
    // TBD: we can probably implement this more efficiently by
    // remembering the pointer, instead of doing an index
    // calculation?
    int idx = (int)(pcf->current_iter % pcf->buffer_size);
    return pcf->control_data->iter_buffer + idx;
}

static inline  __cilkrts_pipe_iter_data*
piper_get_next_iter_data(__cilkrts_pipe_control_frame *pcf)
{
    int idx = (int)((pcf->current_iter + 1) % pcf->buffer_size);
    return pcf->control_data->iter_buffer + idx;
}

static pipe_iter_full_frame*
piper_get_current_iter_ff(__cilkrts_pipe_control_frame *pcf)
{
    int idx = (int)(pcf->current_iter % pcf->buffer_size);
    return pcf->control_data->ff_buffer + idx;
}



static pipe_iter_full_frame*
make_iter_frame(__cilkrts_worker *w,
                __cilkrts_pipe_control_frame* pcf)
{

    __cilkrts_pipe_iter_data* iter_data = __cilkrts_piper_get_current_iter_data(pcf);
    pipe_iter_full_frame* iter_ff = piper_get_current_iter_ff(pcf);
    CILK_ASSERT(pcf->current_iter == iter_data->iter_num);
    CILK_ASSERT(!iter_ff->in_use);
    
    // Grab the next full frame from the control buffer, and reset it
    // for next use.
    iter_ff->in_use = true;
    __cilkrts_reset_full_frame(iter_ff, NULL);

    CILK_ASSERT(FF_PIPE_ITER == iter_ff->frame_type);
    CILK_ASSERT(iter_ff == iter_ff->pipe_iter_root);
    CILK_ASSERT(NULL == iter_ff->saved_reducer_map);
    CILK_ASSERT(NULL == iter_ff->suspended_ff);

    PIPER_DBG_PRINTF(1,
                     "W=%d: making iter_ff=%p for iter %ld\n",
                     w->self,
                     iter_ff,
                     pcf->current_iter);
    return iter_ff;
}

static void
destroy_iter_frame(__cilkrts_worker *w,
                   pipe_iter_full_frame *iter_ff)
{
    if (PIPER_DEBUG >= 0) {
        __cilkrts_pipe_iter_data* iter_data = iter_ff->data;
        __cilkrts_pipe_control_frame *pcf = iter_data->control_sf;
        int idx = (iter_data->iter_num % pcf->buffer_size);
        pipe_iter_full_frame *iter_ff_check =
            &pcf->control_data->ff_buffer[idx];
        CILK_ASSERT(iter_ff == iter_ff_check);
        CILK_ASSERT(iter_ff->in_use);
    }

    CILK_ASSERT(FF_PIPE_ITER == iter_ff->frame_type);

    // This method is the equivalent of "destroy_full_frame", except
    // specialized for iteration full frames.
    validate_full_frame(iter_ff);
    CILK_ASSERT(iter_ff->children_reducer_map == 0);
    CILK_ASSERT(iter_ff->right_reducer_map == 0);
    CILK_ASSERT(NULL == iter_ff->pending_exception);
    CILK_ASSERT(NULL == iter_ff->child_pending_exception);
    CILK_ASSERT(NULL == iter_ff->right_pending_exception);

    iter_ff->in_use = false;
}


/***********************************************************************/
// Functions exported to scheduler.c, in piper_c_internals.h

full_frame*
piper_create_iter_ff_for_stolen_control_frame(__cilkrts_worker *w,
                                              __cilkrts_stack_frame *control_sf,
                                              full_frame *control_ff)
{
    CILK_ASSERT(control_sf->flags & CILK_FRAME_PIPER);
    // It is safe to cast control_sf into a pipe control frame,
    // because only pipe control frames should have the
    // CILK_FRAME_PIPER flag set.
    __cilkrts_pipe_control_frame* pcf =
        piper_cast_pipe_control_frame(control_sf);
    CILK_ASSERT(pcf->control_data);
        
    if (!pcf->control_ff) {
        PIPER_DBG_PRINTF(1,
                         "W=%d. steal of pcf = %p. Should be first steal.  Spawned iteration was %ld. Set ff to %p\n",
                         w->self, pcf, pcf->current_iter, control_ff);
        CILK_ASSERT(pcf->buffer_size >= 1);
        pcf->control_ff = control_ff;
    }
    else {
        // Any subsequent times we steal the control frame, the
        // full frame should still match.
        CILK_ASSERT(control_ff ==
                    pcf->control_ff);
    }

    // Allocate memory for the full frame for the child.
    // (Eventually, this memory should just come from the control
    // buffer.)
    pipe_iter_full_frame* iter_ff = make_iter_frame(w, pcf);
    iter_ff->parent = control_ff;
        
    PIPER_DBG_PRINTF(2,
                     "W=%d: at this point, control_sf->worker = %p, w=%p\n",
                     w->self,
                     control_sf->worker,
                     w);
    CILK_ASSERT(FF_PIPE_ITER == iter_ff->frame_type);        
    CILK_ASSERT((control_sf->worker == NULL) || (control_sf->worker == w));
    return iter_ff;
}

void piper_install_iteration_frame(__cilkrts_worker *w,
                                   __cilkrts_stack_frame *control_sf,
                                   full_frame *base_iter_ff,
                                   __cilkrts_worker *victim)
{
    pipe_iter_full_frame* iter_ff =
        static_cast<pipe_iter_full_frame*>(base_iter_ff);
    int is_self_steal = (w == victim);

    CILK_ASSERT(control_sf->flags & CILK_FRAME_PIPER);
    // It is safe to cast control_sf into a pipe control frame,
    // because only pipe control frames should have the
    // CILK_FRAME_PIPER flag set.
    __cilkrts_pipe_control_frame* pcf =
        piper_cast_pipe_control_frame(control_sf);

    // Link the iteration full frame we just created to the
    // corresponding stack frame.
    __cilkrts_pipe_iter_data *iter_data =
        __cilkrts_piper_get_current_iter_data(pcf);

    // Set the fields in the previous iteration stack frame,
    // check_right operations can find this full frame, to try to
    // resume it.
    BEGIN_WITH_PIPE_DATA_LOCK(iter_data) {
        CILK_ASSERT(ITER_STANDARD_EXEC == iter_ff->iter_status);
        iter_ff->iter_status = ITER_PROMOTED_ACTIVE;
        
        // Set the implicit flags on the victim worker we are
        // installing into.
        CILK_ASSERT(!victim->l->implicit_deque_flags.parent_resume);
        CILK_ASSERT(!victim->l->implicit_deque_flags.iter_ff_on_top);
        victim->l->implicit_deque_flags.iter_ff_on_top = 1;
        if (is_self_steal) {
            // Don't fiddle with this flag on the victim unless we
            // are the victim.
            w->l->implicit_deque_flags.parent_resume =
                CILK_PIPE_PARENT_SELF_STEAL;
        }

        // The equivalent of make_runnable() on iter_ff.  But we
        // want to execute it in this file, and do it while
        // holding the lock.
        //victim->l->frame_ff = iter_ff;
        victim->l->core_frame_ff = iter_ff;
        iter_ff->call_stack = 0;
    } END_WITH_PIPE_DATA_LOCK(iter_data);
}


void
piper_save_worker_state_for_suspend_control(__cilkrts_worker *w,
                                            __cilkrts_stack_frame *control_sf)
{
    __cilkrts_pipe_control_frame* pcf =
        piper_cast_pipe_control_frame(control_sf);

    PIPER_DBG_PRINTF(2,
                     "W=%d: saving state for pcf %p\n",
                     w->self, pcf);
    
    // Save the pedigree of the frame from the worker.
    // 
    // TBD: Actually, I think this is unnecessary for a control frame:
    // we should be able to flatten pedigrees for a pipe_while loop by
    // just using the iteration number?
    if (CILK_FRAME_VERSION_VALUE(pcf->sf.flags) >= 1) {
        pcf->sf.parent_pedigree.rank = w->pedigree.rank;
        pcf->sf.parent_pedigree.parent = w->pedigree.parent;
    }

    CILK_ASSERT(NULL != pcf->control_ff);
    // Save the reducer map into the full frame when
    // suspending.
    CILK_ASSERT(NULL == pcf->control_data->saved_reducer_map);
    pcf->control_data->saved_reducer_map = w->reducer_map;
    w->reducer_map = NULL;
    // TBD: do we need to save pending exception information
    // here as well, like we do for reducers, or is that field
    // always guaranteed to be NULL?
}

// The inverse of pipe_frame_save_state_for_suspend
void
piper_restore_worker_state_from_suspend_control(__cilkrts_worker *w,
                                                __cilkrts_stack_frame *control_sf)
{
    __cilkrts_pipe_control_frame* pcf =
        piper_cast_pipe_control_frame(control_sf);

    PIPER_DBG_PRINTF(2,
                     "W=%d: restoring state for pcf %p\n",
                     w->self,
                     pcf);
    
    // Restore the worker's pedigree from the frame.
    // 
    // TBD: Actually, I think this is unnecessary for a control frame:
    // we should be able to flatten pedigrees for a pipe_while loop by
    // just using the iteration number?
    if (CILK_FRAME_VERSION_VALUE(pcf->sf.flags) >= 1) {
        w->pedigree.rank = pcf->sf.parent_pedigree.rank;
        w->pedigree.parent = pcf->sf.parent_pedigree.parent;
    }

    // Save the reducer map into the full frame when
    // suspending.
    CILK_ASSERT(NULL == w->reducer_map);
    w->reducer_map = pcf->control_data->saved_reducer_map;
    pcf->control_data->saved_reducer_map = NULL;
}

void piper_save_worker_state_for_suspend_iter(__cilkrts_worker *w,
                                              full_frame *iter_ff_uncast,
                                              full_frame *suspending_ff)
{
    pipe_iter_full_frame* iter_ff =
        static_cast<pipe_iter_full_frame*>(iter_ff_uncast);
    __cilkrts_pipe_iter_data* iter_data = iter_ff->data;

    PIPER_DBG_PRINTF(2,
                     "W=%d: saving state into iter_ff %p, iter_data %p\n",
                     w->self, iter_ff, iter_data);
    
    // Save the pedigree of the frame from the worker into the
    // iteration helper frame.
    if (CILK_FRAME_VERSION_VALUE(iter_data->sf->flags) >= 1) {
        iter_data->sf->parent_pedigree.rank = w->pedigree.rank;
        iter_data->sf->parent_pedigree.parent = w->pedigree.parent;
    }

    // Save the reducer map into the full frame when
    // suspending.
    CILK_ASSERT(NULL == iter_ff->saved_reducer_map);
    iter_ff->saved_reducer_map = w->reducer_map;
    w->reducer_map = NULL;

    CILK_ASSERT(NULL == iter_ff->suspended_ff);
    iter_ff->suspended_ff = suspending_ff;

    // TBD: is there exception information we need to save?  Or is
    // that field always guaranteed to be NULL?
}                                       


// The inverse of pipe_frame_save_state_for_suspend
full_frame* piper_restore_worker_state_from_suspend_iter(__cilkrts_worker *w,
                                                         full_frame *iter_ff_uncast)
{
    pipe_iter_full_frame* iter_ff =
        static_cast<pipe_iter_full_frame*>(iter_ff_uncast);
    __cilkrts_pipe_iter_data* iter_data = iter_ff->data;
    full_frame* suspended_ff;
    
    // At this point, either:
    // 
    //  1. iter_data->sf == current_sf, if the body of the pipeline
    //     loop itself is not a spawning function, or
    //  2. iter_data->sf is the call parent of current_sf, if the body
    //     of the pipeline loop is a spawning function.
    //
    // We can't assert this fact, because in case 2, the link to the
    // call parent is likely broken by the steal.
    //
    // All the interesting information is stored into iter_data->sf,
    // since that is the actual stack frame for a pipeline iteration.
    // The current_sf itself may or may not be a pipeline iteration.

    PIPER_DBG_PRINTF(2,
                     "W=%d: restoring state for ff=%p, iter_data=%p\n",
                     w->self,
                     ff,
                     iter_data);
    
    // Restore the worker's pedigree from the frame.
    // 
    // TBD: Actually, I think this is unnecessary for a control frame:
    // we should be able to flatten pedigrees for a pipe_while loop by
    // just using the iteration number?
    if (CILK_FRAME_VERSION_VALUE(iter_data->sf->flags) >= 1) {
        w->pedigree.rank = iter_data->sf->parent_pedigree.rank;
        w->pedigree.parent = iter_data->sf->parent_pedigree.parent;
    }

    // Save the reducer map into the full frame when
    // suspending.
    CILK_ASSERT(NULL == w->reducer_map);
    w->reducer_map = iter_ff->saved_reducer_map;
    iter_ff->saved_reducer_map = NULL;

    CILK_ASSERT(iter_ff->suspended_ff);
    suspended_ff = iter_ff;
    iter_ff->suspended_ff = NULL;
    return suspended_ff;
}




// If this function is called without holding a lock on
// control_ff, then it may return a false positive, i.e., it may
// say we need to throttle when we really don't.
//
// But if it returns 0, it is never a false negative (at least
// when executed by the worker that is / was executing the pipe
// control frame itself.  Once you know you have enough space in
// the buffer to start the next pipeline iteration, that remains
// true because only the pipe control frame ever starts new
// iterations.
static int piper_need_to_throttle_control_frame(full_frame *control_ff,
                                                __cilkrts_stack_frame *control_sf)
{
    // Throttle if the next iteration slot is not free.

    __cilkrts_pipe_control_frame *pcf =
        piper_cast_pipe_control_frame(control_sf);
    __cilkrts_pipe_iter_data* iter_data =
        __cilkrts_piper_get_current_iter_data(pcf);
    __cilkrts_pipe_iter_data* next_iter_data = iter_data->right;

    int need_to_throttle = (BLOCK_AVAILABLE != next_iter_data->iter_ff_link->block_status);

    PIPER_DBG_PRINTF(1,
                     "w=%p, pcf->w=%p, Need to throttle = %d. iter_data = %p, num = %ld, iter_data->next_block_status=%d\n",
                     __cilkrts_get_tls_worker(),
                     pcf->sf.worker,
                     need_to_throttle,
                     iter_data,
                     iter_data->iter_num,
                     next_iter_data->iter_ff_link->block_status);
    return need_to_throttle;
}


// Try to suspend the control frame.  This function should be
// called while holding the full frame lock on the control frame.
// 
// Returns 1 if control_ff is really suspended afterwards, or 0 if
// we don't need to suspend.
int
piper_try_suspend_control_frame_at_throttle(__cilkrts_worker *w,
                                            full_frame *control_ff,
                                            __cilkrts_stack_frame *control_sf)
{
    __cilkrts_pipe_control_frame* pcf =
        piper_cast_pipe_control_frame(control_sf);
    __cilkrts_pipe_iter_data* next_iter_data =
        piper_get_next_iter_data(pcf);
    pipe_iter_full_frame *next_iter_ff = next_iter_data->iter_ff_link;
    
    CILK_ASSERT(pcf->control_ff);
    CILK_ASSERT(pcf->sf.flags & CILK_FRAME_UNSYNCHED);
    int is_suspending = 1;

    BEGIN_WITH_PIPE_DATA_LOCK(next_iter_data) {
        cleanup_counter_start_throttle_point(next_iter_ff);
        
        if (!cleanup_counter_ready_for_reuse(next_iter_ff)) {
            // Need to actually suspend. Mark the full frame.
            control_ff->resume_flags = FF_PIPE_SUSPENDED_CONTROL;

            // Clear the worker field for cleanliness.
            control_sf->worker = NULL;

            PIPER_DBG_PRINTF(1,
                             "W=%d: pcf=%p. marking iter_data->next_block as SUSPENDED_AT_THROTTLE, next_iter_data->my_num is %ld\n",
                             w->self,
                             pcf,
                             next_iter_data->iter_num);
        }
        else {
            // We can actually continue.  Just immediately resume.
            CILK_ASSERT(!piper_need_to_throttle_control_frame(pcf->control_ff,
                                                              &pcf->sf));
            control_ff->resume_flags = FF_PIPE_RESUMING_CONTROL;
            is_suspending = 0;
        }
    } END_WITH_PIPE_DATA_LOCK(next_iter_data);
    return is_suspending;
}




/**
 * Finish the final user stage (i.e., move the stage counter,
 * past the final user stage so the next iteration can start
 * its final user stage.
 */
static inline
void
piper_finish_final_user_stage(__cilkrts_pipe_iter_data* iter_data)
{
    CILK_ASSERT(CILK_STAGE_USER_TO_SYS(CILK_PIPE_FINAL_USER_STAGE)
                == iter_data->stage);
    iter_data->stage = CILK_STAGE_USER_TO_SYS(CILK_PIPE_POST_ITER_CLEANUP_STAGE);
}
    
static inline
void
piper_move_iter_stage_to_done(__cilkrts_pipe_iter_data* iter_data)
{
    CILK_ASSERT(CILK_STAGE_USER_TO_SYS(CILK_PIPE_POST_ITER_CLEANUP_STAGE)
                == iter_data->stage);
    iter_data->stage = CILK_PIPE_RUNTIME_FINISH_ITER_STAGE;
}

static
int
piper_need_to_stall_at_stage_wait(__cilkrts_pipe_iter_data* iter_data)
{
    CILK_ASSERT(iter_data->left);
    __cilkrts_pipe_stage_num_t left_iter_stage = iter_data->left->stage;
    return (left_iter_stage <= iter_data->stage);
}


/**
 * Helper method that checks the next iteration, and marks it as
 * resumed if it is suspended.  Should be executed only while
 * holding the lock on the next iteration's block.
 */
static
full_frame*
piper_try_resume_next_iteration_helper(__cilkrts_worker *w,
                                       __cilkrts_pipe_iter_data* iter_data)
{
    full_frame *resume_ff = NULL;
    __cilkrts_pipe_iter_data* next_iter_data = iter_data->right;
    pipe_iter_full_frame* next_iter_ff = next_iter_data->iter_ff_link;
        
    // If the next iteration is suspended, then we want to
    // resume it.
    if (ITER_PROMOTED_SUSPENDED == next_iter_ff->iter_status) {
        
        // If the next stage does not need to stall:
        if (!piper_need_to_stall_at_stage_wait(next_iter_data)) {
            // Mark the status of the next iteration accordingly.
            next_iter_ff->iter_status = ITER_PROMOTED_ACTIVE;
            resume_ff = next_iter_ff;
            CILK_ASSERT(resume_ff);
            
            CILK_ASSERT(resume_ff->resume_flags == FF_PIPE_SUSPENDED_ITER);
            resume_ff->resume_flags = FF_PIPE_RESUMING_ITER;

            PIPER_DBG_PRINTF(1,
                             "W=%d: implicit resume flag set.We think we are next iteration %p. It was suspended.\n",
                             w->self,
                             resume_ff);
        }
    }
    return resume_ff;
}

static inline full_frame*
piper_try_remove_implicit_parent(__cilkrts_worker *w,
                                 full_frame *control_ff)
{
    ASSERT_WORKER_LOCK_OWNED(w);
    full_frame* resume_ff = NULL;
    // If the implicit resume flag on the worker is set, check it,
    // grab the parent, and clear the flag.
    if (w->l->implicit_deque_flags.parent_resume) {
        cilk_pipe_parent_resume_t parent_type =
            w->l->implicit_deque_flags.parent_resume;
        w->l->implicit_deque_flags.parent_resume = CILK_PIPE_NO_PARENT;

        PIPER_DBG_PRINTF(1,
                         "W=%d: implicit resume flag set.We think we are resuming parent resume_ff =%p. parent_type = %d\n",
                         w->self, control_ff, parent_type);

        __cilkrts_stack_frame *control_sf = control_ff->call_stack;
        if (CILK_PIPE_PARENT_SELF_STEAL == parent_type) {
            // On a self-steal, we should always succeed.
            CILK_ASSERT(FF_NORMAL_CILK == control_ff->resume_flags);
            resume_ff = control_ff;
            CILK_ASSERT(control_sf);
        }
        else {
            // If it isn't a self-steal, then it might be
            // suspended.
            CILK_ASSERT(CILK_PIPE_PARENT_THROTTLE_SUSPEND == parent_type);
            if (FF_PIPE_SUSPENDED_CONTROL == control_ff->resume_flags)
            {
                control_ff->resume_flags = FF_PIPE_RESUMING_CONTROL;
                resume_ff = control_ff;
                    
                PIPER_DBG_PRINTF(1,
                                 "W=%d: Parent %p was suspended, being resumed from  self-steal.\n",
                                 w->self, control_ff);
                CILK_ASSERT(control_sf);
                CILK_ASSERT(!piper_need_to_throttle_control_frame(control_ff,
                                                                  control_sf));
            }
            else {
                // Someone else beat us to the resume?
                // Is this case even possible?
                PIPER_DBG_PRINTF(0,
                                 "W=%d: someone else beat us to resuming the control_ff = %p\n",
                                 w->self, control_ff);
            }
        }
    }

    PIPER_DBG_PRINTF(3,
                     "W=%d: try_implicit_parent_steal gives control_ff=%p, resume_ff=%p\n",
                     w->self, control_ff, resume_ff);
    return resume_ff;
}

                                                       
/**
 * When trying to suspend the current iteration, checks the next
 * iteration, to see if we should resume the next iteration if it
 * is suspended, or the parent if it is implicitly on our deque.
 *
 * This method checks for full frames to resume in the following
 * order:
 *
 * 1. First, check if the next pipeline iteration is suspended.
 *    If yes, then resume it.
 *
 * 2. Otherwise, check if the parent (control) frame is implicitly
 *    on our deque.  If it is, then resume it.
 */
static
full_frame*
piper_try_resume_next_iter_on_suspend(__cilkrts_worker *w,
                                      full_frame *iter_ff)
{
    full_frame *resume_ff = NULL;
    CILK_ASSERT(FF_PIPE_ITER == iter_ff->frame_type);

    pipe_iter_full_frame* pipe_iter_ff =
        static_cast<pipe_iter_full_frame*>(iter_ff);
    __cilkrts_pipe_iter_data* iter_data = pipe_iter_ff->data;
    __cilkrts_pipe_iter_data* next_iter_data = iter_data->right;
    CILK_ASSERT(iter_data);

    BEGIN_WITH_PIPE_DATA_LOCK(next_iter_data) {
        PIPER_DBG_PRINTF(1, 
                         "W=%d: try_resume_next_iter_on_suspend, iter_ff=%p, next_iter_ff=%p\n",
                         w->self,
                         iter_ff,
                         next_iter_data->iter_ff_link);

        // If the next iteration is suspended, then we want to
        // resume it.
        resume_ff = piper_try_resume_next_iteration_helper(w, iter_data);
            
        if (!resume_ff) {
            // If the implicit resume flag on the worker is set, check it,
            // grab the parent, and clear the flag.
            if (w->l->implicit_deque_flags.parent_resume) {
#if PIPER_DEBUG >= 1                    
                cilk_pipe_parent_resume_t parent_type
                    = w->l->implicit_deque_flags.parent_resume;
#endif
                resume_ff = iter_ff->parent;
                w->l->implicit_deque_flags.parent_resume = CILK_PIPE_NO_PARENT;

                PIPER_DBG_PRINTF(1,
                                 "W=%d: implicit resume flag set.We think we are resuming parent resume_ff =%p, type = %d. It was suspended.\n",
                                 w->self,
                                 resume_ff,
                                 parent_type);
            }
            else {
                // Check if the parent frame is suspended.  If so,
                // then try to resume it.  We only do this check
                // if we are finishing an iteration.

                PIPER_DBG_PRINTF(1,
                                 "W=%d:  parent is not suspended... iter_ff=%p, resume_ff will be %p\n",
                                 w->self,
                                 iter_ff,
                                 resume_ff);
            }
        }

        // Clear the flag that says we have a pipeline iteration on
        // top.  If we are going to resume the next iteration, we'll
        // set it again later.
        if (w->l->implicit_deque_flags.iter_ff_on_top) {
            w->l->implicit_deque_flags.iter_ff_on_top = 0;
        }
    } END_WITH_PIPE_DATA_LOCK(next_iter_data);
    
    return resume_ff;
}


full_frame *
piper_try_suspend_at_stage_wait(__cilkrts_worker *w,
                                full_frame *iter_ff_uncast)
{
    pipe_iter_full_frame* iter_ff
        = static_cast<pipe_iter_full_frame*>(iter_ff_uncast);
    __cilkrts_pipe_iter_data* iter_data = iter_ff->data;
    full_frame *resume_ff = NULL;
    int need_to_stall;

    BEGIN_WITH_PIPE_DATA_LOCK(iter_data) {
        need_to_stall = piper_need_to_stall_at_stage_wait(iter_data);

        CILK_ASSERT((iter_ff == iter_ff->suspended_ff) ||
                    (iter_ff == (iter_ff->suspended_ff->parent)));

        if (need_to_stall) {
            // Actually do the suspend.
            CILK_ASSERT(ITER_PROMOTED_ACTIVE == iter_ff->iter_status);

            iter_ff->iter_status = ITER_PROMOTED_SUSPENDED;
            iter_ff->resume_flags = FF_PIPE_SUSPENDED_ITER;
            resume_ff = piper_try_resume_next_iter_on_suspend(w, iter_ff);
        }
        else {
            // Otherwise, we are just going to resume this iteration
            // immediately.
            iter_ff->resume_flags = FF_PIPE_RESUMING_ITER;
            resume_ff = iter_ff;
        }
    } END_WITH_PIPE_DATA_LOCK(iter_data);
    return resume_ff;
}


/**
 * Finish a pipeline iteration and figure out the next frame to
 * execute, if there is one.
 *
 * This method performs the following steps, in the following
 * order:
 *
 *   1. Decrements the cleanup counter on the previous iteration's
 *      data block.  If this counter reaches 0, we can free the
 *      previous block and mark it as available.  Marking the
 *      block as available may enable the parent (because it was
 *      suspended at throttling).  If so, then set the implicit
 *      flags to push the parent on our deque.
 *
 *   2. Try to resume the next iteration, if it is suspended.
 *      (We are finishing the current iteration).
 *
 *   3. Decrement the cleanup counter on the current iteration
 *      block.  As with the previous iteration, if this counter
 *      reaches 0, we will free the current block and mark it as
 *      available for reuse, possibly pushing the parent
 *      implicitly back on our deque.
 *
 *   4. Destroy the current iteration frame.
 *
 *   5. Clear the flag saying we have an iteration on top of this
 *      deque.
 *
 *   6. On returning, if the parent frame is implicitly on top of
 *      our deque, then pick it up and try to resume it.
 * 
 * All these actions are protected by the lock on the current
 * worker, and the lock on the parent (control) full frame.
 *
 * Step 1 should be done while holding the lock on the previous
 * iteration's data block.
 *
 * Steps 2, 3, and 4 should happen while holding the lock on the
 * current iteration's data block.
 *
 * Steps 5 and 6 are protected by the worker lock and the lock on
 * the parent full frame lock, i.e., on iter_ff->parent.
 *
 * The locks on the data block are necessary because the
 * __cilkrts_piper_stage_wait and __cilkrts_piper_throttle
 * routines only lock data blocks when working with iterations,
 * not the parent full frame.
 */ 
full_frame*
piper_finish_iter_and_get_next(__cilkrts_worker *w,
                               full_frame *iter_ff_uncast)
{
    full_frame *resume_ff = NULL;
    CILK_ASSERT(FF_PIPE_ITER == iter_ff_uncast->frame_type);
        
    pipe_iter_full_frame* pipe_iter_ff =
        static_cast<pipe_iter_full_frame*>(iter_ff_uncast);
    __cilkrts_pipe_iter_data* iter_data = pipe_iter_ff->data;
    __cilkrts_pipe_iter_data* next_iter_data = iter_data->right;
    __cilkrts_pipe_iter_data *prev_iter_data = iter_data->left;
    full_frame* parent_ff = pipe_iter_ff->parent;

    // Step 1: Decrement cleanup counter on previous iteration,
    piper_try_free_iter_data_block_common(prev_iter_data,
                                          w,
                                          true /*need_lock_acquire */);
        
    // Finishing the current iteration should hold the lock on the
    // current iteration.
    BEGIN_WITH_PIPE_DATA_LOCK(iter_data) {
        // Move to the runtime finish iteration stage.
        piper_move_iter_stage_to_done(iter_data);
            
        // Step 2: if the next iteration is suspended, then we
        // want to resume it.

        BEGIN_WITH_PIPE_DATA_LOCK(next_iter_data) {
            resume_ff = piper_try_resume_next_iteration_helper(w, iter_data);
        } END_WITH_PIPE_DATA_LOCK(next_iter_data);
        
        // Step 3: Decrement the cleanup counter on the current
        // iteration.
        piper_try_free_iter_data_block_common(iter_data,
                                              w,
                                              false /* we already have lock */);

        // Step 4: Destroy the current iter_ff frame.
        destroy_iter_frame(w, pipe_iter_ff);
    } END_WITH_PIPE_DATA_LOCK(iter_data);

    // Step 5.  Clear the flag that says we have a pipeline
    // iteration on top.  If we are going to resume the next
    // iteration, we'll set it again later.
    if (w->l->implicit_deque_flags.iter_ff_on_top) {
        w->l->implicit_deque_flags.iter_ff_on_top = 0;
    }

    // Step 6: If we aren't going to resume the next iteration,
    // try to resume the parent instead.  We know parent_ff won't
    // disappear on us because we are holding the full frame lock
    // on it.
    if (!resume_ff) {
        resume_ff = piper_try_remove_implicit_parent(w, parent_ff);
    }
    return resume_ff;
}

full_frame *piper_try_implicit_steals(__cilkrts_worker *w,
                                      __cilkrts_worker *victim,
                                      full_frame *iter_ff_uncast)
{
    full_frame *resume_ff = NULL;
    CILK_ASSERT(FF_PIPE_ITER == iter_ff_uncast->frame_type);
    ASSERT_WORKER_LOCK_OWNED(victim);
    pipe_iter_full_frame* pipe_iter_ff =
        static_cast<pipe_iter_full_frame*>(iter_ff_uncast);
    __cilkrts_pipe_iter_data* iter_data = pipe_iter_ff->data;
    __cilkrts_pipe_iter_data* next_iter_data = iter_data->right;
    CILK_ASSERT(iter_data);

    // First try to get the implicit parent.
    //
    // FIXME!  I think there could be a race bug here, and we
    // somehow end up in a situation where multiple workers end up
    // with the implicit parent resume flag set.
    //
    // Need to clarify what the synchronization on the
    // parent_resume bits is...
    resume_ff = piper_try_remove_implicit_parent(victim,
                                                 pipe_iter_ff->parent);

    if (!resume_ff) {
        // Next, try pick up a suspended next iteration.

        BEGIN_WITH_PIPE_DATA_LOCK(next_iter_data) {
            PIPER_DBG_PRINTF(3,
                             "W=%d: iter_ff=%p, going to look at next_iter_ff=%p\n",
                             w->self, iter_ff, next_iter_data->iter_ff_link);
            resume_ff = piper_try_resume_next_iteration_helper(w, iter_data);

            PIPER_DBG_PRINTF(3,
                             "W=%d: implicit steal from victim %d. iter_ff=%p, resume_ff=%p, call_stack=%p\n",
                             w->self,
                             victim->self,
                             iter_ff,
                             resume_ff,
                             resume_ff ? resume_ff->call_stack : NULL);
        } END_WITH_PIPE_DATA_LOCK(next_iter_data);            
    }
    return resume_ff;
}

static void
piper_leave_iter_frame_attached_exit(__cilkrts_pipe_iter_data* iter_data)
{
    PIPER_DBG_PRINTF(1,
                     "W=%p, iter_data=%p ... leave_iter_helper normal exit, num=%ld\n",
                     iter_data->sf->worker,
                     iter_data,
                     iter_data->iter_num);

    // Since the continuation of the spawn of current iteration
    // has not been stolen, we know cleaning up either the
    // previous iteration or this one can not reenable the control
    // frame via a throttlign edge. (Throttling edges need to be
    // at least from cleanup of iteration i to the beginning of
    // iteration of i + K, with K >= 2.

    // Move to the runtime finish iteration stage.
    piper_move_iter_stage_to_done(iter_data);

    // Cleanup the previous iteration's data block.
    piper_try_free_iter_data_block_common(iter_data->left, NULL, true);

    // Cleanup for our iteration.
    piper_try_free_iter_data_block_common(iter_data, NULL, true);
}


// A simulated steal finishes up a pipeline iteration in the same
// way as a normal exit.
void piper_leave_iter_frame_for_simulated_steal(__cilkrts_stack_frame *control_sf)
{
    __cilkrts_pipe_control_frame* pcf =
        piper_cast_pipe_control_frame(control_sf);
    __cilkrts_pipe_iter_data* iter_data =
        __cilkrts_piper_get_current_iter_data(pcf);

    // Quit the current iteration normally, as though it was not
    // stolen.
    piper_leave_iter_frame_attached_exit(iter_data);
}

__cilkrts_pipe_iter_data*
piper_get_iter_data_for_ff(full_frame *iter_ff)
{
    return static_cast<pipe_iter_full_frame*>(iter_ff)->data;
}


/********************************************************************/
// Other functions defined for ABI.

static void piper_allocate_memory_for_control_data(__cilkrts_pipe_control_frame *pcf) {
    size_t header_size = sizeof(pipe_control_data_t);

    // Round the header size up to the next 64-byte boundary.
    const size_t align_mask = 64 - 1;
    size_t remainder = header_size & align_mask;
    if (remainder) {
        header_size += (align_mask + 1 - remainder);
    }
    CILK_ASSERT((header_size & align_mask) == 0);

    size_t control_data_size = header_size +
        sizeof(__cilkrts_pipe_iter_data) * pcf->buffer_size;
    pcf->control_data =
        (pipe_control_data_t*)__cilkrts_malloc(control_data_size);

    pcf->control_data->control_data_size = control_data_size;

    // Calculate a pointer to the memory that comes immediately
    // after the control data header for the iteration buffer.
    // TBD(jsukha): I should probably verify that iteration frames
    // are aligned properly...
    pcf->control_data->iter_buffer =
        (__cilkrts_pipe_iter_data*)((size_t)(pcf->control_data) + header_size);

    // Allocate memory for full frames ahead of time.
    pcf->control_data->ff_buffer =
        (pipe_iter_full_frame*)__cilkrts_malloc(sizeof(pipe_iter_full_frame) *
                                                pcf->buffer_size);

#if PIPER_DEBUG >= 1
    {
        __cilkrts_worker *dbg_w = __cilkrts_get_tls_worker();
        PIPER_DBG_PRINTF(1,
                         "W=%d. control_data_size = %llu, control_data = %p, iter_buffer=%p, header size = %llu\n",
                         dbg_w ? dbg_w->self : -1,
                         control_data_size,
                         pcf->control_data,
                         pcf->control_data->iter_buffer,
                         header_size);
    }
#endif // PIPER_DEBUG >= 1
}

static void piper_deallocate_memory_for_control_data(__cilkrts_pipe_control_frame *pcf)
{
    CILK_ASSERT(pcf);
    CILK_ASSERT(pcf->control_data->ff_buffer);
    __cilkrts_free(pcf->control_data->ff_buffer);
    __cilkrts_free(pcf->control_data);
}    


static void
pipe_control_frame_init_data_structures(__cilkrts_pipe_control_frame* pcf,
                                        int buffer_size) 
{
    PIPER_DBG_PRINTF(1, "Control frame %p. init the data structures\n", pcf);
        
    // Initialize the fields for the control frame
    pcf->buffer_size = buffer_size;
    
    // Iteration 0 starts as a sentinel.  Iteration 1 is the next one
    // we should actually start executing, and it initially enabled.
    pcf->current_iter = 1;
    
    // Test condition starts as true.
    pcf->test_condition = true;

    // The control full frame initially starts as NULL.  When we steal
    // the control (stack) frame for the first time, we lazily
    // populate this field.
    pcf->control_ff = NULL;

    // Not lazily allocating space for control data any more.
    // TBD: We should really just put this "allocation" on the stack...
    piper_allocate_memory_for_control_data(pcf);

    // For saving reducer map when suspending execution.
    pcf->control_data->saved_reducer_map = NULL;

    // Go ahead and initialize all the full frames.
    //  TBD: We could be lazy about this process, and initialize only
    //  when we use it for the first time.  
    for (int i = 0; i < pcf->buffer_size; ++i) {
        pipe_iter_full_frame* iter_ff =
            &pcf->control_data->ff_buffer[i];
        // Initialize the "normal" fields of this full frame.
        __cilkrts_init_full_frame(iter_ff, NULL);
        // Mark this full frame as an iteration frame.
        iter_ff->frame_type = FF_PIPE_ITER;
        // We are our own pipe_iter root.
        iter_ff->pipe_iter_root = iter_ff;
        iter_ff->saved_reducer_map = NULL;
        iter_ff->suspended_ff = NULL;

        // Initialize fields that live in the full frame.
        spin_mutex_init(&iter_ff->pipe_data_lock);
        iter_ff->iter_status = ITER_FINISHED;
        iter_ff->block_status = BLOCK_AVAILABLE;
        
        // Link together the data block and the iteration buffer.
        pcf->control_data->iter_buffer[i].iter_ff_link = iter_ff;
        iter_ff->data = &pcf->control_data->iter_buffer[i];
        pcf->control_data->iter_buffer[i].control_sf = pcf;

        // But we aren't using it yet.
        pcf->control_data->ff_buffer[i].in_use = false;
    }

    CILK_ASSERT(buffer_size >= 2);

    // Link all the iteration frames in the control buffer together.
    __cilkrts_pipe_iter_data* buf =
        pcf->control_data->iter_buffer;
    buf[0].left = &buf[buffer_size-1];
    buf[buffer_size-1].right = &buf[0];
    for (int i = 1; i < buffer_size; ++i) {
        buf[i].left = &buf[i-1];
        buf[i-1].right = &buf[i];
        PIPER_DBG_PRINTF(1, "buffer iter %d: has ptr %p\n", i, &buf[i]);
    }

    // Initialize the cleanup counters for each of the iterations.
    cleanup_counter_init_buffer(pcf->control_data->ff_buffer,
                                buffer_size);

    // Mark the first iteration as a sentinel.
    // Sentinel iteration starts with cleanup counter of 1, and
    // standard exec.  The sentinel iteration looks as though it was
    // executed serially before the real first iteration.
    buf[0].iter_num = 0;
    buf[0].stage = CILK_PIPE_RUNTIME_FINISH_ITER_STAGE;


    pcf->control_data->ff_buffer[0].block_status = BLOCK_IN_USE;
    pcf->control_data->ff_buffer[0].iter_status = ITER_STANDARD_EXEC;

    // Iteration immediately after 0 is reserved, but hasn't started
    // executing yet.
    pcf->control_data->ff_buffer[1].block_status = BLOCK_IN_USE;
}

static void
pipe_control_frame_cleanup_data_structures(__cilkrts_pipe_control_frame* pcf)
{
    PIPER_DBG_PRINTF(1, "Control frame %p. cleanup the data structures.\n", pcf);
    
    if (pcf->control_data) {
        piper_deallocate_memory_for_control_data(pcf);
        pcf->control_data = NULL;
    }
}


CILK_ABI_VOID
__cilkrts_piper_enter_control_frame(__cilkrts_pipe_control_frame* pcf,
                                    int buffer_size)
{
    // Enter the frame.
    __cilk_pipe_fake_enter_frame(&pcf->sf);

    // Mark the control frame as special.  Set the flags after the
    // enter frame, because the enter frame clears the flags...
    pcf->sf.flags |= CILK_FRAME_PIPER;
    //        pcf->sf.frame_type = CILK_PIPE_CONTROL_SF;

    // Initialize data structures.
    pipe_control_frame_init_data_structures(pcf, buffer_size);
    PIPER_DBG_PRINTF(1,
                     "W=%d: starting with pcf->worker=%p, pcf=%p. pcf->current_iter is %ld.\n",
                     pcf->sf.worker->self, pcf->sf.worker, pcf, pcf->current_iter);
    if (PIPER_DEBUG >= 1) {
        CILK_ASSERT(pcf->sf.worker == __cilkrts_get_tls_worker());
    }
}

CILK_ABI_VOID
__cilkrts_piper_cleanup_extended_control_frame(__cilkrts_pipe_control_frame* pcf)
{
    // This frame should have its worker field set properly.
    if (PIPER_DEBUG >= 1) {
        CILK_ASSERT(pcf->sf.worker == __cilkrts_get_tls_worker());
    }
    pipe_control_frame_cleanup_data_structures(pcf);
}

CILK_ABI_VOID
__cilkrts_piper_init_iter_frame(__cilkrts_pipe_iter_data *iter_data,
                                __cilkrts_pipe_iter_num_t iter_num)
{
    pipe_iter_full_frame *iter_ff = iter_data->iter_ff_link;
    CILK_ASSERT(BLOCK_IN_USE == iter_ff->block_status);
    CILK_ASSERT(ITER_FINISHED == iter_ff->iter_status);

    iter_ff->iter_status = ITER_STANDARD_EXEC;

    // iter_data->has_enabled_next_iter = 0;
    iter_data->iter_num = iter_num;
    iter_data->stage = CILK_STAGE_USER_TO_SYS(0);

    // Start with a reasonable cached value for the left stage
    // counter.
    iter_data->cached_left_stage = CILK_STAGE_USER_TO_SYS(0);

    // Cleanup counter starts at 3.
    CILK_ASSERT(cleanup_counter_is_reset(iter_ff));
}

CILK_ABI_VOID
__cilkrts_piper_leave_iter_helper_frame(__cilkrts_pipe_iter_data* iter_data)
{
    __cilkrts_pipe_iter_num_t cilk_user_stage_num
        = iter_data->iter_num - CILK_PIPE_INITIAL_ITER_OFFSET;
    CILK_ASSERT(iter_data->sf->worker);

    // Pop the iteration helper off the call chain.
    CILK_PIPE_FAKE_POP_FRAME(*(iter_data->sf));

    // Finish the final user stage (i.e., move the stage counter,
    // past the final user stage so the next iteration can start
    // its final user stage).
    piper_finish_final_user_stage(iter_data);

    // TBD: Does this intrinsic notification need to be later for
    // Cilkscreen?  Not quite sure.
    PIPER_ZCA("cilk_pipe_iter_leave_end", &cilk_user_stage_num);

    __cilkrts_leave_frame(iter_data->sf);
        
    piper_leave_iter_frame_attached_exit(iter_data);
}

CILK_ABI_VOID
__cilkrts_piper_iter_helper_demotion(__cilkrts_pipe_iter_data* iter_data,
                                     __cilkrts_pipe_control_frame* pcf)
{
    __cilkrts_worker *w = __cilkrts_get_tls_worker();
    PIPER_DBG_PRINTF(1,
                     "W=%d: iter helper demotion. iter_data = %p, pcf=%p, w->current_sf=%p\n",
                     w->self,
                     iter_data,
                     pcf,
                     w->current_stack_frame);

    // The current frame had better be stolen and promoted.
    CILK_ASSERT(w->current_stack_frame->flags & CILK_FRAME_STOLEN);

    __cilkrts_stack_frame *current_sf = w->current_stack_frame;
    __cilkrts_stack_frame *iter_sf = iter_data->sf;

    PIPER_DBG_PRINTF(1,
                     "W=%d: iter_helper_demotion: current_sf = %p\n",
                     w->self, current_sf);

    // The current full frame is either the iteration helper
    // itself, or the iteration body iteration itself, if the
    // iteration body is a spawning function.  "Return" once or
    // twice until we have returned from the spawn helper.

    // Return from the iteration body, if it exists.
    if (current_sf != iter_data->sf) {
        __cilkrts_return(w);
    }

    CILK_ASSERT(iter_sf == w->current_stack_frame);

    // Now return from the helper itself.  It ok to return
    // from the iteration helper because we know at this
    // point, we are the rightmost iteration, and no
    // additional steals have happened from my parent.  So it
    // is still ok to just "pretend" like I was never
    // stolen...

    // Actually do the return from the iteration helper.
    __cilkrts_return(w);
        
    PIPER_DBG_PRINTF(1,
                     "W=%d: iter_helper_demotion: after fake return, new w->current_stack_frame is %p. iter_data=%p, current_sf=%p, w->l->core_frame_ff=%p\n",
                     w->self,
                     w->current_stack_frame,
                     iter_data,
                     current_sf,
                     w->l->core_frame_ff);
        
    // Push the one or two __cilkrts_stack_frame's we just popped
    // back onto the call chain.
    iter_data->sf->flags = CILK_PIPE_FAKE_VERSION_FLAG;
    iter_data->sf->call_parent = w->current_stack_frame;
    if (current_sf != iter_sf) {
        current_sf->flags = CILK_PIPE_FAKE_VERSION_FLAG;
        current_sf->call_parent = iter_sf;
    }
    w->current_stack_frame = current_sf;
}
    
CILK_ABI_VOID
__cilkrts_piper_throttle(__cilkrts_pipe_control_frame *pcf)
{
    __cilkrts_pipe_iter_data *iter_data =
        __cilkrts_piper_get_current_iter_data(pcf);
    __cilkrts_pipe_iter_data* next_iter_data = iter_data->right;
    pipe_iter_full_frame *next_iter_ff = next_iter_data->iter_ff_link;
        
    int try_suspend = 0;

    BEGIN_WITH_PIPE_DATA_LOCK(next_iter_data) {
        if (cleanup_counter_ready_for_reuse(next_iter_ff)) {
            // Fast path.  Block is ready to be reused, so we can
            // resume immediately.
            cleanup_counter_start_throttle_point(next_iter_ff);
            CILK_ASSERT(cleanup_counter_ready_to_resume_after_throttle(next_iter_ff));
        }
        else {
            // Slow path.  We are going to try to suspend.
            CILK_ASSERT(pcf->control_ff);
            CILK_ASSERT(pcf->sf.flags & CILK_FRAME_UNSYNCHED);
            try_suspend = 1;
        }
    } END_WITH_PIPE_DATA_LOCK(next_iter_data);

    if (try_suspend) {
        // To try to suspend, we we first switch stacks into the
        // runtime, and save state into the full frame.  The
        // update to the cleanup count for throttling happens as
        // the last step, so that the other decrements to cleanup
        // count have a chance to happen.

        piper_sched_suspend_pipe_control_frame(pcf->sf.worker,
                                               pcf->control_ff,
                                               &pcf->sf);

        // The above method does not return until we are ready to
        // reuse the iteration block and resume execution.
    }
    
    // When we reach this point, then we should not need to
    // throttle any more, and we should set up for the next
    // iteration.
    CILK_ASSERT(!piper_need_to_throttle_control_frame(pcf->control_ff,
                                                      &pcf->sf));

    // Now that we can pass the throttling point, we can setup
    // the block for the next iteration.
    cleanup_counter_reset_for_new_iter(next_iter_ff);
    next_iter_ff->block_status = BLOCK_IN_USE;

    CILK_ASSERT(cleanup_counter_is_reset(next_iter_ff));
    // Execution gets here after we can start the next iteration,
    // and after we have sufficient space in the buffer.
    CILK_ASSERT((!pcf->control_ff) ||
                (pcf->control_ff->join_counter < pcf->buffer_size));

    if (PIPER_DEBUG >= 1) {
        CILK_ASSERT(pcf->sf.worker == __cilkrts_get_tls_worker());
    }

    // I believe a fence is not needed here to push out the reset of
    // the cleanup counter, because the next iteration that this
    // worker spawns should have a fence to push out the write anyway?
}


CILK_ABI_VOID
__cilkrts_piper_stage_wait(__cilkrts_pipe_iter_data* iter_data,
                           __cilkrts_stack_frame *active_sf)
{
    // Figure out active_sf, if we don't know it already.
    if (!active_sf) {
        __cilkrts_worker *check_w = __cilkrts_get_tls_worker();
        CILK_ASSERT(check_w);
        active_sf = check_w->current_stack_frame;
        CILK_ASSERT(check_w == active_sf->worker);
    }

    // Conceptually, we should have either (active_sf->call_parent
    // == iter_data->sf), or (active_sf == iter_data->sf).
    // But we can't actually assert this fact, because the call_parent
    // links get broken. <grumble...>

    __cilkrts_worker *w = active_sf->worker;
    PIPER_DBG_PRINTF(1,
                     "W=%d: piper_stage_wait. iter_data = %p, (iter %ld, user stage %ld). active_sf=%p\n",
                     w->self,
                     iter_data,
                     iter_data->iter_num,
                     CILK_STAGE_SYS_TO_USER(iter_data->stage),
                     active_sf);

    // Check whether we need to stall or not.
    if (!piper_need_to_stall_at_stage_wait(iter_data))
        return;

    const int backoff_threshhold = 5;
    int backoff_count = 0;

    do {
        if (PIPER_DEBUG >=1) {
            CILK_ASSERT(w == __cilkrts_get_tls_worker());
        }

        // Backoff once for every stage.
        // If we are the first stage, backoff several times. 
        if ((backoff_count == 0) ||
            ((CILK_STAGE_SYS_TO_USER(iter_data->stage) <= 1) &&
             (backoff_count < backoff_threshhold))) {
            __cilkrts_short_pause();
            backoff_count++;
        }
        else {
            // Shouldn't be suspending at a sentinel stage.
            CILK_ASSERT(CILK_PIPE_RUNTIME_FINISH_ITER_STAGE != iter_data->stage);

            pipe_iter_full_frame* iter_ff = iter_data->iter_ff_link;

            // This iteration should be executing, either standard or
            // promoted. 
            CILK_ASSERT((ITER_STANDARD_EXEC == iter_ff->iter_status) ||
                        (ITER_PROMOTED_ACTIVE == iter_ff->iter_status));

            // If the currently executing iteration has is not
            // promoted, then we need to promote it in order to
            // suspend.
            if (ITER_STANDARD_EXEC == iter_ff->iter_status) {
                PIPER_DBG_PRINTF(1,
                                 "W=%d: in wait, iter %ld. should be promoting rightmost iter\n",
                                 w->self,
                                 iter_data->iter_num);

                BEGIN_WITH_WORKER_LOCK(w) {
                    // Check again for sure, while holding the
                    // worker lock.  A steal which sets iter_ff
                    // has to happen while holding the worker
                    // lock.
                    if (ITER_PROMOTED_ACTIVE == iter_ff->iter_status) {
                        full_frame* iter_ff_check =
                            static_cast<full_frame*>(iter_data->iter_ff_link);
                        CILK_ASSERT(iter_ff_check == w->l->core_frame_ff);
                    }
                    else {
                        piper_sched_promote_rightmost_iter(w, iter_data);
                    }
                } END_WITH_WORKER_LOCK(w);
            }
            CILK_ASSERT(iter_ff->data == iter_data);

            // We found an iter_ff, but we did a speculative
            // check, since we didn't grab the w's worker lock.
            // So technically a steal from w may be in progress,
            // and we didn't quite finish the steal yet.  Grab the
            // lock and wait for the steal to finish...

            full_frame* current_ff = w->l->core_frame_ff;

            // We expect either iter_ff is the current full frame,
            // or (and active_sf is the iteration stack frame), or
            // that the current full frame is a child of iter_ff
            // (if active_sf is the child of iter_data->sf).
            int iter_is_active = (current_ff == iter_ff);
            int call_child_of_iter_is_active = (current_ff && (current_ff->parent == iter_ff));
            if (!(iter_is_active || call_child_of_iter_is_active)) {
                BEGIN_WITH_WORKER_LOCK(w) {
                    current_ff = w->l->core_frame_ff;
                    CILK_ASSERT(current_ff);
                    CILK_ASSERT((current_ff == iter_ff) ||
                                (current_ff->parent == iter_ff));
                    CILK_ASSERT(w->l->implicit_deque_flags.iter_ff_on_top);
                } END_WITH_WORKER_LOCK(w);
            }
                
            PIPER_DBG_PRINTF(1,
                             "W=%d: about to call suspend frame on iter %p, iter_num=%ld\n",
                             w->self,
                             &iter_data->sf,
                             iter_data->iter_num);

            w = piper_sched_suspend_pipe_iter_frame(w,
                                                    iter_ff,
                                                    active_sf);

                
            if (PIPER_DEBUG >= 1)
            {
                __cilkrts_worker *check_w = __cilkrts_get_tls_worker();
                CILK_ASSERT(check_w == w);
                if (check_w->current_stack_frame != iter_data->sf) {
                    PIPER_DBG_PRINTF(1,
                                     "CheckW=%d: we are resuming a frame %p that isn't %p\n",
                                     check_w->self,
                                     check_w->current_stack_frame,
                                     iter_data->sf);
                }
                else {
                    CILK_ASSERT(check_w == iter_data->sf->worker);
                }
                CILK_ASSERT(check_w->current_stack_frame == active_sf);
            }
        }
    } while (piper_need_to_stall_at_stage_wait(iter_data));
}

__CILKRTS_END_EXTERN_C
