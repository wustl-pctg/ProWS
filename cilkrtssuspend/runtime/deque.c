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
#   define ASSERT_WORKER_LOCK_OWNED(w) \
        { \
            __cilkrts_worker *tls_worker = __cilkrts_get_tls_worker(); \
            CILK_ASSERT((w)->l->lock.owner == tls_worker); \
        }
#else
#   define ASSERT_WORKER_LOCK_OWNED(w)
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

#define BEGIN_WITH_FRAME_LOCK(w, ff)                                     \
    do { full_frame *_locked_ff = ff; __cilkrts_frame_lock(w, _locked_ff); do

#define END_WITH_FRAME_LOCK(w, ff)                       \
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

void reset_THE_exception(__cilkrts_worker *w)
{
    // The currently executing worker must own the worker lock to touch
    // w->exc
    ASSERT_WORKER_LOCK_OWNED(w);

    *w->exc = *w->head;
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

    w->l->team = victim->l->team;

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
        loot_ff = unroll_call_stack(w, parent_ff, sf);

        #if REDPAR_DEBUG >= 3
        fprintf(stderr, "[W=%d, victim=%d, desc=detach, parent_ff=%p, loot=%p]\n",
                w->self, victim->self,
                parent_ff, loot_ff);
        #endif

        if (WORKER_USER == victim->l->type &&
            NULL == victim->l->last_full_frame) {
						//						d == &victim->l->active_deque) {
            // Mark this looted frame as special: only the original user worker
            // may cross the sync.
            // 
            // This call is a shared access to
            // victim->l->last_full_frame.
            set_sync_master(victim, loot_ff);
        }

        /* LOOT is the next frame that the thief W is supposed to
           run, unless the thief is stealing from itself, in which
           case the thief W == VICTIM executes CHILD and nobody
           executes LOOT. */
        if (w == victim && d == w->l->active_deque) {
            /* Pretend that frame has been stolen */
            loot_ff->call_stack->flags |= CILK_FRAME_UNSYNCHED;
            loot_ff->simulated_stolen = 1;
        } else
            __cilkrts_push_next_frame(w, loot_ff);

        // After this "push_next_frame" call, w now owns loot_ff.
        child_ff = make_child(w, loot_ff, 0, fiber);

        BEGIN_WITH_FRAME_LOCK(w, child_ff) {
            /* install child in the victim's work queue, taking
               the parent_ff's place */
            /* child is referenced by victim */
            incjoin(child_ff);

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

void deque_init(deque *d, size_t ltqsize)
{
	d->ltq = (__cilkrts_stack_frame **)
		__cilkrts_malloc(ltqsize * sizeof(__cilkrts_stack_frame*));
	d->ltq_limit = d->ltq + ltqsize;
	d->head = d->tail = d->exc = d->ltq;
	d->protected_tail = d->ltq_limit;
	d->frame_ff = 0;
	d->resumeable_fiber = NULL;
}
/* int deque_add(__cilkrts_worker *w) */
/* { */
/* 	int index; */
/* 	deque d; */
/* 	d.ltq = (__cilkrts_stack_frame **) */
/* 		__cilkrts_malloc(w->g->ltqsize * sizeof(__cilkrts_stack_frame*)); */
/* 	d.ltq_limit = d.ltq + w->g->ltqsize; */
/* 	d.head = d.tail = d.exc = d.ltq; */
/* 	d.protected_tail = d.ltq_limit; */
/* 	d.frame_ff = 0; */

/* 	//	BEGIN_WITH_WORKER_LOCK(w) { */
/* 		index = w->l->num_active_deques; */
/* 		CILK_ASSERT(index < 1); */
		
/* 		w->l->deques[index] = d; */
/* 		w->l->num_active_deques++; */
/* 		//	} END_WITH_WORKER_LOCK(w); */
	
/* 	return index; */
/* } */

/* void deque_switch(__cilkrts_worker *w, int n) */
void deque_switch(__cilkrts_worker *w, deque *d)
{
	//	BEGIN_WITH_WORKER_LOCK(w) {
		/* CILK_ASSERT(n < w->l->num_active_deques); */
	CILK_ASSERT(d == w->l->active_deque);

	d->resumeable_fiber = NULL;
	w->tail = &d->tail;
	w->head = &d->head;
	w->exc = &d->exc;
	w->protected_tail = &d->protected_tail;
	w->ltq_limit = &d->ltq_limit;
	w->l->current_ltq = &d->ltq;
	w->l->frame_ff = &d->frame_ff;
	w->pedigree = d->saved_ped;
	
	// This deque is now active, so remove that saved pedigree information
	memset(&d->saved_ped, 0, sizeof(__cilkrts_pedigree));
	
	//	w->l->active_deque = &w->l->deques[n];

		//	} END_WITH_WORKER_LOCK(w);

}

