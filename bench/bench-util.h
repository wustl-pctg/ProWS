#ifdef CILKRR
#include <cilkrr.h>
#include <mutex.h>
typedef cilkrr::mutex lock_t;
#else 
#include <pthread.h>
typedef pthread_spinlock_t lock_t;
#endif

inline void lock_init(lock_t *l, int i) {
#ifdef CILKRR                                                                   
    new (l) cilkrr::mutex(i);                                              
#else                                                                           
    if( pthread_spin_init(l, PTHREAD_PROCESS_PRIVATE) ) {
        fprintf(stderr, "Error initializing lock!  Exit.\n");
        exit(1);
    }
#endif
}

inline void lock_destroy(lock_t *l) {
#ifdef CILKRR
    l->~mutex();
#else
    pthread_spin_destroy(l);
#endif
}

inline void lock(lock_t *l) {
#ifdef CILKRR
    l->lock();
#else
    if(pthread_spin_lock(l)) {
        fprintf(stderr, "Error acquiring lock!  Exit.\n");
        exit(1);
    }
#endif
}

inline void unlock(lock_t *l) {
#ifdef CILKRR
    l->unlock();
#else
    if(pthread_spin_unlock(l)) {
        fprintf(stderr, "Error releasing lock!  Exit.\n");
        exit(1);
    }
#endif
}
