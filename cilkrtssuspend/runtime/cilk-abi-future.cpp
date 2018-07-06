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
extern CILK_ABI_VOID __cilkrts_switch_fibers_back(__cilkrts_stack_frame* first_frame, cilk_fiber* curr_fiber, cilk_fiber* new_fiber);
extern CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame);

extern "C" {
extern CILK_ABI_VOID __cilkrts_detach(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_pop_frame(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_enter_frame_1(__cilkrts_stack_frame *sf);
}


static void __attribute__((noinline)) __spawn_future_helper(std::function<void(void)> func) {
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

    // Read the initial fiber so we can switch back to it without taking
    // any locks later.
    cilk_fiber *volatile initial_fiber = cilk_fiber_get_current_fiber();

    if(!CILK_SETJMP(sf.ctx)) { 
        __cilkrts_switch_fibers(&sf);

    // The CILK_FRAME_FUTURE_PARENT flag gets cleared on a steal
    } else if (sf.flags & CILK_FRAME_FUTURE_PARENT) {
        __spawn_future_helper(std::move(func));

        __cilkrts_worker *curr_worker = __cilkrts_get_tls_worker_fast();
        cilk_fiber *fut_fiber = __cilkrts_pop_tail_future_fiber();

        __cilkrts_switch_fibers_back(&sf, fut_fiber, initial_fiber);
    }

    // TODO: Rework it so we don't do this on futures
    if (sf.flags & CILK_FRAME_UNSYNCHED) {
        if (!CILK_SETJMP(sf.ctx)) {
            __cilkrts_future_sync(&sf);
        }
    }


    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}

// TODO: Move this to a file that makes more sense!! It is here to get the -fno-omit-frame-pointer flag
extern "C" {
extern void do_suspend_return_from_initial(__cilkrts_worker *w, full_frame *ff, __cilkrts_stack_frame *sf);
extern void kyles_longjmp_into_runtime(__cilkrts_worker *w, scheduling_stack_fcn_t fcn, __cilkrts_stack_frame *sf);

void suspend_return_from_initial(__cilkrts_worker *w)  {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    sf.call_parent = NULL;
    CILK_ASSERT((sf.flags & CILK_FRAME_LAST) == 0);
    (*w->l->frame_ff)->call_stack = &sf;

    if (!CILK_SETJMP(sf.ctx)) {
        //printf("Greetings, human!\n");
        // KYLE_TODO: This function assumes there is a parent for the full frame!!!!
        //            Also that the fiber has been "removed" from the full frame first.
        __cilkrts_put_stack((*w->l->frame_ff), &sf);
        kyles_longjmp_into_runtime(w, do_suspend_return_from_initial, &sf);
    }

    // Just pop; cilkrts_leave_frame would do nothing
    // but check a bunch of branching conditions
    __cilkrts_pop_frame(&sf);
}
}
