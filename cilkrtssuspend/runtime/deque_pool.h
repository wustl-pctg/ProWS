#ifndef INCLUDED_DEQUE_POOL_DOT_H
#define INCLUDED_DEQUE_POOL_DOT_H

#include <stdlib.h>

#include "deque.h"
#include "cilk_fiber.h"

/* struct pentry { */
/* 	size_t pos; // position in conceptual pool of deques */
/* 	deque *d; */
/* 	struct pentry *next; */
/* }; */

/* typedef struct deque_pool_s { */
/* 	size_t count; */
/* 	struct pentry[]; */
/* } deque_pool; */

/* struct deque */
/* { */
/*   __cilkrts_stack_frame *volatile *volatile tail; */
/*   __cilkrts_stack_frame *volatile *volatile head; */
/*   __cilkrts_stack_frame *volatile *volatile exc; */
/*   __cilkrts_stack_frame *volatile *volatile protected_tail; */
/*   __cilkrts_stack_frame *volatile *ltq_limit; */
/* 	__cilkrts_stack_frame ** ltq; */
/*   struct full_frame *frame_ff; */
/* }; */

// I'm calling this a deque pool, but really it's a deque of deques.
typedef struct deque_pool_s {

	deque *volatile *volatile tail;
	deque *volatile *volatile head;
	deque *volatile *volatile exc;
	deque *volatile * protected_tail;
	deque **ltq_limit;
	deque **ltq;
	//	deque *active; // Not stored in the deque of deques, like frame_ff
} deque_pool;

cilk_fiber* deque_pool_suspend(__cilkrts_worker *w);
void deque_pool_init(deque_pool *p, size_t ltqsize);
void deque_pool_free(deque_pool *p);


#endif
