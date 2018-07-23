#ifndef __CILK__FUTURE_H__
#define __CILK__FUTURE_H__

#include <assert.h>
#include <functional>
#include <vector>
#include <internal/abi.h>
#include <pthread.h>

#define MAX_TOUCHES (10)

extern void __spawn_future_helper_helper(std::function<void(void)>);

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
  void **volatile m_deques;
  int m_num_suspended_deques;
  
public:

 future() {
    m_num_suspended_deques = 0;
    m_status = status::CREATED;
    m_suspended_deques[0] = NULL;
    m_deques = m_suspended_deques;
  };

  ~future() {
  }

  void* __attribute__((always_inline)) put(T result) {
    assert(m_status != status::DONE);
    m_result = result;
    __asm__ volatile ("" ::: "memory");
    m_status = status::DONE;
    
    void **suspended_deques;
    do {
        while (m_deques == NULL);
        suspended_deques = (void**)__sync_val_compare_and_swap(&m_deques, m_deques, NULL);
    } while (suspended_deques == NULL);

    // make resumable can be heavy, so keep it outside the lock
    void *ret = suspended_deques[0];
    for (int i = 1; i < m_num_suspended_deques; i++) {
        __cilkrts_make_resumable(suspended_deques[i]);
    }

    return ret;
  };

  bool __attribute__((always_inline)) ready() {
    // If the status is done, then the value is ready.
    return m_status==status::DONE;
  } 

  T __attribute__((always_inline)) get() {
        if (!this->ready()) {
            void *deque = __cilkrts_get_deque();
            void **suspended_deques;
            do {
                while (!this->ready() && m_deques == NULL);
                suspended_deques = (void**)__sync_val_compare_and_swap(&m_deques, m_deques, NULL);
            } while (suspended_deques == NULL && !this->ready());

            if (suspended_deques) {
                assert(m_num_suspended_deques < MAX_TOUCHES);
                suspended_deques[m_num_suspended_deques++] = deque;
                __asm__ volatile ("" ::: "memory");
                m_deques = suspended_deques;
                __cilkrts_suspend_deque();
            }
        }

    assert(m_status==status::DONE);
    return m_result;
  }
}; // class future

} // namespace cilk

#endif // #ifndef __CILK__FUTURE_H__
