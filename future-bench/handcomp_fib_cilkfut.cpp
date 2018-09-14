#include <cilk/cilk.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include "ktiming.h"
#include "internal/abi.h"
#include "cilk/future.h"
//#include "../cilkrtssuspend/runtime/cilk_fiber.h"
//#include "../cilkrtssuspend/runtime/os.h"
//#include "../cilkrtssuspend/runtime/jmpbuf.h"
//#include "../cilkrtssuspend/runtime/global_state.h"
//#include "../cilkrtssuspend/runtime/full_frame.h"
//#include "../cilkrtssuspend/runtime/scheduler.h"
//#include "../cilkrtssuspend/runtime/local_state.h"

#ifndef TIMES_TO_RUN
#define TIMES_TO_RUN 10 
#endif

//#define TEST_INTEROP_PRE_FUTURE_CREATE
//#define TEST_INTEROP_POST_FUTURE_CREATE
//#define TEST_INTEROP_MULTI_FUTURE

//#define FUTURE_AFTER_SYNC

/* 
 * fib 39: 63245986
 * fib 40: 102334155
 * fib 41: 165580141 
 * fib 42: 267914296
 */

int fib(int n);
void fib_helper(int *res, int n);

void  __attribute__((noinline)) fib_fut(cilk::future<int> *x, int n) {
    FUTURE_HELPER_PREAMBLE;
    
    void *__cilk_deque = x->put(fib(n));
    if (__builtin_expect(__cilk_deque != NULL, 0)) {
            __cilkrts_resume_suspended(__cilk_deque, 2);
    }

    FUTURE_HELPER_EPILOGUE;
}

int  __attribute__((noinline)) fib(int n) {
    int x;
    int y;

    if(n < 2) {
        return n;
    }

    CILK_FUNC_PREAMBLE;

    #ifdef TEST_INTEROP_PRE_FUTURE_CREATE
        #pragma message ("using spawn pre fut fib interop")
        if (!CILK_SETJMP(sf.ctx)) {
            fib_helper(&y, n-2);
        }
    #endif

    cilk::future<int> x_fut = cilk::future<int>();

    START_FIRST_FUTURE_SPAWN;
        fib_fut(&x_fut, n-1);
    END_FUTURE_SPAWN;

    #ifdef TEST_INTEROP_POST_FUTURE_CREATE
        #pragma message ("using using spawn post fut fib interop")
        if (!CILK_SETJMP(sf.ctx)) {
            fib_helper(&y, n-2);
        }
    #elif defined(TEST_INTEROP_MULTI_FUTURE)
        #pragma message ("using future fib interop")
        cilk::future<int> y_fut = cilk::future<int>();

        START_FUTURE_SPAWN;
            fib_fut(&y_fut, n-2);
        END_FUTURE_SPAWN;
    
        y = y_fut.get();
    #elif !defined(TEST_INTEROP_PRE_FUTURE_CREATE)
        #pragma message ("using regular fib (no interop)")
        y = fib(n-2);
    #endif

    #ifndef FUTURE_AFTER_SYNC
        #pragma message ("future get before sync")
        x = x_fut.get();
    #endif

    #if defined(TEST_INTEROP_PRE_FUTURE_CREATE) || defined(TEST_INTEROP_POST_FUTURE_CREATE)
        #pragma message ("Added a synch to the function")

        SYNC;
    #endif

    #ifdef FUTURE_AFTER_SYNC
        #pragma message ("future get after sync")
        x = x_fut.get();
    #endif

    int _tmp = x+y;

    // If we aren't careful, it turns out lto
    // gets too agressive and starts popping
    // frames at inappropriate moments
    __asm__ volatile ("" ::: "memory");
    CILK_FUNC_EPILOGUE;

    return _tmp;
}

void __attribute__((noinline)) fib_helper(int* res, int n) {
    SPAWN_HELPER_PREAMBLE;

    *res = fib(n);
    
    SPAWN_HELPER_EPILOGUE;
}

int __attribute__((noinline)) run(int n, uint64_t *running_time) {
    CILK_FUNC_PREAMBLE;

    int res;
    clockmark_t begin, end; 

    for(int i = 0; i < TIMES_TO_RUN; i++) {
        begin = ktiming_getmark();

        if (!CILK_SETJMP(sf.ctx)) {
            fib_helper(&res, n);
        }

        SYNC;

        end = ktiming_getmark();
        running_time[i] = ktiming_diff_usec(&begin, &end);
    }


    CILK_FUNC_EPILOGUE;

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

    int res = run(n, &running_time[0]);
//    cilkg_set_param("local stacks", "128");
//    cilkg_set_param("shared stacks", "128");

    printf("Res: %d\n", res);

    if( TIMES_TO_RUN > 10 ) 
        print_runtime_summary(running_time, TIMES_TO_RUN); 
    else 
        print_runtime(running_time, TIMES_TO_RUN); 

    return 0;
}

