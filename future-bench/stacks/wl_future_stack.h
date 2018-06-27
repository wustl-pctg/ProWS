#ifndef __WL_FUTURE_STACK_H__
#define __WL_FUTURE_STACK_H__

#include "../../src/future.h"
#include <unordered_map>
#include <stack>
#include <sys/types.h>

template T
class WLFutureStack {
    private:
        static const unsigned int DEFAULT_SLACK = 10;
        unsigned int mSlack;
        cilk::future<T>* mLastAccess;
        // TODO: I believe this should be two stacks of futures (push & pop)
        std::unordered_map<pid_t,std::stack> mThreadLocalStacks;
        std::stack<T> mStack;
        pthread_mutex_t mThreadLocalStacksLock;
        
        bool internalPush(T key, bool cleanFuture);
        T internalPop();

    public:
        WLFutureStack(unsigned int slack = WLFutureStack::DEFAULT_SLACK);
        ~WLFutureStack();
        
        cilk::future<bool>* push(T key, bool cleanFuture=false);
        cilk::future<T>* pop();
        unsigned int getSlack() { return mSlack; };
        void setSlack(unsigned int slack) { mSlack = slack; };
};

#endif
