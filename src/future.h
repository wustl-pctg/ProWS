#ifndef __CILK__FUTURE_H__
#define __CILK__FUTURE_H__

#include <assert.h>
#include "cilk/cilk.h"
#include <functional>

//#ifndef cilk_spawn_future
//  #define cilk_spawn_future cilk_spawn
//#endif

extern "C" {

extern void __spawn_future_helper_helper(std::function<void(void)> func);

}
//__attribute__((weak)) void __cilk_spawn_future(std::function<void()> func) {
//  std::cout << "Using the dummy future spawn!!! cilk_sync will wait for it to complete :(" << std::endl;
//  cilk_spawn func();
//}

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
  
public:

 future() {
    m_status = status::CREATED;
    printf("Called future constructor!\n");
  };

  void put(T result) {
    assert(m_status != status::DONE);
    m_result = result;
    m_status = status::DONE;
  };

  bool ready() {
    // If the status is done, then the value is ready.
    return m_status==status::DONE;
  } 

  T get() {
    // TODO: Treat this the same way spin locks are handled.
    // TODO: For a first pass, could just USE the spinlocks?
    // TODO: (take the lock on create, then try to take on get?)
    while (!this->ready());
    assert(m_status==status::DONE);
    return m_result;
  }
}; // class future

} // namespace cilk

#endif // #ifndef __CILK__FUTURE_H__
