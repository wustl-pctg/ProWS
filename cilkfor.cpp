#include <cstdio>
#include <cassert>
#include <chrono>
#include <unistd.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "mutex.h"
#include "acquire.h"

#include "papi.h"

#define NLOOPS 10
cilkrr::mutex g_mutex;

int main(int argc, char *argv[])
{
  int niter = 1000;
  int nacquires = 1;
  int sleep_ms = 0;
  int work_ms = 0;

  // performance debugging
  const size_t num_papi_events = 3;
  int events[num_papi_events] = {PAPI_L1_DCM, PAPI_L2_TCM, PAPI_L3_TCM};
  char* papi_strings[num_papi_events] = {"L1 data misses",
                                        "L2 misses",
                                         "L3 misses"};
  long long event_vals[3];

  assert(PAPI_num_counters() >= 3);
  
  switch (argc) {
  case 5:
    work_ms = atoi(argv[4]);
  case 4:
    sleep_ms = atoi(argv[3]);
  case 3:
    nacquires = atoi(argv[2]);
  case 2:
    niter = atoi(argv[1]);
  case 1:
    break;
  default:
    fprintf(stderr, "Invalid # args\n");
    std::abort();
  }
  
	int count = 0;
  double t1 = (double)sleep_ms / 1000.0;
  double t2 = (double)work_ms / 1000.0;

  int ret = PAPI_start_counters(events, 3);
  assert(ret == PAPI_OK);
	auto start = std::chrono::high_resolution_clock::now();
  
	for (int i = 0; i < NLOOPS; ++i) {
    
#pragma cilk grainsize = 1
	cilk_for(int j = 0; j < niter; ++j) {

    if (t1 > 0.0) sleep(t1);
    
    for (int k = 0; k < nacquires; ++k) {
      g_mutex.lock();

      if (t2 > 0.0) sleep(t2);
      if (i % 2 == 0) count++;
      else count--;
      
      g_mutex.unlock();
    }
	}
	//fprintf(stderr, "After loop %i: %s\n", i, cilkrr::get_pedigree_str().c_str());
	//__cilkrts_verify_synced();
	}
	assert(count == 0);
	auto end = std::chrono::high_resolution_clock::now();
  ret = PAPI_read_counters(event_vals, 3);
  assert(ret == PAPI_OK);
		
    std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << std::endl;
    for (int i = 0; i < num_papi_events; ++i)
      printf("%s: %lld\n", papi_strings[i], event_vals[i]);
	return 0;
}
