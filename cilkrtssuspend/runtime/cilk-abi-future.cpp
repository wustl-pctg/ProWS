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

static CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame, cilk_fiber* curr_fiber, cilk_fiber* new_fiber) {
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
    printf("Going to new fiber from worker %d\n", curr_worker->self);
    cilk_fiber_suspend_self_and_resume_other(curr_fiber, new_exec_fiber);
    // TODO: I think that ideally I do not make it here...
    printf("Back from new fiber in worker %d\n", __cilkrts_get_tls_worker()->self); fflush(stdout);
}

static CILK_ABI_VOID __attribute__((noinline)) __spawn_future(std::function<void(void)>* func) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);
    __cilkrts_detach(&sf);

    CILK_ASSERT(sf.call_parent->flags & CILK_FRAME_FUTURE_PARENT);

    (*func)();
    delete func;

    printf("Func finished!\n"); fflush(stdout);
    __cilkrts_pop_frame(&sf);
    printf("About to leave the future frame!\n"); fflush(stdout);
    __cilkrts_leave_frame(&sf);
    // If we get here, then out parent was not stolen!
    //CILK_ASSERT(! "Leaving future frame when unstolen parent path not yet supported!");
}

void __attribute__((noinline)) __my_cilk_spawn_future(std::function<void(void)> func) {
  __cilkrts_stack_frame sf;
  __cilkrts_enter_frame_1(&sf);
  sf.flags |= CILK_FRAME_FUTURE_PARENT;

  __cilkrts_worker* original_worker = __cilkrts_get_tls_worker();
  cilk_fiber* orig_fiber = original_worker->l->frame_ff[0]->fiber_self;
  if(!CILK_SETJMP(sf.ctx)) { 
    CILK_ASSERT(sf.ctx);
    __cilkrts_switch_fibers(&sf);
    // TODO: What is the correct thing to do here??? This happens after the future is finished...
    //CILK_ASSERT(! "Returned after switching fibers :(");
  } else {
    printf("Hi guys!\n"); fflush(stdout);
    if(!CILK_SETJMP(sf.ctx)) { 
      std::function<void(void)>* heap_func = new std::function<void(void)>(func);
      __spawn_future(heap_func);
      // Jump back into the old frame!
      __cilkrts_switch_fibers(&sf, original_worker->l->frame_ff[0]->future_fiber, original_worker->l->frame_ff[0]->fiber_self);
      //CILK_ASSERT(! "Should not be returning from spawn future right now...");
    }
  }

  if (original_worker != __cilkrts_get_tls_worker()) {
    assert(__cilkrts_get_tls_worker()->l->frame_ff[0]->rightmost_child->is_future);
    assert(__cilkrts_get_tls_worker()->l->frame_ff[0]->fiber_self != orig_fiber);

    printf("Continuation of future_spawn stolen!\n"); fflush(stdout);
  }

  printf("sf: %p\n", &sf);

  // TODO: Do not need to do this for futures.
  if (sf.flags & CILK_FRAME_UNSYNCHED) {
    if (!CILK_SETJMP(sf.ctx)) {
      printf("Syncing future!\n"); fflush(stdout);
      __cilkrts_sync(&sf);
    }
    printf("Done syncing future!\n"); fflush(stdout);
  }

  printf("Past syncing future!\n"); fflush(stdout);
  CILK_ASSERT(__cilkrts_get_tls_worker()->l->frame_ff[0]->is_future == 0);

  __cilkrts_pop_frame(&sf);
  printf("Popped frame for my_spawn_future!\n"); fflush(stdout);
  __cilkrts_leave_frame(&sf);
  printf("Left frame for my_spawn_future!\n"); fflush(stdout);
}

}
