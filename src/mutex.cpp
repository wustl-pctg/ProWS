#include "mutex.h"
#include <cstdio>
#include <iostream>
#include <cassert>
#include <cstring>

#include <atomic>
#define MEM_FENCE std::atomic_thread_fence(std::memory_order_seq_cst)
#define LOAD_FENCE std::atomic_thread_fence(std::memory_order_acquire)
#define STORE_FENCE std::atomic_thread_fence(std::memory_order_release)

#include <internal/abi.h>
#include "cilk/cilk_api.h"

namespace cilkrr {

  void mutex::init()
  {
    pthread_spin_init(&m_lock, PTHREAD_PROCESS_PRIVATE);
  }

  mutex::mutex()
    //: m_acquires(new acquire_container(g_rr_state->register_mutex()))
#ifndef ACQ_PTR
  : m_acquires(g_rr_state->register_mutex())
#endif
  {
    init();
#ifdef ACQ_PTR
    m_acquires = new (g_rr_state->register_mutex()) acquire_container();
#endif
  }
  
  mutex::mutex(uint64_t id)
    //: m_acquires(new acquire_container(g_rr_state->register_mutex(id)))
#ifndef ACQ_PTR
  : m_acquires(g_rr_state->register_mutex(id))
#endif
  {
    init();
#ifdef ACQ_PTR
    m_acquires = new (g_rr_state->register_mutex(id)) acquire_container();
#endif
  }

  mutex::~mutex()
  {
    pthread_spin_destroy(&m_lock);
    g_rr_state->unregister_mutex(m_acquires.m_size);
  }

  inline void mutex::acquire()
  {
#ifdef DEBUG_ACQUIRE
    m_owner = __cilkrts_get_tls_worker();
#endif
#ifdef PORR_STATS
    m_num_acquires++;
#endif
  }
  
  inline void mutex::release()
  {
#ifdef DEBUG_ACQUIRE
    m_owner = nullptr;
    m_active = nullptr;
#endif
    pthread_spin_unlock(&m_lock);
  }

  void mutex::lock()
  {
    enum mode m = get_mode();
    pedigree_t p;
    if (m != NONE) p = get_pedigree();

    
    if (get_mode() == REPLAY) {
      // returns locked, but not acquired
#ifdef ACQ_PTR
      replay_lock(m_acquires->find((const pedigree_t)p));
#else
      replay_lock(m_acquires.find((const pedigree_t)p));
#endif
    } else {
      pthread_spin_lock(&m_lock);
    }
    acquire();
    if (get_mode() == RECORD) record_acquire(p);
  }

  bool mutex::try_lock()
  {
    fprintf(stderr, "try_lock not implemented in this version of PORRidge!\n");
    std::abort();
    pthread_spin_trylock(&m_lock);
  }

  void mutex::unlock()
  {
    if (get_mode() == REPLAY) replay_unlock();
    else release();
    if (get_mode() != NONE)
      __cilkrts_bump_worker_rank();
  }

  void mutex::record_acquire(pedigree_t& p)
  {
#ifdef ACQ_PTR
    acquire_info *a = m_acquires->add(p);
#else
    acquire_info *a = m_acquires.add(p);
#endif
#ifdef DEBUG_ACQUIRE
    m_active = a;
#endif
  }

  void mutex::replay_lock(acquire_info* a)
  {
    // perf debug
    // m_mutex.lock();
    // return;
    // end perf debug
    
    a->suspended_deque = __cilkrts_get_deque();
    MEM_FENCE;

#ifdef ACQ_PTR
    acquire_info *front = m_acquires->current();
#else
    acquire_info *front = m_acquires.current();
#endif
    if (front == a) {

      while (m_checking) ;
      LOAD_FENCE;
      if (front->suspended_deque) {
        front->suspended_deque = nullptr;
        pthread_spin_lock(&m_lock);
        return; // continue
      }
    }
    LSTAT_INC(LSTAT_SUS);
    __cilkrts_suspend_deque();
  }

  void mutex::replay_unlock()
  {
    // for performance debugging
    // m_acquires->next();
    // release();
    // return;
    // end perf
    
    void *deque = nullptr;
    m_checking = true;

#ifdef ACQ_PTR
    m_acquires->next();
    acquire_info *front = m_acquires->current();
#else
    m_acquires.next();
    acquire_info *front = m_acquires.current();
#endif
    
    MEM_FENCE;

    if (front && front->suspended_deque) {
      deque = front->suspended_deque;
      front->suspended_deque = nullptr;
      STORE_FENCE;
    }
    m_checking = false;
    if (deque)
      __cilkrts_resume_suspended(deque, 1);
    else
      release();

  }

}
