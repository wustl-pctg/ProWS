#ifndef __CILK__FUTURE_H__
#define __CILK__FUTURE_H__

#include <assert.h>
//#include "cilk/cilk.h"
#include <functional>
//#include "spinlock.h"
#include <vector>
#include <internal/abi.h>
#include <pthread.h>

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
    __temp_fut->put(functor()); \
  }); \
  }

#define cilk_future_get(fut)              (fut)->get()

#define cilk_future_create(T,fut,func,args...) \
  { \
  auto functor = std::bind(func, ##args);  \
  fut = new cilk::future<T>();  \
  auto __temp_fut = fut; \
  __spawn_future_helper_helper([__temp_fut,functor]() -> void { \
    __temp_fut->put(functor()); \
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

  pthread_mutex_t m_acquires_lock;
  //porr::spinlock m_acquires_lock;
  // Treat gets like lock acquires
  //std::vector<porr::acquire_info*> m_acquires;
  //porr::acquire_info* m_get;
  void* m_get;
  
public:

 future() {
    m_status = status::CREATED;
    m_get = NULL;
    pthread_mutex_init(&m_acquires_lock, NULL);
  };

  ~future() {
    pthread_mutex_destroy(&m_acquires_lock);
  }

  void put(T result) {
    assert(m_status != status::DONE);
    m_result = result;
    m_status = status::DONE;
    //m_acquires_lock.lock();
    pthread_mutex_lock(&m_acquires_lock);
    if (m_get) {
        //printf("Resuming a suspended deque! %p\n", m_get);
        __cilkrts_resume_suspended(m_get, 1);
        //delete m_get;
        m_get = NULL;
    }
    pthread_mutex_unlock(&m_acquires_lock);
    //m_acquires_lock.unlock();
  };

  bool ready() {
    // If the status is done, then the value is ready.
    return m_status==status::DONE;
  } 

  T get() {
    // TODO: Treat this the same way spin locks are handled.
    // TODO: For a first pass, could just USE the spinlocks?
    // TODO: (take the lock on create, then try to take on get?)
    //while (!this->ready());
    if (!this->ready()) {
        pthread_mutex_lock(&m_acquires_lock);
        //m_acquires_lock.lock();

        if (!this->ready()) {
            assert(m_get == NULL);
            //m_get = new porr::acquire_info(porr::get_pedigree());
            m_get = __cilkrts_get_deque();
        }

        //m_acquires_lock.unlock();
        pthread_mutex_unlock(&m_acquires_lock);
        __cilkrts_suspend_deque();
    }
    assert(m_status==status::DONE);
    return m_result;
  }
}; // class future

/*
template<typename T>
class future<void> {
private:
  enum class status { 
    CREATED, // memory allocated, initialized
    DONE, // strand has finished execution
  };

  volatile status m_status;
  //volatile T m_result;
  
public:

 future<void>() {
    m_status = status::CREATED;
  };

  void put<void>(void) {
    assert(m_status != status::DONE);
    //m_result = result;
    m_status = status::DONE;
  };

  bool ready<void>() {
    // If the status is done, then the value is ready.
    return m_status==status::DONE;
  } 

  T get() {
    // TODO: Treat this the same way spin locks are handled.
    // TODO: For a first pass, could just USE the spinlocks?
    // TODO: (take the lock on create, then try to take on get?)
    while (!this->ready());
    assert(m_status==status::DONE);
    //return m_result;
  }
}; // class future
*/
} // namespace cilk

#endif // #ifndef __CILK__FUTURE_H__
