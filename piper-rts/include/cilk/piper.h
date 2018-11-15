/* piper.h                  -*-C++-*-
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
 * @file piper.h
 *
 * @brief Definitions of structures for pipeline parallelism.
 */

#ifndef INCLUDED_PIPER_DOT_H
#define INCLUDED_PIPER_DOT_H

#include <cilk/piper_abi.h>
#include <cilk/piper_fake_macros.h>
#include <cilk/cilk_api.h>

// GCC Cilk Plus is currently unhappy with the asserts + nested
// lambdas?  You may need to turn off asserts to get it to compile.
#if PIPER_DEBUG > 0
#   define PIPER_ASSERT(x) assert(x)
#   define PIPER_DBG_CHECK_SF()                                                \
        do {                                                                   \
           __cilkrts_worker* check_w = __cilkrts_get_tls_worker();             \
           __cilkrts_stack_frame *check_sf = check_w->current_stack_frame;     \
           if (check_sf->worker != check_w)  {                                 \
               PIPER_DBG_PRINTF(0, "ERROR: w=%p, csf=%p, csf->worker_%p\n",    \
                                check_w, check_sf, check_sf->worker);          \
           }                                                                   \
        } while (0)
#else
#   define PIPER_ASSERT(x)
#   define PIPER_DBG_CHECK_SF() 
#endif


/// Inline attribute for iteration helper.  
#if defined(_WIN32) 
#    define CILK_PIPER_NO_INLINE __declspec(noinline)
#else
#    define CILK_PIPER_NO_INLINE __attribute__((noinline))
#endif

/// Define this macro if we want to serialize all our pipe-while
/// loops.
#ifndef CILK_PIPER_SERIALIZE

/// Define the normal macros if we aren't trying to serialize the
/// pipe-while loops.

/// This wrapper function exists to prevent the compiler from inlining
/// the call to the iteration helper.  If this function does get
/// inlined, then addresses to stack-local variables start being
/// shared between iterations...
template <typename IterHelperFunc>
CILK_PIPER_NO_INLINE
void
__cilk_pipe_call_iter_helper_wrapper(__cilkrts_pipe_iter_data* iter_data,
                                     const IterHelperFunc& iter_helper_func)
{
    iter_helper_func(iter_data);
}



__CILKRTS_INLINE
void __cilk_pipe_fake_iter_helper_enter(__cilkrts_pipe_control_frame& pcf,
                                        __cilkrts_pipe_iter_data* iter_data,
                                        __cilkrts_stack_frame& iter_sf)
{
    __cilkrts_pipe_iter_num_t iter_num =
        iter_data->iter_num - CILK_PIPE_INITIAL_ITER_OFFSET;

    PIPER_ZCA("cilk_pipe_iter_enter_begin", &iter_num);
    iter_data->sf = &iter_sf;
    __cilk_pipe_fake_helper_enter_frame(&iter_sf, &(pcf.sf));
    {                                                                   
        /* The part of the detach that initializes the pedigree. */     
        PIPER_ASSERT(iter_sf.call_parent == &(pcf.sf));
        __cilkrts_worker* w = pcf.sf.worker;
        __cilk_pipe_fake_enter_frame_fast(&iter_sf, w);

        // Do pedigree updates if we are working with ABI-1 frames.
#if __CILKRTS_ABI_VERSION >= 1
        iter_sf.spawn_helper_pedigree = w->pedigree;
        pcf.sf.parent_pedigree = w->pedigree;
        w->pedigree.rank = 0;
        w->pedigree.parent = &(iter_sf.spawn_helper_pedigree);
#endif
    }
    PIPER_ZCA("cilk_pipe_iter_enter_end", &iter_num);
}

/**
 * Call this function at the beginning of the body of a pipeline
 * iteration if the iteration body never spawns.
 *
 * This macro forces initialization of __cilk_active_sf.
 */
#define CILK_PIPE_FAKE_NONSPAWNING_ITER_BODY_PROLOG()              \
    do {                                                           \
        __cilk_active_sf = &__cilk_iter_sf;                        \
    } while (0)

/**
 * Call this function at the beginning of the body of a pipeline
 * iteration, if the iteration body is a spawning function.
 * 
 * This macro forces the initialization and push of a
 * __cilkrts_stack_frame for iteration body.
 */
#define CILK_PIPE_FAKE_SPAWNING_ITER_BODY_PROLOG()                 \
    do {                                                           \
        /* Force initialization of current stack frame. */         \
        _Cilk_spawn [](){}();                                      \
        _Cilk_sync;                                                \
        __cilkrts_worker *current_w = __cilkrts_get_tls_worker();  \
        PIPER_ASSERT(current_w);                                   \
        __cilk_active_sf = current_w->current_stack_frame;         \
    } while (0)



