#include <iostream>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "mutex.h"
#include "syncstream.h"

cilkrr::mutex g_mutex;

int main(int argc, char *argv[])
{
	int n = (argc > 1) ? atoi(argv[1]) : 5;
	fprintf(stderr, "Begin: %s\n", cilkrr::get_pedigree().c_str());
	cilk_for(int i = 0; i < n; ++i) {

		__cilkrts_bump_loop_rank();

		fprintf(stderr, "i(%i): %s\n", i,
						cilkrr::get_pedigree().c_str());

		// g_mutex.lock();
		// g_mutex.unlock();

	}
	cilkrr::sout << "End: " << cilkrr::get_pedigree() << cilkrr::endl;
	return 0;
}
