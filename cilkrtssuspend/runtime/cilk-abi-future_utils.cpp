#include <cilk/cilk.h>
#include <internal/abi.h>
#include "cilk_fiber.h"
#include "full_frame.h"
#include "local_state.h"

CILK_ABI_VOID __attribute__((always_inline)) __cilkrts_switch_fibers_back(cilk_fiber* curr_fiber, cilk_fiber* new_fiber) {
    __cilkrts_worker *curr_worker = __cilkrts_get_tls_worker_fast();

    cilk_fiber_setup_for_future_return(curr_fiber, &(curr_worker->l->fiber_pool), new_fiber);

    __cilkrts_pop_tail_future_fiber();
}

CILK_ABI(cilk_fiber*) __attribute__((always_inline)) __cilkrts_switch_fibers() {
    __cilkrts_worker* curr_worker = __cilkrts_get_tls_worker_fast();

    // This is a little faster than normal fiber allocate when used for future fibers.
    cilk_fiber* new_exec_fiber = cilk_fiber_allocate_with_try_allocate_from_pool(&(curr_worker->l->fiber_pool));
    CILK_ASSERT(new_exec_fiber != NULL);

    cilk_fiber *curr_fiber = cilk_fiber_get_current_fiber();
    __cilkrts_enqueue_future_fiber(new_exec_fiber);

    cilk_fiber_setup_for_future(curr_fiber, new_exec_fiber);
    //cilk_fiber_suspend_self_and_run_future(curr_fiber, new_exec_fiber, first_frame);

    //first_frame->flags &= ~(CILK_FRAME_FUTURE_PARENT);

    return new_exec_fiber;
}