// Push the iteration helper onto the worker's deque.
#define CILK_FAKE_PIPE_DETACH_ITER(iter_sf)                                 \
    do {                                                                    \
       __cilkrts_worker* w = (iter_sf)->worker;                             \
       __cilkrts_stack_frame *volatile *tail = *w->tail;                     \
       *tail++ = (iter_sf)->call_parent;                                    \
       *w->tail = tail;                                                      \
       (iter_sf)->flags |= CILK_FRAME_DETACHED;                             \
    } while (0)                      

#define CILK_FAKE_PIPE_TRY_DETACH_FOR_NEXT_ITER(iter_data, iter_sf)            \
    do {                                                                       \
       if (!(CILK_FRAME_DETACHED & (iter_sf)->flags)) {                        \
           /* If the spawn helper is marked as stolen already,          */     \
           /* that means stage 0 had parallelism and was stolen from.   */     \
           /* We need to go into the runtime and fix the call chain     */     \
           /* and worker state so it can be detached properly.          */     \
           if (CILK_FRAME_STOLEN & (iter_sf)->flags) {                         \
               __cilkrts_piper_iter_helper_demotion((iter_data),               \
                                                    (iter_data)->control_sf);  \
           }                                                                   \
           CILK_FAKE_PIPE_DETACH_ITER(iter_sf);                                \
       }                                                                       \
    } while (0)


/**
 * Code to advance the stage number on the current iteration.
 * Should only be invoked from within the pipe_while body.
 *
 * This macro depends explicitly on the names __cilk_iter_sf and
 * __cilk_pcf.
 */ 

#define CILK_STAGE_ADVANCE_NO_DETACH_BODY(user_stage)                               \
    PIPER_ASSERT(CILK_STAGE_USER_TO_SYS(user_stage) >=  __cilk_iter_data->stage);   \
    __cilk_iter_data->stage = CILK_STAGE_USER_TO_SYS(user_stage)                   

#define CILK_STAGE_ADVANCE_BODY(user_stage)                                        \
    CILK_STAGE_ADVANCE_NO_DETACH_BODY(user_stage);                                 \
    CILK_FAKE_PIPE_TRY_DETACH_FOR_NEXT_ITER(__cilk_iter_data, &__cilk_iter_sf)   

#define CILK_STAGE_WAIT_ON_LEFT_DEPENDENCY_BODY(user_stage)            \
   __cilkrts_pipe_stage_num_t left_iter_stage;                         \
   do {                                                                \
       /* First check our cached stage counter */                      \
       left_iter_stage = __cilk_iter_data->cached_left_stage;          \
       if (left_iter_stage > __cilk_iter_data->stage)                  \
           break;                                                      \
       /* Otherwise, read the real stage counter value. */             \
       left_iter_stage = __cilk_iter_data->left->stage;                \
       __cilk_iter_data->cached_left_stage = left_iter_stage;          \
       if (left_iter_stage > __cilk_iter_data->stage)                  \
           break;                                                      \
       /* If we made it to here, then we might need to wait */         \
       __cilkrts_piper_stage_wait(__cilk_iter_data, __cilk_active_sf); \
       PIPER_DBG_CHECK_SF();                                           \
   } while (1)


#define CILK_STAGE_WAIT_BODY(user_stage)                                   \
    /* Now check left to see if our dependency is satisfied.*/             \
    do {                                                                   \
       __cilkrts_pipe_stage_num_t target_s[1] = { (user_stage) };          \
       PIPER_ZCA("cilk_stage_wait_begin", target_s);                       \
       CILK_STAGE_ADVANCE_BODY(user_stage);                                \
       CILK_STAGE_WAIT_ON_LEFT_DEPENDENCY_BODY();                          \
       PIPER_ZCA("cilk_stage_wait_end", target_s);                         \
    } while(0)                                                     

// Same was the body of the wait macro, except that we don't check for
// a detach.  Should only be called when the current stage is > 0.
#define CILK_STAGE_WAIT_NO_DETACH_BODY(user_stage)                         \
    /* Now check left to see if our dependency is satisfied.*/             \
    do {                                                                   \
       __cilkrts_pipe_stage_num_t target_s[1] = { (user_stage) };          \
       PIPER_ZCA("cilk_stage_wait_begin", target_s);                       \
       CILK_STAGE_ADVANCE_NO_DETACH_BODY(user_stage);                      \
       CILK_STAGE_WAIT_ON_LEFT_DEPENDENCY_BODY();                          \
       PIPER_ZCA("cilk_stage_wait_end", target_s);                         \
    } while(0)                                                     
    

