#include "cilk-ittnotify.h"
#include "except.h"
#include "pedigrees.h"
#include "record-replay.h"
#include "full_frame.h"
#include <internal/abi.h>
#include "scheduler.h"
#include "local_state.h"
#include "sysdep.h"

#define verify_current_wkr(w)   ;

#define BEGIN_WITH_WORKER_LOCK(w) __cilkrts_worker_lock(w); do
#define END_WITH_WORKER_LOCK(w)   while (__cilkrts_worker_unlock(w), 0)

#   define ASSERT_WORKER_LOCK_OWNED(w) \
        { \
            __cilkrts_worker *tls_worker = __cilkrts_get_tls_worker(); \
            CILK_ASSERT((w)->l->lock.owner == tls_worker); \
        }

#define BEGIN_WITH_WORKER_LOCK_OPTIONAL(w) __cilkrts_worker_lock(w); do
#define END_WITH_WORKER_LOCK_OPTIONAL(w)   while (__cilkrts_worker_unlock(w), 0)

#define BEGIN_WITH_FRAME_LOCK(w, ff)                                     \
    do { full_frame *_locked_ff = ff; __cilkrts_frame_lock(w, _locked_ff); do

#define END_WITH_FRAME_LOCK(w, ff)                       \
    while (__cilkrts_frame_unlock(w, _locked_ff), 0); } while (0)

typedef struct {
    /** A pointer to the location of our left reducer map. */
    struct cilkred_map **map_ptr;

    /** A pointer to the location of our left exception. */
    struct pending_exception_info **exception_ptr;
} splice_left_ptrs;

enum provably_good_steal_t {
    ABANDON_EXECUTION,  // Not the last child to the sync - attempt to steal work
    CONTINUE_EXECUTION, // Last child to the sync - continue executing on this worker
    WAIT_FOR_CONTINUE   // The replay log indicates that this was the worker
                        // which continued.  Loop until we are the last worker
                        // to the sync.
};

static void incjoin(full_frame *ff) {
    ++ff->join_counter;
}

static int decjoin(full_frame *ff) {
    CILK_ASSERT(ff->join_counter > 0);
    return (--ff->join_counter);
}

static int simulate_decjoin(full_frame *ff) {
  CILK_ASSERT(ff->join_counter > 0);
  return (ff->join_counter - 1);
}

static int __cilkrts_undo_detach(__cilkrts_stack_frame *sf) {
    __cilkrts_worker *w = sf->worker;
    __cilkrts_stack_frame *volatile *t = w->tail;

/*    DBGPRINTF("%d - __cilkrts_undo_detach - sf %p\n", w->self, sf); */

    --t;
    w->tail = t;
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
    //CILK_ASSERT(t < w->exc);
    return __builtin_expect(t < w->exc, 0);
}

static void make_runnable(__cilkrts_worker *w, full_frame *ff) {
    w->l->frame_ff = ff;

    /* CALL_STACK is invalid (the information is stored implicitly in W) */
    ff->call_stack = 0;
}

static inline
void restore_frame_for_spawn_return_reduction(__cilkrts_worker *w,
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
    make_runnable(w, ff);
}

static inline
splice_left_ptrs compute_left_ptrs_for_spawn_return(__cilkrts_worker *w,
                                                    full_frame *ff) {
    // ASSERT: we hold the lock on ff->parent

    splice_left_ptrs left_ptrs;
    if (ff->left_sibling) {
        left_ptrs.map_ptr = &ff->left_sibling->right_reducer_map;
        left_ptrs.exception_ptr = &ff->left_sibling->right_pending_exception;
    }
    else {
        full_frame *parent_ff = ff->parent;
        left_ptrs.map_ptr = &parent_ff->children_reducer_map;
        left_ptrs.exception_ptr = &parent_ff->child_pending_exception;
    }
    return left_ptrs;
}

static inline
void splice_exceptions_for_spawn(__cilkrts_worker *w,
                                 full_frame *ff,
                                 struct pending_exception_info **left_exception_ptr) {
    // ASSERT: parent_ff == child_ff->parent.
    // ASSERT: We own parent_ff->lock

    // Merge current exception into the slot where the left
    // exception should go.
    *left_exception_ptr =
        __cilkrts_merge_pending_exceptions(w,
                                           *left_exception_ptr,
                                           ff->pending_exception);
    ff->pending_exception = NULL;


    // Merge right exception into the slot where the left exception
    // should go.
    *left_exception_ptr =
        __cilkrts_merge_pending_exceptions(w,
                                           *left_exception_ptr,
                                           ff->right_pending_exception);
    ff->right_pending_exception = NULL;
}

