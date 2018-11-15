#include "cilktool.h"
#include "../include/cilk/batcher.h"

#ifdef __cplusplus
#define EXTERN_C extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C
#define EXTERN_C_END
#endif

EXTERN_C

void cilk_tool_init(void) { }
void cilk_tool_destroy(void) { }
void cilk_tool_print(void) { }

void cilk_tool_c_function_enter(void* this_fn, void* rip) { }
void cilk_tool_c_function_leave(void* rip) { }
                                
void cilk_enter_begin(__cilkrts_stack_frame* sf, void* this_fn, void* rip) { }
void cilk_enter_helper_begin(__cilkrts_stack_frame* sf, void* this_fn, void* rip) { }
void cilk_enter_end (__cilkrts_stack_frame* sf, void* rsp) { }
void cilk_spawn_prepare (__cilkrts_stack_frame* sf) { }
void cilk_spawn_or_continue (int in_continuation) { }
void cilk_detach_begin (__cilkrts_stack_frame* parent) { }
void cilk_detach_end (void) { }
void cilk_sync_begin (__cilkrts_stack_frame* sf) { }
void cilk_sync_end (__cilkrts_stack_frame* sf) { }
void cilk_leave_begin (__cilkrts_stack_frame *sf) { }
void cilk_leave_end (void) { }
void cilk_leave_stolen(__cilkrts_worker* w, __cilkrts_stack_frame *saved_sf,
                       int is_original, char* stack_base) { }
void cilk_sync_abandon(__cilkrts_stack_frame *sf) { }
void cilk_continue(__cilkrts_stack_frame *sf, char* new_sp) { }
void cilk_done_with_stack(__cilkrts_stack_frame *sf_at_sync, char* stack_base) { }
void cilk_steal_success(__cilkrts_worker* w, __cilkrts_worker* victim, __cilkrts_stack_frame* sf) { }
void cilk_return_to_first_frame(__cilkrts_worker* w, __cilkrts_worker* team, __cilkrts_stack_frame* sf) { }

int cilk_tool_om_try_lock_all(__cilkrts_worker* w) { return batcher_trylock(w); }

EXTERN_C_END
