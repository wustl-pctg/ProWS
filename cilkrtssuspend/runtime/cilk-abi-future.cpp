#include <cilk/cilk.h>
#include <internal/abi.h>
#include <runtime/rts-common.h>
#include <iostream>
#include <functional>
#include <assert.h>
#include "local_state.h"
#include "full_frame.h"

extern "C" {

extern CILK_ABI_VOID __cilkrts_detach(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_pop_frame(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_enter_frame_1(__cilkrts_stack_frame *sf);

// TODO: This is temporary.
void __assert_future_counter(int count) {
    assert(__cilkrts_get_tls_worker()->l->frame_ff[0]->future_counter == count);
}   

static CILK_ABI_VOID __attribute__((noinline)) __spawn_future(std::function<void(void)> func) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);
    assert(sf.call_parent->flags & CILK_FRAME_FUTURE_PARENT);
    // TODO: Detach in such a way that the parent does not need to wait...
    __cilkrts_detach(&sf);


    func();

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}

void __attribute__((noinline)) __my_cilk_spawn_future(std::function<void(void)> func) {
  __cilkrts_stack_frame sf;
  __cilkrts_enter_frame_1(&sf);

  sf.flags |= CILK_FRAME_FUTURE_PARENT;

  __cilkrts_worker* original_worker = __cilkrts_get_tls_worker();
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
