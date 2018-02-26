#include <cilk/cilk.h>
//#include <internal/future-abi.hpp>
#include <internal/abi.h>
#include <iostream>

extern "C" {

/*void __my_cilk_spawn_future(std::function<void(void)> func) {
  // BEGIN Setup current stack frame information...
  __cilkrts_stack_frame sf;
  __cilkrts_worker* w = __cilkrts_get_tls_worker();
  sf.call_parent = w->current_stack_frame;
  sf.worker = w;
  w->current_stack_frame = &sf;
  // END Setup current stack frame information...

  // BEGIN cilk_spawn (__cilk_spawn_future helper function)
  if(!CILK_SETJMP(sf.ctx)) { 
    __cilk_spawn(func);
  }
  // END cilk_spawn

  // The continuation could be on another worker
	struct __cilkrts_worker *w2 = __cilkrts_get_tls_worker();
  //if (w2 != w) {
  //  std::cout << "Continuation stolen!" << std::endl;
  //  __cilk_spawn_future_remove_sync_details(w2, &sf);
  //}

  // BEGIN cilk_sync
  if (sf.flags & CILK_FRAME_UNSYNCHED) {
    if (!CILK_SETJMP(sf.ctx)) {
      __cilkrts_future_sync(&sf);
    }
  }

	w2->current_stack_frame = sf.call_parent;
	sf.call_parent = 0;

  if (sf.flags)
    __cilkrts_leave_frame(&sf);
  // END cilk_sync
}*/

}
