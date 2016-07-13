#include <cstdio>
#include <cassert>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "mutex.h"
#include "acquire.h"

cilkrr::mutex g_mutex;

int main(int argc, char *argv[])
{
	int niter = (argc > 1) ? atoi(argv[1]) : 1000;
	int nloops = 10;
	int count = 0;

	for (int i = 0; i < nloops; ++i) {
#pragma cilk grainsize = 1
	cilk_for(int j = 0; j < niter; ++j) {
		g_mutex.lock();
		if (i % 2 == 0) count++;
		else count--;
		g_mutex.unlock();
	}
	//fprintf(stderr, "After loop %i: %s\n", i, cilkrr::get_pedigree_str().c_str());
	//__cilkrts_verify_synced();
	}

	assert(count == 0);
	return 0;
}
