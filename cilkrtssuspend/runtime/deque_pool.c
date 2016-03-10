#include "deque_pool.h"
#include "local_state.h"
#include "full_frame.h"
#include "worker_mutex.h" // __cilkrts_mutex_lock/unlock
#include "scheduler.h" // __cilkrts_worker_lock/unlock

#define BEGIN_WITH_WORKER_LOCK(w) __cilkrts_worker_lock(w); do
#define END_WITH_WORKER_LOCK(w)   while (__cilkrts_worker_unlock(w), 0)
#define BEGIN_WITH_DEQUE_LOCK(w,d) __cilkrts_mutex_lock(w, &d->lock); do
#define END_WITH_DEQUE_LOCK(w,d)   while (__cilkrts_mutex_unlock(w, &d->lock), 0)

void deque_pool_init(deque_pool *p, size_t ltqsize)
{
	p->ltq = (deque**) __cilkrts_malloc(ltqsize * sizeof(deque*));
	p->ltq_limit = p->ltq + ltqsize;
	p->head = p->tail = p->exc = p->ltq;
	p->protected_tail = p->ltq_limit;
}

void deque_pool_free(deque_pool *p)
{
	// There should be no suspended deques
	CILK_ASSERT(p->head == p->tail);
	__cilkrts_free(p->ltq);
}

cilk_fiber* deque_pool_suspend(__cilkrts_worker *w, deque *new_deque)
{
	deque_pool *p = &w->l->dod;
	deque *d = w->l->active_deque;

	CILK_ASSERT(d->fiber == NULL);
	d->saved_ped = w->pedigree;
	d->worker = w;
	d->call_stack = w->current_stack_frame;

	if (new_deque) {
		CILK_ASSERT(new_deque->worker = w);
		new_deque->fiber = NULL; // This needs to be pulled out beforehand.
	} else {
		new_deque = __cilkrts_malloc(sizeof(deque));
		deque_init(new_deque, w->g->ltqsize);

		// Stay on the same team if we are a user worker
		if (w->l->type == WORKER_USER)
			new_deque->team = w;
	}
	w->l->active_deque = new_deque;

	cilk_fiber *fiber;
	BEGIN_WITH_WORKER_LOCK(w) {
		CILK_ASSERT(d->frame_ff);
		fiber = d->frame_ff->fiber_self;
		CILK_ASSERT(fiber);

		d->fiber = fiber;

		d->self = (deque**) p->tail;
		*(p->tail++) = d;
		deque_switch(w, w->l->active_deque);
		
		w->l->num_suspended_deques++;
	} END_WITH_WORKER_LOCK(w);

	return fiber;
}

void* __cilkrts_get_deque(void)
{
	__cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
	return (void*)w->l->active_deque;
}

void __cilkrts_suspend_deque()
{
	__cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
	cilk_fiber *current_fiber;
	current_fiber = deque_pool_suspend(w, NULL);
	
	// suspend fiber and go back to work stealing
	CILK_ASSERT(current_fiber);
	cilk_fiber_suspend_self_and_resume_other(current_fiber,
																					 w->l->scheduling_fiber);
}

void __cilkrts_resume_suspended(void* _deque, int enable_resume)
{
	__cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
	deque *deque_to_resume = (deque*) _deque;
	cilk_fiber *fiber_to_resume;

	CILK_ASSERT(deque_to_resume->resumable == 0);

	// User workers can only resume deques from the same team
	CILK_ASSERT(w->l->type == WORKER_SYSTEM
							|| w->l->active_deque->team == deque_to_resume->team);

	CILK_ASSERT(deque_to_resume->worker);
	__cilkrts_worker *victim = deque_to_resume->worker;

	fprintf(stderr, "(w: %i) resuming %p for %i\n", w->self,
					deque_to_resume, victim->self);

	// This deque may not quite be finished suspending yet. We have to
	// do this because it's hard to separate the suspension of a deque
	// and of a fiber. We could just give up here, but then I'm not sure
	// how to guarantee that the deque gets resumed.
	while (!deque_to_resume->fiber
				 || !cilk_fiber_is_resumable(deque_to_resume->fiber));
	fiber_to_resume = deque_to_resume->fiber;
	CILK_ASSERT(fiber_to_resume);

	/// @todo{Should get deque locks instead of worker locks in most cases.}
	__cilkrts_mutex_lock(w, &victim->l->lock); {

		deque_pool *p = &victim->l->dod;
		(*(p->head))->self = deque_to_resume->self;
		*(deque_to_resume->self) = *(p->head);
		*(p->head) = NULL;
		if (p->head != p->tail) p->head++;
		else {
			CILK_ASSERT(victim->l->num_suspended_deques == 1);
			p->head = p->tail = p->ltq;
		}
		victim->l->num_suspended_deques--;

		deque_to_resume->worker = w;
		deque_to_resume->self = NULL;

	} __cilkrts_mutex_unlock(w, &victim->l->lock);
		

	deque *d = w->l->active_deque;
	CILK_ASSERT(d->resumable == 0);
	cilk_fiber *current_fiber = deque_pool_suspend(w, deque_to_resume);
	
	d->fiber = current_fiber;
	__asm__ volatile("": : :"memory");
	d->resumable = (enable_resume) ? 1 : 0;

	// switch fibers
	cilk_fiber_data *data = cilk_fiber_get_data(fiber_to_resume);
	CILK_ASSERT(!data->resume_sf);
	data->owner = w;
	
	CILK_ASSERT(deque_to_resume->call_stack);
	CILK_ASSERT(deque_to_resume->call_stack == w->current_stack_frame);

	cilk_fiber_suspend_self_and_resume_other(current_fiber, fiber_to_resume);

}
