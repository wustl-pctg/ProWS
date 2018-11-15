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
  struct full_frame *frame_ff;
};

/** @} */

__CILKRTS_END_EXTERN_C

#endif // ! defined(INCLUDED_DEQUE_DOT_H)
