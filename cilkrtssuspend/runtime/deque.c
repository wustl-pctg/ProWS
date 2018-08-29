#include <string.h> // memset
#include "deque.h"
#include "local_state.h"
#include "full_frame.h"
#include "os.h"
#include "scheduler.h"

// Repeated from scheduler.c:
//#define DEBUG_LOCKS 1
#ifdef DEBUG_LOCKS
// The currently executing worker must own this worker's lock
#   define ASSERT_WORKER_LOCK_OWNED(w)                          \
  {                                                             \
    __cilkrts_worker *tls_worker = __cilkrts_get_tls_worker();  \
    CILK_ASSERT((w)->l->lock.owner == tls_worker);              \
  }
#   define ASSERT_DEQUE_LOCK_OWNED(d)                           \
  {                                                             \
    __cilkrts_worker *tls_worker = __cilkrts_get_tls_worker();  \
    CILK_ASSERT((d)->lock.owner == tls_worker);                 \
  }
#else
#   define ASSERT_WORKER_LOCK_OWNED(w)
#   define ASSERT_DEQUE_LOCK_OWNED(d)
#endif // DEBUG_LOCKS

/* Lock macro Usage:
   BEGIN_WITH_WORKER_LOCK(w) {
   statement;
   statement;
   BEGIN_WITH_FRAME_LOCK(w, ff) {
   statement;
   statement;
   } END_WITH_FRAME_LOCK(w, ff);
   } END_WITH_WORKER_LOCK(w);
*/
#define BEGIN_WITH_WORKER_LOCK(w) __cilkrts_worker_lock(w); do
#define END_WITH_WORKER_LOCK(w)   while (__cilkrts_worker_unlock(w), 0)

#define BEGIN_WITH_DEQUE_LOCK(w) __cilkrts_mutex_lock(w, &w->l->active_deque->lock); do
#define END_WITH_DEQUE_LOCK(w)   while (__cilkrts_mutex_unlock(w, &w->l->active_deque->lock), 0)

#define BEGIN_WITH_FRAME_LOCK(w, ff)                                    \
  do { full_frame *_locked_ff = ff; __cilkrts_frame_lock(w, _locked_ff); do

#define END_WITH_FRAME_LOCK(w, ff)                              \
  while (__cilkrts_frame_unlock(w, _locked_ff), 0); } while (0)


/********************************************************************
 * THE protocol:
 ********************************************************************/
/*
 * This is a protocol for work stealing that minimizes the overhead on
 * the victim.
 *
 * The protocol uses three shared pointers into the worker's deque:
 * - T - the "tail"
 * - H - the "head"
 * - E - the "exception"  NB: In this case, "exception" has nothing to do
 * with C++ throw-catch exceptions -- it refers only to a non-normal return,
 * i.e., a steal or similar scheduling exception.
 *
 * with H <= E, H <= T.  
 *
 * Stack frames SF, where H <= E < T, are available for stealing. 
 *
 * The worker operates on the T end of the stack.  The frame being
 * worked on is not on the stack.  To make a continuation available for
 * stealing the worker pushes a from onto the stack: stores *T++ = SF.
 * To return, it pops the frame off the stack: obtains SF = *--T.
 *
 * After decrementing T, the condition E > T signals to the victim that
 * it should invoke the runtime system's "THE" exception handler.  The
 * pointer E can become INFINITY, in which case the victim must invoke
 * the THE exception handler as soon as possible.
 *
 * See "The implementation of the Cilk-5 multithreaded language", PLDI 1998,
 * http://portal.acm.org/citation.cfm?doid=277652.277725, for more information
 * on the THE protocol.
 */

void __attribute__((always_inline)) __cilkrts_insert_deque_into_list(__cilkrts_deque_link *volatile *tail) {
  CILK_ASSERT(tail);
  CILK_ASSERT(*tail);
  deque* d = (deque*)__cilkrts_get_deque();
  d->link.next = NULL;
  __cilkrts_deque_link* old_tail = __atomic_exchange_n(tail, &(d->link), __ATOMIC_SEQ_CST);
  old_tail->next = &(d->link);
}

