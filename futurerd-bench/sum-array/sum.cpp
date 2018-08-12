#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <future.hpp>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include "../util/util.hpp"

#include "internal/abi.h"

class cilk_fiber;

extern "C" {
cilk_fiber* cilk_fiber_get_current_fiber();
void cilk_fiber_do_post_switch_actions(cilk_fiber*);
void __cilkrts_detach(__cilkrts_stack_frame*);
void __cilkrts_pop_frame(__cilkrts_stack_frame*);
void** cilk_fiber_get_resume_jmpbuf(cilk_fiber*);
}

char* __cilkrts_switch_fibers();
void __cilkrts_switch_fibers_back(cilk_fiber*);
void __cilkrts_leave_future_frame(__cilkrts_stack_frame*);

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
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);

    void *__cilkrts_deque = fut->put(produce(n));
    if (__cilkrts_deque) __cilkrts_resume_suspended(__cilkrts_deque, 2);
    
    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_future_frame(&sf);
}

pair_t __attribute__((noinline)) produce(unsigned long n) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);

    pair_t res(n);

    if(n > 0) { 
        //cilk::future<pair_t> *fut;
        res.fut = new cilk::future<pair_t>(); 
        cilk_fiber *initial_fiber = cilk_fiber_get_current_fiber();
        sf.flags |= CILK_FRAME_FUTURE_PARENT;
        if (!CILK_SETJMP(cilk_fiber_get_resume_jmpbuf(initial_fiber))) {
            char *new_sp = __cilkrts_switch_fibers();
            char *old_sp = NULL;

            __asm__ volatile ("mov %%rsp, %0" : "=r" (old_sp));
            __asm__ volatile ("mov %0, %%rsp" : : "r" (new_sp));

            produce_fut_helper(res.fut, n-1);

            __asm__ volatile ("mov %0, %%rsp" : : "r" (old_sp));

            __cilkrts_switch_fibers_back(initial_fiber);
        } 
        cilk_fiber_do_post_switch_actions(initial_fiber);
        sf.flags &= ~(CILK_FRAME_FUTURE_PARENT);
        //cilk_future_create(pair_t, res.fut, produce, n-1);
    } 

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);

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

    return res;
} 

int main(int argc, char *argv[]) {
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);

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

    pair_t data(n);
    data.fut = new cilk::future<pair_t>(); 
    cilk_fiber *initial_fiber = cilk_fiber_get_current_fiber();
    sf.flags |= CILK_FRAME_FUTURE_PARENT;
    if (!CILK_SETJMP(cilk_fiber_get_resume_jmpbuf(initial_fiber))) {
        char *new_sp = __cilkrts_switch_fibers();
        char *old_sp = NULL;

        __asm__ volatile ("mov %0, %%rsp" : "=r" (old_sp));
        __asm__ volatile ("mov %%rsp, %0" : : "r" (new_sp));

        produce_fut_helper(data.fut, n-1);

        __asm__ volatile ("mov %%rsp, %0" : : "r" (old_sp));

        __cilkrts_switch_fibers_back(initial_fiber);
    } 
    cilk_fiber_do_post_switch_actions(initial_fiber);
    sf.flags &= ~(CILK_FRAME_FUTURE_PARENT);

    //pair_t data = produce(n);
    unsigned long sum = consume(0, data);
    unsigned long check_sum = n * (n + 1) / 2;
    
    if(check_sum == sum) {
        printf("Check passed.\n");
    } else {
        printf("Check FAILED.\n");
    }
    printf("Result: %lu\n", sum);

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_frame(&sf);

    return 0;
}

