#ifndef _UTIL_H
#define _UTIL_H

#include <string>

#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

namespace cilkrr {
	typedef std::string pedigree_t;
	pedigree_t get_pedigree();

	enum mode {
		NONE = 0,
		RECORD,
		REPLAY,
	};

	enum mode cilkrr_mode();
}

#endif // ifndef _UTIL_H
