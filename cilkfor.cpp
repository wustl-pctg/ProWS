#include <cstdio>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "mutex.h"

cilkrr::mutex g_mutex;

int main(int argc, char *argv[])
{
	int n = (argc > 1) ? atoi(argv[1]) : 5;
	int count = 0;
	//fprintf(stderr, "Begin: %s\n", cilkrr::get_pedigree().c_str());

#pragma cilk grainsize=1
	cilk_for(int i = 0; i < n; ++i) {

		// fprintf(stderr, "i(%i): %zu\n", i,
		// 				cilkrr::get_pedigree());
		// fprintf(stderr, "i(%i): %s\n", i,
		// 				cilkrr::get_full_pedigree().c_str());

		g_mutex.lock();
		count++;
		g_mutex.unlock();
	}
	//fprintf(stderr, "End: %s\n", cilkrr::get_pedigree().c_str());
	return 0;
}
