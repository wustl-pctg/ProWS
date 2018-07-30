#ifndef __UTIL_HPP__
#define __UTIL_HPP__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Ensure that we run serially
static void ensure_serial_execution(void) {
  // assert(1 == __cilkrts_get_nworkers());
  fprintf(stderr, "Forcing CILK_NWORKERS=1.\n");
  char *e = getenv("CILK_NWORKERS");
  if (!e || 0!=strcmp(e, "1")) {
    // fprintf(err_io, "Setting CILK_NWORKERS to be 1\n");
    if( setenv("CILK_NWORKERS", "1", 1) ) {
      fprintf(stderr, "Error setting CILK_NWORKERS to be 1\n");
      exit(1);
    }
  }
}

__attribute__((unused)) static
void gen_rand_string(char * s, int s_length, int range) {
  for(int i = 0; i < s_length; ++i ) {
    s[i] = (char)(rand() % range + 97);
  }
}


// @deprecated:
extern "C" {
void cilk_for_iteration_begin();
void cilk_for_iteration_end();
void cilk_for_begin();
void cilk_for_end();
}

#ifdef RACE_DETECT
#define CILKFOR_ITER_BEGIN cilk_for_iteration_begin()
#define CILKFOR_ITER_END cilk_for_iteration_end()
#define CILKFOR_BEGIN cilk_for_begin()
#define CILKFOR_END cilk_for_end()
#else
#define CILKFOR_ITER_BEGIN
#define CILKFOR_ITER_END
#define CILKFOR_BEGIN
#define CILKFOR_END
#endif

#endif // __UTIL_HPP__
