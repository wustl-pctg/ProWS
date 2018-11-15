/** @file batcher.c               -*-C-*-
 * @author Robert Utterback
 * @author Tom Wilkinson
 *
 * @brief The heart of the Batcher runtime system.
 *
 */

#include <cilk/batcher.h>
#include "deque.h"
#include "scheduler.h"
#include "cilk_fiber.h"
#include "full_frame.h"
#include "stats.h"
#include "local_state.h"
#include "os.h" // For __cilkrts_short_pause()

#include <stdio.h>
#include <string.h> // for memcpy

//#define _BATCH_DEBUG 1
#ifdef _BATCH_DEBUG
#   include <stdio.h>
    __CILKRTS_INLINE void* GetCurrentFiber() { return 0; }
    __CILKRTS_INLINE void* GetWorkerFiber(__cilkrts_worker* w) { return 0; }
#       define BATCH_DBGPRINTF(_fmt, ...) fprintf(stderr, _fmt, __VA_ARGS__)
#else
#       define BATCH_DBGPRINTF(_fmt, ...)
#endif

// @TODO?
//cilk_fiber_sysdep* cilkos_get_tls_cilk_fiber(void)

// @TODO @HACK why are these not just defined in scheduler.h?
#define BEGIN_WITH_WORKER_LOCK(w) __cilkrts_worker_lock(w); do
#define END_WITH_WORKER_LOCK(w)   while (__cilkrts_worker_unlock(w), 0)
#define BEGIN_WITH_FRAME_LOCK(w, ff)                                     \
    do { full_frame *_locked_ff = ff; __cilkrts_frame_lock(w, _locked_ff); do

#define END_WITH_FRAME_LOCK(w, ff)                       \
    while (__cilkrts_frame_unlock(w, _locked_ff), 0); } while (0)

void scheduling_fiber_prepare_to_resume_user_code(__cilkrts_worker *w,
                                                  full_frame *ff,
                                                  __cilkrts_stack_frame *sf);
cilk_fiber* try_to_start_batch(__cilkrts_worker* w);
full_frame* pop_next_frame(__cilkrts_worker* w);
full_frame* check_for_work(__cilkrts_worker* w, steal_t s);
void setup_for_execution(__cilkrts_worker* w, full_frame *ff, int is_return_from_call);

COMMON_PORTABLE
unsigned int collect_batch(struct batch* pending, struct batch_record* records)
{
  unsigned int num_ops = 0;
  int p = cilkg_get_nworkers();

  __cilkrts_worker* w = __cilkrts_get_tls_worker();
  CILK_ASSERT(w->g->batch_lock.owner == w);
  CILK_ASSERT(records[w->self].status == ITEM_WAITING);

  for (int i = 0; i < p; ++i) {
    CILK_ASSERT(records[i].status != ITEM_IN_PROGRESS);
    if (ITEM_WAITING == records[i].status
        && pending->operation == records[i].operation) {

      records[i].status = ITEM_IN_PROGRESS;
      assert(pending->data_size == sizeof(int));
      memcpy(((int*)pending->work_array) + num_ops, &records[i].data,
             pending->data_size);

      num_ops++;
    }
  }
  CILK_ASSERT(num_ops > 0);
  return num_ops;
}

COMMON_PORTABLE
struct batch* create_batch(struct batch_record* record)
{
  __cilkrts_worker* w = __cilkrts_get_tls_worker();
  struct batch* b = &w->g->pending_batch;
  struct batch_record* records = w->g->batch_records;

  b->operation = record->operation;
  b->ds = record->ds;
  b->data_size = record->data_size;
  assert(b->data_size == sizeof(int));

  /// @todo For FPSP, we don't do this. This should really be a
  /// weakly-linked function so that a data structure designer can
  /// override it.
  //  b->num_ops = collect_batch(b, records);

  BATCH_DBGPRINTF("Batch %d has %d ops.\n---", b->id, b->num_ops);

  return b;
}