static struct cilkred_map**
fast_path_reductions_for_spawn_return(__cilkrts_worker *w,
                                      full_frame *ff) {
    // ASSERT: we hold ff->parent->lock.
    splice_left_ptrs left_ptrs;

    CILK_ASSERT(NULL == w->l->pending_exception);

    // Figure out the pointers to the left where I want
    // to put reducers and exceptions.
    left_ptrs = compute_left_ptrs_for_spawn_return(w, ff);
    
    // Go ahead and merge exceptions while holding the lock.
    splice_exceptions_for_spawn(w, ff, left_ptrs.exception_ptr);

    // Now check if we have any reductions to perform.
    //
    // Consider all the cases of left, middle and right maps.
    //  0. (-, -, -)  :  finish and return 1
    //  1. (L, -, -)  :  finish and return 1
    //  2. (-, M, -)  :  slide over to left, finish, and return 1.
    //  3. (L, M, -)  :  return 0
    //  4. (-, -, R)  :  slide over to left, finish, and return 1.
    //  5. (L, -, R)  :  return 0
    //  6. (-, M, R)  :  return 0
    //  7. (L, M, R)  :  return 0
    //
    // In terms of code:
    //  L == *left_ptrs.map_ptr
    //  M == w->reducer_map
    //  R == f->right_reducer_map.
    //
    // The goal of the code below is to execute the fast path with
    // as few branches and writes as possible.
    
    int case_value = (*(left_ptrs.map_ptr) != NULL);
    case_value += ((w->reducer_map != NULL) << 1);
    case_value += ((ff->right_reducer_map != NULL) << 2);

    // Fastest path is case_value == 0 or 1.
    if (case_value >=2) {
        switch (case_value) {
        case 2:
            *(left_ptrs.map_ptr) = w->reducer_map;
            w->reducer_map = NULL;
            return NULL;
            break;
        case 4:
            *(left_ptrs.map_ptr) = ff->right_reducer_map;
            ff->right_reducer_map = NULL;
            return NULL;
        default:
            // If we have to execute the slow path, then
            // return the pointer to the place to deposit the left
            // map.
            return left_ptrs.map_ptr;
        }
    }

    // Do nothing
    return NULL;
}

