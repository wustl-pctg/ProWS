#include "wl_future_stack.h"
#include "../../src/future.h"
#include <sys/types.h>

template T
WLFutureStack<T>::WLFutureStack(unsigned int slack) {
    mSlack = slack;
    mLastAccess = NULL;
    mThreadLocalStacks.emplace(gettid(), std::stack());
    pthread_mutex_init(&mThreadLocalStacksLock, NULL);
}

template T
WLFutureStack<T>::~WLFutureStack() {
    if (mLastAccess != NULL) {
        mLastAccess->get();
        delete mLastAccess;
        mLastAccess = NULL;
    }
    for (auto each : mThreadLocalStacks) {
        each.second.clear();
    }
    mThreadLocalStacks.clear();
    pthread_mutex_destroy(&mThreadLocalStacksLock);
}

template T
bool WLFutureStack<T>::internalPush(T key, bool cleanFuture) {
    return true;
}

template T
cilk::future<bool>* WLFutureStack<T>::push(T key, bool cleanFuture) {
    cilk::future<bool> *futureHandle;

    auto mLocalStackIt = mThreadLocalStacks.find(gettid());

    if (__builtin_expect(mLocalStackIt == mThreadLocalStacks.end(), 0)) {
        pthread_mutex_lock(&mThreadLocalStacksLock);
        mLocalStackIt = mThreadLocalStacks.insert(gettid());
        pthread_mutex_unlock(&mThreadLocalStacksLock);
    }

    std::stack &localStack = mLocalStackIt.second;

    if (mLocalStackIt.second.size() == (mSlack-1)) {
        cilk_future_create(bool, futureHandle, WLFutureStack::internalPush, this, key, cleanFuture);
    } else {
        mLocalStackIt.second.push(key);
        // Return a dummy future handle
        futureHandle = new cilk::future<bool>();
        futureHandle->put(true);
    }
    return futureHandle;
}

template T
cilk::future<T>* WLFutureStack<T>::pop() {
    cilk::future<T> *futureHandle;

    auto mLocalStackIt = mThreadLocalStacks.find(gettid());

    // If we don't have a thread local stack yet (we haven't produced anything),
    // Or if our local stack is empty, launch a future to go to the shared memory
    if (mLocalStackIt == mThreadLocalStacks.end() || mLocalSlackIt.second.empty()) {
        cilk_future_create(bool, futureHandle, WLFutureStack::internalPop, this);
    } else {
        // We can speed things up by just taking a value from the thread local stack
        futureHandle = new cilk::future<T>();
        futureHandle->put(mLocalStackIt.second.pop());
    }

    return futureHandle;
}
