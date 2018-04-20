#include "except.h"
#include "cilk-ittnotify.h"
#include "sysdep.h"
#include "local_state.h"
#include "record-replay.h"
#include "reducer_impl.h"
#include "scheduler.h"

#define verify_current_wkr(w)   ;

#define BEGIN_WITH_FRAME_LOCK(w, ff)                                     \
    do { full_frame *_locked_ff = ff; __cilkrts_frame_lock(w, _locked_ff); do

#define END_WITH_FRAME_LOCK(w, ff)                       \
    while (__cilkrts_frame_unlock(w, _locked_ff), 0); } while (0)

#define BEGIN_WITH_WORKER_LOCK(w) __cilkrts_worker_lock(w); do
#define END_WITH_WORKER_LOCK(w)   while (__cilkrts_worker_unlock(w), 0)

#define BEGIN_WITH_WORKER_LOCK_OPTIONAL(w) __cilkrts_worker_lock(w); do
#define END_WITH_WORKER_LOCK_OPTIONAL(w)   while (__cilkrts_worker_unlock(w), 0)

#   define ASSERT_WORKER_LOCK_OWNED(w) \
        { \
            __cilkrts_worker *tls_worker = __cilkrts_get_tls_worker(); \
            CILK_ASSERT((w)->l->lock.owner == tls_worker); \
        }

enum provably_good_steal_t
{
    ABANDON_EXECUTION,  // Not the last child to the sync - attempt to steal work
    CONTINUE_EXECUTION, // Last child to the sync - continue executing on this worker
    WAIT_FOR_CONTINUE   // The replay log indicates that this was the worker
                        // which continued.  Loop until we are the last worker
                        // to the sync.
};


typedef struct {
    /** A pointer to the location of our left reducer map. */
    struct cilkred_map **map_ptr;

    /** A pointer to the location of our left exception. */
    struct pending_exception_info **exception_ptr;
} splice_left_ptrs;

static inline
splice_left_ptrs compute_left_ptrs_for_sync(__cilkrts_worker *w,
                                            full_frame *ff) {
    // ASSERT: we hold the lock on ff
    splice_left_ptrs left_ptrs;

    // Figure out which map to the left we should merge into.
    if (ff->rightmost_child) {
        CILK_ASSERT(ff->rightmost_child->parent == ff);
        left_ptrs.map_ptr = &(ff->rightmost_child->right_reducer_map);
        left_ptrs.exception_ptr = &(ff->rightmost_child->right_pending_exception);
    }
    else {
        // We have no children.  Then, we should be the last
        // worker at the sync... "left" is our child map.
        left_ptrs.map_ptr = &(ff->children_reducer_map);
        left_ptrs.exception_ptr = &(ff->child_pending_exception);
    }
    return left_ptrs;
}

static inline
int fast_path_reductions_for_sync(__cilkrts_worker *w,
                                  full_frame *ff) {
    // Return 0 if there is some reduction that needs to happen.
    return !(w->reducer_map  || ff->pending_exception);
}

