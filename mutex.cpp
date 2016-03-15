#include "mutex.h"
#include "syncstream.h"
#include <cassert>
#include <memory>
#include <atomic>

#include <internal/abi.h>

namespace cilkrr {

	mutex::mutex()
	{
		// Static initialization order is tricky, but this hack works.
		if (g_rr_state == nullptr)
			g_rr_state = new cilkrr::state();


		// You *could* protect this insert with a lock, but we must have
		// deterministic ids anyway, so the lock would be pointless.
		m_id = g_rr_state->add_mutex();
		m_acquires = g_rr_state->get_mutex(m_id);

		if (g_rr_state->m_mode == REPLAY)
			m_it = m_acquires->begin();

		// std::cerr << "Creating cilkrr mutex: " << this
		// 					<< ", " << m_id << std::endl;
	}

	mutex::~mutex()
	{
		// std::cerr << "Destroy cilkrr mutex: " << this << std::endl;

		// Warning: All mutexes must be created before any can be destroyed!
		if (g_rr_state->remove_mutex(m_id) == 0)
			delete g_rr_state;
	}

	inline void mutex::acquire() { m_owner = __cilkrts_get_tls_worker(); }
	inline void mutex::release()
	{
		m_owner = nullptr;
		m_mutex.unlock();
		__cilkrts_bump_worker_rank();
	}

	
	void mutex::lock()
	{
		if (cilkrr_mode() == REPLAY) return replay_lock();
		m_mutex.lock();
		acquire();
		if (cilkrr_mode() == RECORD) record_acquire();
	}

	bool mutex::try_lock()
	{
		bool result;
		if (cilkrr_mode() == REPLAY)
			result = replay_try_lock();
		else
			result = m_mutex.try_lock();
		
		if (result) {
			acquire();
			if (cilkrr_mode() == RECORD) record_acquire();
		}
		return result;
	}

	void mutex::unlock()
	{
		if (cilkrr_mode() == REPLAY) replay_unlock();
		else release();
	}

	void mutex::record_acquire()
	{
		m_acquires->push_back(acquire_info());
	}

	void mutex::replay_lock()
	{
		pedigree_t p = get_pedigree();
		if (p != (*m_it).ped)
			suspend(p);
		else // may need to wait constant time as owner decides to release lock
			m_mutex.lock();
		  // Invariant: Upon return, the lock is locked by this worker
		// } else {
		// 	bool result = m_mutex.try_lock();
		// 	assert(result == true);
		// }
		acquire();
		assert(m_owner = __cilkrts_get_tls_worker_fast());
	}

	bool mutex::replay_try_lock()
	{

		/// @todo this isn't quite right. For try_locks, I think we'll
		/// need to record failures as well.
		if (get_pedigree() == (*m_it).ped) {
			assert(m_owner == nullptr);
			bool res = m_mutex.try_lock();
			assert(res == true);
			return res;
		}
		return false;
	}

	void mutex::replay_unlock()
	{
		m_it++;
		
		//		To resume a suspended deque, we retain the lock, so we don't release
		__sync_synchronize();
	
		if (!(m_it == m_acquires->end()) && (*m_it).suspended_deque) {
			void *d = (*m_it).suspended_deque;
			cilkrr::sout << "(w: " << __cilkrts_get_tls_worker_fast()->self
									 << ") about to resume suspended deque in replay_unlock" << cilkrr::endl;
			__cilkrts_resume_suspended(d, 1);
			cilkrr::sout << "(w: " << __cilkrts_get_tls_worker()->self
									 << ") resuming in replay_unlock (p: "
									 << get_pedigree() << ")" << cilkrr::endl;
		} else { // Like a real release
			m_owner = nullptr;
			m_mutex.unlock();
		}

		// We don't do a real release here since we may retain the lock to
		// resume a suspended deque. But in any case we need to bump the rank.
		__cilkrts_bump_worker_rank();

	}

	/// @todo use a hash table so we don't have to iterate through
	acquire_info* mutex::find_acquire(pedigree_t& p)
	{
		assert(g_rr_state->m_mode == REPLAY);

		for (auto it = m_it; it != m_acquires->end(); ++it) {
			if (it->ped == p) {
				return &(*it);
				break;
			}
		}
		cilkrr::sout << "Error: can't find " << p << cilkrr::endl;
		assert(0);
	}

	void mutex::suspend(pedigree_t& p)
	{
		acquire_info *a = find_acquire(p);
		void *deque = __cilkrts_get_deque();

		a->suspended_deque = deque;
		__sync_synchronize();

		cilkrr::sout << "(w: " << __cilkrts_get_tls_worker()->self << ") "
								 << "suspending deque " << a->suspended_deque << " "
								 << "at: " << p << cilkrr::endl;

		// if (front->suspended_deque != nullptr
		// 		/// @todo There is some vanishingly small chance that m_it will change here...
		// 		&& m_mutex.try_lock()) {
		//			acquire();

		// if (deque != nullptr
		// 		&& (deque =
		// 				__sync_val_compare_and_swap(&front->suspended_deque,
		// 																		 deque, nullptr))) {

		// 	// We are now responsible for deque...
		// 	if (!m_mutex.try_lock()) { // didn't get lock, must try again
		// 		a = front;
		// 		p = front->ped;
		// 		goto begin;
		// 	}
		acquire_info *front = &(*m_it);
		//		deque = front->suspended_deque;
		if (front == a && m_mutex.try_lock()) {

			// Have lock, MUST resume
			acquire();
			
			// If this is the front, just continue on
			//			if (&(*m_it) != a) {
			// if (deque != __cilkrts_get_deque()) {
			// 	cilkrr::sout << "(w: " << __cilkrts_get_tls_worker()->self
			// 							 << ") about to resume suspended deque in mutex::suspend" << cilkrr::endl;
			// 	__cilkrts_resume_suspended(deque, 0);
			//	}
		} else {
			cilkrr::sout << "(w: " <<  __cilkrts_get_tls_worker()->self
									 << ") about to resume stealing in mutex::suspend" << cilkrr::endl;
			__cilkrts_suspend_deque();
		}

		// When this fiber is resumed, the mutex is locked!
		//		acquire();
		std::atomic_thread_fence(std::memory_order_seq_cst);
		assert(this->m_owner == __cilkrts_get_tls_worker());

		cilkrr::sout << "(w: " << __cilkrts_get_tls_worker()->self
								 << ") Resume at: " << get_pedigree() << cilkrr::endl;
	}



}
