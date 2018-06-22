#include <cilk/cilk.h>
#include <internal/abi.h>
#include <runtime/rts-common.h>
#include <iostream>
#include <functional>
#include <assert.h>
#include "local_state.h"
#include "full_frame.h"
#include "cilk_fiber.h"
#include "scheduler.h"
#include "os.h"
#include "sysdep.h"
#include "cilk-ittnotify.h"
#include "jmpbuf.h"
#include <cstring>

extern unsigned long ZERO;

extern CILK_ABI_VOID __cilkrts_future_sync(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_frame(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_parent_frame(__cilkrts_stack_frame *sf);
extern "C" {
extern CILK_ABI_VOID user_code_resume_after_switch_into_runtime(cilk_fiber*);
extern CILK_ABI_THROWS_VOID __cilkrts_rethrow(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID update_pedigree_after_sync(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_detach(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_pop_frame(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_enter_frame_1(__cilkrts_stack_frame *sf);
extern void fiber_proc_to_resume_user_code_for_random_steal(cilk_fiber *fiber);
extern char* get_sp_for_executing_sf(char* stack_base,
                                     full_frame *ff,
                                     __cilkrts_stack_frame *sf);

typedef void (*void_func_t)(void);


static void fiber_proc_to_resume_user_code_for_future(cilk_fiber *fiber) {

    cilk_fiber_data *data = cilk_fiber_get_data(fiber);
    __cilkrts_stack_frame* sf = data->resume_sf;
    full_frame *ff;

    CILK_ASSERT(sf);

    // When we pull the resume_sf out of the fiber to resume it, clear
    // the old value.
    data->resume_sf = NULL;
    CILK_ASSERT(sf->worker == data->owner);

    ff = *(sf->worker->l->frame_ff);

    // For Win32, we need to overwrite the default exception handler
    // in this function, so that when the OS exception handling code
    // walks off the top of the current Cilk stack, it reaches our stub
    // handler.
    
    // Also, this function needs to be wrapped into a try-catch block
    // so the compiler generates the appropriate exception information
    // in this frame.
    
    {
        char* new_sp = get_sp_for_executing_sf(cilk_fiber_get_stack_base(fiber), ff, sf);
        SP(sf) = new_sp;
        
        // Notify the Intel tools that we're stealing code
        ITT_SYNC_ACQUIRED(sf->worker);
        NOTIFY_ZC_INTRINSIC("cilk_continue", sf);

        // TBD: We'd like to move TBB-interop methods into the fiber
        // eventually.
        cilk_fiber_invoke_tbb_stack_op(fiber, CILK_TBB_STACK_ADOPT);
        

        sf->flags &= ~CILK_FRAME_SUSPENDED;

        // longjmp to user code.  Don't process exceptions here,
        // because we are resuming a stolen frame.
        sysdep_longjmp_to_sf(new_sp, sf, NULL);
        /*NOTREACHED*/
        // Intel's C compiler respects the preceding lint pragma
        CILK_ASSERT(! "Should not return into this function! :(\n");
    }
}

CILK_ABI_VOID __cilkrts_switch_fibers_back(__cilkrts_stack_frame* first_frame, cilk_fiber* curr_fiber, cilk_fiber* new_fiber) {
    cilk_fiber_data* new_fiber_data = cilk_fiber_get_data(new_fiber);

    cilk_fiber_remove_reference_from_self_and_resume_other(curr_fiber, &(__cilkrts_get_tls_worker()->l->fiber_pool), new_fiber);
}

CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame) {
    __cilkrts_worker* curr_worker = __cilkrts_get_tls_worker();
    cilk_fiber* new_exec_fiber = cilk_fiber_allocate(&(curr_worker->l->fiber_pool));
    // TODO: Handle the case that it is null more gracefully
    CILK_ASSERT(new_exec_fiber != NULL);
    CILK_ASSERT(first_frame);
    cilk_fiber_data* new_exec_fiber_data = cilk_fiber_get_data(new_exec_fiber);

    new_exec_fiber_data->resume_sf = first_frame;
    cilk_fiber_reset_state(new_exec_fiber, fiber_proc_to_resume_user_code_for_future);

    cilk_fiber *curr_fiber = NULL;
    // TODO: Should this be a full_frame lock or something? Do we need a lock?
    __cilkrts_worker_lock(curr_worker);
    full_frame *ff = *curr_worker->l->frame_ff;
    __cilkrts_frame_lock(curr_worker, ff);

        //ff->call_stack->worker = curr_worker;
        //curr_worker->current_stack_frame = first_frame;

        if (ff->future_fibers_tail) {
            curr_fiber = ff->future_fibers_tail->fiber;
        } else {
            curr_fiber = ff->fiber_self;
        }
        __cilkrts_enqueue_future_fiber(ff, new_exec_fiber);
        //ff->future_fiber = new_exec_fiber;
        

    __cilkrts_frame_unlock(curr_worker, ff);
    __cilkrts_worker_unlock(curr_worker);

    //cilk_fiber_get_data(curr_fiber)->resume_sf = NULL;

    cilk_fiber_suspend_self_and_resume_other(curr_fiber, new_exec_fiber);

    if (first_frame->flags & CILK_FRAME_STOLEN) {
        user_code_resume_after_switch_into_runtime(curr_fiber);
        CILK_ASSERT(! "We should not return here!");
    }
}

static CILK_ABI_VOID __attribute__((noinline)) __spawn_future_helper(std::function<void(void)> func) {
    int* dummy = (int*) alloca(ZERO);
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

        func();

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_future_frame(&sf);
}

CILK_ABI_VOID __attribute__((noinline)) __spawn_future_helper_helper(std::function<void(void)> func) {
    int* dummy = (int*) alloca(ZERO);
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);


    sf.flags |= CILK_FRAME_FUTURE_PARENT;

    // At one point this wasn't true. Fixed a bug in cilk-abi.c that set the owner when it should not have.
    CILK_ASSERT(NULL == cilk_fiber_get_data(__cilkrts_get_tls_worker()->l->scheduling_fiber)->owner);

    // Just me being paranoid...
    //CILK_ASSERT(__cilkrts_get_tls_worker()->l->frame_ff->future_flags == 0);
    CILK_ASSERT((sf.flags & CILK_FRAME_STOLEN) == 0);

    __cilkrts_worker *w = __cilkrts_get_tls_worker();
    full_frame *ff = NULL;
    __cilkrts_stack_frame *ff_call_stack = NULL;

    __CILK_JUMP_BUFFER ctx_bkup;
    volatile int done = 0;
    if(!CILK_SETJMP(sf.ctx)) { 
        // TODO: There should be a method that avoids this...
        memcpy(ctx_bkup, sf.ctx, 5*sizeof(void*));
        __cilkrts_switch_fibers(&sf);
    } else {
        // This SHOULD occur after switching fibers; steal from here
        if (!done) {
            done = 1;
            cilkg_increment_pending_futures(__cilkrts_get_tls_worker()->g);
            memcpy(sf.ctx, ctx_bkup, 5*sizeof(void*));
            __spawn_future_helper(std::move(func));
            
            __cilkrts_worker *curr_worker = __cilkrts_get_tls_worker();
            // Return to the original fiber
            __cilkrts_worker_lock(curr_worker);
            full_frame *frame = *curr_worker->l->frame_ff;
            __cilkrts_frame_lock(curr_worker, frame);
                CILK_ASSERT(frame->future_fibers_head && "The head is null for some reason...");
                cilk_fiber *fut_fiber = __cilkrts_pop_tail_future_fiber(frame);
                cilk_fiber *prev_fiber;
                if (frame->future_fibers_tail) {
                    prev_fiber = frame->future_fibers_tail->fiber;
                } else {
                    prev_fiber = frame->fiber_self;
                }
            __cilkrts_frame_unlock(curr_worker, frame);
            __cilkrts_worker_unlock(curr_worker);
            
            __cilkrts_switch_fibers_back(&sf, fut_fiber, prev_fiber);
        }
        CILK_ASSERT(sf.flags & CILK_FRAME_STOLEN);
    }

    // TODO: Rework it so we don't do this on futures
    if (sf.flags & CILK_FRAME_UNSYNCHED) {
        if (!CILK_SETJMP(sf.ctx)) {
            __cilkrts_future_sync(&sf);
        }
        //__cilkrts_rethrow(&sf);
        update_pedigree_after_sync(&sf);
    }

    __cilkrts_worker *curr_worker = __cilkrts_get_tls_worker();
    __cilkrts_worker_lock(curr_worker);
    ff = *curr_worker->l->frame_ff;
    __cilkrts_frame_lock(curr_worker, ff);

    cilk_fiber_get_data(ff->fiber_self)->resume_sf = NULL;

    __cilkrts_frame_unlock(curr_worker, ff);
    __cilkrts_worker_unlock(curr_worker);

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}

}
