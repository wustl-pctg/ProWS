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

#define ALIGN_MASK  (~((uintptr_t)0xFF))

static char* __attribute__((always_inline)) kyles_get_sp_for_executing_sf(char* stack_base) {
    // Make the stack pointer 256-byte aligned
    char* new_stack_base = stack_base - 256;
    new_stack_base = (char*)((size_t)new_stack_base & ALIGN_MASK);
    return new_stack_base;
}

static void fiber_proc_to_resume_user_code_for_future(cilk_fiber *fiber) {

    cilk_fiber_data *data = cilk_fiber_get_data(fiber);
    __cilkrts_stack_frame* sf = data->resume_sf;
    CILK_ASSERT(sf);

    // When we pull the resume_sf out of the fiber to resume it, clear
    // the old value.
    data->resume_sf = NULL;
    CILK_ASSERT(sf->worker == data->owner);

    SP(sf) = kyles_get_sp_for_executing_sf(cilk_fiber_get_stack_base(fiber));

    #ifdef RESTORE_X86_FP_STATE
        restore_x86_fp_state(sf);
    #endif

    CILK_LONGJMP(sf->ctx);

    /*NOTREACHED*/
    // Intel's C compiler respects the preceding lint pragma
    CILK_ASSERT(! "Should not return into this function!");
}

CILK_ABI_VOID __attribute__((always_inline)) __cilkrts_switch_fibers_back(__cilkrts_stack_frame* first_frame, cilk_fiber* curr_fiber, cilk_fiber* new_fiber) {
    cilk_fiber_remove_reference_from_self_and_resume_other(curr_fiber, &(__cilkrts_get_tls_worker()->l->fiber_pool), new_fiber);
}

CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame) {
    __cilkrts_worker* curr_worker = __cilkrts_get_tls_worker_fast();
    // This is a little faster than normal allocate, though provides
    // greater std. dev. of runtime when run on multiple cores.
    //cilk_fiber* new_exec_fiber = cilk_fiber_allocate_with_try_allocate_from_pool(&(curr_worker->l->fiber_pool));
    cilk_fiber* new_exec_fiber = cilk_fiber_allocate(&(curr_worker->l->fiber_pool));
    CILK_ASSERT(new_exec_fiber != NULL);

    cilk_fiber_data *fdata = cilk_fiber_get_data(new_exec_fiber);
    fdata->resume_sf = first_frame;
    //new_exec_fiber->get_data()->resume_sf = first_frame;

    //new_exec_fiber_data->resume_sf = first_frame;
    //new_exec_fiber->reset_state(fiber_proc_to_resume_user_code_for_future);
    cilk_fiber_reset_state(new_exec_fiber, fiber_proc_to_resume_user_code_for_future);
    cilk_fiber *curr_fiber = cilk_fiber_get_current_fiber();
    __cilkrts_enqueue_future_fiber(new_exec_fiber);

    //curr_fiber->suspend_self_and_resume_other(new_exec_fiber);
    cilk_fiber_suspend_self_and_resume_other(curr_fiber, new_exec_fiber);

    first_frame->flags &= ~(CILK_FRAME_FUTURE_PARENT);
}
