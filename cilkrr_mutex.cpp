#include "cilkrr_mutex.h"
#include <iostream>
#include <sstream>
#include <vector>

std::ostream& operator<< (std::ostream &out, struct acquire_info_s s)
{
	out << "w: " << s.worker_id << ", n: " << s.n << "; " << s.ped;
	return out;
}


namespace cilkrr {

	// The *only* purpose of this class is to control when we output the
	// lock acquires. Normally I would use a program destructor, but
	// m_mutexes might have already been destroyed.
	class cilkrr_context {
	public:
		std::vector< std::list< acquire_info_t > * > m_context;
		void info();
		cilkrr_context(){}
		~cilkrr_context()
		{
			std::cerr << "Destroying global cilkrr structs." << std::endl;
		
			int i = 0;
			for (auto it = m_context.cbegin(); it != m_context.cend(); ++it) {
				std::cerr << "Acquires of mutex " << i << ":" << std::endl;
				for (auto acq = (*it)->cbegin(); acq != (*it)->cend(); ++acq) {
					std::cerr << *acq << std::endl;
				}
				++i;
			}

		}
	};

	cilkrr_context *g_context = nullptr;
	uint64_t g_next_mutex_id = 0;

	mutex::mutex()
	{
		// Static initialization order is tricky, but this hack works.
		if (g_next_mutex_id == 0)
			g_context = new cilkrr_context();
		
		// You *could* protect this increment with a lock, but we must
		// have deterministic ids anyway, so the lock would be pointless.
		m_id = g_next_mutex_id++;

		m_acquires = new std::list<acquire_info_t>;
		g_context->m_context.push_back(m_acquires);
		std::cerr << "Creating cilkrr mutex: " << this
							<< ", " << m_id << std::endl;

	}

	mutex::~mutex()
	{
		std::cerr << "Destroy cilkrr mutex: " << this << std::endl;

		// Warning: All mutexes must be created before any can be destroyed!
		if (--g_next_mutex_id == 0)
			delete g_context;
	}
	
	void mutex::lock(int n)
	{
		m_mutex.lock();
		m_owner = __cilkrts_get_tls_worker();
		record_acquire(n);
	}

	bool mutex::try_lock(int n)
	{
		bool result = m_mutex.try_lock();
		if (result) {
			m_owner = __cilkrts_get_tls_worker();
			record_acquire(n);
		}
	}

	void mutex::unlock()
	{
		m_owner = nullptr;
		m_mutex.unlock();
	}

	void mutex::record_acquire(int n)
	{
		pedigree_t ped = get_pedigree();
		acquire_info_t a = {ped, __cilkrts_get_worker_number(), n};
		m_acquires->push_back(a);
		__cilkrts_bump_worker_rank();
  
		//		std::cerr << "Acquiring " << this << ": " << a << std::endl;
	}


	pedigree_t mutex::get_pedigree()
	{
		const __cilkrts_pedigree tmp = __cilkrts_get_pedigree();
		const __cilkrts_pedigree *current = &tmp;
		
		pedigree_t p = "]";
		while (current) {
			// Prepending is probably not very performant...
			p = std::to_string(current->rank) + "," + p;
			current = current->parent;
		}
		p = "[" + p;
		return p;
	}

}
