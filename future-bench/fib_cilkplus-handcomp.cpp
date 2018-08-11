#include <cilk/cilk.h>
#include <stdio.h>
#include <stdlib.h>
#include "ktiming.h"

#include "internal/abi.h"

#ifndef TIMES_TO_RUN
#define TIMES_TO_RUN 10 
#endif

/* 
 * fib 39: 63245986
 * fib 40: 102334155
 * fib 41: 165580141 
 * fib 42: 267914296
 */

extern "C" {
void __cilkrts_detach(__cilkrts_stack_frame*);
void __cilkrts_pop_frame(__cilkrts_stack_frame*);
}

int fib(int n);

void __attribute__((noinline)) fib_helper(int *x, int n) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

    *x = fib(n);

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}

int __attribute__((noinline)) fib(int n) {
    int x = 0, y = 0;

    if (n < 2) {
        return n;
    }

    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);
    
    if (!CILK_SETJMP(sf.ctx)) {
      fib_helper(&x, n-1);
    }

    y = fib(n - 2);

    if (sf.flags & CILK_FRAME_UNSYNCHED) {
      if (!CILK_SETJMP(sf.ctx)) {
        __cilkrts_sync(&sf);
      }
    }

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);

    return x+y;
}


int __attribute__((noinline)) run(int n, uint64_t *running_time, int i) {
    int res = 0;
    clockmark_t begin, end; 

        begin = ktiming_getmark();
        res = fib(n);
        end = ktiming_getmark();
        running_time[i] = ktiming_diff_usec(&begin, &end);

    return res;
}

void __attribute__((noinline)) run_helper(int *res, int n, uint64_t *running_time, int i) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

    *res = run(n, running_time, i);

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);
}

int main(int argc, char * args[]) {
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
    for (int i = 0; i < TIMES_TO_RUN; i++) {
        if (!CILK_SETJMP(sf.ctx)) {
          run_helper(&res, n, running_time, i);
        }
        if (sf.flags & CILK_FRAME_UNSYNCHED) {
          if (!CILK_SETJMP(sf.ctx)) {
            __cilkrts_sync(&sf);
          }
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
