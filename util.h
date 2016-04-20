#ifndef _UTIL_H
#define _UTIL_H

#include <string>
#include <cstdint>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#define PNONE 0
#define PSTRING 1
#define PARRAY 2
#define PINT 3

#define INONE 0
#define IHASH 1
#define ILL 2
#define IHASHCONFLICT 3

#define PMNONE 0
#define PMWALK 1
#define PMGET 2
#define PMDOT 3
#define PMPRE 4

#ifndef PTYPE
#define PTYPE PSTRING
#endif
#ifndef IMETHOD
#define IMETHOD IHASH
#endif
#ifndef PMETHOD
#define PMETHOD PMGET
#endif

namespace cilkrr {

#if PTYPE == PARRAY
	typedef struct pedigree_s{
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

	struct array_hasher {
		size_t operator()(const pedigree_t& k) const
		{
			size_t h;
			for (int i = 0; i < k.length; ++i)
				h = (h + (324723947 + k.array[i])) ^ 93485734985;
			//				h ^= std::hash<uint64_t>()(k.array[i]);

			return h;
		}
	};
#elif PTYPE == PNONE || PTYPE == PSTRING
	typedef std::string pedigree_t;
#elif PTYPE == PINT
	typedef uint64_t pedigree_t;

#  if IMETHOD == IHASHCONFLICT
	typedef struct {
		size_t length;
		uint64_t *array;
	} full_pedigree_t;
	full_pedigree_t get_full_pedigree();
#  endif
#else
#error "Invalid PTYPE"
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
