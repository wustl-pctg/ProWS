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

//#define FUTURE_IS_CILK_SPAWN
extern unsigned long ZERO;

extern CILK_ABI_VOID __cilkrts_future_sync(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_frame(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_parent_frame(__cilkrts_stack_frame *sf);
extern "C" {
extern CILK_ABI_VOID user_code_resume_after_switch_into_runtime(cilk_fiber*);
extern CILK_ABI_THROWS_VOID __cilkrts_rethrow(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID update_pedigree_after_sync(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_detach(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_pop_frame(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_enter_frame_1(__cilkrts_stack_frame *sf);
extern void fiber_proc_to_resume_user_code_for_random_steal(cilk_fiber *fiber);
extern char* get_sp_for_executing_sf(char* stack_base,
                                     full_frame *ff,
                                     __cilkrts_stack_frame *sf);

typedef void (*void_func_t)(void);


static void fiber_proc_to_resume_user_code_for_future(cilk_fiber *fiber) {

    cilk_fiber_data *data = cilk_fiber_get_data(fiber);
    __cilkrts_stack_frame* sf = data->resume_sf;
    full_frame *ff;

    CILK_ASSERT(sf);

    // When we pull the resume_sf out of the fiber to resume it, clear
    // the old value.
    data->resume_sf = NULL;
    CILK_ASSERT(sf->worker == data->owner);

    // KYLE_TODO: This is stupid. Do the correct thing to prevent crashes.
    //while (sf->worker->l->frame_ff->parent == NULL);

    ff = sf->worker->l->frame_ff;

    // For Win32, we need to overwrite the default exception handler
    // in this function, so that when the OS exception handling code
    // walks off the top of the current Cilk stack, it reaches our stub
    // handler.
    
    // Also, this function needs to be wrapped into a try-catch block
    // so the compiler generates the appropriate exception information
    // in this frame.
    
    {
        // KYLE_TODO: The following function calls modify the SP!!
        //            This causes us to crash when we return to the first full frame!!!
        //char* new_sp = sysdep_reset_jump_buffers_for_resume(fiber, ff, sf);
        char* new_sp = get_sp_for_executing_sf(cilk_fiber_get_stack_base(fiber), ff, sf);
        SP(sf) = new_sp;
        
        // Notify the Intel tools that we're stealing code
        ITT_SYNC_ACQUIRED(sf->worker);
        NOTIFY_ZC_INTRINSIC("cilk_continue", sf);

        // TBD: We'd like to move TBB-interop methods into the fiber
        // eventually.
        cilk_fiber_invoke_tbb_stack_op(fiber, CILK_TBB_STACK_ADOPT);
        

        sf->flags &= ~CILK_FRAME_SUSPENDED;

        // longjmp to user code.  Don't process exceptions here,
        // because we are resuming a stolen frame.
        sysdep_longjmp_to_sf(new_sp, sf, NULL);
        /*NOTREACHED*/
        // Intel's C compiler respects the preceding lint pragma
        CILK_ASSERT(! "Should not return into this function! :(\n");
    }
}

CILK_ABI_VOID __assert_not_on_scheduling_fiber() {
//    CILK_ASSERT(cilk_fiber_get_current_fiber() != __cilkrts_get_tls_worker()->l->scheduling_fiber);
}

// TODO: This is temporary.
CILK_ABI_VOID __assert_future_counter(int count) {
    //CILK_ASSERT(__cilkrts_get_tls_worker()->l->frame_ff[0]->future_counter == count);
}   

CILK_ABI_VOID __print_curr_stack(char* str) {
    __cilkrts_worker *w = __cilkrts_get_tls_worker();
    cilk_fiber *fiber = w->l->frame_ff->fiber_self;
//    printf("%s curr fiber: %p (worker %d, references %d)\n", str, fiber, w->self, cilk_fiber_get_ref_count(fiber));
}

static CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame, cilk_fiber* curr_fiber, cilk_fiber* new_fiber) {
    cilk_fiber_data* new_fiber_data = cilk_fiber_get_data(new_fiber);
    // KYLE_TODO: Do I need this? It doesn't seem like it
    //new_fiber_data->resume_sf = first_frame;

    cilk_fiber_remove_reference_from_self_and_resume_other(curr_fiber, &(__cilkrts_get_tls_worker()->l->fiber_pool), new_fiber);
}

static CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame) {
    __cilkrts_worker* curr_worker = __cilkrts_get_tls_worker();
    cilk_fiber* new_exec_fiber = cilk_fiber_allocate(&(curr_worker->l->fiber_pool));
    // TODO: Handle the case that it is null more gracefully
    CILK_ASSERT(new_exec_fiber != NULL);
    CILK_ASSERT(first_frame);
    cilk_fiber_data* new_exec_fiber_data = cilk_fiber_get_data(new_exec_fiber);

    new_exec_fiber_data->resume_sf = first_frame;
    cilk_fiber_reset_state(new_exec_fiber, fiber_proc_to_resume_user_code_for_future);

    cilk_fiber *curr_fiber = NULL;
    // TODO: Should this be a full_frame lock or something? Do we need a lock?
    __cilkrts_worker_lock(curr_worker);
    full_frame *ff = curr_worker->l->frame_ff;
    __cilkrts_frame_lock(curr_worker, ff);

        //ff->call_stack->worker = curr_worker;
        //curr_worker->current_stack_frame = first_frame;

        

        ff->future_fiber = new_exec_fiber;
        curr_fiber = curr_worker->l->frame_ff->fiber_self;

    __cilkrts_frame_unlock(curr_worker, ff);
    __cilkrts_worker_unlock(curr_worker);

    //cilk_fiber_get_data(curr_fiber)->resume_sf = NULL;

    cilk_fiber_suspend_self_and_resume_other(curr_fiber, new_exec_fiber);
    if (first_frame->flags & CILK_FRAME_STOLEN) {
        printf("Hi?\n");
        user_code_resume_after_switch_into_runtime(curr_fiber);
        CILK_ASSERT(! "We should not return here!");
    }
}

static CILK_ABI_VOID __attribute__((noinline)) __spawn_future_helper(std::function<void(void)> func) {
    int* dummy = (int*) alloca(ZERO);
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);

    #ifndef FUTURE_IS_CILK_SPAWN
        //CILK_ASSERT(__cilkrts_get_tls_worker()->l->frame_ff->future_fiber);
    #endif

    __cilkrts_detach(&sf);

        func();

    __cilkrts_pop_frame(&sf);
    #ifndef FUTURE_IS_CILK_SPAWN
        __cilkrts_leave_future_frame(&sf);
    #else
        __cilkrts_leave_frame(&sf);
    #endif
}

CILK_ABI_VOID __attribute__((noinline)) __spawn_future_helper_helper(std::function<void(void)> func) {
    int* dummy = (int*) alloca(ZERO);
    printf("%p orig fiber\n", __cilkrts_get_tls_worker()->l->frame_ff->fiber_self);
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);


    #ifndef FUTURE_IS_CILK_SPAWN
        sf.flags |= CILK_FRAME_FUTURE_PARENT;
    #endif

    // At one point this wasn't true. Fixed a bug in cilk-abi.c that set the owner when it should not have.
    CILK_ASSERT(NULL == cilk_fiber_get_data(__cilkrts_get_tls_worker()->l->scheduling_fiber)->owner);

    // Just me being paranoid...
    CILK_ASSERT(__cilkrts_get_tls_worker()->l->frame_ff->future_flags == 0);
    CILK_ASSERT((sf.flags & CILK_FRAME_STOLEN) == 0);

    __cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
    full_frame *ff = NULL;
    __cilkrts_stack_frame *ff_call_stack = NULL;

    #ifndef FUTURE_IS_CILK_SPAWN
    if(!CILK_SETJMP(sf.ctx)) { 
        //BEGIN_WITH_WORKER_LOCK(w) {
            //ff = w->l->frame_ff;
        //    CILK_ASSERT(w->l->frame_ff);
            //w->l->frame_ff = NULL;
            //BEGIN_WITH_FRAME_LOCK(w, ff) {
                //ff_call_stack = ff->call_stack;
                //CILK_ASSERT(ff_call_stack && !ff_call_stack->call_parent);
                //setup_for_execution(w, ff, 0);
                //ff->call_stack = NULL;
            //} END_WITH_FRAME_LOCK(w, ff);
        //} END_WITH_WORKER_LOCK(w);

        // SWITCH FIBERS!
        __cilkrts_switch_fibers(&sf);
    } else {
    #endif
        // This SHOULD occur after switching fibers; steal from here
        if (!CILK_SETJMP(sf.ctx)) {
            // Run the future helper on the new fiber
            __spawn_future_helper(func);
            CILK_ASSERT((sf.flags & CILK_FRAME_STOLEN) == 0);
            #ifndef FUTURE_IS_CILK_SPAWN
                // Return to the original fiber
                __cilkrts_switch_fibers(&sf, __cilkrts_get_tls_worker()->l->frame_ff->future_fiber, __cilkrts_get_tls_worker()->l->frame_ff->fiber_self);
            #endif
        }
        CILK_ASSERT(sf.flags & CILK_FRAME_STOLEN);

    #ifndef FUTURE_IS_CILK_SPAWN
    }
    #endif

    printf("Hiiii!\n");

    #ifndef FUTURE_IS_CILK_SPAWN
    if (sf.flags & CILK_FRAME_STOLEN) {
        CILK_ASSERT(__cilkrts_get_tls_worker()->l->frame_ff->future_flags == CILK_FUTURE_PARENT);
    }
    #endif

    // TODO: Rework it so we don't do this on futures
    if (sf.flags & CILK_FRAME_UNSYNCHED) {
        if (!CILK_SETJMP(sf.ctx)) {
            #ifndef FUTURE_IS_CILK_SPAWN
                __cilkrts_future_sync(&sf);
            #else
                __cilkrts_sync(&sf);
            #endif
        }
        //__cilkrts_rethrow(&sf);
        update_pedigree_after_sync(&sf);
    }

    printf("I'm here! wherever here is...\n");
    printf("Stack: %p\n", sf.worker->l->frame_ff->fiber_self);
    cilk_fiber_get_data(__cilkrts_get_tls_worker()->l->frame_ff->fiber_self)->resume_sf = NULL;

    __cilkrts_pop_frame(&sf);
    #ifndef FUTURE_IS_CILK_SPAWN
        __cilkrts_leave_future_parent_frame(&sf);
    #else
        __cilkrts_leave_frame(&sf);
    #endif
    printf("Going to parent sf %p (prev %p)\n", __cilkrts_get_tls_worker()->current_stack_frame, &sf);
}

}
