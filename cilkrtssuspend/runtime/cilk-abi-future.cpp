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

extern "C" {

extern CILK_ABI_VOID __cilkrts_detach(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_pop_frame(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_enter_frame_1(__cilkrts_stack_frame *sf);

typedef void (*void_func_t)(void);

// TODO: This is temporary.
void __assert_future_counter(int count) {
    assert(__cilkrts_get_tls_worker()->l->frame_ff[0]->future_counter == count);
}   


static CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame) {
    //STOP_INTERVAL(w, INTERVAL_WORKING);
    //START_INTERVAL(w, INTERVAL_IN_RUNTIME);
    __cilkrts_worker* curr_worker = __cilkrts_get_tls_worker();
    cilk_fiber* new_exec_fiber = cilk_fiber_allocate(&(curr_worker->l->fiber_pool));
    CILK_ASSERT(new_exec_fiber != NULL);
    cilk_fiber_data* new_exec_fiber_data = cilk_fiber_get_data(new_exec_fiber);
    printf("future create: %p\n", new_exec_fiber_data); fflush(stdout);
    printf("Me in future: %p\n", curr_worker);
    new_exec_fiber_data->resume_sf = first_frame;
    //if (curr_worker->l->fiber_to_free) {
    //    cilk_fiber_deallocate_from_thread(curr_worker->l->fiber_to_free);
    //}
    //curr_worker->l->fiber_to_free = new_exec_fiber;
    cilk_fiber_reset_state(new_exec_fiber, resume_user_code_on_another_fiber);
    printf("pre create: %p\n", cilk_fiber_get_data(curr_worker->l->frame_ff[0]->fiber_self)); fflush(stdout);
    cilk_fiber *curr_fiber = NULL;
    __cilkrts_worker_lock(curr_worker);
    curr_worker->l->frame_ff[0]->future_fiber = new_exec_fiber;
    curr_fiber = curr_worker->l->frame_ff[0]->fiber_self;
    __cilkrts_worker_unlock(curr_worker);
    // Should I be "running" instead of resuming? The resume at least jumps to a new fiber...
    cilk_fiber_suspend_self_and_resume_other(curr_fiber, new_exec_fiber);
    // TODO: The suspend_self_and_resume_other DOES return! :o
    CILK_ASSERT(! "Uh-oh, spaghettios!"); // We should not make it here...
}

static CILK_ABI_VOID __attribute__((noinline)) __spawn_future(std::function<void(void)>* func) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);
    __cilkrts_detach(&sf);
    assert(sf.call_parent->flags & CILK_FRAME_FUTURE_PARENT);
    if (!CILK_SETJMP(sf.ctx)) {
        __cilkrts_switch_fibers(&sf);
        // We should not make it here
        CILK_ASSERT(0);
    }

    (*func)();
    delete func;

    printf("Func finished!\n"); fflush(stdout);
    __cilkrts_pop_frame(&sf);
    printf("About to leave the future frame!\n"); fflush(stdout);
    __cilkrts_leave_frame(&sf);
    // If we get here, then out parent was not stolen!
    CILK_ASSERT(! "Leaving future frame when unstolen parent path not yet supported!");
}

void __attribute__((noinline)) __my_cilk_spawn_future(std::function<void(void)> func) {
  __cilkrts_stack_frame sf;
  __cilkrts_enter_frame_1(&sf);

  sf.flags |= CILK_FRAME_FUTURE_PARENT;

  __cilkrts_worker* original_worker = __cilkrts_get_tls_worker();
  cilk_fiber* orig_fiber = original_worker->l->frame_ff[0]->fiber_self;
  if(!CILK_SETJMP(sf.ctx)) { 
    std::function<void(void)>* heap_func = new std::function<void(void)>(func);
    __spawn_future(heap_func);
  }
  if (original_worker != __cilkrts_get_tls_worker()) {
    assert(__cilkrts_get_tls_worker()->l->frame_ff[0]->rightmost_child->is_future);
    assert(__cilkrts_get_tls_worker()->l->frame_ff[0]->fiber_self != orig_fiber);

    printf("Continuation of future_spawn stolen!\n"); fflush(stdout);
  }

  // TODO: Do not need to do this for futures.
  if (sf.flags & CILK_FRAME_UNSYNCHED) {
    if (!CILK_SETJMP(sf.ctx)) {
      printf("Syncing future!\n"); fflush(stdout);
      __cilkrts_sync(&sf);
      printf("Done syncing future!\n"); fflush(stdout);
    }
  }

  __cilkrts_pop_frame(&sf);
  __cilkrts_leave_frame(&sf);
}

}
