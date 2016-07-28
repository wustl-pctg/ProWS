// Similar to the CBT synthetic benchmark in the DOTMIX paper.

#include <cstdio>
#include <cassert>
#include <iostream>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "mutex.h"
int count = 0;
cilkrr::mutex g_mutex;

void tree(int level)
{
	if (level == 0) {
		g_mutex.lock();
		count++;
		g_mutex.unlock();
		return;
	}

	cilk_spawn tree(level/2);
	tree(level/2);
	cilk_sync;
	return;
}

int main(int argc, char *argv[])
{
	int n = (argc > 1) ? std::atoi(argv[1]) : 1024;
	int k = (argc > 2) ? std::atoi(argv[2]) : 256;
	assert(n > k);
	assert(n % k == 0);
	assert(k % 2 == 0);

	for (int i = 0; i < n/k; ++i)
		tree(k);

	return 0;
}
