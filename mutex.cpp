#include "mutex.h"
#include <cstdio>
#include <cassert>
#include <memory>
#include <atomic>

#include <internal/abi.h>
#include "cilk/cilk_api.h"

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
		//__cilkrts_bump_worker_rank();
	}
	
	inline void mutex::release()
	{
		m_owner = nullptr;
		m_mutex.unlock();
	}

	void mutex::lock()
	{
		enum mode m = get_mode();
		pedigree_t p;
		if (m != NONE) 
			p = get_pedigree();
		if (get_mode() == REPLAY)
			replay_lock(p); // returns locked, but not acquired
		else 
			m_mutex.lock();
		acquire();
#if IMETHOD != INONE
		if (get_mode() == RECORD) record_acquire(p);
#endif
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
		if (get_mode() != NONE)
			__cilkrts_bump_worker_rank();
	}

	void mutex::record_acquire(pedigree_t& p) { m_acquires->add(p); }

	void mutex::replay_lock(pedigree_t& p)
	{
		if (!(p == m_acquires->current()->ped))
			suspend(p); // returns locked, but not acquired
		else // may need to wait constant time as owner decides to release lock
			m_mutex.lock();
		// Invariant: Upon return, the lock is locked by this worker
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
			fprintf(stderr, "(w: %d) about to resume suspended deque in replay_unlock\n",
							__cilkrts_get_internal_worker_number());
			
			__cilkrts_resume_suspended(d, 1);

			// fprintf(stderr, "(w: %d) resuming in replay_unlock (p: %s)\n",
			// 				__cilkrts_get_internal_worker_number(),
			// 				get_pedigree().c_str());
			
		} else { // Like a real release
			release();
		}


	}

	void mutex::suspend(pedigree_t& p)
	{
		acquire_info *a = m_acquires->find(p);
		if (!a) {
			fprintf(stderr, "Error: can't find pedigree\n");
			//fprintf(stderr, "Error: can't find %s\n", p.c_str());
			//fprintf(stderr, "Error: can't find %lu\n", p);
			std::abort();
		}
		void *deque = __cilkrts_get_deque();

		a->suspended_deque = deque;
		__sync_synchronize();

		acquire_info *front = m_acquires->current();
		// fprintf(stderr, "(w: %d) suspending deque %p at: %s (waiting on %s)\n",
		// 				__cilkrts_get_internal_worker_number(), a->suspended_deque,
		// 				p.c_str(), front->ped.c_str());

		if (front == a && m_mutex.try_lock()) {
			// Have lock, MUST resume
			//acquire();

		} else {
			fprintf(stderr, "(w: %d) about to resume stealing in mutex::suspend\n",
							__cilkrts_get_internal_worker_number());
			__cilkrts_suspend_deque();
		}

		// When this fiber is resumed, the mutex is locked!
		std::atomic_thread_fence(std::memory_order_seq_cst);

		// fprintf(stderr, "(w: %d) Resume at: %s\n",
		// 				__cilkrts_get_internal_worker_number(),
		// 				get_pedigree().c_str());
	}



}

