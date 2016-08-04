// Similar to the CBT synthetic benchmark in the DOTMIX paper.

#include <cstdio>
#include <cassert>
#include <iostream>
#include <chrono> // timing
#include <unistd.h> // sleep()

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "mutex.h"

size_t g_count = 0;
double g_leaf_work;
cilkrr::mutex g_mutex;

void tree_search(size_t index, size_t level)
{
	if (level == 0) {
		g_mutex.lock();
		g_count++;
    if (g_leaf_work > 0.0) sleep(g_leaf_work);
		g_mutex.unlock();
		return;
	}

	cilk_spawn tree_search(index, level-1);
	tree_search(index+1, level-1);
	cilk_sync;
	return;
}

// ./cbt <# acquires> <depth> [leaf work time (ms)]
int main(int argc, char *argv[])
{
  size_t num_acquires = (argc > 1) ? std::strtoul(argv[1],nullptr, 0) : (1 << 23);
  size_t num_leaves = (argc > 2) ? std::strtoul(argv[2], nullptr, 0) : 32;
  g_leaf_work = (argc > 3) ? std::strtod(argv[3], nullptr) : 0.0;

  // These two should always be a power of 2.
  assert(__builtin_popcount(num_acquires) == 1);
  assert(__builtin_popcount(num_leaves) == 1);

  size_t depth = __builtin_ctz(num_leaves);
  size_t iter = (num_acquires >> depth);
  
  auto start = std::chrono::high_resolution_clock::now();
  
  for (size_t i = 0; i < iter; ++i)
    tree_search(0, depth);
  
  auto end = std::chrono::high_resolution_clock::now();
  
  assert(g_count == num_acquires);
  std::cout << "time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
            << std::endl;
	return 0;
}
