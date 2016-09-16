#include <pthread.h>
#include "cilkrr.h"

#ifdef USE_LOCKSTAT
extern "C" {
#include "lockstat.h"
}
#endif

namespace cilkrr {

  class mutex {
  private:
#ifdef USE_LOCKSTAT
    struct spinlock_stat m_lock;
#else
    pthread_spinlock_t m_lock;
#endif

#ifdef DEBUG_ACQUIRE
    __cilkrts_worker *m_owner = nullptr;
    acquire_info volatile *m_active = nullptr;
#endif
#ifdef PORR_STATS
    uint64_t m_num_acquires;
    //uint64_t m_id;
#endif

    // Record/Replay fields
    volatile bool m_checking = false; /// For handoff between acquires
#ifdef ACQ_PTR
    acquire_container* m_acquires;
#else
    acquire_container m_acquires;
#endif
            
    void record_acquire(pedigree_t& p);
    void replay_lock(acquire_info *a);
    void replay_unlock();

    inline void acquire();
    inline void release();
    inline void init();

  public:
    mutex();
    mutex(uint64_t index);
    ~mutex();

    void lock();
    void unlock();
    bool try_lock();

  } __attribute__((aligned(64)));

}