static __cilkrts_worker*
slow_path_reductions_for_spawn_return(__cilkrts_worker *w,
                                      full_frame *ff,
                                      struct cilkred_map **left_map_ptr) {

    // CILK_ASSERT: w is holding frame lock on parent_ff.
#if REDPAR_DEBUG > 0
    CILK_ASSERT(!ff->rightmost_child);
    CILK_ASSERT(!ff->is_call_child);
#endif

    // Loop invariant:
    // When beginning this loop, we should
    //   1. Be holding the lock on ff->parent.
    //   2. left_map_ptr should be the address of the pointer to the left map.
    //   3. All maps should be slid over left by one, if possible.
    //   4. All exceptions should be merged so far.
    while (1) {
        
        // Slide middle map left if possible.
        if (!(*left_map_ptr)) {
            *left_map_ptr = w->reducer_map;
            w->reducer_map = NULL;
        }
        // Slide right map to middle if possible.
        if (!w->reducer_map) {
            w->reducer_map = ff->right_reducer_map;
            ff->right_reducer_map = NULL;
        }

        // Since we slid everything left by one,
        // we are finished if there is no middle map.
        if (!w->reducer_map) {
            verify_current_wkr(w);
            return w;
        }
        else {
            struct cilkred_map* left_map;
            struct cilkred_map* middle_map;
            struct cilkred_map* right_map;

            // Take all the maps from their respective locations.
            // We can't leave them in place and execute a reduction because these fields
            // might change once we release the lock.
            left_map = *left_map_ptr;
            *left_map_ptr = NULL;
            middle_map = w->reducer_map;
            w->reducer_map = NULL;
            right_map = ff->right_reducer_map;
            ff->right_reducer_map = NULL;
        
            // WARNING!!! Lock release here.
            // We have reductions to execute (and we can't hold locks).
            __cilkrts_frame_unlock(w, ff->parent);

            // After we've released the lock, start counting time as
            // WORKING again.
            STOP_INTERVAL(w, INTERVAL_IN_RUNTIME);
            START_INTERVAL(w, INTERVAL_WORKING);

            // Merge all reducers into the left map.
            left_map = repeated_merge_reducer_maps(&w,
                                                   left_map,
                                                   middle_map);
            verify_current_wkr(w);
            left_map = repeated_merge_reducer_maps(&w,
                                                   left_map,
                                                   right_map);
            verify_current_wkr(w);
            CILK_ASSERT(NULL == w->reducer_map);
            // Put the final answer back into w->reducer_map.
            w->reducer_map = left_map;
            
            // Save any exceptions generated because of the reduction
            // process from the returning worker.  These get merged
            // the next time around the loop.
            CILK_ASSERT(NULL == ff->pending_exception);
            ff->pending_exception = w->l->pending_exception;
            w->l->pending_exception = NULL;

            STOP_INTERVAL(w, INTERVAL_WORKING);
            START_INTERVAL(w, INTERVAL_IN_RUNTIME);

            // Lock ff->parent for the next loop around.
            __cilkrts_frame_lock(w, ff->parent);

            // Once we have the lock again, recompute who is to our
            // left.
            splice_left_ptrs left_ptrs;
            left_ptrs = compute_left_ptrs_for_spawn_return(w, ff);

            // Update the pointer for the left map.
            left_map_ptr = left_ptrs.map_ptr;
            // Splice the exceptions for spawn.
            splice_exceptions_for_spawn(w, ff, left_ptrs.exception_ptr);
        }
    }
    // We should never break out of this loop.
    
    CILK_ASSERT(0);
    return NULL;
}

static void double_link(full_frame *left_ff, full_frame *right_ff) {
    if (left_ff)
        left_ff->right_sibling = right_ff;
    if (right_ff)
        right_ff->left_sibling = left_ff;
}

static void unlink_child(full_frame *parent_ff, full_frame *child_ff) {
    double_link(child_ff->left_sibling, child_ff->right_sibling);

    if (!child_ff->right_sibling) {
        /* this is the rightmost child -- update parent link */
        CILK_ASSERT(parent_ff->rightmost_child == child_ff);
        parent_ff->rightmost_child = child_ff->left_sibling;
    }
    child_ff->left_sibling = child_ff->right_sibling = 0; /* paranoia */
}

static inline
void finish_spawn_return_on_user_stack(__cilkrts_worker *w,
                                       full_frame *parent_ff,
                                       full_frame *child_ff) {
    CILK_ASSERT(w->l->fiber_to_free == NULL);

    // Execute left-holder logic for stacks.
    if (child_ff->left_sibling || parent_ff->fiber_child) {
        // Case where we are not the leftmost stack.
        CILK_ASSERT(parent_ff->fiber_child != child_ff->fiber_self);

        // Remember any fiber we need to free in the worker.
        // After we jump into the runtime, we will actually do the
        // free.
        w->l->fiber_to_free = child_ff->fiber_self;
    }
    else {
        // We are leftmost, pass stack/fiber up to parent.
        // Thus, no stack/fiber to free.
        parent_ff->fiber_child = child_ff->fiber_self;
        w->l->fiber_to_free = NULL;
    }

    child_ff->fiber_self = NULL;

    unlink_child(parent_ff, child_ff);
}

