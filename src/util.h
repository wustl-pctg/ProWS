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

//#define USE_LOCKSTAT 1
#ifdef USE_LOCKSTAT
extern "C" {
#include "lockstat.h"
}
extern struct spinlock_stat *the_sls_list;
extern pthread_spinlock_t the_sls_lock;
extern int the_sls_setup;

typedef struct spinlock_stat base_lock_t;
#define base_lock_init(l, id) sls_init(l, id)
#define base_lock_destroy(l) sls_destroy(l)
#define base_lock(l) sls_lock(l)
#define base_trylock(l) sls_trylock(l)
#define base_unlock(l) sls_unlock(l)

#else
typedef pthread_spinlock_t base_lock_t;
#define base_lock_init(l, id) pthread_spin_init(l, PTHREAD_PROCESS_PRIVATE)
#define base_lock_destroy(l) pthread_spin_destroy(l)
#define base_lock(l) pthread_spin_lock(l)
#define base_trylock(l) pthread_spin_trylock(l)
#define base_unlock(l) pthread_spin_unlock(l)

#endif



/* #if PTYPE == PPRE */
#define PRECOMPUTE_PEDIGREES 1
/* #endif */

//#define ACQ_PTR 1

#if STATS > 0
#include "papi.h"
#define NUM_PAPI_EVENTS 3
#define NUM_GLOBAL_STATS 0
#define NUM_LOCAL_STATS 1
#endif

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

namespace porr {

#if STATS > 0
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
  typedef uint64_t rank_t;

  // Need to check for conflicts
	typedef struct full_pedigree_s {
		size_t length;
		rank_t *array;

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

	enum mode porr_mode();
}

#endif // ifndef _UTIL_H
