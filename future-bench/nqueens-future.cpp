#ifndef TIMING_COUNT
#define TIMING_COUNT 0
#endif

#if TIMING_COUNT
#include "ktiming.h"
#endif
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cilk/cilk.h>
#include "internal/abi.h"
#include "future.hpp"

class cilk_fiber;

char* __cilkrts_switch_fibers();
void __cilkrts_switch_fibers_back(cilk_fiber*);

void __cilkrts_leave_future_frame(__cilkrts_stack_frame*);

extern "C" {
cilk_fiber* cilk_fiber_get_current_fiber();
void** cilk_fiber_get_resume_jmpbuf(cilk_fiber*);
void cilk_fiber_do_post_switch_actions(cilk_fiber*);
void __cilkrts_detach(__cilkrts_stack_frame*);
void __cilkrts_pop_frame(__cilkrts_stack_frame*);
}

// int * count;

/* 
 * nqueen  4 = 2 
 * nqueen  5 = 10 
 * nqueen  6 = 4 
 * nqueen  7 = 40 
 * nqueen  8 = 92 
 * nqueen  9 = 352 
 * nqueen 10 = 724
 * nqueen 11 = 2680 
 * nqueen 12 = 14200 
 * nqueen 13 = 73712 
 * nqueen 14 = 365596 
 * nqueen 15 = 2279184 
 */

/*
 * <a> contains array of <n> queen positions.  Returns 1
 * if none of the queens conflict, and returns 0 otherwise.
 */
int ok (int n, char *a) {

  int i, j;
  char p, q;

  for (i = 0; i < n; i++) {
    p = a[i];
    for (j = i + 1; j < n; j++) {
      q = a[j];
      if (q == p || q == p - (j - i) || q == p + (j - i))
        return 0;
    }
  }

  return 1;
}

int nqueens (int n, int j, char *a);

int nqueens_iter_body(int j, char *a, int i, int n) {
    char *b = (char *) alloca((j + 1) * sizeof (char));
    memcpy(b, a, j * sizeof (char));
    b[j] = i;

    if(ok (j + 1, b)) {
        return nqueens(n, j+1, b);
    } else {
        return 0;
    }
}

void __attribute__((noinline)) nqueens_iter_body_helper(cilk::future<int> *fut, int j, char *a, int i, int n) {
  __cilkrts_stack_frame sf;
  __cilkrts_enter_frame_fast_1(&sf);
  __cilkrts_detach(&sf);

  void *__cilkrts_deque = fut->put(nqueens_iter_body(j, a, i, n));
  if (__cilkrts_deque) __cilkrts_resume_suspended(__cilkrts_deque, 2);

  __cilkrts_pop_frame(&sf);
  __cilkrts_leave_future_frame(&sf);
}

int nqueens (int n, int j, char *a) {

  int i;
  cilk::future<int> *count;
  int solNum = 0;

  if (n == j) {
    return 1;
  }

  __cilkrts_stack_frame sf;
  __cilkrts_enter_frame_1(&sf);

  count = (cilk::future<int> *) alloca(n * sizeof(cilk::future<int>));
  //(void) memset(count, 0, n * sizeof (int));

  for (i = 0; i < n; i++) {

    new (&count[i]) cilk::future<int>();
    cilk_fiber *initial_fiber = cilk_fiber_get_current_fiber();
    sf.flags |= CILK_FRAME_FUTURE_PARENT;
    if (!CILK_SETJMP(cilk_fiber_get_resume_jmpbuf(initial_fiber))) {
      char *new_sp = __cilkrts_switch_fibers();
      char *old_sp = NULL;

      __asm__ volatile ("mov %%rsp, %0" : "=r" (old_sp));
      __asm__ volatile ("mov %0, %%rsp" : : "r" (new_sp));

      nqueens_iter_body_helper(&count[i], j, a, i, n);

      __asm__ volatile ("mov %0, %%rsp" : : "r" (old_sp));
      __cilkrts_switch_fibers_back(initial_fiber);
    }
    cilk_fiber_do_post_switch_actions(initial_fiber);
    sf.flags &= ~(CILK_FRAME_FUTURE_PARENT);
  }

  for(i = 0; i < n; i++) {
    solNum += count[i].get();
  }

  __cilkrts_pop_frame(&sf);
  __cilkrts_leave_frame(&sf);

  return solNum;
}


int main(int argc, char *argv[]) { 

  int n = 13;
  char *a;
  int res;

  if (argc < 2) {
    fprintf (stderr, "Usage: %s [<cilk-options>] <n>\n", argv[0]);
    fprintf (stderr, "Use default board size, n = 13.\n");

  } else {
    n = atoi (argv[1]);
    printf ("Running %s with n = %d.\n", argv[0], n);
  }

  a = (char *) alloca (n * sizeof (char));
  res = 0;

#if TIMING_COUNT
  clockmark_t begin, end;
  uint64_t elapsed[TIMING_COUNT];

  for(int i=0; i < TIMING_COUNT; i++) {
    begin = ktiming_getmark();
    res = nqueens(n, 0, a);
    end = ktiming_getmark();
    elapsed[i] = ktiming_diff_usec(&begin, &end);
  }
  print_runtime(elapsed, TIMING_COUNT);
#else
  res = nqueens(n, 0, a);
#endif

  if (res == 0) {
    printf ("No solution found.\n");
  } else {
    printf ("Total number of solutions : %d\n", res);
  }

  return 0;
}
