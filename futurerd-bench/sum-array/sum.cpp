#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cilk/future.hpp>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include "../util/util.hpp"

typedef struct pair_t {
    unsigned long n;
    cilk::future<struct pair_t> *fut;
    pair_t(volatile pair_t const& other) {
      this->n = other.n;
      this->fut = other.fut;
    }
    pair_t(pair_t const& other) {
      this->n = other.n;
      this->fut = other.fut;
    }
    pair_t(unsigned long n=0) {
      this->n = n;
      this->fut = NULL;
    }
    pair_t& operator=(pair_t const& other) volatile {
      this->n = other.n;
      this->fut = other.fut;
      return *(const_cast<pair_t*>(this));
    }
} pair_t;

pair_t produce(unsigned long n);

void __attribute__((noinline)) produce_fut_helper(cilk::future<pair_t> *fut, unsigned long n) {
    FUTURE_HELPER_PREAMBLE;

    void *__cilkrts_deque = fut->put(produce(n));
    if (__cilkrts_deque) __cilkrts_resume_suspended(__cilkrts_deque, 2);
    
    FUTURE_HELPER_EPILOGUE;
}

pair_t __attribute__((noinline)) produce(unsigned long n) {
    CILK_FUNC_PREAMBLE;

    pair_t res(n);

    if(n > 0) { 
        //cilk::future<pair_t> *fut;
        res.fut = new cilk::future<pair_t>(); 
        START_FIRST_FUTURE_SPAWN;
            produce_fut_helper(res.fut, n-1);
        END_FUTURE_SPAWN;
        //cilk_future_create(pair_t, res.fut, produce, n-1);
    } 

    CILK_FUNC_EPILOGUE;

    return res;
} 

static unsigned long consume(unsigned long curr_sum, pair_t data) {
    
    unsigned long res = curr_sum; 

    if (data.fut != NULL) {
        curr_sum += data.n;
        pair_t next_data = (pair_t)data.fut->get();
        res = consume(curr_sum, next_data);
        delete data.fut;
    }
    /*pair_t next_data;
    while (data.fut != NULL) {
        res += data.n;
        next_data = data.fut->get();
        delete data.fut;
        data = next_data;
    }*/

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

    __cilkrts_set_param("stack size","2097152");
    CILK_FUNC_PREAMBLE;

    n = atoi(argv[1]);

    pair_t data(n);
    data.fut = new cilk::future<pair_t>(); 
    START_FIRST_FUTURE_SPAWN;
        produce_fut_helper(data.fut, n-1);
    END_FUTURE_SPAWN;

    //pair_t data = produce(n);
    unsigned long sum = consume(0, data);
    unsigned long check_sum = n * (n + 1) / 2;
    
    if(check_sum == sum) {
        printf("Check passed.\n");
    } else {
        printf("Check FAILED.\n");
    }
    printf("Result: %lu\n", sum);

    CILK_FUNC_EPILOGUE;

    return 0;
}

