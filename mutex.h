#include <mutex>
#include <list>
#include <iostream>
#include <vector>

#include "cilkrr.h"

namespace cilkrr {

	class mutex {
	private:
		std::mutex m_mutex;
		__cilkrts_worker *m_owner;

		// Info for both recording and replaying
		uint64_t m_id; // index into global container of cilkrr_mutexes
		std::list<acquire_info> *m_acquires;

		void record_acquire();
		void replay_lock();
		bool replay_try_lock();
		void replay_unlock();

		inline void acquire();
		inline void release();
		
	public:
		mutex();
		~mutex();
		void lock();
		bool try_lock();
		void unlock();
	};

}
