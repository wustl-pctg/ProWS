#include "mutex.h"
#include "syncstream.h"
#include <cassert>
#include <memory>
#include <atomic>

#include <internal/abi.h>

namespace cilkrr {

	mutex::mutex()
	{
		m_id = g_rr_state->register_mutex();

		if (get_mode() != NONE)
			m_acquires = g_rr_state->get_acquires(m_id);
	}

	mutex::mutex(uint64_t index)
	{
		m_id = g_rr_state->register_mutex(index);
		
		if (get_mode() != NONE)
			m_acquires = g_rr_state->get_acquires(m_id);
	}

	mutex::~mutex() { g_rr_state->unregister_mutex(m_id); }

	// Determinism is enforced at the lock acquires, hence the pedigree
	// must be changed here, not in the release. Example: A programmer
	// might grab the lock, but release it based on certain
	// nondeterministic conditions.
	inline void mutex::acquire()
	{
		m_owner = __cilkrts_get_tls_worker();
		//		m_acq_count++;
		__cilkrts_bump_worker_rank();
	}
	
	inline void mutex::release()
	{
		m_owner = nullptr;
		m_mutex.unlock();
		//__cilkrts_bump_worker_rank();
	}

	// pedigree_t mutex::get_pedigree()
	// {
	// 	pedigree_t p = cilkrr::get_pedigree();
	// 	std::string c = std::to_string(m_acq_count) + ",]";
	// 	p.replace(p.length()-1, c.length(), c);
	// 	return p;
	// }

	// pedigree_t mutex::refresh_pedigree(pedigree_t &p)
	// {
	// 	std::string sub = p.substr(p.rfind(','));
	// 	size_t count = std::stoul(sub);
	// 	if (count != m_acq_count) {
	// 		std::string start = p.substr(0, p.rfind(','));
	// 		return start + std::to_string(m_acq_count) + ",]";
	// 	}
	// 	return p;
	// }
	
	void mutex::lock()
	{
		enum mode m = get_mode();
		pedigree_t p;
		if (m != NONE) p = get_pedigree();
		if (get_mode() == REPLAY)
			replay_lock(p); // returns locked, but not acquired
		else 
			m_mutex.lock();
		acquire();
		if (get_mode() == RECORD) record_acquire(p);
	}

	bool mutex::try_lock()
	{
		bool result;
		pedigree_t p;
		
		if (get_mode() != NONE) p = get_pedigree();
		if (get_mode() == REPLAY)
			result = replay_try_lock(p);
		else
			result = m_mutex.try_lock();
		
		if (result) {
			acquire();
			if (get_mode() == RECORD) record_acquire(p);
		} else
			__cilkrts_bump_worker_rank();
				
		return result;
	}

	void mutex::unlock()
	{
		if (get_mode() == REPLAY) replay_unlock();
		else release();
	}

	void mutex::record_acquire(pedigree_t& p) { m_acquires->add(p); }

	void mutex::replay_lock(pedigree_t& p)
	{
		if (p != m_acquires->current()->ped)
			suspend(p); // returns locked, but not acquired
		else // may need to wait constant time as owner decides to release lock
			m_mutex.lock();
		// // Invariant: Upon return, the lock is locked by this worker
	}

	bool mutex::replay_try_lock(pedigree_t& p)
	{
		acquire_info *a = m_acquires->find(p);
		if (!a) { // this try_lock never succeeded
			// Since we're assuming no determinacy races, this is ok.
			return false;
		}

		if (a == m_acquires->current()) {
			assert(m_owner == nullptr);
			bool res = m_mutex.try_lock();
			assert(res == true);
		} else {

			// This is a bit weird for try_lock to wait, but it must in this
			// case. We always increment the pedigree on try_locks, and this
			// pedigree should be getting the lock, but we need to wait for
			// other locks to complete.
			suspend(p);
		}
		
		return true;
	}

	void mutex::replay_unlock()
	{
		m_acquires->next();
		
		//		To resume a suspended deque, we retain the lock, so we don't release
		__sync_synchronize();

		acquire_info *current = m_acquires->current();
	
		if (!(current == m_acquires->end()) && current->suspended_deque) {
			void *d = current->suspended_deque;
			
			cilkrr::sout << "(w: " << __cilkrts_get_tls_worker_fast()->self
									 << ") about to resume suspended deque in replay_unlock" << cilkrr::endl;
			
			__cilkrts_resume_suspended(d, 1);
			
			cilkrr::sout << "(w: " << __cilkrts_get_tls_worker()->self
									 << ") resuming in replay_unlock (p: "
									 << get_pedigree() << ")" << cilkrr::endl;
		} else { // Like a real release
			release();
		}


	}

	void mutex::suspend(pedigree_t& p)
	{
		acquire_info *a = m_acquires->find(p);
		if (!a) {
			cilkrr::sout << "Error: can't find " << p << cilkrr::endl;
			std::abort();
		}
		void *deque = __cilkrts_get_deque();

		a->suspended_deque = deque;
		__sync_synchronize();

		cilkrr::sout << "(w: " << __cilkrts_get_tls_worker()->self << ") "
								 << "suspending deque " << a->suspended_deque << " "
								 << "at: " << p << cilkrr::endl;

		acquire_info *front = m_acquires->current();
		if (front == a && m_mutex.try_lock()) {
			// Have lock, MUST resume
			//acquire();

		} else {
			cilkrr::sout << "(w: " <<  __cilkrts_get_tls_worker()->self
									 << ") about to resume stealing in mutex::suspend" << cilkrr::endl;
			__cilkrts_suspend_deque();
		}

		// When this fiber is resumed, the mutex is locked!
		std::atomic_thread_fence(std::memory_order_seq_cst);

		cilkrr::sout << "(w: " << __cilkrts_get_tls_worker()->self
								 << ") Resume at: " << get_pedigree() << cilkrr::endl;
	}



}