COMMON_PORTABLE
void terminate_batch(struct batch_record* records)
{
  __cilkrts_worker* w = __cilkrts_get_tls_worker();
  int p = cilkg_get_nworkers();

  BATCH_DBGPRINTF("Worker %i terminating batch %i.\n", w->self, w->g->pending_batch.id);
  
  /// @todo don't need to do this for FPSP...
  /* for (int i = 0; i < p; ++i) { */
  /*   if (records[i].status == ITEM_IN_PROGRESS) { */
  /*     records[i].status = ITEM_DONE; */
  /*     assert(records[i].data_size == sizeof(int)); */
  /*   } */
  /* } */
  
  CILK_ASSERT(!w->l->batch_frame_ff);

  w->g->pending_batch.id++;
  w->g->batch_lock.owner = 0;
  w->g->batch_lock.lock = 0;
  __cilkrts_fence();
  return;
}

CILK_API_VOID __cilkrts_c_terminate_batch()
{
  __cilkrts_worker* w = __cilkrts_get_tls_worker();
  terminate_batch(w->g->batch_records);
}

full_frame* create_batch_frame(__cilkrts_worker* w, cilk_fiber* fiber)
{
  full_frame* ff = __cilkrts_make_full_frame(w, 0);

  CILK_ASSERT(fiber);
  ff->fiber_self = fiber;

  cilk_fiber_set_owner(ff->fiber_self, w);

  CILK_ASSERT(ff->join_counter == 0);
  ff->join_counter = 1;

  /* w->reducer_map = __cilkrts_make_reducer_map(w); */
  /* __cilkrts_set_leftmost_reducer_map(w->reducer_map, 1); */
  /* load_pedigree_leaf_into_user_worker(w); */

  return ff;
}

void call_batch(__cilkrts_worker* w, struct batch* b)
{
  __cilkrts_stack_frame sf;

  CILK_ASSERT(w->current_stack_frame == NULL);

  //  sf.call_parent = w->current_stack_frame;
  sf.call_parent = NULL;
  sf.worker = w;
  sf.flags = CILK_FRAME_VERSION | CILK_FRAME_BATCH;
  w->current_stack_frame = &sf;

  (b->operation)(b->ds, (void*)b->work_array, b->num_ops, NULL);

  if (w->l->batch_frame_ff)
    CILK_ASSERT(w->l->batch_frame_ff->call_stack == NULL);
}

void __cilkrts_c_return_from_batch(__cilkrts_worker* w)
{
  BATCH_DBGPRINTF("Worker %d returning from a batch.\n", w->self);
  BEGIN_WITH_WORKER_LOCK(w) {
    full_frame* ff = *w->l->frame_ff;

    if (ff) {
      CILK_ASSERT(ff->join_counter == 1);
      *w->l->frame_ff = 0;

      CILK_ASSERT(ff->fiber_self);
      //cilk_fiber_tbb_interop_save_info_from_stack(ff->fiber_self);

      __cilkrts_destroy_full_frame(w, ff);
    }

    // deallocate fiber here or later? I think will return to
    // invoke_batch, where the fiber can be deallocated.

    } END_WITH_WORKER_LOCK(w);

    // @TODO reducers and pedigree stuff?
}

COMMON_PORTABLE
void invoke_batch(cilk_fiber *fiber)
{
	__cilkrts_worker * w = __cilkrts_get_tls_worker();

  CILK_ASSERT(w->l->batch_frame_ff == NULL);

  // 1. Make sure deque (ltq) pointers are correctly set.
  CILK_ASSERT(!w->l->batch_frame_ff);
  CILK_ASSERT(w->tail == &w->l->batch_tail);
  CILK_ASSERT(*w->tail = *w->head);

  // 2. Do the collection for the batch.
  struct batch_record* records = w->g->batch_records;
  struct batch* b = &w->g->pending_batch;

  create_batch(&records[w->self]);

  // 3. Call the batch operation.
  BATCH_DBGPRINTF("Worker %i invoking batch %i.\n", w->self, b->id);

  *w->l->frame_ff = create_batch_frame(w, fiber);

  call_batch(w, b);

  w = __cilkrts_get_tls_worker();

  CILK_ASSERT(w->l->batch_head == w->l->batch_tail);

  __cilkrts_c_return_from_batch(w);

  // 4. Terminate the batch.
  terminate_batch(records);

  // 5. Go back to the user code before the call to batchify.

  cilk_fiber_data* current = (cilk_fiber_data*) fiber;
  current->resume_sf = NULL;
  cilk_fiber_remove_reference_from_self_and_resume_other(fiber,
                                                         &w->l->fiber_pool,
                                                         w->l->scheduling_fiber);
}

