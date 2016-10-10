#include "spinlock.h"
#include <cstdio>
#include <iostream>
#include <cassert>
#include <cstring>

//#define FAKE_ATOMIC 1
#ifndef FAKE_ATOMIC
#include <atomic>
#define MEM_FENCE std::atomic_thread_fence(std::memory_order_seq_cst)
#define LOAD_FENCE std::atomic_thread_fence(std::memory_order_acquire)
#define STORE_FENCE std::atomic_thread_fence(std::memory_order_release)
#define FAA(loc, val) __sync_fetch_and_add(loc, val)
#else
#define MEM_FENCE
#define LOAD_FENCE
#define STORE_FENCE
#define FAA(loc, val) (loc += val)
#endif

#include <internal/abi.h>
#include "cilk/cilk_api.h"

namespace porr {

  void spinlock::init(uint64_t id) { base_lock_init(&m_lock, id); }

  spinlock::spinlock()
  : m_acquires(g_rr_state->register_spinlock())
  {
    init(0); // not correct
  }
  
  spinlock::spinlock(uint64_t id)
   : m_acquires(g_rr_state->register_spinlock(id))
  {
    init(id);
  }

  spinlock::~spinlock()
  {
    base_lock_destroy(&m_lock);
    g_rr_state->unregister_spinlock(m_acquires.m_size);
  }

  inline void spinlock::acquire()
  {
#ifdef DEBUG_ACQUIRE
    m_owner = __cilkrts_get_tls_worker();
#endif
#ifdef PORR_STATS
    m_num_acquires++;
#endif
    //m_checking = 0;
    //    m_passed = false;
  }
  
  inline void spinlock::release()
  {
#ifdef DEBUG_ACQUIRE
    m_owner = nullptr;
    m_active = nullptr;
#endif
    base_unlock(&m_lock);
  }

  void spinlock::lock()
  {
    enum mode m = get_mode();
    pedigree_t p;
    if (m != NONE) p = get_pedigree();
    
    if (get_mode() == REPLAY) {
      // returns locked, but not acquired
      replay_lock(m_acquires.find((const pedigree_t)p));
    } else
      base_lock(&m_lock);
      
    acquire();
    if (get_mode() == RECORD) record_acquire(p);
  }

  bool spinlock::try_lock()
  {
    fprintf(stderr, "try_lock not implemented in this version of PORRidge!\n");
    std::abort();
    base_trylock(&m_lock);
  }

  void spinlock::unlock()
  {
    if (get_mode() == REPLAY) replay_unlock();
    else
      release();
    if (get_mode() != NONE)
      __cilkrts_bump_worker_rank();
  }

  void spinlock::record_acquire(pedigree_t& p)
  {
    acquire_info *a = m_acquires.add(p);
#ifdef DEBUG_ACQUIRE
    m_active = a;
#endif
  }

  void spinlock::replay_lock(acquire_info* a)
  {
    acquire_info *front = m_acquires.current();;
    void *deque = __cilkrts_get_deque();
    
    // FAA returns old value
    if (front == a || FAA(&a->suspended_deque, deque)) {
      base_lock(&m_lock);
      return;
    }

    a->suspended_deque = deque;
    LSTAT_INC(LSTAT_SUS);
    __cilkrts_suspend_deque();
  }

  void spinlock::replay_unlock()
  {
    void *deque = nullptr;
    acquire_info *front = m_acquires.current();
    if (!front || !front->next)
      return release();
    front = m_acquires.next();

    // FAA returns old value
    deque = FAA(&front->suspended_deque, (void*)0x1);
    if (deque) {
      __cilkrts_resume_suspended(deque, 1);
    } else {
      release();
    }
  }

}
