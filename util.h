#ifndef _UTIL_H
#define _UTIL_H

#include <string>
#include <cstdint>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

// PTYPE
#define PNONE 0
#define PWALK 1
#define PARRAY 2
#define PDOT 3
#define PPRE 4

#ifndef PTYPE
#define PTYPE PNONE
#endif

// IMETHOD
#define INONE 0
#define ILL 1
#define ICHUNKLL 2
#define IHASH 3
#define IMYHASH 4
#define IMIXED 5

#ifndef IMETHOD
#define IMETHOD INONE
#endif

// Also CONFLICT_CHECK, USE_STL

#ifndef RESERVE_SIZE
#define RESERVE_SIZE 4096
#endif

#if IMETHOD == IHASH
#if !(PTYPE == PDOT || PTYPE == PARRAY)
#error "Can only use hash with compressed pedigrees."
#endif
#endif

// Disabled for benchmarking
/* #if PTYPE == PDOT || PTYPE == PARRAY */
/* #if !(IMETHOD == IHASH || IMETHOD == IHASHCONFLICT) */
/* #error "Compressed pedigrees require using hash" */
/* #endif */
/* #endif */

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

#else
	typedef uint64_t pedigree_t;
		
		// For use with STL unordred_multiset
		struct compressed_hasher {
			size_t operator()(const pedigree_t& k) const { return k; }
		};
#endif
#ifdef CONFLICT_CHECK
	typedef struct {
		size_t length;
		uint64_t *array;
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
