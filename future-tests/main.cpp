#include <iostream>
#include <cilk/cilk.h>
#include <unistd.h>

#include "../src/future.h" 

//cilk::future<int>* test_future = NULL;

cilk::future<int>* test_future = NULL;

int helloFuture() {
  sleep(2);
  std::cout << "Returning value!" << std::endl;
  return 42;
}

void thread1() {
  cilk_future_create(int,test_future,helloFuture);
  std::cout << "Continuing" << std::endl;
  std::cout << cilk_future_get(test_future) << std::endl;
}

int main() {
  cilk_spawn thread1();

  cilk_sync;
  delete test_future;
  return 0;
}