static __cilkrts_worker*
execute_reductions_for_spawn_return(__cilkrts_worker *w,
                                    full_frame *ff,
                                    __cilkrts_stack_frame *returning_sf) { 
    // Step A1 from reducer protocol described above.
    //
    // Coerce the runtime into thinking that 
    // ff/returning_sf are still on the bottom of
    // w's deque.
    restore_frame_for_spawn_return_reduction(w, ff, returning_sf);

    // Step A2 and A3: Execute reductions on user stack.
    BEGIN_WITH_FRAME_LOCK(w, ff->parent) {
        struct cilkred_map **left_map_ptr;
        left_map_ptr = fast_path_reductions_for_spawn_return(w, ff);

        // Pointer will be non-NULL if there are
        // still reductions to execute.
        if (left_map_ptr) {
            // WARNING: This method call may release the lock
            // on ff->parent and re-acquire it (possibly on a
            // different worker).
            // We can't hold locks while actually executing
            // reduce functions.
            w = slow_path_reductions_for_spawn_return(w,
                                                      ff,
                                                      left_map_ptr);
            verify_current_wkr(w);
        }

        finish_spawn_return_on_user_stack(w, ff->parent, ff);      
        // WARNING: the use of this lock macro is deceptive.
        // The worker may have changed here.
    } END_WITH_FRAME_LOCK(w, ff->parent);
    return w;
}

static void reset_THE_exception(__cilkrts_worker *w) {
    // The currently executing worker must own the worker lock to touch
    // w->exc
    ASSERT_WORKER_LOCK_OWNED(w);

    w->exc = w->head;
    __cilkrts_fence();
}