int can_take_fiber_from(deque *d) {
    // Unlike in normal dekker, we SHOULD be able to take the last of the fibers
    return ((d->fiber_head < d->fiber_tail) && (d->fiber_head < d->fiber_protected_tail));
}

int fiber_dekker_protocol(__cilkrts_worker *victim, deque *d) {
    ASSERT_WORKER_LOCK_OWNED(victim);

    if (/*__builtin_expect(*/can_take_fiber_from(d)/*, 1)*/) {
        return 1;
    } else {
        return 0;
    }
}

/* the infinity value of E */
#define EXC_INFINITY  ((__cilkrts_stack_frame **) (-1))

void increment_E(__cilkrts_worker *victim, deque* d)
{
  __cilkrts_stack_frame *volatile *tmp;

  // The currently executing worker must own the worker lock to touch
  // victim->exc
  ASSERT_WORKER_LOCK_OWNED(victim);

  tmp = d->exc;
  if (tmp != EXC_INFINITY) {
    /* On most x86 this pair of operations would be slightly faster
       as an atomic exchange due to the implicit memory barrier in
       an atomic instruction. */
    d->exc = tmp + 1;
    __cilkrts_fence();
  }
}

void decrement_E(__cilkrts_worker *victim, deque* d)
{
  __cilkrts_stack_frame *volatile *tmp;

  // The currently executing worker must own the worker lock to touch
  // victim->exc
  ASSERT_WORKER_LOCK_OWNED(victim);

  tmp = d->exc;
  if (tmp != EXC_INFINITY) {
    /* On most x86 this pair of operations would be slightly faster
       as an atomic exchange due to the implicit memory barrier in
       an atomic instruction. */
    d->exc = tmp - 1;
    __cilkrts_fence(); /* memory fence not really necessary */
  }
}

#if 0
/* for now unused, will be necessary if we implement abort */
void signal_THE_exception(__cilkrts_worker *wparent)
{
  *wparent->exc = EXC_INFINITY;
  __cilkrts_fence();
}
#endif

/// @todo{reset_THE_exception should take a deque parameter}
void reset_THE_exception(__cilkrts_worker *w)
{
  // The currently executing worker must own the worker lock to touch
  // w->exc
  ASSERT_WORKER_LOCK_OWNED(w);
  deque *d = w->l->active_deque;

  //    *w->exc = *w->head;
  d->exc = d->head;
  __cilkrts_fence();
}

/* conditions under which victim->head can be stolen: */
int can_steal_from(__cilkrts_worker *victim, deque *d)
{
  return ((d->head < d->tail) && (d->head < d->protected_tail));
}

/* Return TRUE if the frame can be stolen, false otherwise */
int dekker_protocol(__cilkrts_worker *victim, deque *d)
{
  // increment_E and decrement_E are going to touch victim->exc.  The
  // currently executing worker must own victim's lock before they can
  // modify it
  ASSERT_WORKER_LOCK_OWNED(victim);

  /* ASSERT(E >= H); */

  increment_E(victim, d);

  /* ASSERT(E >= H + 1); */
  if (can_steal_from(victim, d)) {
    /* success, we can steal victim->head and set H <- H + 1
       in detach() */
    return 1;
  } else {
    /* failure, restore previous state */
    decrement_E(victim, d);
    return 0;    
  }
}

/* detach the top of the deque frame from the VICTIM and install a new
   CHILD frame in its place */
