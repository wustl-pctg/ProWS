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

// Currently, I implement this as a deque of deques. This is because I
// was initially concerned with allowing workers to 'steal' deques
// from each other in order to balance the number of suspended deques
// across workers. Even though we currently don't steal suspended
// deques, I'll keep it this way until it starts causing problems.
typedef struct deque_pool_s {

	deque *volatile *volatile tail;
	deque *volatile *volatile head;
	deque *volatile *volatile exc;
	deque *volatile * protected_tail;
	deque **ltq_limit;
	deque **ltq;
	//	deque *active; // Not stored in the deque of deques, like frame_ff
} deque_pool;

void deque_pool_init(deque_pool *p, size_t ltqsize);
void deque_pool_free(deque_pool *p);


#endif