static __cilkrts_worker*
slow_path_reductions_for_sync(__cilkrts_worker *w,
                              full_frame *ff) {
    struct cilkred_map *left_map;
    struct cilkred_map *middle_map;
    
#if (REDPAR_DEBUG > 0)
    CILK_ASSERT(ff);
    CILK_ASSERT(w->head == w->tail);
#endif

    middle_map = w->reducer_map;
    w->reducer_map = NULL;

    // Loop invariant: middle_map should be valid (the current map to reduce). 
    //                 left_map is junk.
    //                 w->reducer_map == NULL.
    while (1) {
        BEGIN_WITH_FRAME_LOCK(w, ff) {
            splice_left_ptrs left_ptrs = compute_left_ptrs_for_sync(w, ff);
            
            // Grab the "left" map and store pointers to those locations.
            left_map = *(left_ptrs.map_ptr);
            *(left_ptrs.map_ptr) = NULL;
            
            // Slide the maps in our struct left as far as possible.
            if (!left_map) {
                left_map = middle_map;
                middle_map = NULL;
            }

            *(left_ptrs.exception_ptr) =
                __cilkrts_merge_pending_exceptions(w,
                                                   *left_ptrs.exception_ptr,
                                                   ff->pending_exception);
            ff->pending_exception = NULL;

            // If there is no middle map, then we are done.
            // Deposit left and return.
            if (!middle_map) {
                *(left_ptrs).map_ptr = left_map;
                #if (REDPAR_DEBUG > 0)
                CILK_ASSERT(NULL == w->reducer_map);
                #endif
                // Sanity check upon leaving the loop.
                verify_current_wkr(w);
                // Make sure to unlock before we return!
                __cilkrts_frame_unlock(w, ff);
                return w;
            }
        } END_WITH_FRAME_LOCK(w, ff);

        // After we've released the lock, start counting time as
        // WORKING again.
        STOP_INTERVAL(w, INTERVAL_IN_RUNTIME);
        START_INTERVAL(w, INTERVAL_WORKING);
        
        // If we get here, we have a nontrivial reduction to execute.
        middle_map = repeated_merge_reducer_maps(&w,
                                                 left_map,
                                                 middle_map);
        verify_current_wkr(w);

        STOP_INTERVAL(w, INTERVAL_WORKING);
        START_INTERVAL(w, INTERVAL_IN_RUNTIME);

        // Save any exceptions generated because of the reduction
        // process.  These get merged the next time around the
        // loop.
        CILK_ASSERT(NULL == ff->pending_exception);
        ff->pending_exception = w->l->pending_exception;
        w->l->pending_exception = NULL;
    }
    
    // We should never break out of the loop above.
    CILK_ASSERT(0);
    return NULL;
}

static void make_runnable(__cilkrts_worker *w, full_frame *ff) {
    w->l->frame_ff = ff;

    /* CALL_STACK is invalid (the information is stored implicitly in W) */
    ff->call_stack = 0;
}

static __cilkrts_worker*
execute_reductions_for_sync(__cilkrts_worker *w,
                            full_frame *ff,
                            __cilkrts_stack_frame *sf_at_sync) {
    int finished_reductions;
    // Step B1 from reducer protocol above:
    // Restore runtime invariants.
    //
    // The following code for this step is almost equivalent to
    // the following sequence:
    //   1. disown(w, ff, sf_at_sync, "sync") (which itself
    //        calls make_unrunnable(w, ff, sf_at_sync))
    //   2. make_runnable(w, ff, sf_at_sync).
    //
    // The "disown" will mark the frame "sf_at_sync"
    // as stolen and suspended, and save its place on the stack,
    // so it can be resumed after the sync. 
    //
    // The difference is, that we don't want the disown to 
    // break the following connections yet, since we are
    // about to immediately make sf/ff runnable again anyway.
    //   sf_at_sync->worker == w
    //   w->l->frame_ff == ff.
    //
    // These connections are needed for parallel reductions, since
    // we will use sf / ff as the stack frame / full frame for
    // executing any potential reductions.
    //
    // TBD: Can we refactor the disown / make_unrunnable code
    // to avoid the code duplication here?

    ff->call_stack = NULL;

    // Normally, "make_unrunnable" would add CILK_FRAME_STOLEN and
    // CILK_FRAME_SUSPENDED to sf_at_sync->flags and save the state of
    // the stack so that a worker can resume the frame in the correct
    // place.
    //
    // But on this path, CILK_FRAME_STOLEN should already be set.
    // Also, we technically don't want to suspend the frame until
    // the reduction finishes.
    // We do, however, need to save the stack before
    // we start any reductions, since the reductions might push more
    // data onto the stack.
    CILK_ASSERT(sf_at_sync->flags | CILK_FRAME_STOLEN);

    __cilkrts_put_stack(ff, sf_at_sync);
    __cilkrts_make_unrunnable_sysdep(w, ff, sf_at_sync, 1,
                                     "execute_reductions_for_sync");
    CILK_ASSERT(w->l->frame_ff == ff);

    // Step B2: Execute reductions on user stack.
    // Check if we have any "real" reductions to do.
    finished_reductions = fast_path_reductions_for_sync(w, ff);
    
    if (!finished_reductions) {
        // Still have some real reductions to execute.
        // Run them here.

        // This method may acquire/release the lock on ff.
        w = slow_path_reductions_for_sync(w, ff);

        // The previous call may return on a different worker.
        // than what we started on.
        verify_current_wkr(w);
    }

#if REDPAR_DEBUG >= 0
    CILK_ASSERT(w->l->frame_ff == ff);
    CILK_ASSERT(ff->call_stack == NULL);
#endif

    // Now we suspend the frame ff (since we've
    // finished the reductions).  Roughly, we've split apart the 
    // "make_unrunnable" call here --- we've already saved the
    // stack info earlier before the reductions execute.
    // All that remains is to restore the call stack back into the
    // full frame, and mark the frame as suspended.
    ff->call_stack = sf_at_sync;
    sf_at_sync->flags |= CILK_FRAME_SUSPENDED;

    // At a nontrivial sync, we should always free the current fiber,
    // because it can not be leftmost.
    //w->l->fiber_to_free = ff->fiber_self;
    ff->fiber_self = NULL;
    return w;
}

