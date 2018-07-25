#ifndef __CILK__FUTURE_H__
#define __CILK__FUTURE_H__

#include <assert.h>
#include <functional>
#include <vector>
#include <internal/abi.h>
#include <pthread.h>

#define MAX_TOUCHES (10)

extern void __spawn_future_helper_helper(std::function<void*(void)>);

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

typedef struct touch_node_t {
    void* deque;
    touch_node_t *volatile next;
} touch_node_t;

template<typename T>
class future {
private:
  volatile T m_result;

  touch_node_t m_suspended_deques[MAX_TOUCHES];
  touch_node_t head = {
    .next = NULL
  };
  touch_node_t *volatile tail;
  volatile int m_num_suspended_deques;
  
public:

 future() {
    m_num_suspended_deques = 0;
    tail = &head;
  };

  ~future() {
  }

  inline void reset() {
    m_num_suspended_deques = 0;
  }

  void* __attribute__((always_inline)) put(T result) {
    //assert(m_status != status::DONE);
    //assert(m_num_suspended_deques >= 0);
    m_result = result;
    __asm__ volatile ("" ::: "memory");

    int num_deques = __atomic_fetch_add(&m_num_suspended_deques, INT32_MIN, __ATOMIC_SEQ_CST);

    void *ret = NULL;

    if (num_deques) {
        touch_node_t *node = &head;
        while (!node->next);
        node = node->next;
        num_deques--;
        ret = node->deque;
        while (num_deques) {
            while (!node->next);
            node = node->next;
            num_deques--;
            __cilkrts_make_resumable(node->deque);
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
            void *deque = __cilkrts_get_deque();
            int ticket = __atomic_fetch_add(&m_num_suspended_deques, 1, __ATOMIC_SEQ_CST);
            if (ticket >= 0) {
                m_suspended_deques[ticket].deque = deque;
                m_suspended_deques[ticket].next = NULL;
                head.next = &m_suspended_deques[ticket];
                //touch_node_t* prev = __atomic_exchange_n(&tail, &m_suspended_deques[ticket], __ATOMIC_SEQ_CST);
                //prev->next = &m_suspended_deques[ticket];

                // Memory barrier
                //__sync_synchronize();
                __cilkrts_suspend_deque();
            }
        }

    //assert(m_status==status::DONE);
    assert(m_num_suspended_deques < 0);
    return m_result;
  }
}; // class future

} // namespace cilk

#endif // #ifndef __CILK__FUTURE_H__
