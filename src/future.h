#ifndef __CILK__FUTURE_H__
#define __CILK__FUTURE_H__

#include <assert.h>
//#include "cilk/cilk.h"
#include <functional>
//#include "spinlock.h"
#include <vector>
#include <internal/abi.h>
#include <pthread.h>

#define MAX_TOUCHES (10)

//#ifndef cilk_spawn_future
//  #define cilk_spawn_future cilk_spawn
//#endif

//__attribute__((weak)) void __cilk_spawn_future(std::function<void()> func) {
//  std::cout << "Using the dummy future spawn!!! cilk_sync will wait for it to complete :(" << std::endl;
//  cilk_spawn func();
//}

extern "C" {
    void __spawn_future_helper_helper(std::function<void(void)>);
}

namespace cilk {
#define reuse_future(T,fut, loc,func,args...)  \
  { \
  auto functor = std::bind(func, ##args);  \
  fut = new (loc) cilk::future<T>();  \
  auto __temp_fut = fut; \
  __spawn_future_helper_helper([__temp_fut,functor]() -> void { \
    void *__cilk_deque = __temp_fut->put(functor()); \
    if (__cilk_deque) __cilkrts_resume_suspended(__cilk_deque, 1);\
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

  pthread_mutex_t m_touches_lock = PTHREAD_MUTEX_INITIALIZER;
  void* m_suspended_deques[MAX_TOUCHES];
  int m_num_suspended_deques;
  
public:

 future() {
    m_num_suspended_deques = 0;
    m_status = status::CREATED;
    //m_suspended_deques = new void*[MAX_TOUCHES]();
    m_suspended_deques[0] = NULL;
  };

  ~future() {
    assert(this->ready());
    // Make sure we don't delete in the middle of a put
    pthread_mutex_lock(&m_touches_lock);
    pthread_mutex_unlock(&m_touches_lock);
    pthread_mutex_destroy(&m_touches_lock);
    //assert(m_suspended_deques == NULL);
  }

  void* put(T result) {
    assert(m_status != status::DONE);
    m_result = result;

    // Make sure no worker is in the middle of
    // suspending its own deque before proceeding.
    pthread_mutex_lock(&m_touches_lock);
    m_status = status::DONE;
    void* suspended_deques[MAX_TOUCHES];
    suspended_deques[0] = NULL;
    for (int i = 0; i < m_num_suspended_deques; i++) {
        suspended_deques[i] = m_suspended_deques[i];
    }
    int num_suspended_deques = m_num_suspended_deques;
    pthread_mutex_unlock(&m_touches_lock);
      //void** suspended_deques = m_suspended_deques;
      //m_suspended_deques = NULL;
      //int num_suspended_deques = m_num_suspended_deques;
      //m_num_suspended_deques = 0;

    
    void *ret = suspended_deques[0];
    for (int i = 1; i < num_suspended_deques; i++) {
        __cilkrts_make_resumable(suspended_deques[i]);
    }

    //delete [] suspended_deques;

    return ret;
  };

  bool ready() {
    // If the status is done, then the value is ready.
    return m_status==status::DONE;
  } 

  T get() {
    if (!this->ready()) {
        pthread_mutex_lock(&m_touches_lock);

        if (!this->ready()) {
            void *deque = __cilkrts_get_deque();
            assert(deque);
            assert(m_num_suspended_deques < MAX_TOUCHES);
            m_suspended_deques[m_num_suspended_deques++] = deque;
            pthread_mutex_unlock(&m_touches_lock);
            __cilkrts_suspend_deque();
        } else {
            pthread_mutex_unlock(&m_touches_lock);
        }

    }
    assert(m_status==status::DONE);
    return m_result;
  }
}; // class future

} // namespace cilk

#endif // #ifndef __CILK__FUTURE_H__
