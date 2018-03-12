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
  printf("Returning value!\n");
  fflush(stdout);
  return 42;
}

int helloMoreFutures() {
  for (volatile uint32_t j = 0; j < UINT32_MAX/4; j++) {
    dummy2 += j;
  }
  printf("Returning value!\n");
  fflush(stdout);
  return 84;
}

void thread1() {
  printf("Creating future\n");
  fflush(stdout);
  cilk_future_create(int,test_future,helloFuture);
  printf("Continuing");
  fflush(stdout);
  auto result = cilk_future_get(test_future);
  printf("%d\n", result);
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

void run(void);

extern "C" {
int main(int argc, char * args[]) {
    run();
    return 0;
}
}

void run() {
    cilk_spawn thread1();

    for (int i = 0; i < 4; i++) {
        cilk_spawn thread2();
    }

    cilk_sync;
    __assert_future_counter(1);

    printf("Moving right along...\n");
    fflush(stdout);
    cilk_spawn thread3();
    for (int i = 0; i < 8; i++) {
        cilk_spawn thread4();
    }

    cilk_sync;
    __assert_future_counter(2);

    delete test_future2;
}