/**
 * The last stage_wait to close out an iteration.
 *
 * As written, this code will try to detach the next iteration if we
 * haven't already done so.  Seems slightly wasteful to detach now,
 * but is it really worth constructing a special case to avoid the
 * detach?
 *
 * If we don't detach now, we need to undo the pedigree updates in
 * "leave_frame", even if the DETACHED flag is not set.
 * Note that we are updating the pedigree terms upon entry to the
 * iteration, rather than at the detach point.
 */
#define CILK_PIPE_FAKE_FINAL_STAGE_WAIT()                            \
   do {                                                              \
       if (__cilk_iter_data->stage < CILK_PIPE_FINAL_USER_STAGE) {   \
           CILK_STAGE_WAIT_BODY(CILK_PIPE_FINAL_USER_STAGE);         \
       }                                                             \
   } while (0)



/************************************************************************/
// Macros for the user.

/// Get the current stage number.
#define CILK_PIPE_STAGE()  \
    CILK_STAGE_SYS_TO_USER(__cilk_iter_data->stage)       

#define CILK_STAGE(user_stage)                               \
    do {                                                     \
        __cilkrts_pipe_stage_num_t target_s = (user_stage);  \
        PIPER_ZCA("cilk_stage_begin", &target_s);            \
        CILK_STAGE_ADVANCE_BODY(user_stage);                 \
        PIPER_ZCA("cilk_stage_end", &target_s);              \
    } while (0)

/// Call this version of CILK_STAGE only in stages after stage 0.
/// After stage 0, there is no need to check for detaching the next
/// iteration.
#define CILK_STAGE_AFTER_STAGE_0(user_stage)                 \
    do {                                                     \
        __cilkrts_pipe_stage_num_t target_s = (user_stage);  \
        PIPER_ZCA("cilk_stage_begin", &target_s);            \
        CILK_STAGE_ADVANCE_NO_DETACH_BODY(user_stage);       \
        PIPER_ZCA("cilk_stage_end", &target_s);              \
    } while (0)

#define CILK_STAGE_WITH_CILK_SYNC(user_stage)                 \
    do {                                                      \
        __cilkrts_pipe_stage_num_t target_s = (user_stage);   \
        PIPER_ZCA("cilk_stage_begin", &target_s);             \
        CILK_STAGE_ADVANCE_BODY(user_stage);                  \
        _Cilk_sync;                                           \
        PIPER_ZCA("cilk_stage_end", &target_s);               \
    } while (0);

#define CILK_STAGE_NEXT()                                        \
    do {                                                         \
        __cilkrts_pipe_stage_num_t next_s =                      \
            CILK_STAGE_SYS_TO_USER(__cilk_iter_data->stage) + 1; \
        PIPER_ZCA("cilk_stage_begin", &next_s);                  \
        CILK_STAGE_ADVANCE_BODY(next_s);                         \
        PIPER_ZCA("cilk_stage_end", &next_s);                    \
    } while (0)

#define CILK_STAGE_WAIT(user_stage)                            \
    CILK_STAGE_WAIT_BODY(user_stage)

#define CILK_STAGE_WAIT_WITH_CILK_SYNC(user_stage)             \
    do {                                                       \
        CILK_STAGE_WAIT_BODY(user_stage);                      \
        _Cilk_sync;                                            \
    } while (0)

/// Only call this macro for a CILK_STAGE_WAIT in stages
/// that are after stage 0.
#define CILK_STAGE_WAIT_AFTER_STAGE_0(user_stage)              \
    CILK_STAGE_WAIT_NO_DETACH_BODY(user_stage)

#define CILK_STAGE_WAIT_NEXT()                                   \
    do {                                                         \
        __cilkrts_pipe_stage_num_t next_s =                      \
            CILK_STAGE_SYS_TO_USER(__cilk_iter_data->stage) + 1; \
        CILK_STAGE_WAIT_BODY(next_s);                            \
    } while (0)



