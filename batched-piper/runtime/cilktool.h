#ifndef INCLUDED_CILKTOOL_DOT_H
#define INCLUDED_CILKTOOL_DOT_H

typedef struct __cilkrts_stack_frame __cilkrts_stack_frame;
typedef struct __cilkrts_worker __cilkrts_worker;

#ifdef __cplusplus
#define EXTERN_C extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C
#define EXTERN_C_END
#endif

EXTERN_C

void __attribute__((weak)) cilk_tool_init(void);
void __attribute__((weak)) cilk_tool_destroy(void);
void __attribute__((weak)) cilk_tool_print(void);

void __attribute__((weak)) cilk_tool_c_function_enter(void* this_fn, void* rip);
void __attribute__((weak)) cilk_tool_c_function_leave(void* rip);

void __attribute__((weak)) 
cilk_enter_begin (__cilkrts_stack_frame* sf, void* this_fn, void* rip);
void __attribute__((weak)) 
cilk_enter_helper_begin(__cilkrts_stack_frame* sf, void* this_fn, void* rip);
void __attribute__((weak)) cilk_enter_end(__cilkrts_stack_frame* sf, void* rsp);
void __attribute__((weak)) cilk_spawn_prepare(__cilkrts_stack_frame* sf);
void __attribute__((weak)) cilk_spawn_or_continue (int in_continuation);
void __attribute__((weak)) cilk_detach_begin(__cilkrts_stack_frame* parent);
void __attribute__((weak)) cilk_detach_end(void);
void __attribute__((weak)) cilk_sync_begin(__cilkrts_stack_frame* sf);
void __attribute__((weak)) cilk_sync_end(__cilkrts_stack_frame* sf);
void __attribute__((weak)) cilk_leave_begin(__cilkrts_stack_frame *sf);
void __attribute__((weak)) cilk_leave_end(void);
void __attribute__((weak)) cilk_leave_stolen(__cilkrts_worker* w,
                                             __cilkrts_stack_frame *saved_sf,
                                             int is_original,
                                             char* stack_base);
void __attribute__((weak)) cilk_sync_abandon(__cilkrts_stack_frame *sf);
void __attribute__((weak)) cilk_continue(__cilkrts_stack_frame *sf, char* new_sp);
void __attribute__((weak)) cilk_done_with_stack(__cilkrts_stack_frame *sf_at_sync, char* stack_base);

void __attribute__((weak))
cilk_steal_success(__cilkrts_worker* w, __cilkrts_worker* victim, __cilkrts_stack_frame* sf);
void __attribute__((weak))
cilk_return_to_first_frame(__cilkrts_worker* w, __cilkrts_worker* team, __cilkrts_stack_frame* sf);

int __attribute__((weak))
cilk_tool_om_try_lock_all(__cilkrts_worker* w);

EXTERN_C_END

#endif  // INCLUDED_CILKTOOL_DOT_H
