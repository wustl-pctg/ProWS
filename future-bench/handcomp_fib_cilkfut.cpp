#include <cilk/cilk.h>
#include "../cilkrtssuspend/include/internal/abi.h"
#include "../cilkrtssuspend/runtime/rts-common.h"
#include "../cilkrtssuspend/runtime/full_frame.h"
#include "../cilkrtssuspend/runtime/local_state.h"
#include "../cilkrtssuspend/runtime/cilk_fiber.h"
#include "../cilkrtssuspend/runtime/scheduler.h"
#include "../cilkrtssuspend/runtime/os.h"
#include "../cilkrtssuspend/runtime/sysdep.h"
#include "../cilkrtssuspend/runtime/jmpbuf.h"
#include "../cilkrtssuspend/runtime/cilk-ittnotify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ktiming.h"
#include "../src/future.h"

#ifndef TIMES_TO_RUN
#define TIMES_TO_RUN 3 
#endif

/* 
 * fib 39: 63245986
 * fib 40: 102334155
 * fib 41: 165580141 
 * fib 42: 267914296
 */

void fib_fut(cilk::future<int>& f, int n);

extern CILK_ABI_VOID __cilkrts_future_sync(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_frame(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_parent_frame(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame);
extern CILK_ABI_VOID __cilkrts_switch_fibers_back(__cilkrts_stack_frame* first_frame, cilk_fiber* curr_fiber, cilk_fiber* new_fiber);

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
}

int fib(int n) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);

    int x;
    int y;

    if(n < 2) {
        return n;
    }
    
    cilk::future<int> x_fut;
    sf.flags |= CILK_FRAME_FUTURE_PARENT;
     
    if (!CILK_SETJMP(sf.ctx)) {
        __cilkrts_switch_fibers(&sf);
    } else if (sf.flags & CILK_FRAME_FUTURE_PARENT){
            cilkg_increment_pending_futures(__cilkrts_get_tls_worker_fast()->g);
            fib_fut(x_fut, n-1);

            __cilkrts_worker *curr_worker = __cilkrts_get_tls_worker_fast();
            __cilkrts_worker_lock(curr_worker);
            full_frame *frame = *curr_worker->l->frame_ff;
            __cilkrts_frame_lock(curr_worker, frame);
                cilk_fiber *fut_fiber = __cilkrts_pop_tail_future_fiber(frame);
                cilk_fiber *prev_fiber = __cilkrts_peek_tail_future_fiber(frame);
                if (!prev_fiber) {
                    prev_fiber = frame->fiber_self;
                }
            __cilkrts_frame_unlock(curr_worker, frame);
            __cilkrts_worker_unlock(curr_worker);

            __cilkrts_switch_fibers_back(&sf, fut_fiber, prev_fiber);
    }

    if (sf.flags & CILK_FRAME_UNSYNCHED) {
        if (!CILK_SETJMP(sf.ctx)) {
            __cilkrts_future_sync(&sf);
        }
    }

    __asm__ volatile ("" ::: "memory");
    __cilkrts_worker *curr_worker = __cilkrts_get_tls_worker_fast();//sf.worker;
    __cilkrts_worker_lock(curr_worker);
    full_frame *ff = *curr_worker->l->frame_ff;
    __cilkrts_frame_lock(curr_worker, ff);

    cilk_fiber_get_data(ff->fiber_self)->resume_sf = NULL;

    __cilkrts_frame_unlock(curr_worker, ff);
    __cilkrts_worker_unlock(curr_worker);
    //cilk_future_create__stack(int, x_fut, fib, n-1);
    //x =  fib(n - 1);
    y = fib(n - 2);
    x = x_fut.get();//cilk_future_get(x_fut);
    //delete x_fut;
    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);

    return x+y;
}

void fib_fut(cilk::future<int>& f, int n) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

    f.put(fib(n));

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_future_frame(&sf);
}

void fib_helper(int* res, int n) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

    *res = fib(n);
    
    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}

int run(int n, uint64_t *running_time) {
    //int* dummy = (int*) alloca(ZERO);
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);
    int res;
    clockmark_t begin, end; 

    for(int i = 0; i < TIMES_TO_RUN; i++) {
        begin = ktiming_getmark();

        fib_helper(&res, n);

        if (sf.flags & CILK_FRAME_UNSYNCHED) {
            if (!CILK_SETJMP(sf.ctx)) {
                __cilkrts_sync(&sf);
            }
        }
        //res = cilk_spawn fib(n);
        //cilk_sync;
        end = ktiming_getmark();
        running_time[i] = ktiming_diff_usec(&begin, &end);
    }

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
    return res;
}

int main(int argc, char * args[]) {
    int n;
    uint64_t running_time[TIMES_TO_RUN];

    if(argc != 2) {
        fprintf(stderr, "Usage: fib [<cilk-options>] <n>\n");
        exit(1);
    }
    
    n = atoi(args[1]);

    int res = run(n, running_time);

    printf("Result: %d\n", res);

    if( TIMES_TO_RUN > 10 ) 
        print_runtime_summary(running_time, TIMES_TO_RUN); 
    else 
        print_runtime(running_time, TIMES_TO_RUN); 

    return 0;
}