template <typename IterHelperFunc>
CILK_PIPER_NO_INLINE
void __cilk_pipe_while_execute_loop(__cilkrts_pipe_control_frame& pcf,
                                    const IterHelperFunc& iter_helper_func,
                                    int K)
{
    /* Prolog for starting a pipe_while loop.  */
    CILK_PIPE_FAKE_FORCE_FRAME_PTR(pcf.sf);
    PIPER_ZCA("cilk_pipe_enter_begin", &pcf);
    __cilkrts_piper_enter_control_frame(&pcf, K);
    /* If the ABI versions on stack frames are confused,      */
    /* execution may die here.                                */
    PIPER_ASSERT(1 == pcf.current_iter);
    PIPER_ZCA("cilk_pipe_enter_end", &pcf);
    
    /* Execution of the loop body. */
    while (pcf.test_condition) {
        // The following statement behaves like        
        //    cilk_spawn iter_func(&pcf, iter);

        // Grab the data block for the current iteration, (which
        // is about to start), and initialize the block.  Make
        // sure to save the iteration number, to avoid a race with
        // the continutation of the spawn.
        __cilkrts_pipe_iter_data* iter_data =
            __cilkrts_piper_get_current_iter_data(&pcf);
        __cilkrts_piper_init_iter_frame(iter_data, pcf.current_iter);

        CILK_PIPE_FAKE_DEFERRED_ENTER_FRAME(pcf.sf);
        PIPER_DBG_PRINTF(2,
                         "After deferred enter frame: pipe_frame->w=%p, this_w=%p\n",
                         pcf.sf.worker,
                         __cilkrts_get_tls_worker());
        CILK_PIPE_FAKE_SAVE_FP(pcf.sf);
        if (__builtin_expect(! CILK_SETJMP(pcf.sf.ctx), 1)) {
            __cilk_pipe_call_iter_helper_wrapper(iter_data,
                                                 iter_helper_func);
        }
        PIPER_DBG_PRINTF(2,
                         "End of iter: pipe_frame.current_iter is %ld\n",
                         pcf.current_iter);

        /* Pause until next iteration has been enabled. This call */
        /* may return on a different worker than it started on.   */
        __cilkrts_piper_throttle(&pcf);

        // Make sure we are resuming on the correct worker.
        if (PIPER_DEBUG >= 1) {
            PIPER_ASSERT(__cilkrts_get_tls_worker() == pcf.sf.worker);
        }

        // Don't make any changes until after we know we are resuming
        // for real.
        pcf.current_iter++;
    }
    /* Do the sync of the pipe_frame. */                                       
    CILK_PIPE_FAKE_SYNC_IMP(pcf.sf);
    PIPER_ZCA("cilk_pipe_leave_begin", &pcf);
    /* Destructor of the piper control frame. */                               
    PIPER_ASSERT(pcf.sf.worker);                                        
    CILK_PIPE_FAKE_POP_FRAME(pcf.sf);                                   
    __cilkrts_piper_cleanup_extended_control_frame(&pcf);               
    PIPER_ZCA("cilk_pipe_leave_end", &pcf);                             
    if ((pcf.sf).flags != CILK_PIPE_FAKE_VERSION_FLAG) {
        __cilkrts_leave_frame(&(pcf.sf));                               
    }                                                                         
}

/**
 * To hand-compile the following pipe_while loop:
 *
 * cilk_pipe_while (i < 10) {
 *    ++i;
 *    int x, y;
 *    x = foo();
 *    cilk_stage_wait(1);    // Begin stage 1, sync with previous iteration.
 *    y= bar(x);
 *    cilk_stage(2);   // Stage 2, but don't sync with previous iteration.
 *    baz(y);
 * }
 *
 * CILK_PIPE_WHILE_BEGIN(i < 10) {
 *    ++i;
 *    int x, y;
 *    x = foo();
 *    CILK_STAGE_WAIT(1);
 *    y = bar(x);
 *    CILK_STAGE(2);
 *    baz(y);
 * } CILK_PIPE_WHILE_END();
 *
 *
 * Variations:
 *
 *   1. To set an explicit throttling limit, begin with:
 *        CILK_PIPE_WHILE_BEGIN_THROTTLED(test_expr, throttle_limit)
 */
#define CILK_PIPE_WHILE_BEGIN(test_expr)                                       \
    CILK_PIPE_WHILE_BEGIN_THROTTLED(test_expr, (__cilkrts_get_nworkers() * 4)) \

