#include <iostream>
#include <stdio.h>
#include "cilk/cilk.h"
#include "internal/abi.h"
//#include "../cilkrtssuspend/runtime/local_state.h"
//#include "../cilkrtssuspend/runtime/full_frame.h"
#include "../src/future.h"
#include <cstring>
#include <unistd.h>

#include <cstdint>

extern unsigned long ZERO;


//extern CILK_ABI_VOID __cilkrts_future_sync(__cilkrts_stack_frame *sf);
//extern CILK_ABI_VOID __cilkrts_leave_future_frame(__cilkrts_stack_frame *sf);
//extern CILK_ABI_VOID __cilkrts_leave_future_parent_frame(__cilkrts_stack_frame *sf);

cilk::future<int> *volatile test_future = NULL;
cilk::future<int> *volatile test_future2 = NULL;
cilk::future<int> *volatile test_future3 = NULL;
volatile int gdummy = 0;
volatile int dummy2 = 0;
//porr::spinlock output_lock;

int helloFuture(cilk::future<int>*);
int helloMoreFutures(void);
void thread1(void);
void thread2(void);
void thread3(void);
void thread4(void);

int helloFuture(cilk::future<int>* other) {
  int* dummy = (int*) alloca(ZERO);
  //printf("helloFuture dummy: %p\n", &dummy);
  //for (volatile uint32_t i = 0; i < UINT32_MAX/8; i++) {
  //  gdummy += i;
  //}
  //sleep(6);
 // cilk_future_create(int,test_future3,helloMoreFutures);
  //cilk_future_get(test_future3);
  //delete test_future3;
 // __assert_future_counter(1);
  //printf("Placing value\n");
  //printf("Leaving helloFuture\n");
  return other->get() / 2;
}


int helloMoreFutures() {
  sleep(5);
  int* dummy = (int *) alloca(ZERO);
  for (volatile uint32_t j = 0; j < UINT32_MAX/4; j++) {
    dummy2 += j;
  }
  return 84;
}

static volatile int nesting = 0;
int helloAnotherFuture() {
    cilk::future<int>* buaHaHa;
    if (nesting < 10) {
        nesting++;
        cilk_future_create(int,buaHaHa,helloAnotherFuture);
    } else {
        cilk_future_create(int,buaHaHa,helloMoreFutures);
    }

    int ret = cilk_future_get(buaHaHa);
    printf("Bua Ha Ha! %d\n", ret);
    delete buaHaHa;
    return ret;
}

void thread1() {
  int* dummy = (int *)alloca(ZERO);
  printf("\n\n\n*****In thread1, creating future!*****\n\n\n");
  //cilk_spawn thread2();
  //sleep(5);
  cilk_future_create(int,test_future2, helloAnotherFuture);
  //cilk_future_create(int,test_future2, helloMoreFutures);
  cilk_future_create(int,test_future, helloFuture, test_future2);
  cilk_future_create(int,test_future3, helloFuture, test_future);
  //test_future = new cilk::future<int>();
  //hello_future_helper_helper();
  //__spawn_future_helper_helper(helloFuture);
  //cilk_sync;
  printf("\n\n\n*****I'm in thread1 again!*****\n\n\n");
  sleep(5);
  //auto result = cilk_future_get(test_future);
  auto result = cilk_future_get(test_future3);
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
  while (test_future==NULL) {
     //sleep(1);
     //printf("test_future is null\n");
  };
  auto result = cilk_future_get(test_future);
  //output_lock.lock();
  printf("\n\n\n*****Thread  got %d*****\n\n\n", result);
  fflush(stdout);
  //output_lock.unlock();
}

void debugThis() {
    int *dummy = (int *)alloca(ZERO);
    printf("\n\n\n*****debugThis*****\n\n\n");
    while (test_future == NULL) {
        //sleep(1);
        //printf("test_future is null\n");
    }

    auto result = cilk_future_get(test_future);
    printf("debugThis got future result!\n");
    printf("\n\n\n*****debugThis  got %d*****\n\n\n", result);
    fflush(stdout);
}

void thread3() {
  int* dummy = (int *)alloca(ZERO);
  //cilk_future_create(int,test_future2,helloMoreFutures);
  reuse_future(int,test_future2,test_future,helloMoreFutures);
  //test_future2 = new (test_future) cilk::future<int>();
  //__spawn_future_helper_helper(helloMoreFutures);
  //hello_future_helper_helper();
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
}

int my_test_of_waiting_for_futures() {
    sleep(7);
    printf("\n\nJust kidding, there was more! :-D\n\n");
    return 9001;
}

void thread5() {
    cilk_future_create(int, test_future3, my_test_of_waiting_for_futures);
}

int main(int argc, char** argv) {
    int* dummy = (int *)alloca(ZERO);


    //cilk_spawn thread1();
    cilk_spawn another_thread();
    printf("\n\n\n*****Simple Future Phase I Syncing*****\n\n\n");

    // TODO: Right now there is only single touch; these would cause us to hang.
    for (int i = 0; i < 8; i++) {
        cilk_spawn thread2();
    }
    assert(__cilkrts_get_tls_worker() != NULL);

    cilk_sync;
    __cilkrts_worker* w = __cilkrts_get_tls_worker();
    assert(w);
    printf("Entry Worker id: %d\n", w->self);
    printf("Synced!\n");

    printf("\n\n\n*****Simple Future Phase II Commencing*****\n\n\n");

    cilk_spawn thread3();

    // TODO: Right now there is only single touch; these would cause us to hang.
    //for (int i = 0; i < 2; i++) {
    //    cilk_spawn thread4();
    //}

    cilk_sync;
    printf("Synced!\n");

    cilk_spawn thread5();
    //int* dummy = (int*)alloca(sizeof(int)*10);
    //*dummy = 0xf00dface; 
    delete test_future2;

    cilk_sync;
    printf("\n\n\n*****That's all there is, and there isn't anymore...*****\n\n\n");
    w = __cilkrts_get_tls_worker();
    printf("Exit worker id: %d\n", w->self);
    fflush(stdout);
    return 0;
}
