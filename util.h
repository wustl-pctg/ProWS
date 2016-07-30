#ifndef _UTIL_H
#define _UTIL_H

#include <string>
#include <cstdint>

// PTYPE
#define PDOT 0
#define PPRE 1
#define PARRAY 2

#ifndef PTYPE
#define PTYPE PPRE
#endif

/* #if PTYPE == PPRE */
#define PRECOMPUTE_PEDIGREES 1
/* #endif */

#ifdef STATS
#include "papi.h"
#define NUM_PAPI_EVENTS 3
#define NUM_GLOBAL_STATS 0
#define NUM_LOCAL_STATS 1
#endif

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>


namespace cilkrr {

#ifdef STATS
  enum g_stat_names {};
  extern uint64_t g_stats[NUM_GLOBAL_STATS];
  extern const char* g_stat_strings[NUM_GLOBAL_STATS];

  enum t_stat_names {
    LSTAT_SUS = 0,
  };
  extern uint64_t* t_stats[NUM_LOCAL_STATS];
  extern const char* t_stat_strings[NUM_LOCAL_STATS];

#define GSTAT_ADD(e,i) (g_stats[e] += i)
#define LSTAT_ADD(e,i) (t_stats[e][__cilkrts_get_worker_number()] += i)
#else
#define GSTAT_ADD(e,i)
#define LSTAT_ADD(e,i)
#endif
  
#define GSTAT_INC(e) GSTAT_ADD(e,1)
#define LSTAT_INC(e) LSTAT_ADD(e,1)


	typedef uint64_t pedigree_t;

  // Need to check for conflicts
	typedef struct full_pedigree_s {
		size_t length;
		uint64_t *array;

    bool operator==(const struct full_pedigree_s &other) const
		{
			if (this->length != other.length) return false;
			for (size_t i = 0; i < other.length; ++i) {
				if (this->array[i] != other.array[i])
					return false;
			}
			return true;
		}

	} full_pedigree_t;
  
	full_pedigree_t get_full_pedigree();
	pedigree_t get_pedigree();

	enum mode {
		NONE = 0,
		RECORD,
		REPLAY,
	};

	enum mode cilkrr_mode();
}

#endif // ifndef _UTIL_H
