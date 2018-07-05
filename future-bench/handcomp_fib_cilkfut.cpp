#include "../src/future.h"
#include <cilk/cilk.h>
#include <stdio.h>
#include <stdlib.h>
#include "ktiming.h"
#include "internal/abi.h"
#include "../cilkrtssuspend/runtime/cilk_fiber.h"
#include "../cilkrtssuspend/runtime/os.h"
#include "../cilkrtssuspend/runtime/jmpbuf.h"
#include "../cilkrtssuspend/runtime/global_state.h"

#ifndef TIMES_TO_RUN
#define TIMES_TO_RUN 3 
#endif

/* 
 * fib 39: 63245986
 * fib 40: 102334155
 * fib 41: 165580141 
 * fib 42: 267914296
 */

extern CILK_ABI_VOID __cilkrts_future_sync(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_frame(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_parent_frame(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_switch_fibers_back(__cilkrts_stack_frame* first_frame, cilk_fiber* curr_fiber, cilk_fiber* new_fiber);
extern CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame);

extern "C" {
extern CILK_ABI_VOID __cilkrts_detach(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_pop_frame(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_enter_frame_1(__cilkrts_stack_frame *sf);
}

int fib(int n);

void __attribute__((noinline)) fib_future_helper(cilk::future<int>& fut, int n) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

        printf("N: %d\n", n);
        int x = fib(n);
        fut.put(x);

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_future_frame(&sf);
}

int __attribute__((noinline)) fib(int n) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);

    CILK_ASSERT(n <= 5);

    int x;
    int y;

    if(n < 2) {
        printf("Hello there?\n");
        __cilkrts_pop_frame(&sf);
        __cilkrts_leave_frame(&sf);
        printf("Hi!\n");
        return n;
    }

    printf("woah there...\n");

    sf.flags |= CILK_FRAME_FUTURE_PARENT;

    //cilk_fiber *volatile initial_fiber = cilk_fiber_get_current_fiber();
    
    cilk::future<int> x_fut = cilk::future<int>();// = new cilk::future<int>();

    //if (!CILK_SETJMP(sf.ctx)) {
    //    __cilkrts_switch_fibers(&sf);
    //} else if (sf.flags & CILK_FRAME_FUTURE_PARENT) {
        cilkg_increment_pending_futures(__cilkrts_get_tls_worker_fast()->g);

        fib_future_helper(x_fut, n-1);

        //cilk_fiber *fut_fiber = __cilkrts_pop_tail_future_fiber();

        //__cilkrts_switch_fibers_back(&sf, fut_fiber, initial_fiber);
    //}

    if (sf.flags & CILK_FRAME_UNSYNCHED) {
        if (!CILK_SETJMP(sf.ctx)) {
            __cilkrts_future_sync(&sf);
        }
    }
    //cilk_future_create__stack(int, x_fut, fib, n-1);
    //x =  fib(n - 1);
    y = fib(n - 2);
    x = x_fut.get();//cilk_future_get(x_fut);
    //delete x_fut;
    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
    return x+y;
}

int run(int n, uint64_t *running_time) {
    int res;
    clockmark_t begin, end; 

    for(int i = 0; i < TIMES_TO_RUN; i++) {
        begin = ktiming_getmark();
        CILK_ASSERT(n == 5);
        res = fib(n);
        end = ktiming_getmark();
        running_time[i] = ktiming_diff_usec(&begin, &end);
    }

    return res;
}

void __attribute__((noinline)) run_helper(int* res, int n, uint64_t* running_time) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

        *res = run(n, running_time);

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}

int __attribute__((noinline)) main(int argc, char * args[]) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);

    int n;
    uint64_t running_time[TIMES_TO_RUN];

    if(argc != 2) {
        fprintf(stderr, "Usage: fib [<cilk-options>] <n>\n");
        exit(1);
    }
    
    n = atoi(args[1]);

    int res = 0;
    run_helper(&res, n, running_time);

    if (sf.flags & CILK_FRAME_UNSYNCHED) {
        if (!CILK_SETJMP(sf.ctx)) {
            __cilkrts_sync(&sf);
        }
    }

    printf("Result: %d\n", res);

    if( TIMES_TO_RUN > 10 ) 
        print_runtime_summary(running_time, TIMES_TO_RUN); 
    else 
        print_runtime(running_time, TIMES_TO_RUN); 

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);

    return 0;
}
