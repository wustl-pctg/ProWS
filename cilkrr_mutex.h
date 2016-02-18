#include <mutex>
#include <list>
#include <string>
#include <iostream>
#include <vector>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

typedef std::string pedigree_t;
typedef struct acquire_info_s {
	pedigree_t ped;
	int worker_id; // for debugging
} acquire_info_t;

std::ostream& operator<< (std::ostream &out, struct acquire_info_s s);


namespace cilkrr {

	class mutex {
	private:
		std::mutex m_mutex;
		__cilkrts_worker *m_owner;

		// Info for both recording and replaying
		uint64_t m_id; // index into global container of cilkrr_mutexes
		std::list<acquire_info_t> *m_acquires;

		void record_acquire();
		void replay_lock();
		bool replay_try_lock();
		void replay_unlock();

		inline void acquire();
		inline void release();
		static pedigree_t get_pedigree();
		
	public:
		mutex();
		~mutex();
		void lock();
		bool try_lock();
		void unlock();
	};

}
