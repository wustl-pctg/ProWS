// (C) Copyright Christian Bienia 2007
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0.
//
//  file : Thread.h
//  author : Christian Bienia - cbienia@cs.princeton.edu
//  description : A C++ thread

#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>
#include <exception>

namespace threads {

//Abstract class which has to be implemented to make a class thread-capable
//The thread class constructor requires a threadable object to instantiate a thread object
class Runnable {
  public:
    virtual ~Runnable() {};
    //Thread objects will call the Run() method of its associated Runnable class
    virtual void Run() =0;
};

//Abstract class which has to be implemented if a thread requires extra steps to stop
//Join() will call the Stop() method of a stoppable class before it waits for the termination of Run()
class Stoppable {
  public:
    virtual ~Stoppable() {};
    //Join call the Stop() method
    virtual void Stop() =0;
};

//Exception which gets thrown if thread creation fails
class ThreadCreationException: public std::exception {
  public:
    virtual const char *what() const throw() {return "Error creating thread";}
};

//A thread
class Thread {
  private:
    Runnable &tobj;
    pthread_t t;

  public:
    Thread(Runnable &) throw(ThreadCreationException);

    //Wait until Thread object has finished
    void Join();
};

} //namespace threads

#endif //THREAD_H