static cilk_fiber* allocate_batch_fiber(__cilkrts_worker* w,
                                        cilk_fiber_pool* pool,
                                        cilk_fiber_proc start_proc)
{
  cilk_fiber* batch_fiber;
	START_INTERVAL(w, INTERVAL_FIBER_ALLOCATE) {
    batch_fiber = cilk_fiber_allocate(pool);

    if (batch_fiber == NULL) {
      // Should manually try to get lock and run batch sequentially?
      __cilkrts_bug("Couldn't get a batch fiber.");
    }

    cilk_fiber_reset_state(batch_fiber, start_proc);
    cilk_fiber_set_owner(batch_fiber, w);
  } STOP_INTERVAL(w, INTERVAL_FIBER_ALLOCATE);


  return batch_fiber;
}

void execute_batch(__cilkrts_worker* w, cilk_fiber* fiber, int batch_id)
{
  volatile int * global_batch_id = &w->g->pending_batch.id;
  full_frame* ff = NULL;

  // while the batch I belong to is not done or have not started yet, loop
  // batch I belong to is either the pending_batch when I added my heavy node
  // or is the on-going batch that I encountered after failing to acquire
  // my own local lock during insert (which could have finished by the time I 
  // loop around here, and I have read the next pending batch id, so checking
  // the owner is necessary in that case --- owner == NULL means that I got
  // the next pending batch_id, and it's not necessary for me to stay).
  while ((*global_batch_id <= batch_id) && w->g->batch_lock.owner) {
    //if (w->g->batch_lock.owner == NULL) return;

    //__cilkrts_short_pause();
    // @@ batch stealing

    ff = pop_next_frame(w);
    if (!ff) {
      START_INTERVAL(w, INTERVAL_BATCH_STEALING) {
        ff = check_for_work(w, STEAL_BATCH);
      } STOP_INTERVAL(w, INTERVAL_BATCH_STEALING);
    }

    if (ff) {
      validate_full_frame(ff);
      __cilkrts_stack_frame* sf;
      BEGIN_WITH_WORKER_LOCK(w) {
        CILK_ASSERT(!*w->l->frame_ff);
        BEGIN_WITH_FRAME_LOCK(w, ff) {
          sf = ff->call_stack;
          CILK_ASSERT(sf && !sf->call_parent);
          setup_for_execution(w, ff, 0);
        } END_WITH_FRAME_LOCK(w, ff);
      } END_WITH_WORKER_LOCK(w);

      scheduling_fiber_prepare_to_resume_user_code(w, ff, sf);
      
      cilk_fiber *other = (*w->l->frame_ff)->fiber_self;
      cilk_fiber_data* other_data = cilk_fiber_get_data(other);
      cilk_fiber_data* current_fiber_data = cilk_fiber_get_data(fiber);

      CILK_ASSERT(NULL == other_data->resume_sf);

      current_fiber_data->resume_sf = NULL;
      CILK_ASSERT(current_fiber_data->owner == w);
      other_data->resume_sf = sf;

      cilk_fiber_suspend_self_and_resume_other(w->l->scheduling_fiber,
                                               other);
      w = __cilkrts_get_tls_worker();
    }
  }
  /*
  fprintf(stderr,
          "Worker %d leaving batch with local id %zu and global id %zu\n",
          w->self, batch_id, *global_batch_id);
  */
}

