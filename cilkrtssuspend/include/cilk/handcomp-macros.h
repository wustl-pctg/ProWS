#ifndef __HANDCOMP_CILLK_FUTURE_MACROS__H__
#define __HANDCOMP_CILLK_FUTURE_MACROS__H__

#include "internal/abi.h"

struct cilk_fiber;

extern char* __cilkrts_switch_fibers();
extern void __cilkrts_switch_fibers_back(cilk_fiber*);

extern "C" {
cilk_fiber* cilk_fiber_get_current_fiber();
void** cilk_fiber_get_resume_jmpbuf(cilk_fiber*);
void cilk_fiber_do_post_switch_actions(cilk_fiber*);
void __cilkrts_detach(__cilkrts_stack_frame*);
void __cilkrts_pop_frame(__cilkrts_stack_frame*);
}

#define SYNC\
  if (sf.flags & CILK_FRAME_UNSYNCHED) {\
    if (!CILK_SETJMP(sf.ctx)) {\
      __cilkrts_sync(&sf);\
    }\
  }

#define START_FUTURE_SPAWN \
  sf.flags |= CILK_FRAME_FUTURE_PARENT;\
  initial_fiber = cilk_fiber_get_current_fiber();\
  if (!CILK_SETJMP(cilk_fiber_get_resume_jmpbuf(initial_fiber))) {\
    char *new_sp = __cilkrts_switch_fibers();\
    char *old_sp = NULL;\
    __asm__ volatile ("mov %%rsp, %0" : "=r" (old_sp));\
    __asm__ volatile ("mov %0, %%rsp" : : "r" (new_sp));

#define START_FIRST_FUTURE_SPAWN\
  cilk_fiber* initial_fiber;\
  START_FUTURE_SPAWN;

#define END_FUTURE_SPAWN \
    __asm__ volatile ("mov %0, %%rsp" : : "r" (old_sp));\
    __cilkrts_switch_fibers_back(initial_fiber);\
  }\
  cilk_fiber_do_post_switch_actions(initial_fiber);\
  sf.flags &= ~(CILK_FRAME_FUTURE_PARENT);

#define FUTURE_HELPER_PREAMBLE\
  __cilkrts_stack_frame sf;\
  __cilkrts_enter_frame_fast_1(&sf);\
  __cilkrts_detach(&sf);

#define FUTURE_HELPER_EPILOGUE\
  __cilkrts_pop_frame(&sf);\
  __cilkrts_leave_frame(&sf);

#define SPAWN_HELPER_PREAMBLE   FUTURE_HELPER_PREAMBLE

#define SPAWN_HELPER_EPILOGUE\
  __cilkrts_pop_frame(&sf);\
  __cilkrts_leave_frame(&sf);

#define CILK_FUNC_PREAMBLE\
  __asm__ volatile ("" ::: "memory");\
  __cilkrts_stack_frame sf;\
  __cilkrts_enter_frame_1(&sf);\
  __asm__ volatile ("" ::: "memory");

#define CILK_FUNC_EPILOGUE\
  __asm__ volatile ("" ::: "memory");\
  SYNC;\
  SPAWN_HELPER_EPILOGUE;


#endif
