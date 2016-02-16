#include "cilkrr_mutex.h"
#include <iostream>

namespace cilkrr {

	mutex::mutex()
	{
		std::cerr << "Creating cilkrr mutex: " << this << std::endl;
	}

	mutex::~mutex()
	{
		std::cerr << "Destroy cilkrr mutex: " << this << std::endl;
		std::cerr << "Lock acquires: " << std::endl;
		for (auto it = m_acquires.begin(); it != m_acquires.end(); ++it) {
			std::cerr << "w: " << it->worker << " acquired at " << it->n
								<< " with pedigree " << it->ped << std::endl;
		}
	}
	
	void mutex::lock(int n)
	{
		m_mutex.lock();
		record_acquire(n);
	}

	bool mutex::try_lock(int n)
	{
		bool result = m_mutex.try_lock();
		record_acquire(n);
	}

	void mutex::unlock()
	{
		m_owner = nullptr;
		m_mutex.unlock();
	}

	void mutex::record_acquire(int n)
	{
		std::cerr << "Acquiring " << this << ":";
		m_owner = __cilkrts_get_tls_worker();
		pedigree_t ped = get_pedigree();
		acquire_info_t a = {ped, m_owner, n};
		m_acquires.push_back(a);
		__cilkrts_bump_worker_rank();
		std::cerr << std::endl;
	}


	pedigree_t mutex::get_pedigree()
	{
		const __cilkrts_pedigree tmp = __cilkrts_get_pedigree();

		// Get size (# integers)
		int i = 0;
		const __cilkrts_pedigree *current = &tmp;
		while (current) {
			i++;
			current = current->parent;
		}

		current = &tmp;
		pedigree_t ped = 0;
		while (current) {
			std::cerr << ' ' << current->rank;
			ped += current->rank << i;
			i--;
			current = current->parent;
		}
		return ped;
	}

}
