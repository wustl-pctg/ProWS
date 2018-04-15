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

extern CILK_ABI_VOID __cilkrts_future_sync(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_frame(__cilkrts_stack_frame *sf);
extern "C" {

extern CILK_ABI_VOID __cilkrts_detach(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_pop_frame(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_enter_frame_1(__cilkrts_stack_frame *sf);
extern void fiber_proc_to_resume_user_code_for_random_steal(cilk_fiber *fiber);

typedef void (*void_func_t)(void);

CILK_ABI_VOID __assert_not_on_scheduling_fiber() {
//    CILK_ASSERT(cilk_fiber_get_current_fiber() != __cilkrts_get_tls_worker()->l->scheduling_fiber);
}

// TODO: This is temporary.
CILK_ABI_VOID __assert_future_counter(int count) {
    //CILK_ASSERT(__cilkrts_get_tls_worker()->l->frame_ff[0]->future_counter == count);
}   

CILK_ABI_VOID __print_curr_stack(char* str) {
    __cilkrts_worker *w = __cilkrts_get_tls_worker();
    cilk_fiber *fiber = w->l->frame_ff->fiber_self;
//    printf("%s curr fiber: %p (worker %d, references %d)\n", str, fiber, w->self, cilk_fiber_get_ref_count(fiber));
}

/*static CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame, cilk_fiber* curr_fiber, cilk_fiber* new_fiber) {
    cilk_fiber_data* new_fiber_data = cilk_fiber_get_data(new_fiber);
    new_fiber_data->resume_sf = first_frame;

    cilk_fiber_remove_reference_from_self_and_resume_other(curr_fiber, &(__cilkrts_get_tls_worker()->l->fiber_pool), new_fiber);
}

static CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame) {
    __cilkrts_worker* curr_worker = __cilkrts_get_tls_worker();
    cilk_fiber* new_exec_fiber = cilk_fiber_allocate(&(curr_worker->l->fiber_pool));
    // TODO: Handle the case that it is null more gracefully
    CILK_ASSERT(new_exec_fiber != NULL);
    CILK_ASSERT(first_frame);
    cilk_fiber_data* new_exec_fiber_data = cilk_fiber_get_data(new_exec_fiber);

    new_exec_fiber_data->resume_sf = first_frame;
    //if (curr_worker->l->fiber_to_free) {
    //    cilk_fiber_deallocate_from_thread(curr_worker->l->fiber_to_free);
    //}
    //curr_worker->l->fiber_to_free = new_exec_fiber;
    cilk_fiber_reset_state(new_exec_fiber, fiber_proc_to_resume_user_code_for_random_steal);

    cilk_fiber *curr_fiber = NULL;
    __cilkrts_worker_lock(curr_worker);
    curr_worker->l->frame_ff->future_fiber = new_exec_fiber;
    CILK_ASSERT(curr_worker->l->frame_ff->future_fiber);
    curr_fiber = curr_worker->l->frame_ff->fiber_self;
    __cilkrts_worker_unlock(curr_worker);

    printf("new exec fiber: %p\n", new_exec_fiber);

    // Should I be "running" instead of resuming? The resume at least jumps to a new fiber...
    cilk_fiber_suspend_self_and_resume_other(curr_fiber, new_exec_fiber);
    // TODO: I think that ideally I do not make it here...
    //printf("Back from new fiber in worker %d\n", __cilkrts_get_tls_worker()->self); fflush(stdout);
}*/

//#include "modified-future-leave-frame.cpp"
//#include "modified-future-sync.cpp"


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
  __cilkrts_enter_frame_fast_1(&sf);
  sf.flags |= CILK_FRAME_FUTURE_PARENT;

  CILK_ASSERT(__cilkrts_get_tls_worker()->l->frame_ff->future_flags == 0);
  CILK_ASSERT((sf.flags & CILK_FRAME_STOLEN) == 0);

  //CILK_ASSERT(cilk_fiber_get_data(orig_fiber)->resume_sf == NULL);
  if(!CILK_SETJMP(sf.ctx)) { 
      __spawn_future_helper(func);
  }

  if (sf.flags & CILK_FRAME_STOLEN) {
      CILK_ASSERT(__cilkrts_get_tls_worker()->l->frame_ff->future_flags == CILK_FUTURE_PARENT);
  }

  // TODO: Do not need to do this for futures.
  if (sf.flags & CILK_FRAME_UNSYNCHED) {
    if (!CILK_SETJMP(sf.ctx)) {
      __cilkrts_future_sync(&sf);
    }
  }

  __cilkrts_pop_frame(&sf);
  __cilkrts_leave_frame(&sf);
}

}
