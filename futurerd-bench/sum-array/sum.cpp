#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <future.hpp>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include "../util/util.hpp"

typedef struct pair_t {
    unsigned long n;
    cilk::future<struct pair_t> *fut;
} pair_t;

static pair_t produce(unsigned long n) {
    pair_t res = { n, NULL };

    if(n > 0) { 
        cilk::future<pair_t> *fut;
        // create_future_handle(pair_t, fut);
        // spawn_proc_with_future_handle(fut, produce, n-1);
        create_future(pair_t, fut, produce, n-1);
        res.fut = fut; 
    } 

    return res;
} 

static unsigned long consume(unsigned long curr_sum, pair_t data) {
    
    unsigned long res = curr_sum; 

    if(data.fut != NULL) {
        curr_sum += data.n;
        pair_t next_data = data.fut->get();
        res = consume(curr_sum, next_data);
    }

    return res;
} 

int main(int argc, char *argv[]) {

    unsigned long n = 1024;

    if(argc < 2) {
        fprintf(stderr, "Usage: sum <n>\n");
        fprintf(stderr, "Note that, due to the recursive nature of this program,\n");
        fprintf(stderr, "input n > 260000 may run out of stack space.\n");
        exit(1);
    }

    ensure_serial_execution();
    n = atoi(argv[1]);

    pair_t data = produce(n);
    unsigned long sum = consume(0, data);
    unsigned long check_sum = n * (n + 1) / 2;
    
    if(check_sum == sum) {
        printf("Check passed.\n");
    } else {
        printf("Check FAILED.\n");
    }
    printf("Result: %lu\n", sum);

    return 0;
}

