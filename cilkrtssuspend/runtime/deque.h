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
#include <internal/abi.h>

__CILKRTS_BEGIN_EXTERN_C

typedef struct deque deque;

struct deque
{
  __cilkrts_stack_frame *volatile *volatile tail;
  __cilkrts_stack_frame *volatile *volatile head;
  __cilkrts_stack_frame *volatile *volatile exc;
  __cilkrts_stack_frame *volatile *volatile protected_tail;
  __cilkrts_stack_frame *volatile *ltq_limit;
	__cilkrts_stack_frame ** ltq;
  struct full_frame *frame_ff;
	__cilkrts_pedigree saved_ped;
	cilk_fiber *resumeable_fiber;
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

void deque_init(deque *d, size_t ltqsize);
void deque_switch(__cilkrts_worker *w, deque *d);

// int deque_add(__cilkrts_worker *w);
//void deque_switch(__cilkrts_worker *w, int n);

/** __cilkrts_c_THE_exception_check should probably be here, too. */

/** @} */

__CILKRTS_END_EXTERN_C

#endif // ! defined(INCLUDED_DEQUE_DOT_H)
