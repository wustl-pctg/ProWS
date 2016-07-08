#ifndef _CILKRR_H
#define _CILKRR_H

#include <string>
#include <vector>
#include <atomic>
#include <limits>

#include "util.h"
#include "acquire.h"

namespace cilkrr {
  #if PTYPE != PPRE
	// 2^64 - 59
	//static size_t big_prime = std::numeric_limits<size_t>::max() - 58;
	static size_t big_prime = (1L << 63) - 58;
#endif

	class state {
	private:
		std::vector< acquire_container * > m_all_acquires;
		std::string m_filename;
		size_t m_size;

		// To make sure we are really done.
		std::atomic<size_t> m_active_size;

		static constexpr size_t m_default_capacity = 32;

	public:
#if PTYPE != PPRE
		uint64_t randvec[256];
#endif
		enum mode m_mode;

		state();
		~state();
		void resize(size_t n);

		// Assumes resize has already been called to the correct size.
		size_t register_mutex(size_t local_id);

		// Does not assume resize has been called, but must be called sequentially.
		size_t register_mutex(); 

		acquire_container* get_acquires(size_t index);
		size_t unregister_mutex(size_t index);
	};
	extern state *g_rr_state;
	inline enum mode get_mode() { return g_rr_state->m_mode; }
	void reserve_locks(size_t n);
}

#endif // ifndef _CILKRR_H
