// (C) Copyright Christian Bienia 2007
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0.
//
//  file : Condition.cpp
//  author : Christian Bienia - cbienia@cs.princeton.edu
//  description : A condition variable

# include <pthread.h>
# include <errno.h>

#include <exception>

#include "Mutex.h"
#include "Condition.h"


namespace threads {

Condition::Condition(Mutex &_M) throw(CondException) {
  int rv;

  M = &_M;

  nWaiting = 0;
  nWakeupTickets = 0;

  rv = pthread_cond_init(&c, NULL);

  switch(rv) {
    case 0:
      //no error
      break;
    case EAGAIN:
    case ENOMEM:
    {
      CondResourceException e;
      throw e;
      break;
    }
    case EBUSY:
    case EINVAL:
    {
      CondInitException e;
      throw e;
      break;
    }
    default:
    {
      CondUnknownException e;
      throw e;
      break;
    }
  }
}

Condition::~Condition() throw(CondException) {
  int rv;

  rv = pthread_cond_destroy(&c);

  switch(rv) {
    case 0:
      //no error
      break;
    case EBUSY:
    case EINVAL:
    {
      CondDestroyException e;
      throw e;
      break;
    }
    default:
    {
      CondUnknownException e;
      throw e;
      break;
    }
  }
}

//Wake up exactly one thread, return number of threads currently waiting (before wakeup)
//If no more threads are waiting, the notification is lost
int Condition::NotifyOne() throw(CondException) {
  int slack;
  int rv;

  slack = nWaiting - nWakeupTickets;
  if(slack > 0) {
    nWakeupTickets++;
    rv = pthread_cond_signal(&c);

    switch(rv) {
      case 0:
        //no error
        break;
      case EINVAL:
      {
        CondException e;
        throw e;
        break;
      }
      default:
      {
        CondUnknownException e;
        throw e;
        break;
      }
    }
  }

  return slack;
}

//Wake up all threads, return number of threads currently waiting (before wakeup)
int Condition::NotifyAll() throw(CondException) {
  int slack;
  int rv;

  slack = nWaiting - nWakeupTickets;
  if(slack > 0) {
    nWakeupTickets = nWaiting;
    rv = pthread_cond_broadcast(&c);

    switch(rv) {
      case 0:
        //no error
        break;
      case EINVAL:
      {
        CondException e;
        throw e;
        break;
      }
      default:
      {
        CondUnknownException e;
        throw e;
        break;
      }
    }
  }

  return slack;
}

//Wait until either NotifyOne() or NotifyAll() is executed
void Condition::Wait() throw(CondException, MutexException) {
  int rv;

  nWaiting++;

  //nWakeupTickets protects against spurious wakeups
  while(nWakeupTickets == 0) {
    rv = pthread_cond_wait(&c, &(M->m));

    switch(rv) {
      case 0:
        //no error
        break;
      case EINVAL:
      {
        CondException e;
        throw e;
        break;
      }
      case EPERM:
      {
        MutexLockingException e;
        throw e;
        break;
      }
      default:
      {
        CondUnknownException e;
        throw e;
        break;
      }
    }
  }

  nWakeupTickets--;
  nWaiting--;
}

} //namespace threads
