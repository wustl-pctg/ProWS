#include <pthread.h>
#include "porr.h"

namespace porr {

  class spinlock {
  private:
    base_lock_t m_lock;

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
    acquire_container m_acquires;
    /* char pad[32]; */
            
    void record_acquire(pedigree_t& p);
    void replay_lock(acquire_info *a);
    void replay_unlock();

    inline void acquire();
    inline void release();
    inline void init(uint64_t id);

  public:
    spinlock();
    spinlock(uint64_t index);
    ~spinlock();

    void lock();
    void unlock();
    bool try_lock();

  };// __attribute__((aligned(64)));

}
