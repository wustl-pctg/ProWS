//#include <cilk/cilk.h>
#include <stdio.h>
#include <stdlib.h>
#include "ktiming.h"

#ifndef TIMES_TO_RUN
#define TIMES_TO_RUN 3 
#endif

int times_to_run = TIMES_TO_RUN;

#define cilk_spawn
#define cilk_sync

/* 
 * fib 39: 63245986
 * fib 40: 102334155
 * fib 41: 165580141 
 * fib 42: 267914296
 */


int fib(int n) {
    int x, y;

    if(n < 2) {
        return n;
    }
    
    x = cilk_spawn fib(n - 1);
    y = fib(n - 2);
    cilk_sync;
    return x+y;
}


int __attribute__((noinline)) run(int n, uint64_t *running_time, int i) {
    int res;
    clockmark_t begin, end; 

        begin = ktiming_getmark();
        res = fib(n);
        end = ktiming_getmark();
        running_time[i] = ktiming_diff_usec(&begin, &end);

    return res;
}

int main(int argc, char * args[]) {
    int n;

    if(argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: fib <n> [<times_to_run>]\n");
        exit(1);
    }
    
    n = atoi(args[1]);
    if (argc == 3) {
       times_to_run = atoi(args[2]);
    }
    uint64_t* running_time = (uint64_t*)malloc(times_to_run * sizeof(uint64_t));

    //uint64_t running_time[TIMES_TO_RUN];

    int res = 0;
    for (int i = 0; i < times_to_run; i++) {
        res = cilk_spawn run(n, running_time, i);
        cilk_sync;
    }

    printf("Result: %d\n", res);

    if( times_to_run > 10 ) 
        print_runtime_summary(running_time, times_to_run); 
    else 
        print_runtime(running_time, times_to_run); 

    free(running_time);

    return 0;
}
