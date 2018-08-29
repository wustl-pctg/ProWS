#include <string.h> // memcpy

#include "deque_pool.h"
#include "local_state.h"
#include "full_frame.h"
#include "worker_mutex.h" // __cilkrts_mutex_lock/unlock
#include "scheduler.h" // __cilkrts_worker_lock/unlock

#define BEGIN_WITH_WORKER_LOCK(w) __cilkrts_worker_lock(w); do
#define END_WITH_WORKER_LOCK(w)   while (__cilkrts_worker_unlock(w), 0)
#define BEGIN_WITH_DEQUE_LOCK(w,d) __cilkrts_mutex_lock(w, &d->lock); do
#define END_WITH_DEQUE_LOCK(w,d)   while (__cilkrts_mutex_unlock(w, &d->lock), 0)

static void resize(deque_pool *p, size_t cap)
{
  p->capacity = cap;
  deque **new_array = (deque**) __cilkrts_malloc(cap * sizeof(deque*));

  if (!new_array)
    __cilkrts_bug("W%d could not resize deque pool to capacity %zu\n",
                  __cilkrts_get_tls_worker()->self, cap);

  memcpy(new_array, p->array, p->size * sizeof(deque*));
  new_array[p->size] = NULL;
  
  deque **old = p->array;
  p->array = new_array;
  __cilkrts_free(old);
}

void deque_pool_init(deque_pool *p, size_t ltqsize)
{
  p->array = NULL;
  p->size = 0;
  resize(p, ltqsize);
}

void deque_pool_free(deque_pool *p)
{
  // There should be no suspended deques
  CILK_ASSERT(p->size == 0);
  __cilkrts_free(p->array);
}

void* __cilkrts_get_deque(void)
{
  __cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
  return (void*)w->l->active_deque;
}

void __cilkrts_suspend_deque()
{
  __cilkrts_worker *w = __cilkrts_get_tls_worker_fast();
  cilk_fiber *current_fiber, *fiber_to_resume;

  // I think this was just for cleanliness, but somewhere along the
  // line it is not always true. I *think* it's okay to just not check
  // it here. But it makes me pretty uneasy...
  /* cilk_fiber_data* data = cilk_fiber_get_data((*w->l->frame_ff)->fiber_self); */
  /* CILK_ASSERT(data && data->resume_sf == NULL); */

  // Sets fiber in active deque
  current_fiber = deque_suspend(w, NULL);
  
  CILK_ASSERT(current_fiber);
  if (w->l->active_deque) {

    // If we're jumping to a mugged deque/fiber, wait to make sure it's resumable
    if (w->l->active_deque->fiber != w->l->scheduling_fiber) {
      while (!w->l->active_deque->fiber
             || !cilk_fiber_is_resumable(w->l->active_deque->fiber));

      // This deque was mugged
      /* w->l->mugged--; */
      /* CILK_ASSERT(w->l->mugged >= 0); */
    }
    
    fiber_to_resume = w->l->active_deque->fiber;
    w->l->active_deque->fiber = NULL;
  } else { // no more memory for deques
    fiber_to_resume = w->l->scheduling_fiber;
  }
  cilk_fiber_suspend_self_and_resume_other(current_fiber,
                                           fiber_to_resume);
}

void __cilkrts_make_resumable(void* _deque)
{
  __cilkrts_worker *w = __cilkrts_get_tls_worker_fast();

  deque *deque_to_resume = (deque*) _deque;

  CILK_ASSERT(deque_to_resume->resumable == 0);

  // At this point, no one else can resume the deque.
  // Wait for the deque to be fully suspended.
  while (!deque_to_resume->fiber
         || !cilk_fiber_is_resumable(deque_to_resume->fiber));

  int previously_owned = 0;
  __cilkrts_worker volatile* victim = deque_to_resume->worker;
  if (victim) { 

    __cilkrts_mutex_lock(w, &victim->l->lock); {
      if (deque_to_resume->worker) { 
        previously_owned = 1;
        deque_pool_remove(&victim->l->suspended_deques, deque_to_resume);
        CILK_ASSERT(deque_to_resume->self == INVALID_DEQUE_INDEX);
        deque_to_resume->resumable = 1;
        deque_pool_add(victim, &victim->l->resumable_deques, deque_to_resume);
      }
      
    } __cilkrts_mutex_unlock(w, &victim->l->lock);
  }


  if (previously_owned) {
    DEQUE_LOG("(w: %i) making %p resumable for %i\n", w->self,
              deque_to_resume, victim->self);
  } else {

    __cilkrts_worker *victim = NULL;
    if (w->g->total_workers > 1) {
      int victim_idx = myrand(w) % (w->g->total_workers);
      int victim2_idx = myrand(w) % (w->g->total_workers - 1);
      if (victim2_idx >= victim_idx) victim2_idx++;

      victim = w->g->workers[victim_idx];
      __cilkrts_worker *potential_victim = w->g->workers[victim2_idx];

      if (victim->l->resumable_deques.size > potential_victim->l->resumable_deques.size) {
          victim = potential_victim;
      }
    } else {
      victim = w;
    }

    CILK_ASSERT(deque_to_resume->self == INVALID_DEQUE_INDEX);
    deque_to_resume->resumable = 1;
    deque_to_resume->worker = victim;
    __cilkrts_mutex_lock(w, &victim->l->lock);
    deque_pool_add(victim, &victim->l->resumable_deques, deque_to_resume);
    __cilkrts_mutex_unlock(w, &victim->l->lock);

    DEQUE_LOG("(w: %i) adding resumable free deque %p to resumables for %i\n",
            w->self, deque_to_resume, victim->self);
  }

}

