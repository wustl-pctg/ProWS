#ifndef _UTIL_H
#define _UTIL_H

#include <string>
#include <cstdint>

// PTYPE
#define PDOT 0
#define PPRE 1
#define PARRAY 2

#ifndef PTYPE
#define PTYPE PARRAY
#endif

/* #if PTYPE == PPRE */
#define PRECOMPUTE_PEDIGREES 1
/* #endif */

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>


namespace cilkrr {

#if PTYPE == PARRAY
	typedef struct pedigree_s {
		size_t length;
		uint64_t *array;

		bool operator==(const struct pedigree_s &other) const
		{
			if (this->length != other.length) return false;
			for (int i = 0; i < other.length; ++i) {
				if (this->array[i] != other.array[i])
					return false;
			}
			return true;
		}
	} pedigree_t;

#else // Dot product or precomputed
	typedef uint64_t pedigree_t;

  // Need to check for conflicts
	typedef struct full_pedigree_s {
		size_t length;
		uint64_t *array;

    bool operator==(const struct full_pedigree_s &other) const
		{
			if (this->length != other.length) return false;
			for (int i = 0; i < other.length; ++i) {
				if (this->array[i] != other.array[i])
					return false;
			}
			return true;
		}

	} full_pedigree_t;
	full_pedigree_t get_full_pedigree();
#endif

	pedigree_t get_pedigree();

	enum mode {
		NONE = 0,
		RECORD,
		REPLAY,
	};

	enum mode cilkrr_mode();
}

#endif // ifndef _UTIL_H