void detach_for_steal(__cilkrts_worker *w,
                      __cilkrts_worker *victim,
                      deque *d, cilk_fiber* fiber)
{
  /* ASSERT: we own victim->lock */

  full_frame *parent_ff, *child_ff, *loot_ff;
  __cilkrts_stack_frame *volatile *h;
  __cilkrts_stack_frame *sf;

  //    w->l->team = victim->l->team;
  w->l->active_deque->team = d->team;

  /// @todo{ Shouldn't need the w == victim part, should *always* be null }
  CILK_ASSERT(*w->l->frame_ff == 0 || w == victim);

  h = d->head;

  CILK_ASSERT(*h);

  d->head = h + 1;

  parent_ff = d->frame_ff;
  BEGIN_WITH_FRAME_LOCK(w, parent_ff) {
    /* parent no longer referenced by victim */
    decjoin(parent_ff);

    /* obtain the victim call stack */
    sf = *h;

    /* perform system-dependent normalizations */
    /*__cilkrts_normalize_call_stack_on_steal(sf);*/

    /* unroll PARENT_FF with call stack SF, adopt the youngest
       frame LOOT.  If loot_ff == parent_ff, then we hold loot_ff->lock,
       otherwise, loot_ff is newly created and we can modify it without
       holding its lock. */
    sf->worker = victim;
    loot_ff = unroll_call_stack(w, parent_ff, sf);

#if REDPAR_DEBUG >= 3
    fprintf(stderr, "[W=%d, victim=%d, desc=detach, parent_ff=%p, loot=%p]\n",
            w->self, victim->self,
            parent_ff, loot_ff);
#endif

    /* if (WORKER_USER == victim->l->type && */
    /*     NULL == victim->l->last_full_frame) { */
    /*if (w->g->original_deque == d
        && d->team->l->last_full_frame == NULL) {
      
      // Mark this looted frame as special: only the original user worker
      // may cross the sync.
      // 
      // This call is a shared access to
      // victim->l->last_full_frame.
      set_sync_master(d->team, loot_ff);
    }*/

    /* LOOT is the next frame that the thief W is supposed to
       run, unless the thief is stealing from itself, in which
       case the thief W == VICTIM executes CHILD and nobody
       executes LOOT. */
    if (w == victim && d == w->l->active_deque) {
      /* Pretend that frame has been stolen */
      loot_ff->call_stack->flags |= CILK_FRAME_UNSYNCHED;
      loot_ff->simulated_stolen = 1;
    } else {
      __cilkrts_push_next_frame(w, loot_ff);
    }


    // After this "push_next_frame" call, w now owns loot_ff.
    child_ff = make_child(w, loot_ff, 0, fiber);

    BEGIN_WITH_FRAME_LOCK(w, child_ff) {
      /* install child in the victim's work queue, taking
         the parent_ff's place */
      /* child is referenced by victim */
      incjoin(child_ff);

      // It would be great to find a cleaner way to do this.
      if (sf->flags & CILK_FRAME_FUTURE_PARENT) {
          child_ff->fiber_self = __cilkrts_pop_head_future_fiber(victim, d);
          CILK_ASSERT(child_ff->fiber_self);
      }

      // With this call, w is bestowing ownership of the newly
      // created frame child_ff to the victim, and victim is
      // giving up ownership of parent_ff.
      //
      // Worker w will either take ownership of parent_ff
      // if parent_ff == loot_ff, or parent_ff will be
      // suspended.
      //
      // Note that this call changes the victim->frame_ff
      // while the victim may be executing.
      make_runnable(victim, child_ff, d);
    } END_WITH_FRAME_LOCK(w, child_ff);
  } END_WITH_FRAME_LOCK(w, parent_ff);
}

/* worker W completely promotes its own deque, simulating the case
   where the whole deque is stolen.  We use this mechanism to force
   the allocation of new storage for reducers for race-detection
   purposes. */
