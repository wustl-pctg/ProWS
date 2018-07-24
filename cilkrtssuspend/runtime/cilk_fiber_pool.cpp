#include "cilk_fiber.h"
#include "cilk_fiber_pool.h"

extern "C" {

  int cilk_fiber_pool_sanity_check(cilk_fiber_pool *pool, const char* desc)
  {
    int errors = 0;
#if FIBER_DEBUG >= 1    
    if ((NULL != pool) && pool->total > 0) {

      // Root pool should not allocate more fibers than alloc_max
      errors += ((pool->parent == NULL) &&
                 (pool->total > pool->alloc_max));
      errors += (pool->total > pool->high_water);

      if (errors) {
        fprintf(stderr, "ERROR at %s: pool=%p has max_size=%u, total=%d, high_water=%d\n",
                desc,
                pool, pool->max_size, pool->total, pool->high_water);
      }
    }
#endif
    return (errors == 0);
  }

  void increment_pool_total(cilk_fiber_pool* pool)
  {
    ++pool->total;
    if (pool->high_water < pool->total)
      pool->high_water = pool->total;
  }

  void decrement_pool_total(cilk_fiber_pool* pool, int fibers_freed)
  {
    pool->total -= fibers_freed;
  }

  /**
   * @brief Free fibers from this pool until we have at most @c
   * num_to_keep fibers remaining, and then put a fiber back.
   *
   * @pre   We do not hold @c pool->lock 
   * @post  After completion, we do not hold @c pool->lock
   */
  void cilk_fiber_pool_free_fibers_from_pool(cilk_fiber_pool* pool,
                                             unsigned num_to_keep,
                                             cilk_fiber* fiber_to_return)
  {
    // Free our own fibers, until we fall below our desired threshold.
    // Each iteration of this loop proceeds in the following stages:
    //   1.  Acquire the pool lock,
    //   2.  Grabs up to B fibers from the pool, stores them into a buffer.
    //   3.  Check if pool is empty enough.  If yes, put the last fiber back,
    //       and remember that we should quit.
    //   4.  Release the pool lock, and actually free any buffered fibers.
    //   5.  Check if we are done and should exit the loop.  Otherwise, try again.
    // 
    const bool need_lock = pool->lock;
    bool last_fiber_returned = false;
    
    do {
      const int B = 10;   // Pull at most this many fibers from the
      // parent for one lock acquisition.  Make
      // this value large enough to amortize
      // against the cost of acquiring and
      // releasing the lock.
      int num_to_free = 0;
      cilk_fiber* fibers_to_free[B];

      // Stage 1: Grab the lock.
      if (need_lock) {
        spin_mutex_lock(pool->lock);
      }
        
      // Stage 2: Grab up to B fibers to free.
      int fibers_freed = 0;
      while ((pool->size > num_to_keep) && (num_to_free < B)) {
        fibers_to_free[num_to_free++] = pool->fibers[--pool->size];
        fibers_freed++;
      }
      decrement_pool_total(pool, fibers_freed);

      // Stage 3.  Pool is below threshold.  Put extra fiber back.
      if (pool->size <= num_to_keep) {
        // Put the last fiber back into the pool.
        if (fiber_to_return) {
          CILK_ASSERT(pool->size < pool->max_size);
          pool->fibers[pool->size] = fiber_to_return;
          pool->size++;
        }
        last_fiber_returned = true;
      }
        
      // Stage 4: Release the lock, and actually free any fibers
      // buffered.
      if (need_lock) {
        spin_mutex_unlock(pool->lock);
      }

      for (int i = 0; i < num_to_free; ++i) {
        fibers_to_free[i]->deallocate_to_heap();
      }
        
    } while (!last_fiber_returned);
  }

  /******************************************************************
   * TBD: We want to simplify / rework the logic for allocating and
   * deallocating fibers, so that they are hopefully simpler and work
   * more elegantly for more than two levels.
   ******************************************************************/
  /**
   * @brief Transfer fibers from @c pool to @c pool->parent.
   *
   * @pre   Must hold @c pool->lock if it exists.
   * @post  After completion, some number of fibers
   *        have been moved from this pool to the parent.
   *        The lock @c pool->lock is still held.
   *
   * TBD: Do we wish to guarantee that the lock has never been
   * released?  It may depend on the implementation...
   */
  void cilk_fiber_pool_move_fibers_to_parent_pool(cilk_fiber_pool* pool,
                                                         unsigned num_to_keep)
  {
    // ASSERT: We should hold the lock on pool (if it has one).
    CILK_ASSERT(pool->parent);
    cilk_fiber_pool* parent_pool = pool->parent;

    // Move fibers from our pool to the parent until we either run out
    // of space in the parent, or hit our threshold.
    //
    // This operation must be done while holding the parent lock.

    // If the parent pool appears to be full, just return early.
    if (parent_pool->size >= parent_pool->max_size)
      return;

    spin_mutex_lock(pool->parent->lock);
    while ((parent_pool->size < parent_pool->max_size) &&
           (pool->size > num_to_keep)) {
      parent_pool->fibers[parent_pool->size++] =
        pool->fibers[--pool->size];
    }

    // If the child pool has deallocated more than fibers to the heap
    // than it has allocated, then transfer this "surplus" to the
    // parent, so that the parent is free to allocate more from the
    // heap.
    // 
    // This transfer means that the total in the parent can
    // temporarily go negative.
    if (pool->total < 0) {
      // Reduce parent total by the surplus we have in the local
      // pool.
      parent_pool->total += pool->total;
      pool->total = 0;
    }

    spin_mutex_unlock(pool->parent->lock);
  }
    
  void cilk_fiber_pool_init(cilk_fiber_pool* pool,
                            cilk_fiber_pool* parent,
                            size_t           stack_size,
                            unsigned         buffer_size,
                            int              alloc_max,
                            int              is_shared)
  {
#if FIBER_DEBUG >= 1    
    fprintf(stderr, "fiber_pool_init, pool=%p, parent=%p, alloc_max=%u\n",
            pool, parent, alloc_max);
#endif

    pool->lock       = (is_shared ? spin_mutex_create() : NULL);
    pool->parent     = parent;
    pool->stack_size = stack_size;
    pool->max_size   = buffer_size;
    pool->size       = 0;
    pool->total      = 0;
    pool->high_water = 0;
    pool->alloc_max  = alloc_max;
    pool->fibers     =
      (cilk_fiber**) __cilkrts_malloc(buffer_size * sizeof(cilk_fiber*));
    CILK_ASSERT(NULL != pool->fibers);

//#ifdef __MIC__
#define PREALLOCATE_FIBERS
//#endif
    
#ifdef PREALLOCATE_FIBERS
    // Pre-allocate 1/4 of fibers in the pools ahead of time.  This
    // value is somewhat arbitrary.  It was chosen to be less than the
    // threshold (of about 3/4) of fibers to keep in the pool when
    // transferring fibers to the parent.
    
    //int pre_allocate_count = buffer_size/4;
    int pre_allocate_count = buffer_size/2 + buffer_size/4;
    for (pool->size = 0; pool->size < pre_allocate_count; pool->size++) {
      pool->fibers[pool->size] = cilk_fiber::allocate_from_heap(pool->stack_size);
    }
#endif
  }


  void cilk_fiber_pool_set_fiber_limit(cilk_fiber_pool* root_pool,
                                       unsigned max_fibers_to_allocate)
  {
    // Should only set limit on root pool, not children.
    CILK_ASSERT(NULL == root_pool->parent);
    root_pool->alloc_max = max_fibers_to_allocate;
  }
                                   
  void cilk_fiber_pool_destroy(cilk_fiber_pool* pool)
  {
    CILK_ASSERT(cilk_fiber_pool_sanity_check(pool, "pool_destroy"));

    // Lock my own pool, if I need to.
    if (pool->lock) {
      spin_mutex_lock(pool->lock);
    }

    // Give any remaining fibers to parent pool.
    if (pool->parent) {
      cilk_fiber_pool_move_fibers_to_parent_pool(pool, 0);
    }

    // Unlock pool.
    if (pool->lock) {
      spin_mutex_unlock(pool->lock);
    }

    // If I have any left in my pool, just free them myself.
    // This method may acquire the pool lock.
    cilk_fiber_pool_free_fibers_from_pool(pool, 0, NULL);

    // Destroy the lock if there is one.
    if (pool->lock) {
      spin_mutex_destroy(pool->lock);
    }
    __cilkrts_free(pool->fibers);
  }

}

