#include <iostream>
#include <cilk/cilk.h>
#include "../src/future.h"

#include <cstdint>

cilk::future<int>* test_future = NULL;
cilk::future<int>* test_future2 = NULL;
volatile int dummy = 0;
volatile int dummy2 = 0;
//porr::spinlock output_lock;

int helloFuture(void);
int helloMoreFutures(void);

int helloFuture() {
  for (volatile uint32_t i = 0; i < UINT32_MAX/8; i++) {
    dummy += i;
  }
  printf("Returning value!\n");
  fflush(stdout);
  return 42;
}

int helloMoreFutures() {
  for (volatile uint32_t j = 0; j < UINT32_MAX/8; j++) {
    dummy2 += j;
  }
  printf("Returning value!\n");
  fflush(stdout);
  return 84;
}

#define SPAWNING_FUNC_PREAMBLE

#define kcilk_spawn(func)   \
    cilk_spawn func();

#define kcilk_sync  \
    cilk_sync;

#define SPAWNING_FUNC_EPILOGUE 

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
    SPAWNING_FUNC_PREAMBLE;

    kcilk_spawn(thread1);

    for (int i = 0; i < 64; i++) {
        kcilk_spawn(thread2);
    }

    kcilk_sync;

    printf("Moving right along...\n");
    fflush(stdout);
    kcilk_spawn(thread3);
    for (int i = 0; i < 64; i++) {
        kcilk_spawn(thread4);
    }

    kcilk_sync;
    delete test_future2;
    SPAWNING_FUNC_EPILOGUE;
}
