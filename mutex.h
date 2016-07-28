#include <pthread.h>
#include "cilkrr.h"

namespace cilkrr {

	class mutex {
	private:
    pthread_spinlock_t m_lock;

#ifdef DEBUG_ACQUIRE
		__cilkrts_worker *m_owner = nullptr; // just for debugging
    acquire_info volatile *m_active = nullptr; // just for debugging??
#endif

		// Record/Replay fields
		uint64_t m_id; /// index into global container of mutexes
		acquire_container* m_acquires = nullptr; /// container of recorded acquires
    volatile bool m_checking = false; /// For handoff between acquires

		void record_acquire(pedigree_t& p);
		void replay_lock(acquire_info *a);
		void replay_unlock();

		inline void acquire();
		inline void release();
		
	public:
		mutex();
		mutex(uint64_t index);
		~mutex();
    
		void lock();
		void unlock();
		bool try_lock();

	};

}
