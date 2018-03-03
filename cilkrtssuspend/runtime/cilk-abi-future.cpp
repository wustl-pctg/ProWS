#include <cilk/cilk.h>
#include <internal/abi.h>
#include <runtime/rts-common.h>
#include <iostream>
#include <functional>

extern "C" {

extern CILK_ABI_VOID __cilkrts_detach(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_pop_frame(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_enter_frame_1(__cilkrts_stack_frame *sf);

static CILK_ABI_VOID __attribute__((noinline)) __spawn_future(std::function<void(void)> func) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);
    // TODO: Detach in such a way that the parent does not need to wait...
    __cilkrts_detach(&sf);

    /*
    __cilkrts_worker* this_worker = __cilkrts_get_tls_worker();
    deque* parent_deque = w->l->active_deque;
    full_frame* parent_frame = w->l->frame_ff;
    cilk_fiber* future_fiber = cilk_fiber_allocate(&w->l->fiber_pool);
    full_frame* future_frame = make_child(this_worker, parent_frame, &sf, future_fiber);
    deque* future_deque = __cilkrts_malloc(sizeof(deque));
    deque_init(future_deque, w->g->ltqsize);
    future_deque->frame_ff = future_frame;
    future_deque->fiber = future_fiber;
    future_deque->team = parent_deque->team;
    deque_switch(w, future_deque);
    */

    // STEP 1: Promote the future sf to a full frame
    // STEP 2: Create a new deque and initialize with details of future
    // STEP 3: Switch to new deque, keeping old deque active
    

    func();

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}

void __attribute__((noinline)) __my_cilk_spawn_future(std::function<void(void)> func) {
  __cilkrts_stack_frame sf;
  __cilkrts_enter_frame_1(&sf);

  if(!CILK_SETJMP(sf.ctx)) { 
    __spawn_future(func);
  }

  // TODO: Do not need to do this for futures.
  if (sf.flags & CILK_FRAME_UNSYNCHED) {
    if (!CILK_SETJMP(sf.ctx)) {
      __cilkrts_sync(&sf);
    }
  }

  __cilkrts_pop_frame(&sf);
  __cilkrts_leave_frame(&sf);
}

}
