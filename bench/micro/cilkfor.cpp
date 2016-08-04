#include <iostream>
#include <chrono> // timing

#include <cassert>
#include <unistd.h> // sleep()

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "mutex.h"
#include "acquire.h"

// cilkfor <# acquires> <# locks> [lock work time (ms)] [unlock work time (ms)]
int main(int argc, char *argv[])
{
  size_t num_acquires = (argc > 1) ? std::strtoul(argv[1], nullptr, 0) : 10000;
  size_t num_locks = (argc > 2) ? std::strtoul(argv[2], nullptr, 0) : 1;
  double lock_time = (argc > 3) ? std::strtod(argv[3], nullptr)/1000.0 : 0.0;
  double unlock_time = (argc > 4) ? std::strtod(argv[4], nullptr)/1000.0 : 0.0;

  assert(num_acquires % num_locks == 0);
  size_t count = 0;
  cilkrr::mutex *locks = new cilkrr::mutex[num_locks];

	auto start = std::chrono::high_resolution_clock::now();
#pragma cilk grainsize = 1
	cilk_for (int i = 0; i < (num_acquires / num_locks); ++i) {

    if (unlock_time > 0.0) sleep(unlock_time);
    
    locks[i % num_locks].lock();
    count++;
    if (lock_time > 0.0) sleep(lock_time);
    locks[i % num_locks].unlock();
  }

	auto end = std::chrono::high_resolution_clock::now();
  assert(count == num_acquires);
  std::cout << "CilkRR time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>
    (end - start).count() << std::endl;
  
  delete[] locks;
	return 0;
  }