COMMON_PORTABLE
void batch_scheduler_function(cilk_fiber *fiber)
{
  while (1) {
    __cilkrts_worker * w = __cilkrts_get_tls_worker();
    volatile struct batch_record* record = &w->g->batch_records[w->self];

    CILK_ASSERT(fiber == w->l->scheduling_fiber);
    CILK_ASSERT(fiber == w->l->batch_scheduling_fiber);
    CILK_ASSERT(cilk_fiber_get_owner(fiber) == w);
    CILK_ASSERT(((cilk_fiber_data*)w->l->scheduling_fiber)->resume_sf == NULL);
    CILK_ASSERT(w->l->frame_ff == &w->l->batch_frame_ff);
    CILK_ASSERT(w->l->batch_frame_ff == NULL);

    BATCH_DBGPRINTF("Worker %i entered scheduler function at batch %i.\n",w->self, w->g->pending_batch.id);

    /// @todo FPSP should just execute this one batch
    execute_batch(w, fiber, w->l->batch_id);

    /* while (record->status != ITEM_DONE) { */

    /*   execute_batch(w, fiber, w->l->batch_id); */
    /*   CILK_ASSERT(w == __cilkrts_get_tls_worker()); */

    /*   if (record->status != ITEM_DONE) { */
    /*     w->l->batch_id = w->g->pending_batch.id; */
    /*     cilk_fiber* new_fiber = try_to_start_batch(w); */
    /*     if (new_fiber) { */
    /*       CILK_ASSERT(w->g->batch_lock.owner == w); */
    /*       CILK_ASSERT(w->g->batch_records[w->self].status == ITEM_WAITING); */
    /*       BATCH_DBGPRINTF("Worker %i restarting a batch.\n", w->self); */

    /*       w->l->batch_id = w->g->pending_batch.id; */
    /*       w->current_stack_frame = NULL; */
    /*       cilk_fiber_suspend_self_and_resume_other(fiber, new_fiber); */
    /*     } */
    /*   } */
    /* } */
    cilk_fiber_suspend_self_and_resume_other(w->l->scheduling_fiber,
                                             w->l->saved_core_fiber);
  }
}


cilk_fiber* switch_to_batch_deque(__cilkrts_worker* w)
{
  cilk_fiber* fiber;
  BEGIN_WITH_WORKER_LOCK(w) {
    CILK_ASSERT(w->l->batch_tail == w->l->batch_head);
    CILK_ASSERT(w->l->batch_tail == w->l->batch_exc);
    CILK_ASSERT(w->l->batch_tail == w->l->batch_ltq);
    CILK_ASSERT(w->l->batch_protected_tail == w->l->batch_ltq + w->g->ltqsize);
    CILK_ASSERT(w->l->batch_frame_ff == NULL);

    fiber = w->l->core_frame_ff->fiber_self;

    w->tail = &w->l->batch_tail;
    w->head = &w->l->batch_head;
    w->exc = &w->l->batch_exc;
    w->protected_tail = &w->l->batch_protected_tail;
    w->l->current_ltq = &w->l->batch_ltq;
    w->l->frame_ff = &w->l->batch_frame_ff;
  } END_WITH_WORKER_LOCK(w);

  return fiber;
}

void switch_to_core_deque(__cilkrts_worker* w)
{
  BEGIN_WITH_WORKER_LOCK(w) {
    w->tail = &w->l->core_tail;
    w->head = &w->l->core_head;
    w->exc = &w->l->core_exc;
    w->protected_tail = &w->l->core_protected_tail;
    w->l->current_ltq = &w->l->core_ltq;

    CILK_ASSERT(!w->l->batch_frame_ff);
    w->l->batch_head = w->l->batch_tail = w->l->batch_exc = w->l->batch_ltq;

    w->l->frame_ff = &w->l->core_frame_ff;
    w->l->batch_id = -1;

  } END_WITH_WORKER_LOCK(w);
}