static inline void provably_good_steal_reducers(__cilkrts_worker *w,
                                                full_frame       *ff) {
    // No-op.
}

static void provably_good_steal_exceptions(__cilkrts_worker *w, 
                                           full_frame       *ff) {
    // ASSERT: we own ff->lock
    ff->pending_exception =
        __cilkrts_merge_pending_exceptions(w,
                                           ff->child_pending_exception,
                                           ff->pending_exception);
    ff->child_pending_exception = NULL;
}

static void provably_good_steal_stacks(__cilkrts_worker *w, full_frame *ff) {
    CILK_ASSERT(NULL == ff->fiber_self);
    ff->fiber_self = ff->fiber_child;
    //printf("Future sync! provably good steal fiber self = %p\n", ff->fiber_self);
    ff->fiber_child = NULL;
}

static void __cilkrts_mark_synched(full_frame *ff) {
    ff->call_stack->flags &= ~CILK_FRAME_UNSYNCHED;
    ff->simulated_stolen = 0;
}

static void unset_sync_master(__cilkrts_worker *w, full_frame *ff) {
    CILK_ASSERT(WORKER_USER == w->l->type);
    CILK_ASSERT(ff->sync_master == w);
    ff->sync_master = NULL;
    w->l->last_full_frame = NULL;
}

static int decjoin(full_frame *ff) {
    CILK_ASSERT(ff->join_counter > 0);
    return (--ff->join_counter);
}

static int simulate_decjoin(full_frame *ff) {
  CILK_ASSERT(ff->join_counter > 0);
  return (ff->join_counter - 1);
}

static full_frame *pop_next_frame(__cilkrts_worker *w) {
    full_frame *ff;
    ff = w->l->next_frame_ff;
    // Remove the frame from the next_frame field.
    //
    // If this is a user worker, then there is a chance that another worker
    // from our team could push work into our next_frame (if it is the last
    // worker doing work for this team).  The other worker's setting of the
    // next_frame could race with our setting of next_frame to NULL.  This is
    // the only possible race condition on next_frame.  However, if next_frame
    // has a non-NULL value, then it means the team still has work to do, and
    // there is no chance of another team member populating next_frame.  Thus,
    // it is safe to set next_frame to NULL, if it was populated.  There is no
    // need for an atomic op.
    if (NULL != ff) {
        w->l->next_frame_ff = NULL;
    }
    return ff;
}

static void setup_for_execution_reducers(__cilkrts_worker *w,
                                         full_frame *ff) {
    // We only need to move ff->children_reducer_map into
    // w->reducer_map in case 1(a).
    //
    // First check whether ff is synched.
    __cilkrts_stack_frame *sf = ff->call_stack;
    if (!(sf->flags & CILK_FRAME_UNSYNCHED)) {
        // In this case, ff is synched. (Case 1).
        CILK_ASSERT(!ff->rightmost_child);

        // Test whether we are in case 1(a) and have
        // something to do.  Note that if both
        // ff->children_reducer_map and w->reducer_map are NULL, we
        // can't distinguish between cases 1(a) and 1(b) here.
        if (ff->children_reducer_map) {
            // We are in Case 1(a).
            CILK_ASSERT(!w->reducer_map);
            w->reducer_map = ff->children_reducer_map;
            ff->children_reducer_map = NULL;
        }
    }
}

