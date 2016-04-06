#include <string.h> // memset
#include "deque.h"
#include "local_state.h"
#include "full_frame.h"
#include "os.h"
#include "scheduler.h"
#include "deque.h"

// Repeated from scheduler.c:
//#define DEBUG_LOCKS 1
#ifdef DEBUG_LOCKS
// The currently executing worker must own this worker's lock
#   define ASSERT_WORKER_LOCK_OWNED(w)													\
	{																															\
		__cilkrts_worker *tls_worker = __cilkrts_get_tls_worker();	\
		CILK_ASSERT((w)->l->lock.owner == tls_worker);							\
	}
#   define ASSERT_DEQUE_LOCK_OWNED(d)														\
	{																															\
		__cilkrts_worker *tls_worker = __cilkrts_get_tls_worker();	\
		CILK_ASSERT((d)->lock.owner == tls_worker);									\
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

#define BEGIN_WITH_FRAME_LOCK(w, ff)																		\
	do { full_frame *_locked_ff = ff; __cilkrts_frame_lock(w, _locked_ff); do

#define END_WITH_FRAME_LOCK(w, ff)															\
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

int deque_init(deque *d, size_t ltqsize)
{
	memset(d, 0, sizeof(deque));
	d->ltq = (__cilkrts_stack_frame **)
		__cilkrts_malloc(ltqsize * sizeof(__cilkrts_stack_frame*));

	if (!d->ltq)
		return -1;
	//__cilkrts_bug("Cilk: out of memory for new deques!\n");
	
	d->ltq_limit = d->ltq + ltqsize;
	d->head = d->tail = d->exc = d->ltq;
	d->protected_tail = d->ltq_limit;
	__cilkrts_mutex_init(&d->lock);
	__cilkrts_mutex_init(&d->steal_lock);
	return 0;
}

void deque_switch(__cilkrts_worker *w, deque *d)
{
	CILK_ASSERT(d == w->l->active_deque);

	// We may have run out of memory for deques, in which case we want
	// to go back to the scheduling loop and try to mug resumable
	// deques.
	if (!d) {
		w->tail = w->head = w->exc = w->protected_tail = NULL;
		w->ltq_limit = w->l->current_ltq = w->l->frame_ff = NULL;
		w->current_stack_frame = NULL;
		return;
	}

	if (d->resumable) CILK_ASSERT(d->fiber);
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

	if (!new_deque) {
		// Before we allocate a new deque, let's see if we have something to resume
		__cilkrts_mutex_lock(w, &w->l->lock);
		int size = w->l->resumable_deques.size;
		if (size > 0) {
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
		} else {
			__cilkrts_free(new_deque);
			new_deque = NULL;
			//__cilkrts_bug("Cilk: out of memory for new deques!\n");
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

	__cilkrts_worker *victim = w->g->workers[myrand(w) % w->g->total_workers];

	cilk_fiber *fiber;
	BEGIN_WITH_WORKER_LOCK(w) { // I'm not entirely sure this is necessary
		// Switch to new deque
		w->l->active_deque = new_deque;

		// This will immediately succeed, but we need to make sure no one 

		{ // d is no longer accessible

			CILK_ASSERT(d->frame_ff);
			fiber = d->frame_ff->fiber_self;
			CILK_ASSERT(fiber);
			d->fiber = fiber;

			if (WORKER_USER == w->l->type &&
					NULL == w->l->last_full_frame) {
				//set_sync_master(w, d->frame_ff);
				
				/* This is big hack. I want to set the sync master for a
				 * stolen frame from this deque, but only the thief will
				 * create the stolen frame. So just don't push to other
				 * workers until someone has stolen from us.
				 */
				/// @todo{ What if a thief doesn't steal from the original
				/// deque? Does it matter that they set the sync master of the
				/// wrong loot frame?! }
				victim = w;
			}

		}

		//		deque_pool_validate(p, w);

		deque_switch(w, w->l->active_deque);

	} END_WITH_WORKER_LOCK(w);

	fprintf(stderr, "(w: %i) pushing suspended deque onto %i\n",
					w->self, victim->self);

	// Might be nice to use trylock here, but I'm not sure what is the
	// right thing to do if it fails.
	__cilkrts_mutex_lock(w, &victim->l->lock); {
		if (d->resumable)
			deque_pool_add(victim, &victim->l->resumable_deques, d);
		else if (d->head != d->tail) // not resumable, but stealable!
			deque_pool_add(victim, &victim->l->suspended_deques, d);
		else { // empty: no need to add, will be resumed later
			d->self = INVALID_DEQUE_INDEX;
			d->worker = NULL;
			//			w->l->mugged++;
		}
	} __cilkrts_mutex_unlock(w, &victim->l->lock);
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

	fprintf(stderr, "(w: %i) mugging %p from %i\n",
					w->self, d, d->worker->self);

	//	d->worker->l->mugged++;
	deque_pool_remove(p, d);
}

/* Assuming we have exclusive access to this deque, i.e. either
 *  (1) w is working from d, so only it could set do_not_steal
 *  (2) w has exclusive access to deque to resum it
 */
void deque_lock(__cilkrts_worker *w, deque *d)
{
	//    validate_worker(w);
	CILK_ASSERT(d->do_not_steal == 0);

	/* tell thieves to stay out of the way */
	d->do_not_steal = 1;
	__cilkrts_fence(); /* probably redundant */

	__cilkrts_mutex_lock(w, &d->lock);
}

/// @todo{Do we want a try_lock version for this}
void deque_lock_other(__cilkrts_worker *w, deque *d)
{
	//	validate_worker(other);

	// compete for the right to disturb
}

void deque_unlock(__cilkrts_worker *w, deque *d)
{
	__cilkrts_mutex_unlock(w, &w->l->lock);
	CILK_ASSERT(w->l->do_not_steal == 1);
	/* The fence is probably redundant.  Use a release
		 operation when supported (gcc and compatibile);
		 that is faster on x86 which serializes normal stores. */
#if defined __GNUC__ && (__GNUC__ * 10 + __GNUC_MINOR__ > 43 || __ICC >= 1110)
	__sync_lock_release(&w->l->do_not_steal);
#else
	w->l->do_not_steal = 0;
	__cilkrts_fence(); /* store-store barrier, redundant on x86 */
#endif
}