#define CILK_PIPE_WHILE_BEGIN_THROTTLED(test_expr, K)                          \
do {                                                                           \
    int __cilk_throttle_limit = (K);                                           \
     __cilkrts_pipe_control_frame __cilk_pcf;                                  \
    auto iter_body_spawn_helper =                                              \
       [&](__cilkrts_pipe_iter_data *__cilk_iter_data) {                       \
       /* The stack frame for this spawn helper. */                            \
       __cilkrts_stack_frame __cilk_iter_sf;                                   \
       /* The pointer to the current stack frame for the body    */            \
       /* of the iteration itself.  This field points to the     */            \
       /*  __cilkrts_stack_frame for the iteration body          */            \
       /* itself, which is either &__cilk_iter_sf, or its child, */            \
       /* depending on whether the iteration body is spawning.   */            \
       __cilkrts_stack_frame* __cilk_active_sf = NULL;                         \
       __cilk_pipe_fake_iter_helper_enter(__cilk_pcf,                          \
                                          __cilk_iter_data,                    \
                                          __cilk_iter_sf);                     \
       PIPER_DBG_PRINTF(2, "iter_data=%ld, cilk_iter_sf=%p\n",                 \
                        __cilk_iter_data->iter_num, &__cilk_iter_sf);          \
       /* NOTE: If this lambda is not nested, then */                          \
       /* we also need to pass in __cilk_iter_sf   */                          \
       /* and __cilk_active_sf                     */                          \
       auto iter_body_func =                                                   \
          [&](__cilkrts_pipe_iter_data * __cilk_iter_data)                     \
          { /* Begin iteration body  */                                        \
            /* Evaluate test condition */                                      \
            __cilk_pcf.test_condition = (test_expr);                           \
            if (__cilk_pcf.test_condition) {                                   \
                PIPER_DBG_PRINTF(2,                                            \
                                 "Actually executing iter %ld on w=%p\n",      \
                                 __cilk_iter_data->iter_num,                   \
                                 __cilkrts_get_tls_worker());                  \
                do /* User code goes into the iter body lambda. */             

                  
#define CILK_PIPE_WHILE_END()                                                  \
                while(0);                                                      \
                PIPER_DBG_PRINTF(2,                                            \
                                 "Actually finished iter %ld on w=%p\n",       \
                                 __cilk_iter_data->iter_num,                   \
                                 __cilkrts_get_tls_worker());                  \
            }                                                                  \
       }; /* End of iteration body function */                                 \
       /* Actually invoke iteration body */                                    \
       iter_body_func(__cilk_iter_data);                                       \
       PIPER_ZCA("cilk_pipe_iter_leave_begin", NULL);                          \
       /* Final stage_wait for the iteration. */                               \
       CILK_PIPE_FAKE_FINAL_STAGE_WAIT();                                      \
       /* Do I actually care about the link to the parent    */                \
       /* control frame?   Maybe not...                      */                \
       PIPER_ASSERT(__cilk_iter_data->control_sf == &__cilk_pcf);              \
       __cilkrts_piper_leave_iter_helper_frame(__cilk_iter_data);              \
    };                                                                         \
    /* Actually execute the pipe_while loop.  */                               \
    __cilk_pipe_while_execute_loop(__cilk_pcf,                                 \
                                   iter_body_spawn_helper,                     \
                                   __cilk_throttle_limit);                     \
} while (0)

#else

/// Serial elision macros.
#define CILK_PIPE_WHILE_BEGIN(test_expr)                   \
    while (test_expr) {                                    \
        __cilkrts_pipe_stage_num_t __cilk_current_s = 0;   \
        do 
#define CILK_PIPE_WHILE_BEGIN_THROTTLED(test_expr, K)      \
    { (K); }                                               \
    while (test_expr) {                                    \
        __cilkrts_pipe_stage_num_t __cilk_current_s = 0;   \
        do 

#define CILK_PIPE_WHILE_END()                              \
        while (0);                                         \
    }


#define CILK_PIPE_STAGE() __cilk_current_s

#define CILK_SERIAL_STAGE_ADVANCE(user_stage) __cilk_current_s = user_stage
#define CILK_SERIAL_STAGE_INCREMENT()         __cilk_current_s++

#define CILK_STAGE_WAIT(user_stage)               CILK_SERIAL_STAGE_ADVANCE(user_stage)
#define CILK_STAGE_WAIT_AFTER_STAGE_0(user_stage) CILK_SERIAL_STAGE_ADVANCE(user_stage)
#define CILK_STAGE(user_stage)                    CILK_SERIAL_STAGE_ADVANCE(user_stage)
#define CILK_STAGE_AFTER_STAGE_0(user_stage)      CILK_SERIAL_STAGE_ADVANCE(user_stage)

#define CILK_STAGE_NEXT()                         CILK_SERIAL_STAGE_INCREMENT()
#define CILK_STAGE_WAIT_NEXT()                    CILK_SERIAL_STAGE_INCREMENT()





#endif // defined(CILK_PIPER_SERIALIZE)

#endif // !defined(INCLUDED_PIPER_DOT_H)
