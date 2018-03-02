#include <cilk/cilk.h>
#include <internal/abi.h>
#include <runtime/rts-common.h>
#include <iostream>
#include <functional>

extern "C" {

volatile int ZERO = 0;

extern CILK_ABI_VOID __cilkrts_detach(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_pop_frame(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_enter_frame_1(__cilkrts_stack_frame *sf);

static CILK_ABI_VOID __attribute__((noinline)) __spawn_future(std::function<void(void)> func) {
    alloca(ZERO);
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);
    __cilkrts_detach(&sf);

    func();

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}

void __attribute__((noinline)) __my_cilk_spawn_future(std::function<void(void)> func) {
    alloca(ZERO);
  // BEGIN Setup current stack frame information...
  __cilkrts_stack_frame sf;
  __cilkrts_enter_frame(&sf);

  // BEGIN cilk_spawn (__cilk_spawn_future helper function)
  if(!CILK_SETJMP(sf.ctx)) { 
    __spawn_future(func);
    //func();
    //__cilk_spawn(func);
  }
  // END cilk_spawn

  // The continuation could be on another worker
	//struct __cilkrts_worker *w2 = __cilkrts_get_tls_worker();
  //if (w2 != w) {
  //  std::cout << "Continuation stolen!" << std::endl;
  //  __cilk_spawn_future_remove_sync_details(w2, &sf);
  //}

  // BEGIN cilk_sync
  if (sf.flags & CILK_FRAME_UNSYNCHED) {
    if (!CILK_SETJMP(sf.ctx)) {
      __cilkrts_sync(&sf);
    }
  }

  __cilkrts_pop_frame(&sf);
  __cilkrts_leave_frame(&sf);
  // END cilk_sync
}

}
