#include "mutex.h"
#include <cassert>

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
		release();
		if (cilkrr_mode() == REPLAY) replay_unlock();
	}

	void mutex::record_acquire()
	{
		m_acquires->push_back(acquire_info());
	}

	void mutex::replay_lock()
	{
		pedigree_t p = get_pedigree();
		//		while (p != m_acquires->front().ped);
		if (p != m_acquires->front().ped)
			suspend(m_acquires, p);
		
		bool res = m_mutex.try_lock();
		assert(res == true);
		acquire();
	}

	bool mutex::replay_try_lock()
	{
		if (get_pedigree() == m_acquires->front().ped) {
			assert(m_owner == nullptr);
			bool res = m_mutex.try_lock();
			assert(res == true);
			return res;
		}
		return false;
	}

	void mutex::replay_unlock()
	{
		m_acquires->pop_front();

		if (!m_acquires->empty()) {
			if (m_acquires->front().suspended_deque != nullptr)
				__cilkrts_resume_suspended(m_acquires->front().suspended_deque);
		}

		// Choice here: If this lock acquire was suspended, should we try
		// to go back to the previous acquire strand?
	}

}
