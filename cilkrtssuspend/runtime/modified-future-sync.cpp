#include "except.h"
#include "cilk-ittnotify.h"
#include "sysdep.h"
#include "local_state.h"
#include "record-replay.h"
#include "reducer_impl.h"
#include "scheduler.h"

#define BEGIN_WITH_WORKER_LOCK(w) __cilkrts_worker_lock(w); do
#define END_WITH_WORKER_LOCK(w)   while (__cilkrts_worker_unlock(w), 0)

#define BEGIN_WITH_WORKER_LOCK_OPTIONAL(w) __cilkrts_worker_lock(w); do
#define END_WITH_WORKER_LOCK_OPTIONAL(w)   while (__cilkrts_worker_unlock(w), 0)

#define BEGIN_WITH_FRAME_LOCK(w, ff)                                    \
    do { full_frame *_locked_ff = ff; __cilkrts_frame_lock(w, _locked_ff); do

#define END_WITH_FRAME_LOCK(w, ff)                                \
    while (__cilkrts_frame_unlock(w, _locked_ff), 0); } while (0)

#define verify_current_wkr(w)    ;

enum provably_good_steal_t {
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


extern int decjoin(full_frame* ff);

static int simulate_decjoin(full_frame* ff) {
    CILK_ASSERT(ff->join_counter > 0);
    return (ff->join_counter - 1);
}

static void unset_sync_master(__cilkrts_worker *w, full_frame *ff) {
    CILK_ASSERT(WORKER_USER == w->l->type);
    CILK_ASSERT(ff->sync_master == w);
    ff->sync_master = NULL;
    w->l->last_full_frame = NULL;
    w->g->original_deque = w->l->active_deque->team->l->active_deque;
}

static inline void provably_good_steal_reducers(__cilkrts_worker *w,
                                                full_frame       *ff)
{
    // No-op.
}

/* at a provably good steal, incorporate the accumulated exceptions of
   children into the parent's exception */
static void provably_good_steal_exceptions(__cilkrts_worker *w, 
                                           full_frame       *ff)
{
    // ASSERT: we own ff->lock
    ff->pending_exception =
        __cilkrts_merge_pending_exceptions(w,
                                           ff->child_pending_exception,
                                           ff->pending_exception);
    ff->child_pending_exception = NULL;
}

/* At sync discard the frame's old stack and take the leftmost child's. */
static void provably_good_steal_stacks(__cilkrts_worker *w, full_frame *ff)
{
    CILK_ASSERT(NULL == ff->fiber_self);
    ff->fiber_self = ff->fiber_child;
    ff->fiber_child = NULL;
}

static void __cilkrts_mark_synched(full_frame *ff)
{
    ff->call_stack->flags &= ~CILK_FRAME_UNSYNCHED;
    ff->simulated_stolen = 0;
}

static enum provably_good_steal_t provably_good_steal(__cilkrts_worker *w, full_frame *ff) {
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

                __cilkrts_worker *original = w->l->active_deque->team;
//                CILK_ASSERT(original == w->g->original_deque->team);
                CILK_ASSERT(original->l->suspended_deques.size == 0);
                CILK_ASSERT(original->l->resumable_deques.size == 0);
                /* unset_sync_master(w->l->active_deque->team, ff); */
                unset_sync_master(original, ff);
                
                __cilkrts_push_next_frame(w->l->active_deque->team, ff);

                // If this is the team leader we're not abandoning the work
                if (w == w->l->active_deque->team)
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

static void do_sync(__cilkrts_worker *w, full_frame *ff, __cilkrts_stack_frame *sf) {
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
                    /* sf->parent_pedigree.rank = w->pedigree.rank; */
                    /* sf->parent_pedigree.parent = w->pedigree.parent; */
                    sf->parent_pedigree = w->pedigree;
                    
                    
                    // Note that the pedigree rank needs to be updated
                    // when setup_for_execution_pedigree runs
                    sf->flags |= CILK_FRAME_SF_PEDIGREE_UNSYNCHED;
                }