static void setup_for_execution_exceptions(__cilkrts_worker *w, 
                                           full_frame *ff) {
    CILK_ASSERT(NULL == w->l->pending_exception);
    w->l->pending_exception = ff->pending_exception;
    ff->pending_exception = NULL;
}

static void setup_for_execution_pedigree(__cilkrts_worker *w) {
    int pedigree_unsynched;
    __cilkrts_stack_frame *sf = w->current_stack_frame;

    CILK_ASSERT(NULL != sf);

    // If this isn't an ABI 1 or later frame, there's no pedigree information
    if (0 == CILK_FRAME_VERSION_VALUE(sf->flags))
        return;

    // Note whether the pedigree is unsynched and clear the flag before
    // we forget
    pedigree_unsynched = sf->flags & CILK_FRAME_SF_PEDIGREE_UNSYNCHED;
    sf->flags &= ~CILK_FRAME_SF_PEDIGREE_UNSYNCHED;

    // If we're just marshalling onto this worker, do not increment
    // the rank since that wouldn't happen in a sequential execution
    if (w->l->work_stolen || pedigree_unsynched)
    {
        if (w->l->work_stolen)
            w->pedigree.rank = sf->parent_pedigree.rank + 1;
        else
            w->pedigree.rank = sf->parent_pedigree.rank;
    }

    w->pedigree.parent = sf->parent_pedigree.parent;
    w->l->work_stolen = 0;
}

static void reset_THE_exception(__cilkrts_worker *w) {
    // The currently executing worker must own the worker lock to touch
    // w->exc
    ASSERT_WORKER_LOCK_OWNED(w);

    w->exc = w->head;
    __cilkrts_fence();
}

static void setup_for_execution(__cilkrts_worker *w, 
                                full_frame *ff,
                                int is_return_from_call) {
    // ASSERT: We own w->lock and ff->lock || P == 1

    setup_for_execution_reducers(w, ff);
    setup_for_execution_exceptions(w, ff);
    /*setup_for_execution_stack(w, ff);*/

    ff->call_stack->worker = w;
    w->current_stack_frame = ff->call_stack;

    // If this is a return from a call, leave the pedigree alone
    if (! is_return_from_call)
        setup_for_execution_pedigree(w);

    __cilkrts_setup_for_execution_sysdep(w, ff);

    w->head = w->tail = w->l->ltq;
    reset_THE_exception(w);

    make_runnable(w, ff);
}

static void enter_runtime_transition_proc(cilk_fiber *fiber) {
    // We can execute this method for one of three reasons:
    // 1. Undo-detach finds parent stolen.
    // 2. Sync suspends frame.
    // 3. Return from Cilk entry point.
    //
    //
    // In cases 1 and 2, the frame may be truly suspended or
    // may be immediately executed by this worker after provably_good_steal.
    //
    // 
    // There is a fourth case, which can, but does not need to execute
    // this function:
    //   4. Starting up the scheduling loop on a user or
    //      system worker.  In this case, we won't have
    //      a scheduling stack function to run.
    __cilkrts_worker* w = cilk_fiber_get_owner(fiber);
    if (w->l->post_suspend) {
        // Run the continuation function passed to longjmp_into_runtime
        run_scheduling_stack_fcn(w);

        // After we have jumped into the runtime and run the
        // scheduling function, any reducer map the worker had before entering the runtime
        // should have already been saved into the appropriate full
        // frame.
        CILK_ASSERT(NULL == w->reducer_map);

        // There shouldn't be any uncaught exceptions.
        //
        // In Windows, the OS catches any exceptions not caught by the
        // user code.  Thus, we are omitting the check on Windows.
        //
        // On Android, calling std::uncaught_exception with the stlport
        // library causes a seg fault.  Since we're not supporting
        // exceptions there at this point, just don't do the check
        //
        // TBD: Is this check also safe to do on Windows? 
        CILKBUG_ASSERT_NO_UNCAUGHT_EXCEPTION();
    }
}

