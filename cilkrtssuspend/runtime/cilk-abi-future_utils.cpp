#include <cilk/cilk.h>
#include <internal/abi.h>
#include "cilk_fiber.h"
#include "full_frame.h"
#include "local_state.h"

#define ALIGN_MASK (~((uintptr_t)0xFF))
char* __attribute__((always_inline)) __cilkrts_get_exec_sp(cilk_fiber* fiber) {
    char *stack_base = cilk_fiber_get_stack_base(fiber);
    char *new_stack_base = stack_base - 256;
    new_stack_base = (char*)((size_t)new_stack_base & ALIGN_MASK);
    return new_stack_base;
}

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
    //__builtin_prefetch(__cilkrts_get_exec_sp(new_exec_fiber), 1, 3);
    //*(__cilkrts_get_exec_sp(new_exec_fiber)) = 0;

    cilk_fiber *curr_fiber = cilk_fiber_get_current_fiber();
    __cilkrts_enqueue_future_fiber(new_exec_fiber);

    cilk_fiber_setup_for_future(curr_fiber, new_exec_fiber);

    return new_exec_fiber;
}
