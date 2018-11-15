/* piper_abi.h                  -*-C-*-
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
 * @file piper_abi.h
 *
 * @brief Data structures and definitions for compiler for pipelining.
 */


#ifndef INCLUDED_PIPER_ABI_H_
#define INCLUDED_PIPER_ABI_H_

//#include <cilk/abi.h>
#include <internal/abi.h>

/// Default debugging level for pipeline constructs.
#ifndef PIPER_DEBUG
#   define PIPER_DEBUG 0
#endif

// Debug printf.
#if PIPER_DEBUG > 0
#   define PIPER_DBG_PRINTF(lvl, _fmt, ...)      \
    do {                                         \
       if (lvl <= PIPER_DEBUG)                   \
           fprintf(stderr, _fmt, __VA_ARGS__);   \
    } while (0)
#else
#   define PIPER_DBG_PRINTF(lvl, _fmt, ...) 
#endif

// Switch to turn off insertion of ZCA intrinsics.
#define PIPER_USE_INTRINSICS 1

#ifdef PIPER_USE_INTRINSICS
#   define PIPER_ZCA(name, arg) __notify_zc_intrinsic((char*)name, (void*)arg)
#else
#   define PIPER_ZCA(name, arg) ((void*)arg)
#endif 

typedef int64_t __cilkrts_pipe_stage_num_t;  ///< Type for stage counter number.
typedef int64_t __cilkrts_pipe_iter_num_t;   ///< Type for iteration number.


#define CILK_PIPE_MAX_STAGE_BIT 61

// The system adds this value to user stage numbers.
// (The user sees stages starting at 0.  The runtime sees stages
//  starting at this value). 
#define CILK_PIPE_INITIAL_STAGE_OFFSET 4

#define CILK_PIPE_INITIAL_ITER_OFFSET 1

#define CILK_STAGE_USER_TO_SYS(user_stage) (((__cilkrts_pipe_iter_num_t)user_stage + CILK_PIPE_INITIAL_STAGE_OFFSET))
#define CILK_STAGE_SYS_TO_USER(sys_stage) (((__cilkrts_pipe_iter_num_t)sys_stage - CILK_PIPE_INITIAL_STAGE_OFFSET))

/// The final stage number a user can specify.
#define CILK_PIPE_FINAL_USER_STAGE        ((((__cilkrts_pipe_iter_num_t)1) <<  (CILK_PIPE_MAX_STAGE_BIT+1)) - CILK_PIPE_INITIAL_STAGE_OFFSET)

/// The stage immediately after the final user stage.
#define CILK_PIPE_POST_ITER_CLEANUP_STAGE (CILK_PIPE_FINAL_USER_STAGE+1)

/// Stage used to mark the sentinel frame.  This stage represents the state after all reductions have finished,
/// and we are ready to mark this frame for reuse.
#define CILK_PIPE_RUNTIME_FINISH_ITER_STAGE CILK_STAGE_USER_TO_SYS(CILK_PIPE_FINAL_USER_STAGE+2)

// Special kinds of __cilkrts_stack_frame objects for pipeline parallelism.
// Runtime gets control when one of these frames is initialized.
typedef struct __cilkrts_pipe_control_frame  __cilkrts_pipe_control_frame;   ///< Forwarded declaration for a control frame.
typedef struct __cilkrts_pipe_iter_data   __cilkrts_pipe_iter_data;       ///< Forwarded declaration for iteration data.

/// Convenience typedef for CILK_ABI macro.  The macro seems to
/// complain if we use a (__cilkrts_pipe_iter_data*) directly in the macro.
typedef struct __cilkrts_pipe_iter_data*     __cilkrts_pipe_iter_data_ptr;   

/// Forwarded declaration of a pipe_iter_full_frame.
typedef struct pipe_iter_full_frame pipe_iter_full_frame;


/**
 * @brief Data block for each iteration of a pipeline.
 */
struct __cilkrts_pipe_iter_data  {
    // First, list the fields that get modified.
    /// Cached value for the stage counter for the previous iteration.
    __cilkrts_pipe_stage_num_t cached_left_stage;
    
    /// Pointer to the current stack frame for this iteration.
    /// This pointer is valid only while the iteration is actually
    /// executing.
    __cilkrts_stack_frame* sf;

    /// Next, mostly read-only fields of this frame.
    /// The name for this iteration.
    __cilkrts_pipe_iter_num_t iter_num;

    /// Pointer to the control frame for this pipeline.
    /// This pointer remains valid until the pipeline loop finishes.
    __cilkrts_pipe_control_frame* control_sf;

    /// Stack frame of left sibling.
    __cilkrts_pipe_iter_data *left;

    /// Stack frame of right sibling. 
    __cilkrts_pipe_iter_data *right;

    /**
     *@brief Link to full frame for this data block.
     *
     * This field starts as NULL, and gets filled in only after the
     * continuation of starting this iteration has been stolen.
     */
    pipe_iter_full_frame* iter_ff_link;

    // Padding to separate the data accessed by workers executing the
    // next iteration.
    char next_iter_pad[64];

    /// Current stage counter for this iteration.
    volatile __cilkrts_pipe_stage_num_t stage;

    /// Padding to separate this iteration data block from the next
    /// when this struct is allocated in an array.
    char final_padding[64];
};


typedef struct pipe_control_data_t pipe_control_data_t;

