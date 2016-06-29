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
    m_active = nullptr;
		m_mutex.unlock();
	}

	void mutex::lock()
	{
		enum mode m = get_mode();
		pedigree_t p;
		if (m != NONE) p = get_pedigree();
    
		if (get_mode() == REPLAY) {
      // returns locked, but not acquired
			replay_lock(m_acquires->find((const pedigree_t)p)); 
    } else {
			m_mutex.lock();
    }
		acquire();
		if (get_mode() == RECORD) record_acquire(p);
	}

	bool mutex::try_lock()
	{
    fprintf(stderr, "try_lock not implemented for CILKRR\n");
    std::abort();
	// 	bool result;
	// 	pedigree_t p;
		
	// 	if (get_mode() != NONE) p = get_pedigree();
	// 	if (get_mode() == REPLAY)
	// 		result = replay_try_lock(p);
	// 	else
	// 		result = m_mutex.try_lock();
		
	// 	if (result) {
	// 		acquire();
	// 		if (get_mode() == RECORD) record_acquire(p);
	// 	} else
	// 		__cilkrts_bump_worker_rank();
				
	// 	return result;
	}

	void mutex::unlock()
	{
		if (get_mode() == REPLAY) replay_unlock();
		else release();
		if (get_mode() != NONE)
			__cilkrts_bump_worker_rank();
	}

	void mutex::record_acquire(pedigree_t& p){ m_active = m_acquires->add(p); }

	void mutex::replay_lock(acquire_info* a)
	{
		// if (!(a == m_acquires->current()))
		// 	suspend(a); // returns locked, but not acquired
		// else // may need to wait constant time as owner decides to release lock
		// 	m_mutex.lock();
    suspend(a);
		// Invariant: Upon return, the lock is locked by this worker
	}

	// bool mutex::replay_try_lock(pedigree_t& p)
	// {
	// 	acquire_info *a = m_acquires->find(p);
	// 	if (!a) { // this try_lock never succeeded
	// 		// Since we're assuming no determinacy races, this is ok.
	// 		return false;
	// 	}

	// 	if (a == m_acquires->current()) {
	// 		assert(m_owner == nullptr);
	// 		bool res = m_mutex.try_lock();
	// 		assert(res == true);
	// 	} else {

	// 		// This is a bit weird for try_lock to wait, but it must in this
	// 		// case. We always increment the pedigree on try_locks, and this
	// 		// pedigree should be getting the lock, but we need to wait for
	// 		// other locks to complete.
	// 		suspend(p);
	// 	}
		
	// 	return true;
	// }

	void mutex::replay_unlock()
	{
    // Original protocol:
    //  if suspended_deque:
    //    resume suspended
    //  else
    //    release

		acquire_info *front = m_acquires->current()->next;

		m_acquires->next();
    __sync_synchronize();
    release();		
	
		// if (!(front == m_acquires->end()) 
    //     && front->suspended_deque
    //     && m_mutex.try_lock()) {


    //   if (front->suspended_deque)
    //     __cilkrts_resume_suspended(front->suspended_deque, 1);
    //     void* deque = front->suspended_deque;
    //     //if (deque && __sync_bool_compare_and_swap(&front->suspended_deque, deque, NULL))
    //     //__cilkrts_resume_suspended(deque, 1);

		// } 
#define CAS(l,o,n) __sync_bool_compare_and_swap(l,o,n)
    if (!front) return;
    void* deque = front->suspended_deque;
    if (deque && CAS(&front->suspended_deque, deque, NULL))
      __cilkrts_resume_suspended(deque, 1);



	}

	void mutex::suspend(acquire_info *a)
	{
		void *deque = __cilkrts_get_deque();

		a->suspended_deque = deque;
		__sync_synchronize();

		acquire_info *front = m_acquires->current();

    //if (front == a && m_mutex.try_lock()) {
    if (front == a && __sync_bool_compare_and_swap(&front->suspended_deque, deque, NULL)) {
			// Have lock, MUST resume
		} else {
			__cilkrts_suspend_deque();
		}
    //a->suspended_deque = NULL;
    // int locked = m_mutex.try_lock();
    // assert(locked);
    m_mutex.lock();
    
		// When this fiber is resumed, the mutex is locked!
		std::atomic_thread_fence(std::memory_order_seq_cst);
	}



}
