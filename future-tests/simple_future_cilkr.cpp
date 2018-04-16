#include <iostream>
#include <stdio.h>
#include "cilk/cilk.h"
#include "../cilkplus-rts/include/internal/abi.h"
#include "../src/future.h"

#include <cstdint>

extern unsigned long ZERO;

extern "C" {
//extern void __assert_future_counter(int count);
//extern void __print_curr_stack(char*);
//extern void __assert_not_on_scheduling_fiber(void);
}

void __assert_future_counter(int count) {}
void __print_curr_stack(char*){}
void __assert_not_on_scheduling_fiber(void){}

cilk::future<int>* test_future = NULL;
cilk::future<int>* test_future2 = NULL;
cilk::future<int>* test_future3 = NULL;
volatile int gdummy = 0;
volatile int dummy2 = 0;
//porr::spinlock output_lock;

int helloFuture(void);
int helloMoreFutures(void);
void thread1(void);
void thread2(void);
void thread3(void);
void thread4(void);

int helloFuture() {
  int* dummy = (int*) alloca(ZERO);
  for (volatile uint32_t i = 0; i < UINT32_MAX/8; i++) {
    gdummy += i;
  }
  __assert_not_on_scheduling_fiber();
 // cilk_future_create(int,test_future3,helloMoreFutures);
  //cilk_future_get(test_future3);
  //delete test_future3;
 // __assert_future_counter(1);
  __print_curr_stack("\nhello future");
  return 42;
}

int helloMoreFutures() {
  int* dummy = (int *) alloca(ZERO);
  for (volatile uint32_t j = 0; j < UINT32_MAX/4; j++) {
    dummy2 += j;
  }
  __assert_not_on_scheduling_fiber();
  __print_curr_stack("\nhello more futures");
  return 84;
}

void thread1() {
  int* dummy = (int *)alloca(ZERO);
  __print_curr_stack("\nthread1 p1");
  __assert_not_on_scheduling_fiber();
  printf("\n\n\n*****In thread1, creating future!\n");
  //cilk_spawn thread2();
  cilk_future_create(int,test_future,helloFuture);
  //cilk_sync;
  printf("\n\n\n*****I'm in thread1 again!\n");
  __assert_not_on_scheduling_fiber();
  __print_curr_stack("\nthread1 p2");
  auto result = cilk_future_get(test_future);
  cilk_spawn thread2();
  __assert_not_on_scheduling_fiber();
  __print_curr_stack("\nthread1 p3");
  printf("\n\n\n*****Syncing thread1!\n");
  cilk_sync;
  cilk_spawn thread2();
  printf("\n\n\n*****Syncing thread1 again!\n");
  cilk_sync;
  printf("\n\n\n*****Done with thread1...\n");
  printf("\n\n\n*****Leaving thread1!\n");
}
void thread2() {
  int* dummy = (int *)alloca(ZERO);
  printf("\n\n\n*****thread 2\n");
  //output_lock.lock();
  //output_lock.unlock();
  while (test_future==NULL);
  auto result = cilk_future_get(test_future);
  //output_lock.lock();
  printf("\n\n\n*****Thread  got %d\n", result);
  fflush(stdout);
  //output_lock.unlock();
}

void thread3() {
  int* dummy = (int *)alloca(ZERO);
  __assert_not_on_scheduling_fiber();
  __print_curr_stack("\nthread3 p1");
  //cilk_future_create(int,test_future2,helloMoreFutures);
  reuse_future(int,test_future2,test_future,helloMoreFutures);
  __assert_not_on_scheduling_fiber();
  __print_curr_stack("\nthread3 p2");
  printf("\n\n\n*****%d\n", cilk_future_get(test_future2));
  __assert_not_on_scheduling_fiber();
  __print_curr_stack("\nthread3 p3");
}
void thread4() {
  int* dummy = (int *)alloca(ZERO);
  printf("\n\n\n*****thread 4\n");
  //output_lock.lock();
  //output_lock.unlock();
  while (test_future2==NULL);
  auto result = cilk_future_get(test_future2);
  //output_lock.lock();
  printf("\n\n\n*****Thread  got %d\n", result);
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
    //printf("\n\n\n*****Hi!\n");
}

void another_thread() {
    int* dummy = (int *)alloca(ZERO);
    printf("\n\n\n*****Hi, this is another thread!\n");
    cilk_spawn thread1();
    printf("\n\n\n*****Spawned thread1 from another_thread\n");
    cilk_sync;
    cilk_spawn thread2();
    printf("\n\n\n*****Spawned thread2 from another_thread\n");
    cilk_sync;
    printf("\n\n\n*****Another thread is finished!\n");
}

void yet_another_thread() {
    int* dummy = (int *)alloca(ZERO);
    printf("\n\n\n*****Hello, this is yet another thread!\n");
    cilk_spawn another_thread();
    printf("\n\n\n*****Spawned another_thread from yet_another_thread\n");
    cilk_sync;
    cilk_spawn thread2();
    printf("\n\n\n*****Spawned thread2 from yet_another_thread\n");
    cilk_sync;
    printf("\n\n\n*****yet_another_thread is finished!\n");
}

int main(int argc, char** argv) {
    int* dummy = (int *)alloca(ZERO);
    //__print_curr_stack("initial");


    //cilk_spawn thread1();
    cilk_spawn yet_another_thread();
    printf("\n\n\n*****\n\nSimple Future Phase I Syncing\n\n");

    //for (int i = 0; i < 1; i++) {
    //    cilk_spawn thread2();
    //}
    //__print_curr_stack("pre-sync");

    cilk_sync;

    int* dummy1 = (int*)alloca(sizeof(int)*10);
    *dummy1 = 0xf00dface; 

    printf("\n\n\n*****\n\nSimple Future Phase II Commencing\n\n");
    //__print_curr_stack("\nphase II");

    cilk_spawn thread3();

    //for (int i = 0; i < 2; i++) {
    //    cilk_spawn thread4();
    //}

    //__print_curr_stack("\nphase II pre-sync");

    cilk_sync;

    //__print_curr_stack("end");

    //int* dummy = (int*)alloca(sizeof(int)*10);
    //*dummy = 0xf00dface; 
    delete test_future2;

    printf("\n\n\n*****That's all there is, and there isn't anymore...\n\n\n");
    return 0;
}