#ifdef TRACK_FIBER_COUNT
void decrement_fiber_count(global_state_t* g);
#endif
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
  /* w->l->mugged--; */
  /* CILK_ASSERT(w->l->mugged >= 0); */
  CILK_ASSERT(deque_to_resume->self == INVALID_DEQUE_INDEX);
  if (mugged) {
    DEQUE_LOG("(w: %i) resuming %p for %i\n", w->self,
              deque_to_resume, victim->self);
  } else {
    DEQUE_LOG("(w: %i) resuming free deque %p\n",
            w->self, deque_to_resume);
  }

  // Resume this "non-resumable" deque
  deque_to_resume->resumable = 1;

  // This must be marked before switching, otherwise a thief may mug
  // the deque, after which no one would resume it, since the critical
  // section has already been executed.
  fiber_to_resume = deque_to_resume->fiber;

  cilk_fiber_data *data = cilk_fiber_get_data(fiber_to_resume);
  CILK_ASSERT(!data->resume_sf);
  data->owner = w;
  deque_to_resume->call_stack->worker = w;

  #ifdef COLLECT_STEAL_STATS
    w->l->ks_stats.deques_resumed++;
  #endif

  // Basically, if we are operating on a true future we can destroy the old deque.
  if (enable_resume == 2) { // && !w->current_stack_frame->call_parent && !(*w->l->frame_ff)->parent && !(w->current_stack_frame->flags & CILK_FRAME_LAST)) {
    cilk_fiber *current_fiber = cilk_fiber_get_current_fiber();

    deque_to_resume->resumable = 0;
    deque *to_destroy = w->l->active_deque;

    BEGIN_WITH_WORKER_LOCK(w) {
        w->l->active_deque = deque_to_resume;
        deque_switch(w, deque_to_resume);
    } END_WITH_WORKER_LOCK(w);

    deque_destroy(to_destroy);

    deque_to_resume->fiber = NULL;

    cilkg_decrement_active_workers(w->g);

    CILK_ASSERT(deque_to_resume->call_stack == w->current_stack_frame);
    CILK_ASSERT(deque_to_resume->call_stack != NULL);

    cilk_fiber_take(fiber_to_resume);
    if (w->l->future_fiber_pool_idx < (MAX_FUTURE_FIBERS_IN_POOL-1)) {
        w->l->future_fiber_pool[++w->l->future_fiber_pool_idx] = current_fiber;
#ifdef TRACK_FIBER_COUNT
        decrement_fiber_count(w->g);
#endif
        cilk_fiber_suspend_self_and_resume_other(current_fiber, fiber_to_resume);
        CILK_ASSERT(!"Should not get back here!");
    } else {
        CILK_ASSERT(!cilk_fiber_is_resumable(current_fiber));
        cilk_fiber_remove_reference_from_self_and_resume_other(current_fiber, &(w->l->fiber_pool), fiber_to_resume);  
    }
  } else {
    w->l->active_deque->resumable = (enable_resume) ? 1 : 0;
    cilk_fiber *current_fiber = deque_suspend(w, deque_to_resume);

    CILK_ASSERT(w->l->active_deque);
    CILK_ASSERT(fiber_to_resume == w->l->active_deque->fiber);
    CILK_ASSERT(fiber_to_resume);
    CILK_ASSERT(deque_to_resume->call_stack == w->current_stack_frame);

    w->l->active_deque->fiber = NULL;
    deque_to_resume->fiber = NULL;

    // switch fibers
    cilk_fiber_suspend_self_and_resume_other(current_fiber, fiber_to_resume);
  }

}

void deque_pool_add(__cilkrts_worker *victim, deque_pool *p, deque *d)
{
  __cilkrts_worker *w = __cilkrts_get_tls_worker();
  CILK_ASSERT(victim->l->lock.owner == w);
  if (d->resumable)
    CILK_ASSERT(&victim->l->resumable_deques == p);
  else 
    CILK_ASSERT(&victim->l->suspended_deques == p);

  if (p->size == p->capacity) {
    size_t cap = 2 * p->capacity;
    DEQUE_LOG("(w: %i) resizing %i's deque to %zu\n",
            w->self, victim->self, cap);
    resize(p, 2 * p->capacity);
  }

  d->self = p->size;
  d->worker = victim;
  __asm__  volatile("": : :"memory");
  p->array[p->size++] = d;
  CILK_ASSERT(p->size <= p->capacity);

  deque_pool_validate(p, victim);
}

void deque_pool_remove(deque_pool *p, deque *d)
{
  __cilkrts_worker *w = __cilkrts_get_tls_worker();
  //  CILK_ASSERT(d->lock.owner == w);

  CILK_ASSERT(d->self >= 0 && d->self < p->size);
  CILK_ASSERT(p->array[d->self] == d);

  // Swap deque with end
  int last = p->size - 1;
  p->array[d->self] = p->array[last];

  // Make sure the swapped deque knows its new position
  p->array[last]->self = d->self;

  // Overwrite for sanity
  p->array[last] = NULL;

  DEQUE_LOG("(w: %i) erasing position %i from %i.\n",
          w->self, last, d->worker->self);

  p->size--;

  CILK_ASSERT(p->size >= 0);
  
  // "Dobby is a free deque!"
  d->self = INVALID_DEQUE_INDEX;
  deque_pool_validate(p, d->worker);
  d->worker = NULL;
}

void deque_pool_validate(deque_pool *p, __cilkrts_worker* w)
{
  CILK_ASSERT(w->l->lock.owner != NULL);

  for (int i = 0; i < p->size; ++i) {
    if (p->array[i] == NULL) {
      DEQUE_LOG("(w: %i) has an invalid deque at position %i.\n",
              w->self, i);
      CILK_ASSERT(0);
    }
  }
}