static void unconditional_steal(__cilkrts_worker *w,
                                full_frame *ff) {
    // ASSERT: we hold ff->lock

    START_INTERVAL(w, INTERVAL_UNCONDITIONAL_STEAL) {
        decjoin(ff);
        __cilkrts_push_next_frame(w, ff);
    } STOP_INTERVAL(w, INTERVAL_UNCONDITIONAL_STEAL);
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
    //printf("Future parent return! provably good steal fiber self = %p\n", ff->fiber_self);
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

static void setup_for_execution(__cilkrts_worker *w, 
                                full_frame *ff,
                                int is_return_from_call)
{
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

static
enum provably_good_steal_t provably_good_steal(__cilkrts_worker *w,
                                               full_frame       *ff)
{
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

static void do_return_from_spawn(__cilkrts_worker *w,
                                 full_frame *ff,
                                 __cilkrts_stack_frame *sf) {
    full_frame *parent_ff;
    enum provably_good_steal_t steal_result = ABANDON_EXECUTION;

    BEGIN_WITH_WORKER_LOCK_OPTIONAL(w) {
        CILK_ASSERT(ff);
        CILK_ASSERT(!ff->is_call_child);
        CILK_ASSERT(sf == NULL);
        parent_ff = ff->parent;
    
        BEGIN_WITH_FRAME_LOCK(w, ff) {
            decjoin(ff);
        } END_WITH_FRAME_LOCK(w, ff);

        BEGIN_WITH_FRAME_LOCK(w, parent_ff) {
            if (parent_ff->simulated_stolen)
                unconditional_steal(w, parent_ff);
            else
                steal_result = provably_good_steal(w, parent_ff);
        } END_WITH_FRAME_LOCK(w, parent_ff);

    } END_WITH_WORKER_LOCK_OPTIONAL(w);

    // Loop here in replay mode
#ifdef CILK_RECORD_REPLAY
    // We don't have to explicitly check for REPLAY_LOG below because
    // steal_result can only get set to WAIT_FOR_CONTINUE during replay.
    // We also don't have to worry about the simulated_stolen flag
    // because steal_result can only be set to WAIT_FOR_CONTINUE by
    // provably_good_steal().
    while(WAIT_FOR_CONTINUE == steal_result)
    {
        __cilkrts_sleep();
        BEGIN_WITH_WORKER_LOCK_OPTIONAL(w)
        {
            BEGIN_WITH_FRAME_LOCK(w, parent_ff)
            {
                steal_result = provably_good_steal(w, parent_ff);
            } END_WITH_FRAME_LOCK(w, parent_ff);
        } END_WITH_WORKER_LOCK_OPTIONAL(w);
    }
#endif  // CILK_RECORD_REPLAY

    // Cleanup the child frame.
    __cilkrts_destroy_full_frame(w, ff);
    //printf("Future parent everything is kosher so far...\n");
    return;
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


static inline NORETURN
cilkrts_resume(__cilkrts_stack_frame *sf, full_frame *ff) {
    // Save the sync stack pointer, and do the bookkeeping
    char* sync_sp = ff->sync_sp;
    __cilkrts_take_stack(ff, sync_sp);  // leaves ff->sync_sp null

    sf->flags &= ~CILK_FRAME_SUSPENDED;
    // Actually longjmp to the user code.
    // We may have exceptions to deal with, since we are resuming
    // a previous-suspended frame.
    sysdep_longjmp_to_sf(sync_sp, sf, ff);
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
    //printf("Future parent still kosher!\n");
}

static NORETURN
user_code_resume_after_switch_into_runtime(cilk_fiber *fiber) {
    //printf("future parent user code resume after switch into runtime\n");
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

static NORETURN
longjmp_into_runtime(__cilkrts_worker *w,
                     scheduling_stack_fcn_t fcn,
                     __cilkrts_stack_frame *sf) {

    //printf("Future Parent Frame longjmp_into_runtime!\n");
    full_frame *ff, *ff2;

    CILK_ASSERT(!w->l->post_suspend);
    ff = w->l->frame_ff;

    // If we've got only one worker, stealing shouldn't be possible.
    // Assume that this is a steal or return from spawn in a force-reduce case.
    // We don't have a scheduling stack to switch to, so call the continuation
    // function directly.
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
    cilk_fiber *current_fiber = ( w->l->fiber_to_free ?
                                  w->l->fiber_to_free :
                                  w->l->frame_ff->parent->fiber_child );
    cilk_fiber_data* fdata = cilk_fiber_get_data(current_fiber);
    //CILK_ASSERT(NULL == w->l->frame_ff->fiber_self);
    //CILK_ASSERT(w->l->fiber_to_free != NULL);
    //CILK_ASSERT(w->l->fiber_to_free == current_fiber);
    //CILK_ASSERT(w->l->frame_ff->parent->fiber_self != current_fiber);
    //CILK_ASSERT(w->l->frame_ff->parent->fiber_child != current_fiber);

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
        CILK_ASSERT(! "Future parent, please don't free me! :(\n");
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
        //printf("Freeing future parent fiber!\n");
        cilk_fiber_remove_reference_from_self_and_resume_other(current_fiber,
                                                               &w->l->fiber_pool,
                                                               w->l->scheduling_fiber);
        // We should never come back here!
        CILK_ASSERT(0);
    }
    else {        
        //CILK_ASSERT(! "We should be freeing the future fiber!!!\n");
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

static void __cilkrts_c_kyles_THE_exception_check(__cilkrts_worker *w, 
                                     __cilkrts_stack_frame *returning_sf) {
    //printf("Future parent frame THE exception check\n");
    full_frame *ff;
    int stolen_p;
    __cilkrts_stack_frame *saved_sf = NULL;

    // For the exception check, stop working and count as time in
    // runtime.
    STOP_INTERVAL(w, INTERVAL_WORKING);
    START_INTERVAL(w, INTERVAL_IN_RUNTIME);

    START_INTERVAL(w, INTERVAL_THE_EXCEPTION_CHECK);

    BEGIN_WITH_WORKER_LOCK(w) {
        ff = w->l->frame_ff;
        CILK_ASSERT(ff);
        /* This code is called only upon a normal return and never
           upon an exceptional return.  Assert that this is the
           case. */
        CILK_ASSERT(!w->l->pending_exception);

        reset_THE_exception(w);
        stolen_p = !(w->head < (w->tail + 1)); /* +1 because tail was
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
            w->tail++;
#if REDPAR_DEBUG > 1            
            // ASSERT our deque is empty.
            CILK_ASSERT(w->head == w->tail);
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

CILK_ABI_VOID __cilkrts_leave_future_parent_frame(__cilkrts_stack_frame *sf) {
    //printf("Leaving Future Parent Frame!\n");
    __cilkrts_worker *w = sf->worker;
    //CILK_ASSERT(w->l->frame_ff->future_flags == CILK_FUTURE_PARENT);

    CILK_ASSERT((sf->flags & CILK_FRAME_DETACHED) == 0); // I removed the related code...

#if CILK_LIB_DEBUG
    sf->flags |= CILK_FRAME_EXITING;
#endif

    if (__builtin_expect(sf->flags & CILK_FRAME_LAST, 0)) {
        CILK_ASSERT(! "We shouldn't be the last frame? Maybe?");
        __cilkrts_c_return_from_initial(w); /* does return */
    } else if (sf->flags & CILK_FRAME_STOLEN) {
        __cilkrts_return(w); /* does return */
    }
}
