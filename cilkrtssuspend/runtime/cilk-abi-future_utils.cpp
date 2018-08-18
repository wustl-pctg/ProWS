#include <cilk/cilk.h>
#include <internal/abi.h>
#include "cilk_fiber.h"
#include "full_frame.h"
#include "local_state.h"

#ifdef TRACK_FIBER_COUNT
extern "C" {
void increment_fiber_count(global_state_t* g);
void decrement_fiber_count(global_state_t* g);
}
#endif

#define ALIGN_MASK (~((uintptr_t)0xFF))
char* __attribute__((always_inline)) __cilkrts_get_exec_sp(cilk_fiber* fiber) {
    char *stack_base = cilk_fiber_get_stack_base(fiber);
    char *new_stack_base = stack_base - 256;
    new_stack_base = (char*)((size_t)new_stack_base & ALIGN_MASK);
    return new_stack_base;
}

CILK_ABI_VOID __attribute__((always_inline)) __cilkrts_switch_fibers_back(cilk_fiber* new_fiber) {
    __cilkrts_worker *curr_worker = __cilkrts_get_tls_worker_fast();
    cilk_fiber *curr_fiber = cilk_fiber_get_current_fiber();

    int dealloc = 1;
    if (curr_worker->l->future_fiber_pool_idx < (MAX_FUTURE_FIBERS_IN_POOL-1)) {
        curr_worker->l->future_fiber_pool[++curr_worker->l->future_fiber_pool_idx] = curr_fiber;
        dealloc = 0;
    }

#ifdef TRACK_FIBER_COUNT
    decrement_fiber_count(curr_worker->g);
#endif
    cilk_fiber_setup_for_future_return(curr_fiber, &(curr_worker->l->fiber_pool), new_fiber, dealloc);

    // Technically, we could use this to get the current fiber.
    // However, optimizations and/or hardware instruction reordering
    // works better if we read the curr_fiber value from thread local
    // storage (ala cilk_fiber_get_current_fiber).
    __cilkrts_pop_tail_future_fiber();
}

CILK_ABI(char*) __attribute__((always_inline)) __cilkrts_switch_fibers() {
    __cilkrts_worker* curr_worker = __cilkrts_get_tls_worker_fast();

    // This is a little faster than normal fiber allocate when used for future fibers.
    cilk_fiber* new_exec_fiber = NULL;
    if (curr_worker->l->future_fiber_pool_idx >= 0) {
        new_exec_fiber = curr_worker->l->future_fiber_pool[curr_worker->l->future_fiber_pool_idx--];
    } else {
        //new_exec_fiber = cilk_fiber_allocate_with_try_allocate_from_pool(&(curr_worker->l->fiber_pool));
        new_exec_fiber = cilk_fiber_allocate(&(curr_worker->l->fiber_pool));
    }
    CILK_ASSERT(new_exec_fiber != NULL);

#ifdef TRACK_FIBER_COUNT
increment_fiber_count(curr_worker->g);
#endif

    // Prefetch the stack so it is less painful to jump to!
    char *new_sp = __cilkrts_get_exec_sp(new_exec_fiber);
    __builtin_prefetch(new_sp, 1, 3);

    cilk_fiber *curr_fiber = cilk_fiber_get_current_fiber();
    __cilkrts_enqueue_future_fiber(new_exec_fiber);

    cilk_fiber_setup_for_future(curr_fiber, new_exec_fiber);

    return new_sp;
}
