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
double g_unlock_work;
cilkrr::mutex g_mutex;

void tree_search(size_t index, size_t level)
{
	if (level == 0) {
		g_mutex.lock();
		g_count++;
    if (g_leaf_work > 0.0) sleep(g_leaf_work);
		g_mutex.unlock();
    if (g_unlock_work > 0.0) sleep(g_unlock_work);
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
  // defaults
  size_t num_acquires = (1L << 23);
  size_t num_leaves = 32;
  g_leaf_work = g_unlock_work = 0.0;
  
  if (argc > 1 && argv[1][0] != '_')
    num_acquires = 1L << std::strtoul(argv[1],nullptr, 0);
  if (argc > 2 && argv[2][0] != '_') 
    num_leaves = std::strtoul(argv[2], nullptr, 0);
  if (argc > 3 && argv[3][0] != '_')
    g_leaf_work = std::strtod(argv[3], nullptr)/1000.0;
  if (argc > 4 && argv[4][0] != '_')
    g_unlock_work = std::strtod(argv[4], nullptr)/1000.0;

  std::cout << num_acquires << std::endl;
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
  std::cout << "CilkRR time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>
    (end - start).count() << std::endl;
	return 0;
}