static inline NORETURN
cilkrts_resume(__cilkrts_stack_frame *sf, full_frame *ff)
{
    // Save the sync stack pointer, and do the bookkeeping
    char* sync_sp = ff->sync_sp;
    __cilkrts_take_stack(ff, sync_sp);  // leaves ff->sync_sp null

    sf->flags &= ~CILK_FRAME_SUSPENDED;
    // Actually longjmp to the user code.
    // We may have exceptions to deal with, since we are resuming
    // a previous-suspended frame.
    sysdep_longjmp_to_sf(sync_sp, sf, ff);
}

static NORETURN
user_code_resume_after_switch_into_runtime(cilk_fiber *fiber) {
    __cilkrts_worker *w = cilk_fiber_get_owner(fiber);
    __cilkrts_stack_frame *sf;
    full_frame *ff;
    sf = w->current_stack_frame;
    ff = sf->worker->l->frame_ff;

#if FIBER_DEBUG >= 1    
    CILK_ASSERT(ff->fiber_self == fiber);
    cilk_fiber_data *fdata = cilk_fiber_get_data(fiber);
    DBGPRINTF ("%d-%p: resume_after_switch_into_runtime, fiber=%p\n",
               w->self, w, fiber);
    CILK_ASSERT(sf == fdata->resume_sf);
#endif

    // Notify the Intel tools that we're stealing code
    ITT_SYNC_ACQUIRED(sf->worker);
    NOTIFY_ZC_INTRINSIC("cilk_continue", sf);
    cilk_fiber_invoke_tbb_stack_op(fiber, CILK_TBB_STACK_ADOPT);

    // Actually jump to user code.
    cilkrts_resume(sf, ff);
}


