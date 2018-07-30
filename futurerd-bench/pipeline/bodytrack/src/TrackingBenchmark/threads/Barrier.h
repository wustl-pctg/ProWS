// (C) Copyright Christian Bienia 2007
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0.
//
//  file : Barrier.h
//  author : Christian Bienia - cbienia@cs.princeton.edu
//  description : A barrier

#ifndef BARRIER_H
#define BARRIER_H

#include <pthread.h>
#include <exception>

namespace threads {

//General barrier exception
class BarrierException: public std::exception {
  public:
    virtual const char *what() const throw() {return "Unspecified barrier error";}
};

//Barrier initialization error
class BarrierInitException: public BarrierException {
  public:
    virtual const char *what() const throw() {return "Unspecified error while initializing barrier";}
};

//Barrier destruction error
class BarrierDestroyException: public BarrierException {
  public:
    virtual const char *what() const throw() {return "Unspecified error while destroying barrier";}
};

//Resources exhausted
class BarrierResourceException: public BarrierException {
  public:
    virtual const char *what() const throw() {return "Insufficient resources";}
};

//Unknown error
class BarrierUnknownException: public BarrierException {
  public:
    virtual const char *what() const throw() {return "Unknown error";}
};

//A standard barrier
class Barrier {
  public:
    Barrier(int) throw(BarrierException);
    ~Barrier() throw(BarrierException);

    //Wait at a barrier, will return true for exactly one thread, false for all other threads
    bool Wait() throw(BarrierException);
    //Get number of threads required to enter the barrier
    const int nThreads() const;

  private:
    pthread_barrier_t b;
    int n;
};

} //namespace threads

#endif //BARRIER_H
