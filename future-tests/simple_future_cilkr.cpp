#include <iostream>
#include <stdio.h>
#include "cilk/cilk.h"
#include "../src/future.h"

#include <cstdint>

extern "C" {
extern void __assert_future_counter(int count);
}

cilk::future<int>* test_future = NULL;
cilk::future<int>* test_future2 = NULL;
cilk::future<int>* test_future3 = NULL;
volatile int dummy = 0;
volatile int dummy2 = 0;
//porr::spinlock output_lock;

int helloFuture(void);
int helloMoreFutures(void);
void thread1(void);
void thread2(void);
void thread3(void);
void thread4(void);

int helloFuture() {
  for (volatile uint32_t i = 0; i < UINT32_MAX/8; i++) {
    dummy += i;
  }
 // cilk_future_create(int,test_future3,helloMoreFutures);
  //cilk_future_get(test_future3);
  //delete test_future3;
 // __assert_future_counter(1);
  printf("This one is helloFuture!\n");
  printf("Returning value!\n");
  fflush(stdout);
  return 42;
}

int helloMoreFutures() {
  for (volatile uint32_t j = 0; j < UINT32_MAX/4; j++) {
    dummy2 += j;
  }
  printf("This one is helloMoreFutures!\n");
  printf("Returning value!\n");
  fflush(stdout);
  return 84;
}

void thread1() {
  printf("Creating future\n");
  fflush(stdout);
  cilk_future_create(int,test_future,helloFuture);
  printf("Continuing\n");
  fflush(stdout);
  auto result = cilk_future_get(test_future);
  printf("Thread 1 finished: %d\n", result);
  fflush(stdout);
}
void thread2() {
  printf("thread 2\n");
  //output_lock.lock();
  //output_lock.unlock();
  while (test_future==NULL);
  auto result = cilk_future_get(test_future);
  //output_lock.lock();
  printf("Thread  got %d\n", result);
  fflush(stdout);
  //output_lock.unlock();
}

void thread3() {
  printf("Reusing future\n");
  fflush(stdout);
  //cilk_future_create(int,test_future2,helloMoreFutures);
  reuse_future(int,test_future2,test_future,helloMoreFutures);
  //test_future = test_future2;
  printf("Continuing\n");
  fflush(stdout);
  printf("%d\n", cilk_future_get(test_future2));
  fflush(stdout);
}
void thread4() {
  printf("thread 4\n");
  //output_lock.lock();
  //output_lock.unlock();
  while (test_future2==NULL);
  auto result = cilk_future_get(test_future2);
  //output_lock.lock();
  printf("Thread  got %d\n", result);
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
    //printf("Hi!\n");
}

int main(int argc, char** argv) {
    long test;
    asm("\t mov %%rbp,%0" : "=r"(test));
    printf("rbp 1: %lX\n", test); fflush(stdout);
    asm("\t mov %%rsp,%0" : "=r"(test));
    printf("rsp 1: %lX\n", test); fflush(stdout);

    cilk_spawn thread1();

    //for (int i = 0; i < 1; i++) {
    //    cilk_spawn thread2();
    //}

    asm("\t mov %%rbp,%0" : "=r"(test));
    printf("rbp 2: %lX\n", test); fflush(stdout);
    asm("\t mov %%rsp,%0" : "=r"(test));
    printf("rsp 2: %lX\n", test); fflush(stdout);
    cilk_sync;
    //printf("Hello, matey!\n");
    test = 1;
    asm("\t mov %%rbp,%0" : "=r"(test));
    printf("rbp 3: %lX\n", test); fflush(stdout);
    asm("\t mov %%rsp,%0" : "=r"(test));
    printf("rsp 3: %lX\n", test); fflush(stdout);
    //printf("----------------Round 2----------------\n");

    is_only_printf_crashing();
    __assert_future_counter(0);

    //printf("Moving right along...\n");
    //fflush(stdout);
    cilk_spawn thread3();
    for (int i = 0; i < 2; i++) {
        cilk_spawn thread4();
    }

    cilk_sync;
    __assert_future_counter(0);

    int* dummy = (int*)alloca(sizeof(int));
    *dummy = 0xf00dface; 
    delete test_future2;

    printf("That's all there is, and there isn't anymore...\n\n\n");
    return 0;
}
