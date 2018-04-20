#ifndef INCLUDED_DEQUE_POOL_DOT_H
#define INCLUDED_DEQUE_POOL_DOT_H

#include <stdlib.h>

#include "deque.h"
#include "cilk_fiber.h"

// This is just a simple vector
typedef struct deque_pool_s {
	deque ** array;
	volatile size_t size;
	size_t capacity;
} deque_pool;

void deque_pool_init(deque_pool *p, size_t ltqsize);
void deque_pool_free(deque_pool *p);
void deque_pool_add(__cilkrts_worker *victim, deque_pool *p, deque *d);
void deque_pool_remove(deque_pool *p, deque *d);
void deque_pool_validate(deque_pool *p, __cilkrts_worker *w);

#endif
