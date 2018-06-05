#include "cilk-ittnotify.h"
#include "except.h"
#include "pedigrees.h"
#include "record-replay.h"
#include "full_frame.h"
#include <internal/abi.h>
#include "scheduler.h"
#include "local_state.h"
#include "sysdep.h"

#define BEGIN_WITH_WORKER_LOCK(w) __cilkrts_worker_lock(w); do
#define END_WITH_WORKER_LOCK(w)   while (__cilkrts_worker_unlock(w), 0)

#define BEGIN_WITH_WORKER_LOCK_OPTIONAL(w) __cilkrts_worker_lock(w); do
#define END_WITH_WORKER_LOCK_OPTIONAL(w)   while (__cilkrts_worker_unlock(w), 0)

#define BEGIN_WITH_FRAME_LOCK(w, ff)                                    \
    do { full_frame *_locked_ff = ff; __cilkrts_frame_lock(w, _locked_ff); do

#define END_WITH_FRAME_LOCK(w, ff)                                \
    while (__cilkrts_frame_unlock(w, _locked_ff), 0); } while (0)

enum provably_good_steal_t {
    ABANDON_EXECUTION,  // Not the last child to the sync - attempt to steal work
    CONTINUE_EXECUTION, // Last child to the sync - continue executing on this worker
    WAIT_FOR_CONTINUE   // The replay log indicates that this was the worker
                        // which continued.  Loop until we are the last worker
                        // to the sync.
};

static int __cilkrts_undo_detach(__cilkrts_stack_frame *sf) {
	//    __cilkrts_worker *w = sf->worker;
	__cilkrts_worker *w = __cilkrts_get_tls_worker();
	__cilkrts_stack_frame *volatile *t = *w->tail;

	/*    DBGPRINTF("%d - __cilkrts_undo_detach - sf %p\n", w->self, sf); */
	--t;
	*w->tail = t;
	/* On x86 the __sync_fetch_and_<op> family includes a
		 full memory barrier.  In theory the sequence in the
		 second branch of the #if should be faster, but on
		 most x86 it is not.  */
#if defined __i386__ || defined __x86_64__
	__sync_fetch_and_and(&sf->flags, ~CILK_FRAME_DETACHED);
#else
	__cilkrts_fence(); /* membar #StoreLoad */
	sf->flags &= ~CILK_FRAME_DETACHED;
#endif

	return __builtin_expect(t < *w->exc, 0);
}

