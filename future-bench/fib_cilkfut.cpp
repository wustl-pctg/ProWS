#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include <stdio.h>
#include <stdlib.h>
#include "ktiming.h"
#include "../src/future.h"

#ifndef TIMES_TO_RUN
#define TIMES_TO_RUN 10
#endif

/* 
 * fib 39: 63245986
 * fib 40: 102334155
 * fib 41: 165580141 
 * fib 42: 267914296
 */


int fib(int n) {
    //cilk::future<int> *x_fut;
    int x;
    int y;

    if(n <= 2) {
        return n;
    }
    
    cilk_future_create__stack(int, x_fut, fib, n-1);
    //x =  fib(n - 1);
    y = fib(n - 2);
    x = x_fut.get();//cilk_future_get(x_fut);
    //delete x_fut;
    return x+y;
}

int run(int n, uint64_t *running_time) {
    int res;
    clockmark_t begin, end; 

    for(int i = 0; i < TIMES_TO_RUN; i++) {
        begin = ktiming_getmark();
        res = fib(n);
        end = ktiming_getmark();
        running_time[i] = ktiming_diff_usec(&begin, &end);
    }

    return res;
}

int main(int argc, char * args[]) {
    //__cilkrts_set_param("local stacks", "128");
    //__cilkrts_set_param("shared stacks", "128");

    int n;
    uint64_t running_time[TIMES_TO_RUN];

    if(argc != 2) {
        fprintf(stderr, "Usage: fib [<cilk-options>] <n>\n");
        exit(1);
    }
    
    n = atoi(args[1]);
    __cilkrts_set_param("local stacks", "128");
    __cilkrts_set_param("shared stacks", "128");

    int res = 0;
    res = cilk_spawn run(n, running_time);
    cilk_sync;

    printf("Result: %d\n", res);

    if( TIMES_TO_RUN > 10 ) 
        print_runtime_summary(running_time, TIMES_TO_RUN); 
    else 
        print_runtime(running_time, TIMES_TO_RUN); 

    return 0;
}
