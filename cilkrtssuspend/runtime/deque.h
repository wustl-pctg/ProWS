/* deque.h                    -*-C++-*-
 * @file deque.h
 * @brief A deque is a double-ended queue containing continuations to
 * be stolen. This struct really just holds the head, tail, exception,
 * and protected_tail pointers.
 * @{
 */

#ifndef INCLUDED_DEQUE_DOT_H
#define INCLUDED_DEQUE_DOT_H

#include "rts-common.h"
#include "cilk_fiber.h"
#include "pedigrees.h"
#include "worker_mutex.h"
#include <internal/abi.h>

__CILKRTS_BEGIN_EXTERN_C

#define SUSDEQUE_DEBUG 0
#define DEQUE_DEBUG 0

#if DEQUE_DEBUG > 0
#define DEQUE_LOG(args...) fprintf(stderr, args)
#else
#define DEQUE_LOG(args...)
#endif

typedef struct deque deque;

#define ACTIVE_DEQUE_INDEX -1
#define INVALID_DEQUE_INDEX -2

struct deque
{
  __cilkrts_stack_frame *volatile *volatile tail;
  __cilkrts_stack_frame *volatile *volatile head;
  __cilkrts_stack_frame *volatile *volatile exc;
  __cilkrts_stack_frame *volatile *volatile protected_tail;
  __cilkrts_stack_frame *volatile *ltq_limit;
  __cilkrts_stack_frame ** ltq;

  cilk_fiber *volatile *volatile fiber_tail;
  cilk_fiber *volatile *volatile fiber_head;
  cilk_fiber *volatile *volatile fiber_protected_tail;
  cilk_fiber *volatile *fiber_ltq_limit;
  cilk_fiber ** fiber_ltq;
  
  struct full_frame *frame_ff;
  int resumable;
  __cilkrts_worker *team;
  
  cilk_fiber *volatile fiber;
  __cilkrts_pedigree saved_ped;
  __cilkrts_stack_frame *call_stack;

  __cilkrts_deque_link link;

  // As long as we allow suspended deques to change which deque_pool
  // they are in, I don't see how to get away with not having these
  // pointers back to a deque's location in a deque pool. This is
  // rather error-prone and inelegant...
  // If we don't allow entire suspended deques to be stolen, then I think we can do without this...
  __cilkrts_worker volatile* worker;
  int self; // index into worker's deque pool
};

void increment_E(__cilkrts_worker *victim, deque* d);
void decrement_E(__cilkrts_worker *victim, deque* d);
void reset_THE_exception(__cilkrts_worker *w);
int can_steal_from(__cilkrts_worker *victim, deque *d);
int dekker_protocol(__cilkrts_worker *victim, deque *d);
void detach_for_steal(__cilkrts_worker *w,
                      __cilkrts_worker *victim,
                      deque *d, cilk_fiber* fiber);
void __cilkrts_promote_own_deque(__cilkrts_worker *w);

int can_take_fiber_from(deque *d);
int fiber_dekker_protocol(__cilkrts_worker *victim, deque *d);

int deque_init(deque *d, size_t ltqsize);
void deque_destroy(deque *d);
void deque_switch(__cilkrts_worker *w, deque *d);
cilk_fiber* deque_suspend(__cilkrts_worker *w, deque *new_deque);
void deque_mug(__cilkrts_worker *w, deque *d);

/** __cilkrts_c_THE_exception_check should probably be here, too. */

/** @} */

__CILKRTS_END_EXTERN_C

#endif // ! defined(INCLUDED_DEQUE_DOT_H)
