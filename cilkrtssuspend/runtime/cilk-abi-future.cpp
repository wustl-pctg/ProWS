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

extern CILK_ABI_VOID __cilkrts_leave_future_frame(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_switch_fibers_back(cilk_fiber* new_fiber);
extern CILK_ABI(char*) __cilkrts_switch_fibers();

extern "C" {
extern CILK_ABI_VOID __cilkrts_detach(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_pop_frame(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_enter_frame_1(__cilkrts_stack_frame *sf);
}


static void __attribute__((noinline)) __spawn_future_helper(std::function<void*(void)> func) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

        void* __cilk_deque = func();
        
        if (__builtin_expect(__cilk_deque != NULL, 0)) {
            __cilkrts_resume_suspended(__cilk_deque, 2);
        }

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}

CILK_ABI_VOID __attribute__((noinline)) __spawn_future_helper_helper(std::function<void*(void)> func) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);

    // Mark the frame as a future parent so
    // that if we steal this frame we will
    // know what to do with the fibers.
    sf.flags |= CILK_FRAME_FUTURE_PARENT;

    // Read the initial fiber so we can switch back to it without taking
    // any locks later.
    cilk_fiber *initial_fiber = cilk_fiber_get_current_fiber();

    if(!CILK_SETJMP(cilk_fiber_get_resume_jmpbuf(initial_fiber))) { 
        char *new_sp = __cilkrts_switch_fibers();

        char *old_sp = NULL;

        // Save the old stack pointer and
        // move it to point to the new fiber.
        __asm__ volatile ("mov %%rsp,%0"
                          : "=r" (old_sp));

        __asm__ volatile ("mov %0,%%rsp"
                          : : "r" (new_sp));

        // Now that we are on the new stack, we can
        // run the future
        __spawn_future_helper(std::move(func));

        // Move our stack back to the original!
        // We were not stolen from.
        __asm__ volatile ("mov %0,%%rsp"
                          : : "r" (old_sp));

        // Set the proper flags & pointers related to switching
        // back to the old fiber.
        __cilkrts_switch_fibers_back(initial_fiber);
    }

    // However we got here, we need to do some post switch
    // actions to clean up data related to the previous fiber.
    cilk_fiber_do_post_switch_actions(initial_fiber);
    sf.flags &= ~(CILK_FRAME_FUTURE_PARENT);

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
