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

    {
        char* new_sp = kyles_get_sp_for_executing_sf(cilk_fiber_get_stack_base(fiber), sf);

        //__CILK_JUMP_BUFFER dest;
        //memcpy(dest, sf->ctx, 5*sizeof(void*));
        //JMPBUF_SP(dest) = new_sp;
        
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

    first_frame->flags &= ~(CILK_FRAME_FUTURE_PARENT);

    cilk_fiber_remove_reference_from_self_and_resume_other(curr_fiber, &(__cilkrts_get_tls_worker()->l->fiber_pool), new_fiber);
}

CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame) {
    __cilkrts_worker* curr_worker = __cilkrts_get_tls_worker_fast();
    // This is a little faster than normal allocate, though provides
    // greater std. dev. of runtime when run on multiple cores.
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

    // The unsynched flag will be cleared if the frame is stolen.
    // TODO: Find a cleaner & faster way to do this, if possible
    volatile void *saved_sp = SP(first_frame);

    cilk_fiber_suspend_self_and_resume_other(curr_fiber, new_exec_fiber);

    // If this flag is still set, then the frame was stolen.
    if (first_frame->flags & CILK_FRAME_FUTURE_PARENT) {
        cilk_fiber_get_data(curr_fiber)->resume_sf = NULL;

        first_frame->flags &= ~(CILK_FRAME_FUTURE_PARENT);
        SP(first_frame) = (void*)saved_sp;

        // Technically, it would be better to hold some locks here, but it is safe
        // because we haven't done anything yet that would allow stealing (and thus
        // the frame cannot change underneath us)
        if (!(first_frame->flags & CILK_FRAME_UNSYNCHED)) {
            (*__cilkrts_get_tls_worker_fast()->l->frame_ff)->sync_sp = 0;
        } else {
            (*__cilkrts_get_tls_worker_fast()->l->frame_ff)->sync_sp -= (char*)saved_sp;
        }

        CILK_LONGJMP(first_frame->ctx);

        CILK_ASSERT(! "We should not return here!");
    }
}
