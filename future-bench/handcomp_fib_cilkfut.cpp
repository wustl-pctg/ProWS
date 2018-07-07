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
#define TIMES_TO_RUN 1 
#endif

/* 
 * fib 39: 63245986
 * fib 40: 102334155
 * fib 41: 165580141 
 * fib 42: 267914296
 */

extern int ZERO;

extern CILK_ABI_VOID __cilkrts_future_sync(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_frame(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_parent_frame(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_switch_fibers_back(__cilkrts_stack_frame* first_frame, cilk_fiber* curr_fiber, cilk_fiber* new_fiber);
extern CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame);

extern "C" {
//extern CILK_ABI_VOID __cilkrts_detach(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_pop_frame(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_enter_frame_1(__cilkrts_stack_frame *sf);
}

inline void my_cilkrts_detach(struct __cilkrts_stack_frame *self) {
	//struct __cilkrts_worker *w = self->worker;
	struct __cilkrts_worker *w = __cilkrts_get_tls_worker();
	struct __cilkrts_stack_frame *parent = self->call_parent;
	struct __cilkrts_stack_frame *volatile *tail = *w->tail;

	self->spawn_helper_pedigree = w->pedigree;
	self->call_parent->parent_pedigree = w->pedigree;
	w->pedigree.rank = 1; // Need nonzero for precomputing pedigrees
	w->pedigree.parent = &self->spawn_helper_pedigree;

	CILK_ASSERT(tail < *w->ltq_limit);
	*tail++ = parent;

	/* The stores are separated by a store fence (noop on x86)
	 *  or the second store is a release (st8.rel on Itanium) */
	*w->tail = tail;
	self->flags |= CILK_FRAME_DETACHED;
}

//
//void fib(int n, int* res);
//
//int __attribute__((noinline)) intermediate_fib(const int n) {
//    if (n < 2) {
//        return n;
//    }
//
//    int res = 0;
//
//    fib(n, &res);
//
//    return res;
//}
//
//void __attribute__((noinline)) fib_future_helper(cilk::future<int>& fut, int n) {
//    __cilkrts_stack_frame sf;
//    __cilkrts_enter_frame_1(&sf);
//    __cilkrts_detach(&sf);
//
//        //printf("N: %d\n", n);
//        //int x = fib(n);
//        //fut.put(x);
//        fut.put(intermediate_fib(n));
//
//    __cilkrts_pop_frame(&sf);
//    __cilkrts_leave_future_frame(&sf);
//}
//
//void __attribute__((noinline)) fib_helper(int *x, int n) {
//    __cilkrts_stack_frame sf;
//    __cilkrts_enter_frame_1(&sf);
//    __cilkrts_detach(&sf);
//
//        *x = intermediate_fib(n);
//
//    __cilkrts_pop_frame(&sf);
//    __cilkrts_leave_future_frame(&sf);
//}
//
//void __attribute__((noinline)) fib(int n, int* res) {
//    __cilkrts_stack_frame sf;
//    __cilkrts_enter_frame_1(&sf);
//
//    int x;
//    int y;
//
//    //sf.flags |= CILK_FRAME_FUTURE_PARENT;
//
//    //cilk_fiber *volatile initial_fiber = cilk_fiber_get_current_fiber();
//    
//    //cilk::future<int> x_fut;
//
//    if (!CILK_SETJMP(sf.ctx)) {
//        fib_helper(&x, n-1);
//        //__cilkrts_switch_fibers(&sf);
//    } /*else if (sf.flags & CILK_FRAME_FUTURE_PARENT) {
//        fib_future_helper(x_fut, n-1);
//
//        cilk_fiber *fut_fiber = __cilkrts_pop_tail_future_fiber();
//
//        __cilkrts_switch_fibers_back(&sf, fut_fiber, initial_fiber);
//    }*/
//
//    if (sf.flags & CILK_FRAME_UNSYNCHED) {
//        if (!CILK_SETJMP(sf.ctx)) {
//            //__cilkrts_future_sync(&sf);
//            __cilkrts_sync(&sf);
//        }
//    }
//
//    y = intermediate_fib(n - 2);
//    //x = x_fut.get();
//
//    *res = x+y;
//
//    __cilkrts_pop_frame(&sf);
//    __cilkrts_leave_frame(&sf);
//}
//
//int __attribute__((noinline)) run(int* n, uint64_t *running_time) {
//    int res;
//    clockmark_t begin, end; 
//
//    for(int i = 0; i < TIMES_TO_RUN; i++) {
//        begin = ktiming_getmark();
//        printf("N: %d\n", *n);
//        res = intermediate_fib(*n);
//        end = ktiming_getmark();
//        running_time[i] = ktiming_diff_usec(&begin, &end);
//    }
//
//    return res;
//}
//
//void __attribute__((noinline)) run_helper(int* res, int* n, uint64_t* running_time) {
//    __cilkrts_stack_frame sf;
//    __cilkrts_enter_frame_1(&sf);
//    __cilkrts_detach(&sf);
//
//        *res = run(n, running_time);
//
//    __cilkrts_pop_frame(&sf);
//    __cilkrts_leave_frame(&sf);
//}
//
//int __attribute__((noinline)) main(int argc, char * args[]) {
//    __cilkrts_stack_frame sf;
//    __cilkrts_enter_frame_1(&sf);
//
//    int* n = (int*)malloc(sizeof(int));
//    uint64_t *running_time = (uint64_t*)malloc(TIMES_TO_RUN * sizeof(uint64_t));
//
//    if(argc != 2) {
//        fprintf(stderr, "Usage: fib [<cilk-options>] <n>\n");
//        exit(1);
//    }
//    
//    *n = atoi(args[1]);
//
//    int res = 0;
//    //run_helper(&res, n, running_time);
//    res = run(n, running_time);
//
//    if (sf.flags & CILK_FRAME_UNSYNCHED) {
//        if (!CILK_SETJMP(sf.ctx)) {
//            __cilkrts_sync(&sf);
//        }
//    }
//
//    printf("Result: %d\n", res);
//
//    if( TIMES_TO_RUN > 10 ) 
//        print_runtime_summary(running_time, TIMES_TO_RUN); 
//    else 
//        print_runtime(running_time, TIMES_TO_RUN); 
//
//    __cilkrts_pop_frame(&sf);
//    __cilkrts_leave_frame(&sf);
//
//    return 0;
//}
int fib(int n);

void  fib_fut(cilk::future<int> *x, int n) {
    int* dummy = (int*) alloca(ZERO);
    __cilkrts_stack_frame* sf = (__cilkrts_stack_frame*) alloca(sizeof(__cilkrts_stack_frame));;
    __cilkrts_enter_frame_fast_1(sf);
    my_cilkrts_detach(sf);
    
    void *__cilk_deque = x->put(fib(n));
    if (__cilk_deque) {
        __cilkrts_resume_suspended(__cilk_deque, 1);
    }

    __cilkrts_pop_frame(sf);
    __cilkrts_leave_future_frame(sf);
}

int  fib(int n) {
    int* dummy = (int*) alloca(ZERO);

    int x;
    int y;

    if(n <= 2) {
        return n;
    }

    __cilkrts_stack_frame* sf = (__cilkrts_stack_frame*) alloca(sizeof(__cilkrts_stack_frame));;
    __cilkrts_enter_frame_1(sf);
    

    sf->flags |= CILK_FRAME_FUTURE_PARENT;

    cilk_fiber *volatile initial_fiber = cilk_fiber_get_current_fiber();
    
    cilk::future<int> x_fut = cilk::future<int>();
     
    if (!CILK_SETJMP(sf->ctx)) {
        __cilkrts_switch_fibers(sf);

    } else if (sf->flags & CILK_FRAME_FUTURE_PARENT) {
        fib_fut(&x_fut, n-1);

        cilk_fiber *fut_fiber = __cilkrts_pop_tail_future_fiber();

        __cilkrts_switch_fibers_back(sf, fut_fiber, initial_fiber);
    }

    if (sf->flags & CILK_FRAME_UNSYNCHED) {
        if (!CILK_SETJMP(sf->ctx)) {
            __cilkrts_future_sync(sf);
            //__cilkrts_sync(&sf);
        }
    }

    //x =  fib(n - 1);
    y = fib(n - 2);
    x = x_fut.get();//cilk_future_get(x_fut);
    //delete x_fut;
    int _tmp = x+y;

    __cilkrts_pop_frame(sf);
    __cilkrts_leave_frame(sf);

    return _tmp;
}
/*
void fib_fut(cilk::future<int>& f, int n) {
    int* dummy = (int*) alloca(ZERO);
    __cilkrts_stack_frame* sf = (__cilkrts_stack_frame*) alloca(sizeof(__cilkrts_stack_frame));;
    __cilkrts_enter_frame_fast_1(sf);
    __cilkrts_detach(sf);

    f.put(fib(n));

    __cilkrts_pop_frame(sf);
    __cilkrts_leave_frame(sf);
    //__cilkrts_leave_future_frame(&sf);
}
*/

void fib_helper(int* res, int n) {
    int* dummy = (int*) alloca(ZERO);
    __cilkrts_stack_frame* sf = (__cilkrts_stack_frame*) alloca(sizeof(__cilkrts_stack_frame));;
    __cilkrts_enter_frame_1(sf);
    my_cilkrts_detach(sf);

    *res = fib(n);
    
    __cilkrts_pop_frame(sf);
    __cilkrts_leave_frame(sf);
}

int run(int n, uint64_t *running_time) {
    int* dummy = (int*) alloca(ZERO);
    __cilkrts_stack_frame* sf = (__cilkrts_stack_frame*) alloca(sizeof(__cilkrts_stack_frame));;
    __cilkrts_enter_frame(sf);
    int res;
    clockmark_t begin, end; 

    printf("%p\n", running_time);

    for(int i = 0; i < TIMES_TO_RUN; i++) {
        begin = ktiming_getmark();

        printf("%d\n", n);
        fib_helper(&res, n);

        if (sf->flags & CILK_FRAME_UNSYNCHED) {
            if (!CILK_SETJMP(sf->ctx)) {
                __cilkrts_sync(sf);
            }
        }
        //res = cilk_spawn fib(n);
        //cilk_sync;
        end = ktiming_getmark();
        running_time[i] = ktiming_diff_usec(&begin, &end);
    }

    printf("Res: %d\n", res);

    __cilkrts_pop_frame(sf);
    __cilkrts_leave_frame(sf);
    return res;
}

int main(int argc, char * args[]) {
    int* dummy = (int*) alloca(ZERO);
    int n;
    //uint64_t running_time[TIMES_TO_RUN];
    uint64_t *running_time = (uint64_t*)malloc(TIMES_TO_RUN * sizeof(uint64_t));

    if(argc != 2) {
        fprintf(stderr, "Usage: fib [<cilk-options>] <n>\n");
        exit(1);
    }
    
    n = atoi(args[1]);

    int res = run(n, &running_time[0]);

    if( TIMES_TO_RUN > 10 ) 
        print_runtime_summary(running_time, TIMES_TO_RUN); 
    else 
        print_runtime(running_time, TIMES_TO_RUN); 

    return 0;
}

