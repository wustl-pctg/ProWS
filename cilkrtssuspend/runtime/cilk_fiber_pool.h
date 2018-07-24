#ifndef INCLUDED_CILK_FIBER_POOL_DOT_H
#define INCLUDED_CILK_FIBER_POOL_DOT_H

#include <cilk/common.h>
#ifdef __cplusplus
#   include <cstddef>
#else
#   include <stddef.h>
#endif
#include "cilk_fiber.h"

__CILKRTS_BEGIN_EXTERN_C

/** @brief Pool of cilk_fiber for fiber reuse
 *
 * Pools form a hierarchy, with each pool pointing to its parent.  When the
 * pool undeflows, it gets a fiber from its parent.  When a pool overflows,
 * it returns some fibers to its parent.  If the root pool underflows, it
 * allocates and initializes a new fiber from the heap but only if the total
 * is less than max_size; otherwise, fiber creation fails.
 */
typedef struct cilk_fiber_pool
{
	spin_mutex*      lock;       ///< Mutual exclusion for pool operations 
	__STDNS size_t   stack_size; ///< Size of stacks for fibers in this pool.
	struct cilk_fiber_pool* parent;     ///< @brief Parent pool.
	///< If this pool is empty, get from parent 

	// Describes inactive fibers stored in the pool.
	cilk_fiber**     fibers;     ///< Array of max_size fiber pointers 
	unsigned         max_size;   ///< Limit on number of fibers in pool 
	unsigned         size;       ///< Number of fibers currently in the pool

	// Statistics on active fibers that were allocated from this pool,
	// but no longer in the pool.
	int              total;      ///< @brief Fibers allocated - fiber deallocated from pool
	///< total may be negative for non-root pools.
	int              high_water; ///< High water mark of total fibers
	int              alloc_max;  ///< Limit on number of fibers allocated from the heap/OS
} cilk_fiber_pool;

int __attribute__((always_inline)) cilk_fiber_pool_sanity_check(cilk_fiber_pool *pool, const char* desc);

void __attribute__((always_inline)) increment_pool_total(cilk_fiber_pool* pool);
void __attribute__((always_inline)) decrement_pool_total(cilk_fiber_pool* pool, int fibers_freed);

void cilk_fiber_pool_move_fibers_to_parent_pool(cilk_fiber_pool* pool, unsigned num_to_keep);

void cilk_fiber_pool_free_fibers_from_pool(cilk_fiber_pool* pool,
                                           unsigned num_to_keep,
                                           cilk_fiber* fiber_to_return);

__CILKRTS_END_EXTERN_C

#endif
