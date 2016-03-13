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

void* __cilkrts_get_deque(void)
{
	__cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
	return (void*)w->l->active_deque;
}

void __cilkrts_suspend_deque()
{
	__cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
	cilk_fiber *current_fiber;
	current_fiber = deque_suspend(w, NULL);
	
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

	// User workers can only resume deques from the same team
	CILK_ASSERT(w->l->type == WORKER_SYSTEM
							|| w->l->active_deque->team == deque_to_resume->team);
	CILK_ASSERT(deque_to_resume->resumable == 0);

	// At this point, no one else can resume the deque.
	// Wait for the deque to be fully suspended.
	while (!deque_to_resume->fiber
				 || !cilk_fiber_is_resumable(deque_to_resume->fiber));


	int mugged = 0;
	__cilkrts_worker volatile* victim = deque_to_resume->worker;
	if (victim) { // deque hasn't been mugged yet

		// At this point, the original owner is done accessing the deque
		// But a thief may be trying to mug this deque
		__cilkrts_mutex_lock(w, &victim->l->lock); {
			if (deque_to_resume->worker) { // still muggable
				mugged = 1;
				deque_mug(w, deque_to_resume);
			}
			
		} __cilkrts_mutex_unlock(w, &victim->l->lock);
	}
	CILK_ASSERT(deque_to_resume->self == NULL);
	if (mugged)
		fprintf(stderr, "(w: %i) resuming %p for %i\n", w->self,
						deque_to_resume, victim->self);
	else
		fprintf(stderr, "(w: %i) resuming free deque %p\n",
						w->self, deque_to_resume);

	// Resume this "non-resumable" deque
	deque_to_resume->resumable = 1;

	// This must be marked before switching, otherwise a thief may mug
	// the deque, after which no one would resume it, since the critical
	// section has already been executed.
	w->l->active_deque->resumable = (enable_resume) ? 1 : 0;

	cilk_fiber *current_fiber = deque_suspend(w, deque_to_resume);

	fiber_to_resume = deque_to_resume->fiber;
	CILK_ASSERT(fiber_to_resume);
	CILK_ASSERT(deque_to_resume->call_stack == w->current_stack_frame);
	deque_to_resume->fiber = NULL;

	// switch fibers
	cilk_fiber_data *data = cilk_fiber_get_data(fiber_to_resume);
	CILK_ASSERT(!data->resume_sf);
	data->owner = w;

	cilk_fiber_suspend_self_and_resume_other(current_fiber, fiber_to_resume);

}
