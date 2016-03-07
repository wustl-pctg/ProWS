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
	CILK_ASSERT(d->frame_ff);
	
	d->saved_ped = w->pedigree;
	d->worker = w;
	d->self = (deque**) p->tail;
	*(p->tail++) = d;

	if (new_deque) {
		CILK_ASSERT(new_deque->worker = w);
	} else {
		new_deque = __cilkrts_malloc(sizeof(deque));
		deque_init(new_deque, w->g->ltqsize);
	}
	w->l->active_deque = new_deque;
	deque_switch(w, w->l->active_deque);

	BEGIN_WITH_WORKER_LOCK(w) {
		w->l->num_suspended_deques++;
	} END_WITH_WORKER_LOCK(w);

	/* cilk_fiber *current_fiber = d->frame_ff->fiber_self; */
	/* d->resumeable_fiber = current_fiber; */

	return d->frame_ff->fiber_self;
}

void* __cilkrts_get_deque(void)
{
	__cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
	return (void*)w->l->active_deque;
}

void __cilkrts_suspend_deque(void)
{
	__cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
	cilk_fiber *current_fiber = deque_pool_suspend(w, NULL);
	
	// suspend fiber and go back to work stealing
	CILK_ASSERT(current_fiber);
	cilk_fiber_suspend_self_and_resume_other(current_fiber,
																					 w->l->scheduling_fiber);
}

void __cilkrts_resume_suspended(void* _deque, int enable_resume)
{
	__cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
	deque *deque_to_resume = (deque*) _deque;

	// TBD: Maybe the right thing to do is for thieves to
	// remove an empty suspended deque when they see one.
	CILK_ASSERT(deque_to_resume->worker);
	__cilkrts_worker *victim = deque_to_resume->worker;
	BEGIN_WITH_WORKER_LOCK(victim) {
		deque_pool *p = &deque_to_resume->worker->l->dod;
		*(deque_to_resume->self) = *(p->head);
		*(p->head) = NULL;
		if (p->head != p->tail) p->head++;
		victim->l->num_suspended_deques--;

		// This might already by null, but also it might not be
		if (deque_to_resume->resumeable_fiber) {
			CILK_ASSERT(deque_to_resume->resumeable_fiber == deque_to_resume->frame_ff->fiber_self);
			deque_to_resume->resumeable_fiber = NULL;
		}
	} END_WITH_WORKER_LOCK(victim);
	deque_to_resume->worker = w;


	deque *d = w->l->active_deque;
	cilk_fiber *current_fiber = deque_pool_suspend(w, deque_to_resume);
	d->resumeable_fiber = (enable_resume) ? current_fiber : NULL;

	cilk_fiber* fiber_to_resume = deque_to_resume->frame_ff->fiber_self;
	CILK_ASSERT(fiber_to_resume);

	// switch fibers
	cilk_fiber_data *data = cilk_fiber_get_data(fiber_to_resume);
	CILK_ASSERT(!data->resume_sf);
	data->owner = w;
	cilk_fiber_suspend_self_and_resume_other(current_fiber, fiber_to_resume);

}