static void kyles_scheduling_fiber_prepare_to_resume_user_code(__cilkrts_worker *w,
                                                  full_frame *ff,
                                                  __cilkrts_stack_frame *sf) {
    w->current_stack_frame = sf;
    sf->worker = w;

    // Lots of debugging checks on the state of the fiber we might be
    // resuming.
#if FIBER_DEBUG >= 1
#   if FIBER_DEBUG >= 3
    {
        fprintf(stderr, "w=%d: ff=%p, sf=%p. about to resume user code\n",
                w->self, ff, sf);
    }
#   endif

    const int flags = sf->flags;
    CILK_ASSERT(flags & CILK_FRAME_SUSPENDED);
    CILK_ASSERT(!sf->call_parent);
    CILK_ASSERT(w->head == w->tail);

    /* A frame can not be resumed unless it was suspended. */
    CILK_ASSERT(ff->sync_sp != NULL);

    /* The leftmost frame has no allocated stack */
    if (ff->simulated_stolen)
        CILK_ASSERT(flags & CILK_FRAME_UNSYNCHED);
    else if (flags & CILK_FRAME_UNSYNCHED)
        /* XXX By coincidence sync_sp could be null. */
        CILK_ASSERT(ff->fiber_self != NULL);
    else
        /* XXX This frame could be resumed unsynched on the leftmost stack */
        CILK_ASSERT((ff->sync_master == 0 || ff->sync_master == w));
    CILK_ASSERT(w->l->frame_ff == ff);
#endif    
}
static NORETURN
longjmp_into_runtime(__cilkrts_worker *w,
                     scheduling_stack_fcn_t fcn,
                     __cilkrts_stack_frame *sf) {
    full_frame *ff, *ff2;

    CILK_ASSERT(!w->l->post_suspend);
    ff = w->l->frame_ff;
    CILK_ASSERT(ff->future_flags == CILK_FUTURE_PARENT);

    if (1 == w->g->P) {
        fcn(w, ff, sf);

        /* The call to function c() will have pushed ff as the next frame.  If
         * this were a normal (non-forced-reduce) execution, there would have
         * been a pop_next_frame call in a separate part of the runtime.  We
         * must call pop_next_frame here to complete the push/pop cycle. */
        ff2 = pop_next_frame(w);

        setup_for_execution(w, ff2, 0);
        kyles_scheduling_fiber_prepare_to_resume_user_code(w, ff2, w->current_stack_frame);
        cilkrts_resume(w->current_stack_frame, ff2);
        
// Suppress clang warning that the expression result is unused
#if defined(__clang__) && (! defined(__INTEL_COMPILER))
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wunused-value"
#endif // __clang__
        /* no return */
        CILK_ASSERT(((void)"returned from __cilkrts_resume", 0));
#if defined(__clang__) && (! defined(__INTEL_COMPILER))
#   pragma clang diagnostic pop
#endif // __clang__
    }

    w->l->post_suspend = fcn;
    w->l->suspended_stack = sf;

    ITT_SYNC_RELEASING(w);
    ITT_SYNC_PREPARE(w);

#if FIBER_DEBUG >= 2
    fprintf(stderr, "ThreadId=%p, W=%d: about to switch into runtime... w->l->frame_ff = %p, sf=%p\n",
            cilkos_get_current_thread_id(),
            w->self, w->l->frame_ff,
            sf);
#endif

    // Current fiber is either the (1) one we are about to free,
    // or (2) it has been passed up to the parent.
    /*cilk_fiber *current_fiber = ( w->l->fiber_to_free ?
                                  w->l->fiber_to_free :
                                  w->l->frame_ff->parent->fiber_child );
    */
    cilk_fiber *current_fiber = w->l->frame_ff->fiber_child;
    cilk_fiber_data* fdata = cilk_fiber_get_data(current_fiber);
    CILK_ASSERT(NULL == w->l->frame_ff->fiber_self);

    // Clear the sf in the current fiber for cleanliness, to prevent
    // us from accidentally resuming a bad sf.
    // Technically, resume_sf gets overwritten for a fiber when
    // we are about to resume it anyway.
    fdata->resume_sf = NULL;
    CILK_ASSERT(fdata->owner == w);

    // Set the function to execute immediately after switching to the
    // scheduling fiber, but before freeing any fibers.
    cilk_fiber_set_post_switch_proc(w->l->scheduling_fiber,
                                    enter_runtime_transition_proc);
    cilk_fiber_invoke_tbb_stack_op(current_fiber, CILK_TBB_STACK_ORPHAN);
    
    if (w->l->fiber_to_free) {
        //printf("Future parent freeing fiber in sync (%p)\n", current_fiber);
        // Case 1: we are freeing this fiber.  We never
        // resume this fiber again after jumping into the runtime.
        w->l->fiber_to_free = NULL;

        // Extra check. Normally, the fiber we are about to switch to
        // should have a NULL owner.
        CILK_ASSERT(NULL == cilk_fiber_get_data(w->l->scheduling_fiber)->owner);
#if FIBER_DEBUG >= 4
        fprintf(stderr, "ThreadId=%p, W=%d: about to switch into runtime.. current_fiber = %p, deallcoate, switch to fiber %p\n",
                cilkos_get_current_thread_id(),
                w->self,
                current_fiber, w->l->scheduling_fiber);
#endif
        cilk_fiber_invoke_tbb_stack_op(current_fiber, CILK_TBB_STACK_RELEASE);
        NOTE_INTERVAL(w, INTERVAL_DEALLOCATE_RESUME_OTHER);
        cilk_fiber_remove_reference_from_self_and_resume_other(current_fiber,
                                                               &w->l->fiber_pool,
                                                               w->l->scheduling_fiber);
        // We should never come back here!
        CILK_ASSERT(0);
    }
    else {        
        // Case 2: We are passing the fiber to our parent because we
        // are leftmost.  We should come back later to
        // resume execution of user code.
        //
        // If we are not freeing a fiber, there we must be
        // returning from a spawn or processing an exception.  The
        // "sync" path always frees a fiber.
        // 
        // We must be the leftmost child, and by left holder logic, we
        // have already moved the current fiber into our parent full
        // frame.
#if FIBER_DEBUG >= 2
        fprintf(stderr, "ThreadId=%p, W=%d: about to suspend self into runtime.. current_fiber = %p, deallcoate, switch to fiber %p\n",
                cilkos_get_current_thread_id(),
                w->self,
                current_fiber, w->l->scheduling_fiber);
#endif

        NOTE_INTERVAL(w, INTERVAL_SUSPEND_RESUME_OTHER);

        cilk_fiber_suspend_self_and_resume_other(current_fiber,
                                                 w->l->scheduling_fiber);
        // Resuming this fiber returns control back to
        // this function because our implementation uses OS fibers.
        //
        // On Unix, we could have the choice of passing the
        // user_code_resume_after_switch_into_runtime as an extra "resume_proc"
        // that resumes execution of user code instead of the
        // jumping back here, and then jumping back to user code.
#if FIBER_DEBUG >= 2
        CILK_ASSERT(fdata->owner == __cilkrts_get_tls_worker());
#endif
        user_code_resume_after_switch_into_runtime(current_fiber);
    }
}