static void restore_frame_for_spawn_return_reduction(__cilkrts_worker *w,
                                              full_frame *ff,
                                              __cilkrts_stack_frame *returning_sf) {
#if REDPAR_DEBUG >= 2
    CILK_ASSERT(returning_sf);
    CILK_ASSERT(returning_sf->worker == w);
#endif
    // Change w's current stack frame back to "returning_sf".
    //
    // Intuitively, w->current_stack_frame should be
    // returning_sf->call_parent at this point.
    //
    // We can not assert this, however, because the pop of
    // returning_sf from the call chain has already cleared
    // returning_sf->call_parent.  We don't want to restore the call
    // parent of returning_sf, because its parent has been stolen, and
    // the runtime assumes that steals break this link.

    // We cannot assert call_parent is NULL either, since that's not true for
    // Win64 exception handling
//    CILK_ASSERT(returning_sf->call_parent == NULL);
    w->current_stack_frame = returning_sf;

    // Make the full frame "ff" runnable again, in preparation for
    // executing the reduction.
    make_runnable(w, ff, w->l->active_deque);
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

static inline void finish_spawn_return_on_user_stack(__cilkrts_worker *w, full_frame *parent_ff, full_frame *child_ff) {
    w->l->fiber_to_free = child_ff->fiber_self;
    child_ff->fiber_self = NULL;
}

static NORETURN longjmp_into_runtime(__cilkrts_worker *w, scheduling_stack_fcn_t fcn, __cilkrts_stack_frame *sf) {
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
    cilk_fiber *current_fiber = ( w->l->fiber_to_free ?
                                  w->l->fiber_to_free :
                                  (*w->l->frame_ff)->parent->fiber_child );
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

static void do_return_from_spawn(__cilkrts_worker *w, full_frame *ff, __cilkrts_stack_frame *sf) {
    enum provably_good_steal_t steal_result = ABANDON_EXECUTION;

    BEGIN_WITH_WORKER_LOCK_OPTIONAL(w) {
      BEGIN_WITH_FRAME_LOCK(w, ff) {
        decjoin(ff);
      } END_WITH_FRAME_LOCK(w, ff);
    } END_WITH_WORKER_LOCK_OPTIONAL(w);

    __cilkrts_destroy_full_frame(w, ff);
    return;
}

static __cilkrts_worker* execute_reductions_for_spawn_return(__cilkrts_worker *w, full_frame *ff, __cilkrts_stack_frame *returning_sf) {
    // Step A1 from reducer protocol described above.
    //
    // Coerce the runtime into thinking that 
    // ff/returning_sf are still on the bottom of
    // w's deque.
    restore_frame_for_spawn_return_reduction(w, ff, returning_sf);

    w->l->fiber_to_free = ff->fiber_self;
    ff->fiber_self = NULL;
    w->l->pending_exception = NULL;
    ff->pending_exception = NULL;
    if (w->reducer_map) {
        __cilkrts_destroy_reducer_map(w, w->reducer_map);
    }
    w->reducer_map = NULL;

    return w;
}

void __attribute__((noinline)) __cilkrts_c_kyles_THE_exception_check(__cilkrts_worker *w, __cilkrts_stack_frame *returning_sf) {
    full_frame *ff;
    int stolen_p;
    __cilkrts_stack_frame *saved_sf = NULL;

    // For the exception check, stop working and count as time in
    // runtime.
    STOP_INTERVAL(w, INTERVAL_WORKING);
    START_INTERVAL(w, INTERVAL_IN_RUNTIME);

    START_INTERVAL(w, INTERVAL_THE_EXCEPTION_CHECK);

    BEGIN_WITH_WORKER_LOCK(w) {
        ff = *w->l->frame_ff;
        CILK_ASSERT(ff);
        /* This code is called only upon a normal return and never
           upon an exceptional return.  Assert that this is the
           case. */
        CILK_ASSERT(!w->l->pending_exception);

        reset_THE_exception(w);
        stolen_p = !(*w->head < (*w->tail + 1)); /* +1 because tail was
                                                    speculatively
                                                    decremented by the
                                                    compiled code */

        if (stolen_p) {
            /* XXX This will be charged to THE for accounting purposes */
            __cilkrts_save_exception_state(w, ff);

            // Save the value of the current stack frame.
            saved_sf = w->current_stack_frame;

            // Reverse the decrement from undo_detach.
            // This update effectively resets the deque to be
            // empty (i.e., changes w->tail back to equal w->head). 
            // We need to reset the deque to execute parallel
            // reductions.  When we have only serial reductions, it
            // does not matter, since serial reductions do not
            // change the deque.
            (*w->tail)++;
#if REDPAR_DEBUG > 1            
            // ASSERT our deque is empty.
            CILK_ASSERT(*w->head == *w->tail);
#endif
        }
    } END_WITH_WORKER_LOCK(w);

    STOP_INTERVAL(w, INTERVAL_THE_EXCEPTION_CHECK);

    if (stolen_p)
    {
        w = execute_reductions_for_spawn_return(w, ff, returning_sf);

        // "Mr. Policeman?  My parent always told me that if I was in trouble
        // I should ask a nice policeman for help.  I can't find my parent
        // anywhere..."
        //
        // Write a record to the replay log for an attempt to return to a stolen parent
        replay_record_orphaned(w);

        // Update the pedigree only after we've finished the
        // reductions.
        update_pedigree_on_leave_frame(w, returning_sf);

        // Notify Inspector that the parent has been stolen and we're
        // going to abandon this work and go do something else.  This
        // will match the cilk_leave_begin in the compiled code
        NOTIFY_ZC_INTRINSIC("cilk_leave_stolen", saved_sf);

        DBGPRINTF ("%d: longjmp_into_runtime from __cilkrts_c_THE_exception_check\n", w->self);
        longjmp_into_runtime(w, do_return_from_spawn, 0);
        DBGPRINTF ("%d: returned from longjmp_into_runtime from __cilkrts_c_THE_exception_check?!\n", w->self);
    }
    else
    {
        NOTE_INTERVAL(w, INTERVAL_THE_EXCEPTION_CHECK_USELESS);

        // If we fail the exception check and return, then switch back
        // to working.
        STOP_INTERVAL(w, INTERVAL_IN_RUNTIME);
        START_INTERVAL(w, INTERVAL_WORKING);
        return;
    }
}

CILK_ABI_VOID __cilkrts_leave_future_frame(__cilkrts_stack_frame *sf) {
	//__cilkrts_worker *w = sf->worker;
	__cilkrts_worker *w = __cilkrts_get_tls_worker();
    cilkg_decrement_pending_futures(w->g);

	/*    DBGPRINTF("%d-%p __cilkrts_leave_frame - sf %p, flags: %x\n", w->self, GetWorkerFiber(w), sf, sf->flags); */

#ifdef _WIN32
	/* if leave frame was called from our unwind handler, leave_frame should
		 proceed no further. */
	if (sf->flags & CILK_FRAME_UNWINDING)
    {
			/*        DBGPRINTF("%d - __cilkrts_leave_frame - aborting due to UNWINDING flag\n", w->self); */

			// If this is the frame of a spawn helper (indicated by the
			// CILK_FRAME_DETACHED flag) we must update the pedigree.  The pedigree
			// points to nodes allocated on the stack.  Failing to update it will
			// result in a accvio/segfault if the pedigree is walked.  This must happen
			// for all spawn helper frames, even if we're processing an exception
			if ((sf->flags & CILK_FRAME_DETACHED))
        {
					update_pedigree_on_leave_frame(w, sf);
        }
			return;
    }
#endif

#if CILK_LIB_DEBUG
	/* ensure the caller popped itself */
	CILK_ASSERT(w->current_stack_frame != sf);
#endif

	/* The exiting function should have checked for zero flags,
		 so there is no check for flags == 0 here. */

#if CILK_LIB_DEBUG
	if (__builtin_expect(sf->flags & (CILK_FRAME_EXITING|CILK_FRAME_UNSYNCHED), 0))
		__cilkrts_bug("W%u: function exiting with invalid flags %02x\n",
									w->self, sf->flags);
#endif

	/* Must return normally if (1) the active function was called
		 and not spawned, or (2) the parent has never been stolen. */
	if ((sf->flags & CILK_FRAME_DETACHED)) {
		/*        DBGPRINTF("%d - __cilkrts_leave_frame - CILK_FRAME_DETACHED\n", w->self); */

#ifndef _WIN32
		if (__builtin_expect(sf->flags & CILK_FRAME_EXCEPTING, 0)) {
			// Pedigree will be updated in __cilkrts_leave_frame.  We need the
			// pedigree before the update for record/replay
			//	    update_pedigree_on_leave_frame(w, sf);
			__cilkrts_return_exception(sf);
			/* If return_exception returns the caller is attached.
				 leave_frame is called from a cleanup (destructor)
				 for the frame object.  The caller will reraise the
				 exception. */
	    return;
		}
#endif

		// During replay, check whether w was the last worker to continue
		replay_wait_for_steal_if_parent_was_stolen(w);

		// Attempt to undo the detach
		if (__builtin_expect(__cilkrts_undo_detach(sf), 0)) {
			// The update of pedigree for leaving the frame occurs
			// inside this call if it does not return.
			__cilkrts_c_kyles_THE_exception_check(w, sf);
		}

		update_pedigree_on_leave_frame(w, sf);

		/* This path is taken when undo-detach wins the race with stealing.
			 Otherwise this strand terminates and the caller will be resumed
			 via setjmp at sync. */
		if (__builtin_expect(sf->flags & CILK_FRAME_FLAGS_MASK, 0))
			__cilkrts_bug("W%u: frame won undo-detach race with flags %02x\n",
										w->self, sf->flags);

		return;
	}

#if CILK_LIB_DEBUG
	sf->flags |= CILK_FRAME_EXITING;
#endif

	if (__builtin_expect(sf->flags & CILK_FRAME_LAST, 0))
		__cilkrts_c_return_from_initial(w); /* does return */
	else if (sf->flags & CILK_FRAME_STOLEN)
		__cilkrts_return(w); /* does return */

	/*    DBGPRINTF("%d-%p __cilkrts_leave_frame - returning, StackBase: %p\n", w->self, GetWorkerFiber(w)); */
}