void __cilkrts_promote_own_deque(__cilkrts_worker *w)
{
  // Remember the fiber we start this method on.
  CILK_ASSERT(*w->l->frame_ff);
  cilk_fiber* starting_fiber = (*w->l->frame_ff)->fiber_self;
  deque* d = ((deque*)(w->tail));
    
  BEGIN_WITH_WORKER_LOCK(w) {
    while (dekker_protocol(w, d)) {
      /* PLACEHOLDER_FIBER is used as non-null marker to tell detach()
         and make_child() that this frame should be treated as a spawn
         parent, even though we have not assigned it a stack. */
      detach_for_steal(w, w, d, PLACEHOLDER_FIBER);
    }
  } END_WITH_WORKER_LOCK(w);


  // TBD: The management of full frames and fibers is a bit
  // sketchy here.  We are promoting stack frames into full frames,
  // and pretending they are stolen away, but no other worker is
  // actually working on them.  Some runtime invariants
  // may be broken here.
  //
  // Technically, if we are simulating a steal from w
  // w should get a new full frame, but
  // keep the same fiber.  A real thief would be taking the
  // loot frame away, get a new fiber, and starting executing the
  // loot frame.
  //
  // What should a fake thief do?  Where does the frame go? 

  // In any case, we should be finishing the promotion process with
  // the same fiber with.
  CILK_ASSERT(*w->l->frame_ff);
  CILK_ASSERT((*w->l->frame_ff)->fiber_self == starting_fiber);
}

int deque_init(deque *d, size_t ltqsize)
{
  memset(d, 0, sizeof(deque));
  d->link.d = d;
  d->ltq = (__cilkrts_stack_frame **)
    __cilkrts_malloc(ltqsize * sizeof(__cilkrts_stack_frame*));


  if (!d->ltq)
    return -1;
  //__cilkrts_bug("Cilk: out of memory for new deques!\n");

  d->fiber_ltq = (cilk_fiber **)
    __cilkrts_malloc(ltqsize * sizeof(cilk_fiber*));

  if (!d->fiber_ltq) {
    __cilkrts_free(d->ltq);
    d->ltq = NULL;
    return -1;
  }
  
  d->ltq_limit = d->ltq + ltqsize;
  d->head = d->tail = d->exc = d->ltq;
  d->protected_tail = d->ltq_limit;

  d->fiber_ltq_limit = d->fiber_ltq + ltqsize;
  d->fiber_head = d->fiber_tail = d->fiber_ltq;
  d->fiber_protected_tail = d->fiber_ltq_limit;

  return 0;
}

void deque_destroy(deque *d)
{
  //CILK_ASSERT(d->worker == NULL);
  CILK_ASSERT(d->head == d->tail);

  // if/when we use d->lock or d->steal_lock
  //__cilkrts_mutex_destroy
  __cilkrts_free(d->ltq);
  __cilkrts_free(d->fiber_ltq);
  __cilkrts_free(d);
}

void deque_switch(__cilkrts_worker *w, deque *d)
{
  CILK_ASSERT(d == w->l->active_deque);

  if (!d || d->saved_ped.rank == 0) { // 0 is no longer valid rank
    w->pedigree.parent = NULL;
    w->pedigree.rank = 1;

#ifdef PRECOMPUTE_PEDIGREES
    w->pedigree.actual = w->g->ped_compression_vec[0];
    w->pedigree.length = 1;
#endif

  }

  // We may have run out of memory for deques, in which case we want
  // to go back to the scheduling loop and try to mug resumable
  // deques.
  if (!d) {
    w->tail = w->head = w->exc = w->protected_tail = NULL;
    w->ltq_limit = w->l->current_ltq = w->l->frame_ff = NULL;
    w->current_stack_frame = NULL;
    return;
  } 

  if (d->resumable) {
    CILK_ASSERT(d->fiber);
    CILK_ASSERT(d->call_stack);
    d->call_stack->worker = w;
  }
  CILK_ASSERT(d->worker == NULL);
  d->worker = w;
  d->self = ACTIVE_DEQUE_INDEX;//&w->l->active_deque;
  
  w->tail = &d->tail;
  w->head = &d->head;
  w->exc = &d->exc;
  w->protected_tail = &d->protected_tail;
  w->ltq_limit = &d->ltq_limit;
  w->l->current_ltq = &d->ltq;
  w->l->frame_ff = &d->frame_ff;
  w->pedigree = d->saved_ped;
  w->current_stack_frame = d->call_stack;
  
  // This deque is now active, so remove that saved pedigree information
  memset(&d->saved_ped, 0, sizeof(__cilkrts_pedigree));
}