static
enum provably_good_steal_t provably_good_steal(__cilkrts_worker *w,
                                               full_frame       *ff) {
    // ASSERT: we hold w->lock and ff->lock

    enum provably_good_steal_t result = ABANDON_EXECUTION;

    // If the current replay entry is a sync record matching the worker's
    // pedigree, AND this isn't the last child to the sync, return
    // WAIT_FOR_CONTINUE to indicate that the caller should loop until
    // we find the right frame to steal and CONTINUE_EXECUTION is returned.
    int match_found = replay_match_sync_pedigree(w);
    if (match_found && (0 != simulate_decjoin(ff)))
        return WAIT_FOR_CONTINUE;

    START_INTERVAL(w, INTERVAL_PROVABLY_GOOD_STEAL) {
        if (decjoin(ff) == 0) {
            provably_good_steal_reducers(w, ff);
            provably_good_steal_exceptions(w, ff);
            provably_good_steal_stacks(w, ff);
            __cilkrts_mark_synched(ff);

            // If the original owner wants this frame back (to resume
            // it on its original thread) pass it back now.
            if (NULL != ff->sync_master) {
                // The frame wants to go back and be executed by the original
                // user thread.  We can throw caution to the wind and push the
                // frame straight onto its queue because the only way we have
                // gotten to this point of being able to continue execution of
                // the frame is if the original user worker is spinning without
                // work.

                unset_sync_master(w->l->team, ff);
                __cilkrts_push_next_frame(w->l->team, ff);

                // If this is the team leader we're not abandoning the work
                if (w == w->l->team)
                    result = CONTINUE_EXECUTION;
            } else {
                __cilkrts_push_next_frame(w, ff);
                result = CONTINUE_EXECUTION;  // Continue working on this thread
            }

            // The __cilkrts_push_next_frame() call changes ownership
            // of ff to the specified worker.
        }
    } STOP_INTERVAL(w, INTERVAL_PROVABLY_GOOD_STEAL);

    // Only write a SYNC record if:
    // - We're recording a log *AND*
    // - We're the worker continuing from this sync
    replay_record_sync(w, result == CONTINUE_EXECUTION);

    // If we're replaying a log, and matched a sync from the log, mark the
    // sync record seen if the sync isn't going to be abandoned.
    replay_advance_from_sync (w, match_found, result == CONTINUE_EXECUTION);

    return result;
}