static inline
void insert_batch_record(__cilkrts_worker* w, batch_function_t func, void* ds,
                         int data, size_t data_size)
{
  struct batch_record* record = &w->g->batch_records[w->self];
  record->operation           = func;
  record->ds                  = ds;
  record->data                = data;
  record->data_size           = data_size;
  record->status              = ITEM_WAITING;
}

cilk_fiber* try_to_start_batch(__cilkrts_worker* w)
{
  CILK_ASSERT(w->head == &w->l->batch_head);

  // Have to do this differently for race detection
  //  int is_batch_owner = __cilkrts_mutex_trylock(w, &w->g->batch_lock);
  int is_batch_owner = cilk_tool_om_try_lock_all(w);

  if (is_batch_owner) {
    if (w->g->batch_records[w->self].status == ITEM_WAITING)
      return allocate_batch_fiber(w, &w->l->fiber_pool, invoke_batch);
    else
      __cilkrts_mutex_unlock(w, &w->g->batch_lock);
  }

  return NULL;
}

int batcher_trylock(__cilkrts_worker* w)
{
  return __cilkrts_mutex_trylock(w, &w->g->batch_lock);
}

CILK_API_VOID cilk_set_next_batch_owner()
{
	__cilkrts_worker *w = __cilkrts_get_tls_worker();

  // In the context of race detection this should never fail.
  int result = __cilkrts_mutex_trylock(w, &w->g->batch_lock);
  CILK_ASSERT(result);
}

void execute_until_op_done(__cilkrts_worker* w, cilk_fiber* current_fiber)
{
  __cilkrts_worker* saved_team = NULL;
  cilk_fiber *batch_fiber = NULL;
  cilk_fiber* saved_fiber = NULL;
  __cilkrts_stack_frame* saved_stack_frame;

  CILK_ASSERT(w->l->core_frame_ff);

  batch_fiber = try_to_start_batch(w);

  /* if (w->g->batch_records[w->self].status == ITEM_DONE) { */
  /*   CILK_ASSERT(!batch_fiber); */
  /*   return; */
  /* } */

  w->l->saved_core_fiber = current_fiber;
  saved_fiber = w->l->scheduling_fiber;
  saved_stack_frame = w->current_stack_frame;
  w->current_stack_frame = NULL;
  w->l->scheduling_fiber = w->l->batch_scheduling_fiber;

  if (!batch_fiber) batch_fiber = w->l->scheduling_fiber;

  BATCH_DBGPRINTF("Worker %d batchifying %d.\n", w->self, w->l->batch_id);
  cilk_fiber_suspend_self_and_resume_other(current_fiber, batch_fiber);
  BATCH_DBGPRINTF("Worker %d done with %d.\n", w->self, w->l->batch_id);

  w->l->scheduling_fiber = saved_fiber;
  w->l->saved_core_fiber = NULL;
  w->current_stack_frame = saved_stack_frame;
}

CILK_API_VOID cilk_batchify(batch_function_t f, void* ds,
                            batch_data_t data, size_t data_size)
{
  // @TODO save w->reducer_map
	__cilkrts_worker * w = __cilkrts_get_tls_worker();

  CILK_ASSERT(w->l->frame_ff == &w->l->core_frame_ff);
  CILK_ASSERT(w->l->team != NULL);

  cilk_fiber* current_fiber = switch_to_batch_deque(w);
  if( w->l->batch_id == -1) {
    w->l->batch_id = w->g->pending_batch.id;
  }
  // fprintf(stderr, "Worker %d joining batch %zu\n", w->self, w->l->batch_id);
  insert_batch_record(w, f, ds, data, sizeof(int));
  execute_until_op_done(w, current_fiber);
  switch_to_core_deque(w);
  CILK_ASSERT(!w->l->batch_frame_ff);
}

CILK_API_VOID __cilkrts_set_batch_id(__cilkrts_worker* w)
{
  w->l->batch_id = w->g->pending_batch.id;
}

CILK_API(int) __cilkrts_get_batch_id(__cilkrts_worker* w)
{
  return w->l->batch_id;
}
