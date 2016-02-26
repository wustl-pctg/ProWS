#include "deque_pool.h"
#include "local_state.h"
#include "full_frame.h"

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

cilk_fiber* deque_pool_suspend(__cilkrts_worker *w)
{
	deque_pool *p = &w->l->dod;
	deque *d = w->l->active_deque;
	d->saved_ped = w->pedigree;
	//	BEGIN_WITH_WORKER_LOCK(w) {
	cilk_fiber *current_fiber = d->frame_ff->fiber_self;
	*(p->tail++) = d;

	w->l->active_deque = __cilkrts_malloc(sizeof(deque));
	deque_init(w->l->active_deque, w->g->ltqsize);
	deque_switch(w, w->l->active_deque);
	w->l->num_suspended_deques++;

		//	} END_WITH_WORKER_LOCK(w);
	return current_fiber;
}

void* __cilkrts_get_deque(void)
{
	__cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
	/* cilk_fiber *f = (*w->l->frame_ff)->fiber_self; */
	/* CILK_ASSERT(f); */
	/* return (void*)f; */
	return (void*)w->l->active_deque;
}

void __cilkrts_suspend_deque(void)
{
	__cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
	cilk_fiber *current_fiber = deque_pool_suspend(w);
	
	// suspend fiber and go back to work stealing
	CILK_ASSERT(current_fiber);
	cilk_fiber_suspend_self_and_resume_other(current_fiber,
																					 w->l->scheduling_fiber);
}

void __cilkrts_resume_suspended(void* _deque)
{
	__cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
	deque *deque_to_resume = (deque*) _deque;
	
	deque *d = w->l->active_deque;
	cilk_fiber *current_fiber = deque_pool_suspend(w);
	d->resumeable_fiber = current_fiber;

	w->l->active_deque = deque_to_resume;
	cilk_fiber* fiber_to_resume = deque_to_resume->frame_ff->fiber_self;
	CILK_ASSERT(fiber_to_resume);
	deque_switch(w, deque_to_resume);
	
	cilk_fiber_suspend_self_and_resume_other(current_fiber, fiber_to_resume);
}
