#include <mutex>
#include <list>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

typedef uint64_t pedigree_t;
typedef struct acquire_info_s {
	uint64_t ped;
	__cilkrts_worker *worker;
	int n;
} acquire_info_t;

namespace cilkrr {

	class mutex {
	private:
		std::mutex m_mutex;
		std::list<acquire_info_t> m_acquires;
		__cilkrts_worker *m_owner;

		void record_acquire(int n);
		static pedigree_t get_pedigree();
		
	public:
		mutex();
		~mutex();
		void lock(int n);
		bool try_lock(int n);
		void unlock();
	};

}
