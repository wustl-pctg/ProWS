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
#include "sysdep.h"
#include "cilk-ittnotify.h"
#include "jmpbuf.h"
#include <cstring>

extern "C" {

extern CILK_ABI_VOID user_code_resume_after_switch_into_runtime(cilk_fiber*);

extern void fiber_proc_to_resume_user_code_for_random_steal(cilk_fiber *fiber);

//extern char* get_sp_for_executing_sf(char* stack_base,
//                                     full_frame *ff,
//                                     __cilkrts_stack_frame *sf);

}

static char* __attribute__((alwaysinline)) kyles_get_sp_for_executing_sf(char* stack_base, __cilkrts_stack_frame *sf) {
    char* new_stack_base = stack_base - 256;
    const uintptr_t align_mask = ~(256 -1);
    new_stack_base = (char*)((size_t)new_stack_base & align_mask);
    return new_stack_base;
}

static void fiber_proc_to_resume_user_code_for_future(cilk_fiber *fiber) {

    cilk_fiber_data *data = cilk_fiber_get_data(fiber);
    __cilkrts_stack_frame* sf = data->resume_sf;
    full_frame *ff;

    CILK_ASSERT(sf);

    // When we pull the resume_sf out of the fiber to resume it, clear
    // the old value.
    data->resume_sf = NULL;
    CILK_ASSERT(sf->worker == data->owner);

    // For Win32, we need to overwrite the default exception handler
    // in this function, so that when the OS exception handling code
    // walks off the top of the current Cilk stack, it reaches our stub
    // handler.
    
    // Also, this function needs to be wrapped into a try-catch block
    // so the compiler generates the appropriate exception information
    // in this frame.
    
    {
        char* new_sp = kyles_get_sp_for_executing_sf(cilk_fiber_get_stack_base(fiber), sf);

        //__CILK_JUMP_BUFFER dest;
        //memcpy(dest, sf->ctx, 5*sizeof(void*));
        //JMPBUF_SP(dest) = new_sp;
        
        // TBD: We'd like to move TBB-interop methods into the fiber
        // eventually.
        cilk_fiber_invoke_tbb_stack_op(fiber, CILK_TBB_STACK_ADOPT);
        
        //CILK_ASSERT((sf->flags & CILK_FRAME_SUSPENDED) == 0);
        //sf->flags &= ~CILK_FRAME_SUSPENDED;

        //restore_x86_fp_state(sf);
        //printf("Jumping to stack frame!\n");
        //for (int i = 0; i < 5; i++) {
        //    printf("%p vs %p\n", sf->ctx[i], dest[i]);
        //}
        //CILK_LONGJMP(dest);
        
        sysdep_longjmp_to_sf(new_sp, sf, NULL);
        /*NOTREACHED*/
        // Intel's C compiler respects the preceding lint pragma
        CILK_ASSERT(! "Should not return into this function!");
    }
}

CILK_ABI_VOID __cilkrts_switch_fibers_back(__cilkrts_stack_frame* first_frame, cilk_fiber* curr_fiber, cilk_fiber* new_fiber) {
    cilk_fiber_data* new_fiber_data = cilk_fiber_get_data(new_fiber);

    cilk_fiber_remove_reference_from_self_and_resume_other(curr_fiber, &(__cilkrts_get_tls_worker()->l->fiber_pool), new_fiber);
}

CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame) {
    __cilkrts_worker* curr_worker = __cilkrts_get_tls_worker_fast();
    // This is a little faster on 1 core than normal allocate; there seems to be less of an effect on multicore
    cilk_fiber* new_exec_fiber = cilk_fiber_allocate_with_try_allocate_from_pool(&(curr_worker->l->fiber_pool));
    //cilk_fiber* new_exec_fiber = cilk_fiber_allocate(&(curr_worker->l->fiber_pool));

    // TODO: Handle the case that it is null more gracefully
    CILK_ASSERT(new_exec_fiber != NULL);
    cilk_fiber_data* new_exec_fiber_data = cilk_fiber_get_data(new_exec_fiber);

    new_exec_fiber_data->resume_sf = first_frame;
    cilk_fiber_reset_state(new_exec_fiber, fiber_proc_to_resume_user_code_for_future);

    cilk_fiber *curr_fiber = cilk_fiber_get_current_fiber();
    __cilkrts_enqueue_future_fiber(new_exec_fiber);

    cilk_fiber_get_data(curr_fiber)->resume_sf = NULL;

    const int saved_flags = first_frame->flags;
    first_frame->flags &= ~(CILK_FRAME_STOLEN);

    cilk_fiber_suspend_self_and_resume_other(curr_fiber, new_exec_fiber);

    // We don't jump to the sf until the "user_code_resume..." func on a steal
    if (first_frame->flags & CILK_FRAME_STOLEN) {
        cilk_fiber_get_data(curr_fiber)->resume_sf = first_frame;
        user_code_resume_after_switch_into_runtime(curr_fiber);
        CILK_ASSERT(! "We should not return here!");
    }
    first_frame->flags |= saved_flags & CILK_FRAME_STOLEN;
}