/**
 * @brief Definition of a control frame of a pipeline.
 */
struct __cilkrts_pipe_control_frame {
    /// The normal fields of a __cilkrts_stack_frame.
    /// This field must be first!
    __cilkrts_stack_frame sf;
    
    // Extra compiler-visible fields needed in the control frame.

    /// The iteration we want to try to start in this control frame.
    __cilkrts_pipe_iter_num_t current_iter;
    
    /// The test to check whether we should continue to the next
    /// iteration.
    int test_condition;

    /// Padding to split the fields that are changing (above) from the
    /// fields below which are mostly read-only.
    char split_pad[64];
    
    /// Size of the control buffer.
    int buffer_size; 

    /// Control data linked to this pipe frame.
    // In particular, it points to the buffer of iterations for the
    // pipe_while.
    pipe_control_data_t* control_data;

    /// The control full frame linked to this stack frame.
    /// This full frame is populated the first time
    /// we steal from the control frame.
    full_frame *control_ff;
};



/*********************************************************************
 *  These are the runtime functions that the compiler should know
 *  about.
 *
 *  Compilation of a cilk_pipe_while loop generates calls to these
 *  runtime functions.
 *********************************************************************/

__CILKRTS_BEGIN_EXTERN_C

    /**
     * @brief Get the pointer to the data block for the current
     * iteration, i.e., the last iteration that was spawned / is about
     * to be spawned from the pipe control loop.
     */ 
    CILK_ABI(__cilkrts_pipe_iter_data_ptr)
    __cilkrts_piper_get_current_iter_data(__cilkrts_pipe_control_frame* pcf);

    /**
     *@brief Initialize the control frame for the pipeline loop, and
     *  push it onto the call chain.
     */
    CILK_ABI(void)
    __cilkrts_piper_enter_control_frame(__cilkrts_pipe_control_frame* pcf,
                                        int buffer_size);

    /**
     *@brief Cleanup for the control frame of a pipeline loop.
     *
     * This function should always be run after the sync of the all
     * the pipeline iterations, whether or not a steal happened from
     * the loop.
     */
    CILK_ABI(void)
    __cilkrts_piper_cleanup_extended_control_frame(__cilkrts_pipe_control_frame* pcf);


    /**
     *@brief Initialize the iteration helper frame for spawning an
     *iteration.
     */
    CILK_ABI(void)
    __cilkrts_piper_init_iter_frame(__cilkrts_pipe_iter_data *iter_sf,
                                    __cilkrts_pipe_iter_num_t iter_num);

    /**
     * @brief Leave-frame for the iteration helper.
     */
    CILK_ABI(void)
    __cilkrts_piper_leave_iter_helper_frame(__cilkrts_pipe_iter_data* iter_sf);


    /**
     * When we are about to detach the iteration helper frame, fix the
     * call chain in the runtime if iteration frame has already been
     * stolen from, to pretend that this call chain has never been
     * stolen from / promoted into full frames.
     *
     * This call chain can be promoted if stage 0 of the iteration has
     * nested parallelism.
     *
     * This function is called right before we detach the iteration
     * from the control frame.
     *
     * This effect is achieved by conceptually having the runtime
     * pretend "return" from the iteration body (back to its iteration
     * spawn helper) (in terms of full frames), and restoring the call
     * chain.  Thus, after this call, all data structures look like a
     * steal never happened from the iteration frame.
     */
    CILK_ABI(void)
    __cilkrts_piper_iter_helper_demotion(__cilkrts_pipe_iter_data *iter_sf,
                                         __cilkrts_pipe_control_frame *pcf);

    /**
     * @brief Possibly stall until we can start the next iteration.
     *
     * If the worker starting this function can not keep going, it may
     * go off to work-steal or do other work.  This call may return on
     * a different worker than it started on,
     */
    CILK_ABI(void)
    __cilkrts_piper_throttle(__cilkrts_pipe_control_frame* pcf);
    
    /**
     * @brief Stall until the previous iteration has moved past
     * iteration iter_data->stage.
     * 
     * If the worker starting this function can not keep going, it may
     * go off to work-steal or do other work.  Thus, this call may
     * return on a different worker than it started on.
     *
     * This function is called as part of a @c cilk_stage_wait in the
     * body of pipeline iteration.
     *
     * If the body of the pipeline iteration itself has pushed a
     * __cilkrts_stack_frame onto the call chain (e.g., it is a
     * function that has spawned), then @c active_sf should be the
     * call child of @c iter_data->sf.
     *
     * Otherwise, @c active_sf should equal @c iter_data->sf (and the
     * pipeline iteration should not have a __cilkrts_stack_frame
     * associated with it).
     * 
     * NOTE: For now, we allow @c active_sf to be NULL.  In this case,
     * the runtime will perform a TLS lookup of the current worker, to
     * try to figure out what the active stack frame is.  (I think it
     * should be faster if the compiled code passed it in, but maybe
     * it is difficult to generate?)
     *
     * @param iter_data   The data block for the current iteration.
     * @param active_sf  The __cilkrts_stack_frame that is currently executing, or NULL.
     */
     CILK_ABI(void)
    __cilkrts_piper_stage_wait(__cilkrts_pipe_iter_data *iter_data,
                               __cilkrts_stack_frame *active_sf);

__CILKRTS_END_EXTERN_C


#endif // INCLUDED_PIPER_ABI_H_
