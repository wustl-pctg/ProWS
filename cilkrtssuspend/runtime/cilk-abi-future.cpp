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

extern CILK_ABI_VOID __cilkrts_future_sync(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_frame(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_parent_frame(__cilkrts_stack_frame *sf);
extern "C" {
extern CILK_ABI_VOID __cilkrts_detach(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_pop_frame(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_enter_frame_1(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_switch_fibers_back(__cilkrts_stack_frame* first_frame, cilk_fiber* curr_fiber, cilk_fiber* new_fiber);
extern CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame);


static CILK_ABI_VOID __attribute__((noinline)) __spawn_future_helper(std::function<void(void)> func) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

        func();

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_future_frame(&sf);
}

CILK_ABI_VOID __attribute__((noinline)) __spawn_future_helper_helper(std::function<void(void)> func) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);


    sf.flags |= CILK_FRAME_FUTURE_PARENT;

    __CILK_JUMP_BUFFER ctx_bkup;
    volatile int done = 0;
    if(!CILK_SETJMP(sf.ctx)) { 
        // TODO: There should be a method that avoids this...
        memcpy(ctx_bkup, sf.ctx, 5*sizeof(void*));
        __cilkrts_switch_fibers(&sf);
    } else {
        // This SHOULD occur after switching fibers; steal from here
        if (!done) {
            cilkg_increment_pending_futures(sf.worker->g);
            memcpy(sf.ctx, ctx_bkup, 5*sizeof(void*));
            done = 1;
            __spawn_future_helper(std::move(func));
            
            __cilkrts_worker *curr_worker = sf.worker;
            // Return to the original fiber
            __cilkrts_worker_lock(curr_worker);
            full_frame *frame = *curr_worker->l->frame_ff;
            __cilkrts_frame_lock(curr_worker, frame);
                cilk_fiber *fut_fiber = __cilkrts_pop_tail_future_fiber(frame);
                cilk_fiber *prev_fiber;
                prev_fiber = __cilkrts_peek_tail_future_fiber(frame);
                if (!prev_fiber) {
                    prev_fiber = frame->fiber_self;
                }
            __cilkrts_frame_unlock(curr_worker, frame);
            __cilkrts_worker_unlock(curr_worker);
            
            __cilkrts_switch_fibers_back(&sf, fut_fiber, prev_fiber);
        }
    }

    // TODO: Rework it so we don't do this on futures
    if (sf.flags & CILK_FRAME_UNSYNCHED) {
        if (!CILK_SETJMP(sf.ctx)) {
            __cilkrts_future_sync(&sf);
        }
    }

    __cilkrts_worker *curr_worker = sf.worker;
    __cilkrts_worker_lock(curr_worker);
    full_frame *ff = *curr_worker->l->frame_ff;
    __cilkrts_frame_lock(curr_worker, ff);

    cilk_fiber_get_data(ff->fiber_self)->resume_sf = NULL;

    __cilkrts_frame_unlock(curr_worker, ff);
    __cilkrts_worker_unlock(curr_worker);

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}

}
