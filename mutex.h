#include <mutex>
#include <list>
#include <iostream>
#include <vector>
#include <string>

#include "cilkrr.h"

namespace cilkrr {

	class mutex {
	private:
		std::mutex m_mutex;
		__cilkrts_worker *m_owner = nullptr;

		// Info for both recording and replaying
		uint64_t m_id; // index into global container of cilkrr_mutexes
		acquire_container* m_acquires = nullptr;

		void record_acquire(pedigree_t& p);
		void replay_lock(acquire_info *a);
		/* bool replay_try_lock(pedigree_t& p); */
		void replay_unlock();

		inline void acquire();
		inline void release();

		void suspend(acquire_info* a);
		
	public:
		mutex();
		mutex(uint64_t index);
		~mutex();
		void lock();
		void unlock();

		// Must be called deterministically! No determinacy races!
		// i.e. races only within critical sections!
		/* bool try_lock(); */

	};

}
