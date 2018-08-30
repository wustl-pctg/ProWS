#include "cilk_fiber.h"
#include "deque.h"
#include "local_state.h"
#include "scheduler.h"

extern "C" {

void __cilkrts_reset_future_fiber_deque() {
    __cilkrts_worker *curr_worker = __cilkrts_get_tls_worker_fast();
    deque* d = curr_worker->l->active_deque;

    d->fiber_tail = d->fiber_head = d->fiber_ltq;
}

void __cilkrts_enqueue_future_fiber(cilk_fiber *fiber) {
    __cilkrts_worker *curr_worker = __cilkrts_get_tls_worker_fast();

    cilk_fiber *volatile *tail = curr_worker->l->active_deque->fiber_tail;
    *tail++ = fiber;
    CILK_ASSERT(tail < curr_worker->l->active_deque->fiber_ltq_limit);

    curr_worker->l->active_deque->fiber_tail = tail;
}

#define BEGIN_WITH_WORKER_LOCK(w) __cilkrts_worker_lock(w); do
#define END_WITH_WORKER_LOCK(w)   while (__cilkrts_worker_unlock(w), 0)

cilk_fiber* __cilkrts_pop_tail_future_fiber() {
    __cilkrts_worker *curr_worker = __cilkrts_get_tls_worker_fast();

    cilk_fiber *volatile *t = curr_worker->l->active_deque->fiber_tail;
    t--; 
    CILK_ASSERT( t >= curr_worker->l->active_deque->fiber_head );
    curr_worker->l->active_deque->fiber_tail = t;
    
    
    // TODO: I don't think we need to check if it is safe; it should
    //       ALWAYS be safe, since we synchronize on the corresponding
    //       stack frame when necessary.
    //if (/*__builtin_expect(*/t < curr_worker->l->active_deque->fiber_head/*, 0)*/) {
    //    int stolen_p = 0;

    //    BEGIN_WITH_WORKER_LOCK(curr_worker) {
    //        stolen_p = !(curr_worker->l->active_deque->fiber_head < (curr_worker->l->active_deque->fiber_tail + 1));

    //        if (/*__builtin_expect(*/stolen_p/*, 0)*/) {
    //            *curr_worker->l->active_deque->fiber_tail++;
    //            CILK_ASSERT(curr_worker->l->active_deque->fiber_tail == curr_worker->l->active_deque->fiber_head);
    //            CILK_ASSERT(!"There should be a fiber tail if we are popping");
    //            return NULL;
    //        }
    //    } END_WITH_WORKER_LOCK(curr_worker);
    //}

    return *t;
}

cilk_fiber* __cilkrts_pop_head_future_fiber(__cilkrts_worker *victim, deque *d) {
    // TODO: I don't think we need to check if it is safe; it should
    //       ALWAYS be safe, since we synchronize on the corresponding
    //       stack frame when necessary.
    //if (/*__builtin_expect(*/fiber_dekker_protocol(victim, d)/*, 1)*/) {
        cilk_fiber **h = (cilk_fiber **)d->fiber_head;
        d->fiber_head = d->fiber_head + 1;
        CILK_ASSERT( d->fiber_head <= d->fiber_tail );
        return *h;
    //} else {
    //    CILK_ASSERT("There should be a head when we pop future fibers");
    //    return NULL;
    //}
}

}
