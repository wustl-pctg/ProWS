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

template<typename T>
class future {
private:
  enum class status { 
    CREATED, // memory allocated, initialized
    DONE, // strand has finished execution
  };

  typedef struct __touch_node {
    struct __touch_node *next;
    void *deque;
  } __touch_node;

  volatile status m_status;
  volatile T m_result;

  pthread_mutex_t m_acquires_lock;
  __touch_node *m_gets;
  
public:

 future() {
    m_status = status::CREATED;
    m_gets = new __touch_node();
    m_gets->next = NULL;
    m_gets->deque = NULL;
    pthread_mutex_init(&m_acquires_lock, NULL);
  };

  ~future() {
    pthread_mutex_destroy(&m_acquires_lock);
    delete m_gets;
    m_gets = NULL;
  }

  void* put(T result) {
    assert(m_status != status::DONE);
    m_result = result;
    m_status = status::DONE;

    // Make sure no worker is in the middle of
    // suspending its own deque before proceeding.
    pthread_mutex_lock(&m_acquires_lock);
    pthread_mutex_unlock(&m_acquires_lock);

    // Resume all but the last deque from here;
    // The last deque is returned and resumed from
    // outside the future class.
    while (m_gets->next && m_gets->next->next) {

        __touch_node *node = m_gets;

        void *deque = node->deque;
        assert(deque);

        __cilkrts_resume_suspended(deque, 1); 

        assert(m_gets != node->next);
        m_gets = node->next;

        assert(m_gets);

        delete node;
    }

    void *ret = m_gets->deque;
    if (m_gets->next) {
        __touch_node *node = m_gets;
        m_gets = m_gets->next;
        delete node;
    }
    return ret;
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
            void *deque = __cilkrts_get_deque();
            assert(deque);
            __touch_node *touch = new __touch_node();
            assert(touch);
            touch->deque = deque;
            assert(touch->deque);
            assert(m_gets);
            touch->next = m_gets;
            m_gets = touch;
            pthread_mutex_unlock(&m_acquires_lock);
            __cilkrts_suspend_deque();
        } else {
            pthread_mutex_unlock(&m_acquires_lock);
        }

    }
    assert(m_status==status::DONE);
    return m_result;
  }
}; // class future

} // namespace cilk

#endif // #ifndef __CILK__FUTURE_H__
