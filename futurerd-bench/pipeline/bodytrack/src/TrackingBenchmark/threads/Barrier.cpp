// (C) Copyright Christian Bienia 2007
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0.
//
//  file : Barrier.cpp
//  author : Christian Bienia - cbienia@cs.princeton.edu
//  description : A barrier

# include <pthread.h>
# include <errno.h>

#include <exception>

#include "Barrier.h"


namespace threads {

Barrier::Barrier(int _n) throw(BarrierException) {
  int rv;

  n = _n;
  rv = pthread_barrier_init(&b, NULL, n);

  switch(rv) {
    case 0:
      break;
    case EINVAL:
    case EBUSY:
    {
      BarrierInitException e;
      throw e;
      break;
    }
    case EAGAIN:
    case ENOMEM:
    {
      BarrierResourceException e;
      throw e;
      break;
    }
    default:
    {
      BarrierUnknownException e;
      throw e;
      break;
    }
  }
}

Barrier::~Barrier() throw(BarrierException) {
  int rv;

  rv = pthread_barrier_destroy(&b);

  switch(rv) {
    case 0:
      break;
    case EINVAL:
    case EBUSY:
    {
      BarrierDestroyException e;
      throw e;
      break;
    }
    default:
    {
      BarrierUnknownException e;
      throw e;
      break;
    }
  }
}

//Wait at a barrier
bool Barrier::Wait() throw(BarrierException) {
  int rv;

  rv = pthread_barrier_wait(&b);

  switch(rv) {
    case 0:
      break;
    case PTHREAD_BARRIER_SERIAL_THREAD:
      return true;
      break;
    case EINVAL:
    {
      BarrierException e;
      throw e;
      break;
    }
    default:
    {
      BarrierUnknownException e;
      throw e;
      break;
    }
  }

  return false;
}

const int Barrier::nThreads() const {
  return n;
}

};
