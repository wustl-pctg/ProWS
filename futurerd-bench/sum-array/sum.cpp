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

static pair_t produce(unsigned long n) {
    pair_t res(n);

    if(n > 0) { 
        //cilk::future<pair_t> *fut;
        //res.fut = fut; 
        cilk_future_create(pair_t, res.fut, produce, n-1);
    } 

    return res;
} 

static unsigned long consume(unsigned long curr_sum, pair_t data) {
    
    unsigned long res = curr_sum; 

    if (data.fut != NULL) {
        curr_sum += data.n;
        pair_t next_data = (pair_t)data.fut->get();
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

    __cilkrts_set_param("stack size","2097152");

    //ensure_serial_execution();
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