cilk_fiber* deque_suspend(__cilkrts_worker *w, deque *new_deque)
{
  deque *d = w->l->active_deque;

  // An active deque should:
  CILK_ASSERT(d->self == ACTIVE_DEQUE_INDEX); // know it is an active deque
  CILK_ASSERT(d->fiber == NULL); // not have a fiber (stored in worker)
  CILK_ASSERT(d->worker == w); // Know its worker
  CILK_ASSERT(d->team); // Have a team
  
  d->saved_ped = w->pedigree;
  d->call_stack = w->current_stack_frame;

  int attempt_steal = 0;

  if (!new_deque) {
    // Before we allocate a new deque, let's see if we have something to resume
    __cilkrts_mutex_lock(w, &w->l->lock);
    int size = w->l->resumable_deques.size;
    if (size > 0) {
        #ifdef COLLECT_STEAL_STATS
            w->l->ks_stats.deques_mugged_on_suspend++;
        #endif
      new_deque = w->l->resumable_deques.array[size-1];
      deque_mug(w, new_deque);
    }
    __cilkrts_mutex_unlock(w, &w->l->lock);
    /// @todo{ Check other workers, too? }
  }

  if (!new_deque) { // Must allocate new
    new_deque = __cilkrts_malloc(sizeof(deque));
    if (new_deque
        && -1 != deque_init(new_deque, w->g->ltqsize)) {
      new_deque->team = d->team;
      new_deque->fiber = w->l->scheduling_fiber;
      attempt_steal = 1;
    } else {
      __cilkrts_free(new_deque);
      new_deque = NULL;
      __cilkrts_bug("Cilk: out of memory for new deques!\n");
    }
  } else {
    // This deque should be mugged
    CILK_ASSERT(new_deque->fiber);
    CILK_ASSERT(new_deque->resumable == 1);
    CILK_ASSERT(new_deque->self == INVALID_DEQUE_INDEX);
    CILK_ASSERT(new_deque->worker == NULL);
    new_deque->resumable = 0;
  }
  
  // User workers should stay on the same team
  if (w->l->type == WORKER_USER && new_deque)
    CILK_ASSERT(new_deque->team == w);

  cilk_fiber *fiber;
  BEGIN_WITH_WORKER_LOCK(w) { // I'm not entirely sure this is necessary
    // Switch to new deque
    w->l->active_deque = new_deque;


    // This will immediately succeed, but we need to make sure no one 

    { // d is no longer accessible

      //CILK_ASSERT(d->frame_ff);
      fiber = cilk_fiber_get_current_fiber();
      d->fiber = fiber;//fiber;

      CILK_ASSERT(!cilk_fiber_get_data(fiber)->resume_sf);

      /* if (WORKER_USER == w->l->type && */
      /*     NULL == w->l->last_full_frame) { */
      /*   victim = w; */
      /* } */

    }

    //    deque_pool_validate(p, w);

    deque_switch(w, w->l->active_deque);

    #ifdef COLLECT_STEAL_STATS
    if (!can_steal_from(w, d)) {
      w->l->num_susp_empty++;
    }
#endif

    extern void fiber_proc_to_resume_user_code_for_random_steal(cilk_fiber*);
    w->l->work_stolen = 0;
    if (attempt_steal && can_steal_from(w,d)) {
        #ifdef COLLECT_STEAL_STATS
            w->l->ks_stats.steal_on_suspend_attempts++;
        #endif
        cilk_fiber *steal_fiber = NULL;
        int proceed_with_steal = 0;
        // Only allocate a new fiber if it isn't a future
        if (((*d->head)->flags & CILK_FRAME_FUTURE_PARENT) == 0) {
          START_INTERVAL(w, INTERVAL_FIBER_ALLOCATE) {
              // Verify that we can get a stack.  If not, no need to continue.
              steal_fiber = cilk_fiber_allocate(&w->l->fiber_pool);
          } STOP_INTERVAL(w, INTERVAL_FIBER_ALLOCATE);
          proceed_with_steal = steal_fiber;
        } else {
          proceed_with_steal = 1;
        }

        if (proceed_with_steal) {
            #ifdef COLLECT_STEAL_STATS
                w->l->ks_stats.successful_steal_on_suspend++;
            #endif
            dekker_protocol(w, d);
            cilkg_increment_active_workers(w->g);
            detach_for_steal(w, w, d, steal_fiber);
            w->l->work_stolen = 1;
            if (w->l->next_frame_ff->call_stack->flags & CILK_FRAME_FUTURE_PARENT) {
                CILK_ASSERT(!steal_fiber);
                cilk_fiber_take(w->l->next_frame_ff->fiber_self);
            } else {
                cilk_fiber_take(steal_fiber);
                // Since our steal was successful, finish initialization of
                // the fiber.
                cilk_fiber_reset_state(steal_fiber,
                                       fiber_proc_to_resume_user_code_for_random_steal);
            }
            CILK_ASSERT(!w->l->next_frame_ff->simulated_stolen);
        }
    }

  } END_WITH_WORKER_LOCK(w);


  // Might be nice to use trylock here, but I'm not sure what is the
  // right thing to do if it fails.
  if (d->resumable || d->head != d->tail) {

    __cilkrts_worker *victim = NULL;

    if (__builtin_expect(w->g->total_workers > 1, 1)) {
        // Probably slightly better balls and bins result compared to random;
        // based on very light testing. slightly better performance in practice.
        int victim_idx = myrand(w) % (w->g->total_workers);
        int victim2_idx = myrand(w) % (w->g->total_workers - 1);
        if (victim2_idx >= victim_idx) victim2_idx++;
        victim = w->g->workers[victim_idx];
        __cilkrts_worker *potential_victim = w->g->workers[victim2_idx];

        if ((d->resumable && victim->l->resumable_deques.size > potential_victim->l->resumable_deques.size) || victim->l->suspended_deques.size > potential_victim->l->suspended_deques.size) {
          victim = potential_victim;
        }
    } else {
      victim = w;
    }

        DEQUE_LOG("(w: %i) pushing suspended deque %p on to %i\n",
                  w->self, d, victim->self);

        __cilkrts_mutex_lock(w, &victim->l->lock); {
          if (d->resumable) {
            deque_pool_add(victim, &victim->l->resumable_deques, d);
            /* fprintf(stderr, "(w: %i) pushed resumable deque %p on to %i\n", */
            /*        w->self, d, victim->self); */

          } else if (d->head != d->tail) { // not resumable, but stealable!
            deque_pool_add(victim, &victim->l->suspended_deques, d);
            /* fprintf(stderr, "(w: %i) pushed suspended (stealable) deque %p on to %i\n", */
            /*              w->self, d, victim->self); */

//          } else { // empty: no need to add, will be resumed later


//            d->self = INVALID_DEQUE_INDEX;
//            d->worker = NULL;
            //      w->l->mugged++;

            /* fprintf(stderr, "(w: %i) suspended non-stealable deque %p on to %i\n", */
            /*        w->self, d, victim->self); */

          }
        } __cilkrts_mutex_unlock(w, &victim->l->lock);
  } else {
    d->self = INVALID_DEQUE_INDEX;
    d->worker = NULL;
  }
  //} END_WITH_WORKER_LOCK(w);


  return fiber;

}

void deque_mug(__cilkrts_worker *w, deque *d)
{
  CILK_ASSERT(d->worker);
  CILK_ASSERT(d->worker->l->lock.owner == w);
  CILK_ASSERT(d->fiber);

  deque_pool *p = (d->resumable) ?
    &d->worker->l->resumable_deques : &d->worker->l->suspended_deques;
  deque_pool_validate(p, d->worker);

  DEQUE_LOG("(w: %i) mugged %p from %i\n",
            w->self, d, d->worker->self);

  //  d->worker->l->mugged++;
  deque_pool_remove(p, d);
  d->call_stack->worker = w;
}