                /* the decjoin() occurs in provably_good_steal() */
                steal_result = provably_good_steal(w, ff);

            } END_WITH_FRAME_LOCK(w, ff);
            // set w->l->frame_ff = NULL after checking abandoned
            if (WAIT_FOR_CONTINUE != steal_result) {
                *w->l->frame_ff = NULL;
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
            ff = *w->l->frame_ff;
            BEGIN_WITH_FRAME_LOCK(w, ff)
            {
                steal_result = provably_good_steal(w, ff);
            } END_WITH_FRAME_LOCK(w, ff);
            if (WAIT_FOR_CONTINUE != steal_result)
                *w->l->frame_ff = NULL;
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

static inline NORETURN cilkrts_resume(__cilkrts_stack_frame *sf, full_frame *ff) {
    // Save the sync stack pointer, and do the bookkeeping
    char* sync_sp = ff->sync_sp;
    __cilkrts_take_stack(ff, sync_sp);  // leaves ff->sync_sp null

    sf->flags &= ~CILK_FRAME_SUSPENDED;
    // Actually longjmp to the user code.
    // We may have exceptions to deal with, since we are resuming
    // a previous-suspended frame.
    sysdep_longjmp_to_sf(sync_sp, sf, ff);
}

static NORETURN user_code_resume_after_switch_into_runtime(cilk_fiber *fiber) {
    __cilkrts_worker *w = cilk_fiber_get_owner(fiber);
    __cilkrts_stack_frame *sf;
    full_frame *ff;
    sf = w->current_stack_frame;
    ff = *sf->worker->l->frame_ff;

/* #if FIBER_DEBUG >= 1     */
    CILK_ASSERT(ff->fiber_self == fiber);
    cilk_fiber_data *fdata = cilk_fiber_get_data(fiber);
    DBGPRINTF ("%d-%p: resume_after_switch_into_runtime, fiber=%p\n",
               w->self, w, fiber);
    CILK_ASSERT(sf == fdata->resume_sf);
/* #endif */
    fdata->resume_sf = NULL;

    // Notify the Intel tools that we're stealing code
    ITT_SYNC_ACQUIRED(sf->worker);
    NOTIFY_ZC_INTRINSIC("cilk_continue", sf);
    cilk_fiber_invoke_tbb_stack_op(fiber, CILK_TBB_STACK_ADOPT);

    // Actually jump to user code.
    cilkrts_resume(sf, ff);
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

static inline splice_left_ptrs compute_left_ptrs_for_sync(__cilkrts_worker *w, full_frame *ff) {
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

static int fast_path_reductions_for_sync(__cilkrts_worker *w, full_frame *ff) {
    // Return 0 if there is some reduction that needs to happen.
    return !(w->reducer_map  || ff->pending_exception);
}

static __cilkrts_worker* slow_path_reductions_for_sync(__cilkrts_worker *w, full_frame *ff) {
    struct cilkred_map *left_map;
    struct cilkred_map *middle_map;
    
#if (REDPAR_DEBUG > 0)
    CILK_ASSERT(ff);
    CILK_ASSERT(*w->head == *w->tail);
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

static __cilkrts_worker* execute_reductions_for_sync(__cilkrts_worker *w, full_frame *ff, __cilkrts_stack_frame *sf_at_sync) {
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
    CILK_ASSERT(*w->l->frame_ff == ff);

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
    CILK_ASSERT(*w->l->frame_ff == ff);
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
    ff->fiber_self = NULL;
    return w;
}

static NORETURN
longjmp_into_runtime(__cilkrts_worker *w, scheduling_stack_fcn_t fcn, __cilkrts_stack_frame *sf) {
    full_frame *ff, *ff2;

    CILK_ASSERT(!w->l->post_suspend);
    ff = *w->l->frame_ff;

    // If we've got only one worker, stealing shouldn't be possible.
    // Assume that this is a steal or return from spawn in a force-reduce case.
    // We don't have a scheduling stack to switch to, so call the continuation
    // function directly.

    // ROB: I'm assuming this was just an optimization. We can't do
    // this for the suspended deque case. Even if this particular
    // worker has no suspended deques, it may have saved a fiber into
    // w->l->fiber_to_free, which needs to be cleaned up below.
    w->l->post_suspend = fcn;
    w->l->suspended_stack = sf;

    ITT_SYNC_RELEASING(w);
    ITT_SYNC_PREPARE(w);

#if FIBER_DEBUG >= 2
    fprintf(stderr, "ThreadId=%p, W=%d: about to switch into runtime... w->l->frame_ff = %p, sf=%p\n",
            cilkos_get_current_thread_id(),
            w->self, *w->l->frame_ff,
            sf);
#endif

    // Current fiber is either the (1) one we are about to free,
    // or (2) it has been passed up to the parent.
    /*cilk_fiber *current_fiber = ( w->l->fiber_to_free ?
                                  w->l->fiber_to_free :
                                  (*w->l->frame_ff)->parent->fiber_child );*/
    cilk_fiber *current_fiber = (*w->l->frame_ff)->fiber_child;
    cilk_fiber_data* fdata = cilk_fiber_get_data(current_fiber);
    CILK_ASSERT(NULL == (*w->l->frame_ff)->fiber_self);

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

        //fprintf(stderr, "(w: %d) about to free %p\n", w->self, w->l->fiber_to_free);
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

//        w->l->waiting_stacks++;

        cilk_fiber_suspend_self_and_resume_other(current_fiber,
                                                 w->l->scheduling_fiber);
//        w->l->waiting_stacks--;
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

NORETURN __cilkrts_c_future_sync(__cilkrts_worker *w, __cilkrts_stack_frame *sf_at_sync) {
    full_frame *ff; 
    STOP_INTERVAL(w, INTERVAL_WORKING);
    START_INTERVAL(w, INTERVAL_IN_RUNTIME);

    // Claim: This read of w->l->frame_ff can occur without
    // holding the worker lock because when w has reached a sync
    // and entered the runtime (because it stalls), w's deque is empty
    // and no one else can steal and change w->l->frame_ff.

    ff = *w->l->frame_ff;
#ifdef _WIN32
    __cilkrts_save_exception_state(w, ff);
#else
    // Move any pending exceptions into the full frame
    CILK_ASSERT(NULL == ff->pending_exception);
    ff->pending_exception = w->l->pending_exception;
    w->l->pending_exception = NULL;
#endif
    
    w = execute_reductions_for_sync(w, ff, sf_at_sync);

#if FIBER_DEBUG >= 3
    fprintf(stderr, "ThreadId=%p, w->self = %d. about to longjmp_into_runtime[c_sync] with ff=%p\n",
            cilkos_get_current_thread_id(), w->self, ff);
#endif    

    longjmp_into_runtime(w, do_sync, sf_at_sync);
}

CILK_ABI_VOID __cilkrts_future_sync(__cilkrts_stack_frame *sf) {
	//__cilkrts_worker *w = sf->worker;
	__cilkrts_worker *w = __cilkrts_get_tls_worker();

	// Update the worker's pedigree information if this is an ABI 1 or later
  // frame
  if (CILK_FRAME_VERSION_VALUE(sf->flags) >= 1) {
		sf->parent_pedigree.rank = w->pedigree.rank;
		sf->parent_pedigree.parent = w->pedigree.parent;

#ifdef PRECOMPUTE_PEDIGREES
		sf->parent_pedigree.length = w->pedigree.length; // is this necessary?
		sf->parent_pedigree.actual = w->pedigree.actual;
#endif
	}

	/*    DBGPRINTF("%d-%p __cilkrts_sync - sf %p\n", w->self, GetWorkerFiber(w), sf); */
	if (__builtin_expect(!(sf->flags & CILK_FRAME_UNSYNCHED), 0))
		__cilkrts_bug("W%u: double sync %p\n", w->self, sf);
#ifndef _WIN32
	if (__builtin_expect(sf->flags & CILK_FRAME_EXCEPTING, 0)) {
		__cilkrts_c_sync_except(w, sf);
	}
#endif

	__cilkrts_c_future_sync(w, sf);
}
