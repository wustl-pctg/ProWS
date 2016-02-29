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
	struct mutex lock;
	
	__cilkrts_pedigree saved_ped;

	// This is set ONLY when the fiber for this deque should be resumed
	cilk_fiber *resumeable_fiber;

	// As long as we allow deques to change which deque_pool they are
	// in, I don't see how to get away with not having these pointers
	// back to a deque's location in a deque pool. This is rather
	// error-prone and inelegant...
	__cilkrts_worker *worker;
	deque **self;
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
