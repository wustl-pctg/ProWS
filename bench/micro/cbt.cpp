// Similar to the CBT synthetic benchmark in the DOTMIX paper.
// Actually, no. This benchmark has changed and is now misnamed.

#include <cstdio>
#include <cassert>
#include <iostream>
#include <chrono> // timing
#include <unistd.h> // sleep()

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "mutex.h"

// 2 ^ 23
#define NUM_LOCK_ACQUIRES (1 << 23)
int g_count = 0;
double g_leaf_work = 0.0;
cilkrr::mutex g_mutex;

void tree_search(int index, int level)
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

int main(int argc, char *argv[])
{
	int depth = (argc > 1) ? std::atoi(argv[1]) : 5;
  int iter = (argc > 2) ? std::atoi(argv[2]) : (NUM_LOCK_ACQUIRES >> depth);

  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iter; ++i)
    tree_search(0, depth);
  auto end = std::chrono::high_resolution_clock::now();
  int expected = iter * (1 << depth);
  assert(g_count == expected);
  std::cout << "time: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
            << std::endl;
	return 0;
}
