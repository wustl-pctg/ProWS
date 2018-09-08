#ifndef __CILK__FUTURE_H__
#define __CILK__FUTURE_H__

#include <assert.h>
#include <functional>
#include <vector>
#include <internal/abi.h>
#include <pthread.h>
#include "handcomp-macros.h"

extern void __spawn_future_helper_helper(std::function<void*(void)>);

extern "C" {
void __cilkrts_insert_deque_into_list(__cilkrts_deque_link *volatile *list);
}

namespace cilk {

#define reuse_future(T,fut, loc,func,args...)  \
  { \
  auto functor = std::bind(func, ##args);  \
  fut = new (loc) cilk::future<T>();  \
  auto __temp_fut = fut; \
  __spawn_future_helper_helper([__temp_fut,functor]() -> void* { \
    return __temp_fut->put(functor()); \
  }); \
  }

#define reuse_future_inplace(T,loc,func,args...)  \
  { \
  auto functor = std::bind(func, ##args);  \
  new (loc) cilk::future<T>();  \
  auto __temp_fut = loc; \
  __spawn_future_helper_helper([__temp_fut,functor]() -> void* { \
    return __temp_fut->put(functor()); \
  }); \
  }

#define use_future_inplace(T,fut,func,args...)  \
  { \
  auto functor = std::bind(func, ##args);  \
  auto __temp_fut = fut; \
  __spawn_future_helper_helper([__temp_fut,functor]() -> void* { \
    return __temp_fut->put(functor()); \
  }); \
  }


#define cilk_future_get(fut)              (fut)->get()

#define cilk_future_create(T,fut,func,args...) \
  { \
  auto functor = std::bind(func, ##args);  \
  fut = new cilk::future<T>();  \
  auto __temp_fut = fut; \
  __spawn_future_helper_helper([__temp_fut,functor]() -> void* { \
    return __temp_fut->put(functor()); \
  }); \
  }


#define cilk_future_create__stack(T,fut,func,args...)\
  cilk::future<T> fut;\
  { \
    auto functor = std::bind(func, ##args); \
    __spawn_future_helper_helper([&fut,functor]() -> void* { \
      return fut.put(functor()); \
    }); \
  }

template<typename T>
class future {
private:
  volatile T m_result;

  __cilkrts_deque_link head = {
    .d = NULL, .next = NULL
  };
  __cilkrts_deque_link *volatile tail = &head;
  volatile int m_num_suspended_deques;

  void __attribute__((always_inline)) suspend_deque() {
    int ticket = __atomic_fetch_add(&m_num_suspended_deques, 1, __ATOMIC_SEQ_CST);
    if (ticket >= 0) {
        __cilkrts_insert_deque_into_list(&tail);
        __asm__ volatile ("" ::: "memory");
        __cilkrts_suspend_deque();
    }
  }
  
public:

 future() {
    m_num_suspended_deques = 0;
    tail = &head;
  };

  ~future() {
  }

  inline void reset() {
    m_num_suspended_deques = 0;
    tail = &head;
    head.next = NULL;
  }

  void* __attribute__((always_inline)) put(T result) {
    m_result = result;
    __asm__ volatile ("" ::: "memory");

    int num_deques = __atomic_fetch_add(&m_num_suspended_deques, INT32_MIN, __ATOMIC_SEQ_CST);

    void *ret = NULL;

    if (num_deques > 0) {
        __cilkrts_deque_link *node = &head;
        while (!node->next);
        node = node->next;
        num_deques--;
        ret = node->d;
        while (num_deques) {
            while (!node->next);
            node = node->next;
            num_deques--;
            __cilkrts_make_resumable(node->d);
        }
    }

    return ret;
  };

  bool __attribute__((always_inline)) ready() {
    // If the put has replaced the value with INT32_MIN,
    // then the value is ready.
    return (m_num_suspended_deques < 0);
  } 

  T __attribute__((always_inline)) get() {
    if (!this->ready()) {
      suspend_deque();
    }

    assert(ready());
    return m_result;
  }
}; // class future

// future<void> specialization
// TODO: Try to get rid of all the code duplication
template<>
class future<void> {
private:
  __cilkrts_deque_link head = {
    .d = NULL,
    .next = NULL
  };
  __cilkrts_deque_link *volatile tail = &head;
  volatile int m_num_suspended_deques;

  void __attribute__((always_inline)) suspend_deque() {
    int ticket = __atomic_fetch_add(&m_num_suspended_deques, 1, __ATOMIC_SEQ_CST);
    if (ticket >= 0) {
        __cilkrts_insert_deque_into_list(&tail);
        __asm__ volatile ("" ::: "memory");
        __cilkrts_suspend_deque();
    }
  }
  
public:

 future() {
    m_num_suspended_deques = 0;
    tail = &head;
  };

  ~future() {
  }

  inline void reset() {
    m_num_suspended_deques = 0;
    tail = &head;
    head.next = NULL;
  }

  void* __attribute__((always_inline)) put(void) {
    int num_deques = __atomic_fetch_add(&m_num_suspended_deques, INT32_MIN, __ATOMIC_SEQ_CST);
    __asm__ volatile ("" ::: "memory");

    void *ret = NULL;

    if (num_deques > 0) {
        __cilkrts_deque_link *node = &head;
        while (!node->next);
        node = node->next;
        num_deques--;
        ret = node->d;
        while (num_deques) {
            while (!node->next);
            node = node->next;
            num_deques--;
            __cilkrts_make_resumable(node->d);
        }
    }

    return ret;
  };

  bool __attribute__((always_inline)) ready() {
    // If the put has replaced the value with INT32_MIN,
    // then the value is ready.
    return (m_num_suspended_deques < 0);
  } 

  void __attribute__((always_inline)) get() {
    if (!this->ready()) {
      suspend_deque();
    }

    assert(ready());
  }
}; // class future<void>

} // namespace cilk

#endif // #ifndef __CILK__FUTURE_H__
