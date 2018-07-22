#ifndef __CILK__FUTURE_H__
#define __CILK__FUTURE_H__

#include <assert.h>
#include <functional>
#include <vector>
#include <internal/abi.h>
#include <pthread.h>

#define MAX_TOUCHES (10)

extern void __spawn_future_helper_helper(std::function<void(void)>);

//extern "C" {
//void __cilkrts_fence();
//}

#define __cilkrts_fence()   __sync_synchronize()

namespace cilk {

#define reuse_future(T,fut, loc,func,args...)  \
  { \
  auto functor = std::bind(func, ##args);  \
  fut = new (loc) cilk::future<T>();  \
  auto __temp_fut = fut; \
  __spawn_future_helper_helper([__temp_fut,functor]() -> void { \
    void *__cilk_deque = __temp_fut->put(functor()); \
    if (__cilk_deque) __cilkrts_resume_suspended(__cilk_deque, 2);\
  }); \
  }

#define cilk_future_get(fut)              (fut)->get()

#define cilk_future_create(T,fut,func,args...) \
  { \
  auto functor = std::bind(func, ##args);  \
  fut = new cilk::future<T>();  \
  auto __temp_fut = fut; \
  __spawn_future_helper_helper([__temp_fut,functor]() -> void { \
    void *__cilk_deque = __temp_fut->put(functor()); \
    if (__cilk_deque) __cilkrts_resume_suspended(__cilk_deque, 1);\
  }); \
  }


#define cilk_future_create__stack(T,fut,func,args...)\
  cilk::future<T> fut;\
  { \
    auto functor = std::bind(func, ##args); \
    __spawn_future_helper_helper([&fut,functor]() -> void { \
      void *__cilk_deque = fut.put(functor()); \
      if (__cilk_deque) __cilkrts_resume_suspended(__cilk_deque, 1);\
    }); \
  }

template<typename T>
class future {
private:
  enum class status { 
    CREATED, // memory allocated, initialized
    DONE, // strand has finished execution
  };

  volatile status m_status;
  volatile T m_result;

  void* m_suspended_deques[MAX_TOUCHES];

  void *volatile * m_suspended_deques_tail;
  void *volatile * m_suspended_deques_head;

  void **volatile m_deques;
  int m_num_suspended_deques;
  pthread_spinlock_t lock;
  int m_ntouches;

  bool __attribute__((always_inline)) has_a_deque() {
    return (m_suspended_deques_head < m_suspended_deques_tail);
  }

  bool __attribute__((always_inline)) dekker_protocol() {
    if (has_a_deque()) {
      return true;
    } else {
      return false;
    }
  }

  bool __attribute__((always_inline)) too_late_to_pop() {
    void *volatile *t = m_suspended_deques_tail;
    --t;
    m_suspended_deques_tail = t;

    if (m_suspended_deques_head >= t+1) {
        m_suspended_deques_tail++;
        return true;
    }
    return false;
  }
  
public:

 future(int ntouches = MAX_TOUCHES) {
    //printf("%d\n", lock);
    m_ntouches = ntouches;
    if (ntouches > 1) {
        pthread_spin_init(&lock, PTHREAD_PROCESS_PRIVATE);
    }
    //printf("%d\n", lock);
    //lock = 1;
    m_num_suspended_deques = 0;
    m_suspended_deques_tail = m_suspended_deques_head = &m_suspended_deques[0];
    m_status = status::CREATED;
    m_suspended_deques[0] = NULL;
    m_deques = m_suspended_deques;
  };

  ~future() { if (m_ntouches > 1) pthread_spin_destroy(&lock); }

  void* __attribute__((always_inline)) put(T result) {
    assert(m_status != status::DONE);
    m_result = result;
    __asm__ volatile ("" ::: "memory");
    m_status = status::DONE;
    __asm__ volatile ("" ::: "memory");
    
    /*void **suspended_deques;
    do {
        while (m_deques == NULL);
        suspended_deques = __sync_val_compare_and_swap(&m_deques, m_deques, NULL);
    } while (suspended_deques == NULL);
    */

    // make resumable can be heavy, so keep it outside the lock
    /*void *ret = suspended_deques[0];
    for (int i = 1; i < m_num_suspended_deques; i++) {
        __cilkrts_make_resumable(suspended_deques[i]);
    }
    */
    void *ret = NULL;
    if (dekker_protocol()) {
        void *volatile *h = m_suspended_deques_head;
        m_suspended_deques_head = h+1;
        ret = *h;
        
        while (dekker_protocol()) {
            h = m_suspended_deques_head;
            m_suspended_deques_head = h+1;
            __cilkrts_make_resumable(*h);
        }
    }

    return ret;
  }

  bool __attribute__((always_inline)) ready() {
    // If the status is done, then the value is ready.
    return m_status==status::DONE;
  } 

  T __attribute__((always_inline)) get() {
    if (!this->ready()) {
      void *deque = __cilkrts_get_deque();

      while(m_ntouches > 1 && !pthread_spin_trylock(&lock)) {
        if (ready()) {
          goto no_suspend;
        }
      }

      if (!this->ready()) {
        (*(m_suspended_deques_tail++)) = deque;
        __asm__ volatile ("" ::: "memory");
        if (m_ntouches > 1) pthread_spin_unlock(&lock);
        __cilkrts_suspend_deque();
      } else if (m_ntouches > 1) {
        pthread_spin_unlock(&lock);
      }

    }

    no_suspend:

    // Dear lord, please tell me why this makes things faster!
    assert(m_status==status::DONE);
    return m_result;
  }
}; // class future

} // namespace cilk

#endif // #ifndef __CILK__FUTURE_H__
