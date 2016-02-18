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
} acquire_info_t;

std::ostream& operator<< (std::ostream &out, struct acquire_info_s s);


namespace cilkrr {

	class mutex {
	private:
		std::mutex m_mutex;
		std::list<acquire_info_t> *m_acquires;
		__cilkrts_worker *m_owner;
		uint64_t m_id;

		void record_acquire();
		static pedigree_t get_pedigree();
		
	public:
		mutex();
		~mutex();
		void lock();
		bool try_lock();
		void unlock();
	};

}
