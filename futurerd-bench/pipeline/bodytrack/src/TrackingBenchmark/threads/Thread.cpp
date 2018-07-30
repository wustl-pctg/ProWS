// (C) Copyright Christian Bienia 2007
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0.
//
//  file : Thread.cpp
//  author : Christian Bienia - cbienia@cs.princeton.edu
//  description : A C++ thread

#include <pthread.h>

#include <typeinfo>
#include "Thread.h"


namespace threads {

// Unfortunately, thread libraries such as pthreads which use the C
// calling convention are incompatible with C++ member functions.
// To provide an object-oriented thread interface despite this obstacle,
// we make use of a helper function which will wrap the member function.
extern "C" {
  static void *thread_entry(void *arg) {
    Runnable *tobj = static_cast<Runnable *>(arg);
    tobj->Run();

    return NULL;
  }
}

//Constructor, expects a threadable object as argument
Thread::Thread(Runnable &_tobj) throw(ThreadCreationException) : tobj(_tobj) {
  if(pthread_create(&t, NULL, &thread_entry, (void *)&tobj)) {
    ThreadCreationException e;
    throw e;
  }
}

//Wait until Thread object has finished
void Thread::Join() {
  Stoppable *_tobj;
  bool isStoppable = true;

  //call Stop() function if implemented
  try {
    _tobj = &dynamic_cast<Stoppable &>(tobj);
  } catch(std::bad_cast e) {
    isStoppable = false;
  }
  if(isStoppable) {
    _tobj->Stop();
  }

  pthread_join(t, NULL);
}

} //namespace threads