static void do_sync(__cilkrts_worker *w, full_frame *ff,
                    __cilkrts_stack_frame *sf) {
    //int abandoned = 1;
    enum provably_good_steal_t steal_result = ABANDON_EXECUTION;

    START_INTERVAL(w, INTERVAL_SYNC_CHECK) {
        BEGIN_WITH_WORKER_LOCK_OPTIONAL(w) {

            CILK_ASSERT(ff);
            BEGIN_WITH_FRAME_LOCK(w, ff) {
                CILK_ASSERT(sf->call_parent == 0);
                CILK_ASSERT(sf->flags & CILK_FRAME_UNSYNCHED);

                // Before switching into the scheduling fiber, we should have
                // already taken care of deallocating the current
                // fiber. 
                CILK_ASSERT(NULL == ff->fiber_self);

                // Update the frame's pedigree information if this is an ABI 1
                // or later frame
                if (CILK_FRAME_VERSION_VALUE(sf->flags) >= 1)
                {
                    sf->parent_pedigree.rank = w->pedigree.rank;
                    sf->parent_pedigree.parent = w->pedigree.parent;

                    // Note that the pedigree rank needs to be updated
                    // when setup_for_execution_pedigree runs
                    sf->flags |= CILK_FRAME_SF_PEDIGREE_UNSYNCHED;
                }

                /* the decjoin() occurs in provably_good_steal() */
                steal_result = provably_good_steal(w, ff);
                //printf("Future Parent do_sync fiber child: %p\n", ff->fiber_child);

            } END_WITH_FRAME_LOCK(w, ff);
            // set w->l->frame_ff = NULL after checking abandoned
            if (WAIT_FOR_CONTINUE != steal_result) {
                w->l->frame_ff = NULL;
            }
        } END_WITH_WORKER_LOCK_OPTIONAL(w);
    } STOP_INTERVAL(w, INTERVAL_SYNC_CHECK);

    // Now, if we are in a replay situation and provably_good_steal() returned
    // WAIT_FOR_CONTINUE, we should sleep, reacquire locks, call
    // provably_good_steal(), and release locks until we get a value other
    // than WAIT_FOR_CONTINUE from the function.
#ifdef CILK_RECORD_REPLAY
    // We don't have to explicitly check for REPLAY_LOG below because
    // steal_result can only be set to WAIT_FOR_CONTINUE during replay
    while(WAIT_FOR_CONTINUE == steal_result)
    {
        __cilkrts_sleep();
        BEGIN_WITH_WORKER_LOCK_OPTIONAL(w)
        {
            ff = w->l->frame_ff;
            BEGIN_WITH_FRAME_LOCK(w, ff)
            {
                steal_result = provably_good_steal(w, ff);
            } END_WITH_FRAME_LOCK(w, ff);
            if (WAIT_FOR_CONTINUE != steal_result)
                w->l->frame_ff = NULL;
        } END_WITH_WORKER_LOCK_OPTIONAL(w);
    }
#endif  // CILK_RECORD_REPLAY

#ifdef ENABLE_NOTIFY_ZC_INTRINSIC
    // If we can't make any further progress on this thread, tell Inspector
    // that we're abandoning the work and will go find something else to do.
    if (ABANDON_EXECUTION == steal_result)
    {
        NOTIFY_ZC_INTRINSIC("cilk_sync_abandon", 0);
    }
#endif // defined ENABLE_NOTIFY_ZC_INTRINSIC

    return; /* back to scheduler loop */
}

NORETURN __cilkrts_c_future_sync(__cilkrts_worker *w,
                                 __cilkrts_stack_frame *sf_at_sync) {
    full_frame *ff; 
    STOP_INTERVAL(w, INTERVAL_WORKING);
    START_INTERVAL(w, INTERVAL_IN_RUNTIME);

    // Claim: This read of w->l->frame_ff can occur without
    // holding the worker lock because when w has reached a sync
    // and entered the runtime (because it stalls), w's deque is empty
    // and no one else can steal and change w->l->frame_ff.

    ff = w->l->frame_ff;

    // Move any pending exceptions into the full frame
    CILK_ASSERT(NULL == ff->pending_exception);
    ff->pending_exception = w->l->pending_exception;
    w->l->pending_exception = NULL;
    
    w = execute_reductions_for_sync(w, ff, sf_at_sync);

#if FIBER_DEBUG >= 3
    fprintf(stderr, "ThreadId=%p, w->self = %d. about to longjmp_into_runtim[c_sync] with ff=%p\n",
            cilkos_get_current_thread_id(), w->self, ff);
#endif    

    longjmp_into_runtime(w, do_sync, sf_at_sync);
}

CILK_ABI_VOID __cilkrts_future_sync(__cilkrts_stack_frame *sf) {
    //printf("Syncing future parent!\n");
    __cilkrts_worker *w = sf->worker;

    CILK_ASSERT(sf->flags >= 1);
    if (CILK_FRAME_VERSION_VALUE(sf->flags) >= 1) {
        sf->parent_pedigree.rank = w->pedigree.rank;
        sf->parent_pedigree.parent = w->pedigree.parent;
    }

    if (__builtin_expect(!(sf->flags & CILK_FRAME_UNSYNCHED), 0))
        __cilkrts_bug("W%u: double sync %p\n", w->self, sf);

    if (__builtin_expect(sf->flags & CILK_FRAME_EXCEPTING, 0)) {
        CILK_ASSERT(0); // KYLE_TODO: I think we shouldn't take this path [yet].
        __cilkrts_c_sync_except(w, sf);
    }

    __cilkrts_c_future_sync(w, sf);
}
