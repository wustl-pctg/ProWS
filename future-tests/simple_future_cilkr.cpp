#include <iostream>
#include <stdio.h>
#include "cilk/cilk.h"
#include "internal/abi.h"
#include "../cilkplus-rts/runtime/local_state.h"
#include "../cilkplus-rts/runtime/full_frame.h"
#include "../src/future.h"
#include <cstring>

#include <cstdint>

extern unsigned long ZERO;


extern CILK_ABI_VOID __cilkrts_future_sync(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_frame(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_leave_future_parent_frame(__cilkrts_stack_frame *sf);

extern "C" {

extern CILK_ABI_VOID __cilkrts_switch_fibers_back(__cilkrts_stack_frame* first_frame, cilk_fiber* curr_fiber, cilk_fiber* new_fiber);
extern CILK_ABI_VOID __cilkrts_switch_fibers(__cilkrts_stack_frame* first_frame);
extern CILK_ABI_VOID user_code_resume_after_switch_into_runtime(cilk_fiber*);
extern CILK_ABI_THROWS_VOID __cilkrts_rethrow(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID update_pedigree_after_sync(__cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_detach(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_pop_frame(struct __cilkrts_stack_frame *sf);
extern CILK_ABI_VOID __cilkrts_enter_frame_1(__cilkrts_stack_frame *sf);
extern void fiber_proc_to_resume_user_code_for_random_steal(cilk_fiber *fiber);
extern char* get_sp_for_executing_sf(char* stack_base,
                                     full_frame *ff,
                                     __cilkrts_stack_frame *sf);

extern int cilk_fiber_get_ref_count(cilk_fiber*);
//extern void __assert_future_counter(int count);
}

void __assert_future_counter(int count) {}

cilk::future<int>* test_future = NULL;
cilk::future<int>* test_future2 = NULL;
cilk::future<int>* test_future3 = NULL;
volatile int gdummy = 0;
volatile int dummy2 = 0;
//porr::spinlock output_lock;

void helloFuture(void);
void helloMoreFutures(void);
void thread1(void);
void thread2(void);
void thread3(void);
void thread4(void);

static CILK_ABI_VOID __attribute__((noinline)) hello_future_helper() {
    int* dummy = (int*) alloca(ZERO);
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_fast_1(&sf);
    __cilkrts_detach(&sf);
    //printf("hello_future_helper sf: %p\n", &sf);

        //func();
    helloFuture();
    //printf("Done with helloFuture!\n");

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_future_frame(&sf);
}

static void __attribute__((noinline)) hello_future_helper_helper() {
    int* dummy = (int*) alloca(ZERO);
    printf("%p orig fiber\n", __cilkrts_get_tls_worker()->l->frame_ff->fiber_self);
    __cilkrts_stack_frame sf;
    __cilkrts_enter_frame_1(&sf);
    printf("hello_future_helper_helper sf: %p\n", &sf);


    sf.flags |= CILK_FRAME_FUTURE_PARENT;

    // At one point this wasn't true. Fixed a bug in cilk-abi.c that set the owner when it should not have.
    CILK_ASSERT(NULL == cilk_fiber_get_data(__cilkrts_get_tls_worker()->l->scheduling_fiber)->owner);

    // Just me being paranoid...
    CILK_ASSERT(__cilkrts_get_tls_worker()->l->frame_ff->future_flags == 0);
    CILK_ASSERT((sf.flags & CILK_FRAME_STOLEN) == 0);

    __cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
    full_frame *ff = NULL;
    __cilkrts_stack_frame *ff_call_stack = NULL;

    __CILK_JUMP_BUFFER ctx_bkup;
    int done = 0;
    if(!CILK_SETJMP(sf.ctx)) { 
        memcpy(ctx_bkup, sf.ctx, 5*sizeof(void*));
        __cilkrts_switch_fibers(&sf);
    } else {
        // This SHOULD occur after switching fibers; steal from here
        if (!done) {
            done = 1;
            memcpy(sf.ctx, ctx_bkup, 5*sizeof(void*));
            //__spawn_future_helper(std::move(func));
            hello_future_helper();
            CILK_ASSERT((sf.flags & CILK_FRAME_STOLEN) == 0);
            // Return to the original fiber
            __cilkrts_switch_fibers_back(&sf, __cilkrts_get_tls_worker()->l->frame_ff->future_fiber, __cilkrts_get_tls_worker()->l->frame_ff->fiber_self);
        }
        CILK_ASSERT(sf.flags & CILK_FRAME_STOLEN);

    }

    // TODO: Rework it so we don't do this on futures
    if (sf.flags & CILK_FRAME_UNSYNCHED) {
        if (!CILK_SETJMP(sf.ctx)) {
            __cilkrts_future_sync(&sf);
        }
        //__cilkrts_rethrow(&sf);
        update_pedigree_after_sync(&sf);
    }

    printf("I'm here! wherever here is...\n");
    cilk_fiber_get_data(__cilkrts_get_tls_worker()->l->frame_ff->fiber_self)->resume_sf = NULL;

    __cilkrts_pop_frame(&sf);
    __cilkrts_leave_future_parent_frame(&sf);
    printf("Going to parent sf %p (prev %p)\n", __cilkrts_get_tls_worker()->current_stack_frame, &sf);
}

void helloFuture() {
  int* dummy = (int*) alloca(ZERO);
  printf("helloFuture dummy: %p\n", &dummy);
  for (volatile uint32_t i = 0; i < UINT32_MAX/8; i++) {
    gdummy += i;
  }
 // cilk_future_create(int,test_future3,helloMoreFutures);
  //cilk_future_get(test_future3);
  //delete test_future3;
 // __assert_future_counter(1);
  //printf("Placing value\n");
  test_future->put(42);
  //printf("Leaving helloFuture\n");
}

void helloMoreFutures() {
  int* dummy = (int *) alloca(ZERO);
  for (volatile uint32_t j = 0; j < UINT32_MAX/4; j++) {
    dummy2 += j;
  }
  test_future2->put(84);
}

void thread1() {
  int* dummy = (int *)alloca(ZERO);
  printf("\n\n\n*****In thread1, creating future!*****\n\n\n");
  //cilk_spawn thread2();
  test_future = new cilk::future<int>();
  hello_future_helper_helper();
  //__spawn_future_helper_helper(helloFuture);
  //cilk_sync;
  printf("\n\n\n*****I'm in thread1 again!*****\n\n\n");
  auto result = cilk_future_get(test_future);
  cilk_spawn thread2();
  printf("\n\n\n*****Syncing thread1!*****\n\n\n");
  cilk_sync;
  cilk_spawn thread2();
  printf("\n\n\n*****Syncing thread1 again!*****\n\n\n");
  cilk_sync;
  printf("\n\n\n*****Done with thread1...*****\n\n\n");
  printf("\n\n\n*****Leaving thread1!*****\n\n\n");
}
void thread2() {
  int* dummy = (int *)alloca(ZERO);
  printf("\n\n\n*****thread 2*****\n\n\n");
  //output_lock.lock();
  //output_lock.unlock();
  while (test_future==NULL);
  auto result = cilk_future_get(test_future);
  //output_lock.lock();
  printf("\n\n\n*****Thread  got %d*****\n\n\n", result);
  fflush(stdout);
  //output_lock.unlock();
}

void thread3() {
  int* dummy = (int *)alloca(ZERO);
  //cilk_future_create(int,test_future2,helloMoreFutures);
  //reuse_future(int,test_future2,test_future,helloMoreFutures);
  test_future2 = new (test_future) cilk::future<int>();
  //__spawn_future_helper_helper(helloMoreFutures);
  hello_future_helper_helper();
  printf("\n\n\n*****%d*****\n\n\n", cilk_future_get(test_future2));
}
void thread4() {
  int* dummy = (int *)alloca(ZERO);
  printf("\n\n\n*****thread 4*****\n\n\n");
  //output_lock.lock();
  //output_lock.unlock();
  while (test_future2==NULL);
  auto result = cilk_future_get(test_future2);
  //output_lock.lock();
  printf("\n\n\n*****Thread  got %d*****\n\n\n", result);
  fflush(stdout);
  //output_lock.unlock();
}

//void run(void);

/*extern "C" {
int main(int argc, char * args[]) {
    run();
    return 0;
}
}*/

void is_only_printf_crashing() {
    int* dummy = (int*)alloca(sizeof(int)*10);
    assert(dummy);
    *dummy = 0xf00dface; 
    //printf("\n\n\n*****Hi!*****\n\n\n");
}

void another_thread() {
    int* dummy = (int *)alloca(ZERO);
    printf("\n\n\n*****Hi, this is another thread!*****\n\n\n");
    cilk_spawn thread1();
    printf("\n\n\n*****Spawned thread1 from another_thread*****\n\n\n");
    cilk_sync;
    cilk_spawn thread2();
    printf("\n\n\n*****Spawned thread2 from another_thread*****\n\n\n");
    cilk_sync;
    printf("\n\n\n*****Another thread is finished!*****\n\n\n");
}

void yet_another_thread() {
    int* dummy = (int *)alloca(ZERO);
    printf("\n\n\n*****Hello, this is yet another thread!*****\n\n\n");
    cilk_spawn another_thread();
    printf("\n\n\n*****Spawned another_thread from yet_another_thread*****\n\n\n");
    cilk_sync;
    cilk_spawn thread2();
    printf("\n\n\n*****Spawned thread2 from yet_another_thread*****\n\n\n");
    cilk_sync;
    printf("\n\n\n*****yet_another_thread is finished!*****\n\n\n");
    printf("Fiber ref count: %d\n", cilk_fiber_get_ref_count(__cilkrts_get_tls_worker()->l->frame_ff->fiber_self));
}

int main(int argc, char** argv) {
    int* dummy = (int *)alloca(ZERO);


    //cilk_spawn thread1();
    cilk_spawn another_thread();
    printf("\n\n\n*****Simple Future Phase I Syncing*****\n\n\n");

    //for (int i = 0; i < 1; i++) {
    //    cilk_spawn thread2();
    //}
    assert(__cilkrts_get_tls_worker() != NULL);

    cilk_sync;

    printf("\n\n\n*****Simple Future Phase II Commencing*****\n\n\n");

    cilk_spawn thread3();

    //for (int i = 0; i < 2; i++) {
    //    cilk_spawn thread4();
    //}

    cilk_sync;

    //int* dummy = (int*)alloca(sizeof(int)*10);
    //*dummy = 0xf00dface; 
    delete test_future2;

    printf("\n\n\n*****That's all there is, and there isn't anymore...*****\n\n\n");
    return 0;
}
