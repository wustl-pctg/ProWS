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

#ifdef DEBUG_ACQUIRE
		__cilkrts_worker *m_owner = nullptr; // just for debugging
    acquire_info volatile *m_active = nullptr; // just for debugging??
#endif

		// Record/Replay fields
		uint64_t m_id; /// index into global container of mutexes
		acquire_container* m_acquires = nullptr; /// container of recorded acquires
    bool m_checking = false; /// For handoff between acquires

		void record_acquire(pedigree_t& p);
		void replay_lock(acquire_info *a);
		/* bool replay_try_lock(pedigree_t& p); */
		void replay_unlock();

		inline void acquire();
		inline void release();
		
	public:
		mutex();
		mutex(uint64_t index);
		~mutex();
		void lock();
		void unlock();

		// Must be called deterministically! No determinacy races!
		// i.e. races only within critical sections!
		bool try_lock();

	};

}
