// Multi-threaded work queue manager
// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#ifdef __GLIBC__
#  include <sys/sysinfo.h>      // for get_nprocs
#endif
#ifndef thread_local            // C11 compatibility
#  define thread_local _Thread_local
#endif
#undef atomic
#define atomic _Atomic
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <math.h>
#include "map.h"
#include "timer.h"
#include "wqm.h"

#define assert(cond) assert2((cond), (#cond))
#define thrd_honored(cond) do {int __c = (cond); assert2((__c) == thrd_success || (__c) == thrd_timedout || (__c) == thrd_busy, (#cond)); } while (0)
#define assert2(cond, text) do { if (!(cond)) { fprintf (stderr, "%s:%d:%s: condition \"%s\" failed. Abort.\n", __FILE__, __LINE__, __func__, (text)); abort (); } } while (0)
#ifndef i18n_init
#  define _(s) (s)
#  define i18n_init
#endif

#define threadpool_something_to_process_predicate(threadpool)   ((threadpool)->out != 0)        // Indicates that the FIFO is not empty.
// The FIFO is empty and there is not work in progress or virtual (asynchronous) task or new task that could ever fill it (all expected tasks have been processed).
#define threadpool_is_done_predicate(threadpool)   ( (threadpool)->nb_processing_tasks == 0 && \
                                                     !threadpool_something_to_process_predicate (threadpool) && \
                                                     (threadpool)->concluding && (threadpool)->nb_async_tasks == 0)
// N.B.: Once done, a FIFO cannot be undone by design: there aren't any data being processed left, that could call 'threadpool_add_task' and refill the empty FIFO (see loop in 'thread_worker_starter').
#define threadpool_runoff_predicate(threadpool) (threadpool_is_done_predicate(threadpool) && (threadpool)->nb_alive_workers == 0)

#ifdef __GLIBC__
size_t const TP_WORKER_NB_CPU = 0;
#endif
size_t const TP_WORKER_SEQUENTIAL = 1;
size_t const TP_CANCEL_ALL_PENDING_TASKS = SIZE_MAX - 2;
size_t const TP_CANCEL_NEXT_PENDING_TASK = SIZE_MAX - 1;
size_t const TP_CANCEL_LAST_PENDING_TASK = SIZE_MAX;
const tp_property_t TP_RUN_ALL_TASKS = 1;       // Runs all submitted tasks.
const tp_property_t TP_RUN_ALL_SUCCESSFUL_TASKS = 2;    // Runs submitted tasks until one fails. Cancel automatically other (already or to be) submitted tasks.
const tp_property_t TP_RUN_ONE_SUCCESSFUL_TASK = 4;     // Runs submitted tasks until one succeeds. Cancel automatically other (already or to be) submitted tasks.

struct threadpool
{
  tp_property_t property;
  size_t requested_nb_workers, max_nb_workers;
  thrd_t *worker_id /* [requested_nb_workers] */ ;
  mtx_t mutex;
  void *global_data;
  struct                        // Thread specific local data
  {
    void *(*make) (void);
    void (*destroy) (void *local_data);
  } worker_local_data_manager;
  size_t nb_alive_workers, nb_idle_workers;
  size_t atomic nb_created_tasks, nb_submitted_tasks, nb_pending_tasks, nb_async_tasks, nb_processing_tasks, nb_succeeded_tasks, nb_failed_tasks, nb_canceled_tasks;
  thrd_t **active_worker_id /* [requested_nb_workers] */ ;
  struct elem                   // Elements in FIFO.
  {
    struct elem *next;
    struct task                 // Task to be processed by a worker.
    {
      struct job
      {
        void *data;
        void (*data_delete) (void *data);
      } job;
      int (*work) (struct threadpool * threadpool, void *data);
      size_t id;
      int to_be_continued;
      int is_continuation;
    } task;
  } *in, *out;
  int concluding;               // Indicates that 'threadpool_wait_and_destroy' has been called. Only workers can now add tasks (in 'thread_worker_starter').
  cnd_t proceed_or_conclude_or_runoff;  // Associated with 3 exclusive predicates.
  double idle_timeout;          // Timeout delay of an inactive worker, in seconds.
  struct
  {
    void *(*allocator) (void *global_data);
    void (*deallocator) (void *data);
    void *data;
  } resource;
  // Monitoring
  struct
  {
    void (*displayer) (struct threadpool_monitor, void *argument);      // struct threadpool_monitor is declared in wqm.h.
    void *argument;
    struct threadpool *processor;
    struct timespec t0;
    double last_time;
    int (*filter) (struct threadpool_monitor d);
  } monitor;
};

static thread_local struct      // Thread local worker-specific storage (see also Jens Gustedt, https://stackoverflow.com/a/58087826).
{
  struct threadpool *threadpool;        // thread pool in which a worker is running
  void *local_data;
  struct task *current_task;
} Worker_context = { 0 };

static once_flag THREADPOOL_INIT = ONCE_FLAG_INIT;

// ================= Continuators =================
struct continuator_data
{
  struct job job;
  int (*work) (struct threadpool * threadpool, void *data);
  uint64_t uid;
  uint64_t *p_uid;
  void *timeout_timer;
  struct threadpool *threadpool;
};

static void
continuator_data_clear_on_exit (void *data)
{
  struct continuator_data *c = data;
  free (c->p_uid);
  free (c);
}

static const void *
continuator_data_get_key (void *pa)
{
  const struct continuator_data *a = pa;
  return &a->uid;
}

static int
continuator_data_cmp_key (const void *pa, const void *pb, void *arg)
{
  (void) arg;
  const uint64_t *a = pa;
  const uint64_t *b = pb;
  return *a > *b ? 1 : *a < *b ? -1 : 0;
}

static struct
{
  map *map;
} Continuators = { 0 };

static int
threadpool_task_continuator_timeout_operator (void *data, void *res, int *remove)
{
  (void) res;
  struct continuator_data *continuator = data;
  continuator->threadpool->nb_failed_tasks++;
  if (continuator->job.data_delete)     // The job won't be processed : delete it.
    continuator->job.data_delete (continuator->job.data);
  assert (continuator->threadpool->nb_async_tasks--);   // The predicate threadpool_is_done_predicate is modified: broadcast.
  // Do not broadcast before the continuator is thrown away.
  thrd_honored (cnd_broadcast (&continuator->threadpool->proceed_or_conclude_or_runoff));
  free (continuator);           // Remove the continuator.
  *remove = 1;                  // Remove the continuator fom the map.
  return 0;
}

static void
threadpool_task_continuation_timeout_handler (void *p_uid)      // timer handler
{
  map_find_key (Continuators.map, p_uid, threadpool_task_continuator_timeout_operator, 0);
  free (p_uid);
}

static size_t threadpool_create_task (struct threadpool *threadpool, int (*work) (struct threadpool * threadpool, void *job), void *job, void (*job_delete) (void *job),
                                      int is_continuation);

static int
threadpool_task_continuator_continue_operator (void *data, void *res, int *remove)
{
  (void) res;
  struct continuator_data *continuator = data;
  if (!threadpool_create_task (continuator->threadpool, continuator->work, continuator->job.data, continuator->job.data_delete, /* is_continuation = */ 1))
  {
    fprintf (stderr, "%s: %s\n", __func__, _("Continuation failed."));
    continuator->threadpool->nb_failed_tasks++;
    if (continuator->job.data_delete)   // The job won't be processed : delete it.
      continuator->job.data_delete (continuator->job.data);
  }
  // Remove the asynchronous task (after the continuator has been converted into a task to keep threadpool_is_done_predicate true).
  assert (continuator->threadpool->nb_async_tasks--);
  // Broadcast (but not before the continuator has been converted into a task).
  //timer_unset (continuator->timeout_timer);     // Too slow
  thrd_honored (cnd_broadcast (&continuator->threadpool->proceed_or_conclude_or_runoff));
  // continuator->p_uid is NOT free'd.
  free (continuator);           // Remove the continuator.
  *remove = 1;                  // Remove the continuator from the map.
  return 0;
}

int
threadpool_task_continue (uint64_t uid)
{
  if (!map_find_key (Continuators.map, &uid, threadpool_task_continuator_continue_operator, 0))
  {
    errno = ETIMEDOUT;
    return EXIT_FAILURE;
  }
  else
    return EXIT_SUCCESS;
}

uint64_t
threadpool_task_continuation (int (*work) (struct threadpool *threadpool, void *data), double seconds)
{
  static atomic uint32_t seq = 0;
  if (!Worker_context.threadpool || !Worker_context.current_task || Worker_context.current_task->to_be_continued)
  {
    fprintf (stderr, "%s: %s\n", __func__, _("Operation not permitted."));
    errno = EPERM;
    return 0;
  }
  if (!work)
  {
    fprintf (stderr, "%s: %s\n", __func__, _("Invalid argument."));
    errno = EINVAL;
    return 0;
  }

  struct continuator_data *continuator = calloc (1, sizeof (*continuator));
  if (!continuator)
  {
    fprintf (stderr, "%s: %s\n", __func__, _("Out of memory."));
    errno = ENOMEM;
    return 0;
  }
  continuator->job = Worker_context.current_task->job;
  continuator->work = work;
  struct timespec abs_timeout = delay_to_abs_timespec (seconds > 0 ? seconds : 0);      // from timers.h
  continuator->threadpool = Worker_context.threadpool;
  Worker_context.current_task->to_be_continued = 1;
  do
  {
    continuator->uid = (uint64_t) (++seq ? seq : ++seq) // Less significant (non null sequence)
      + (((uint64_t) rand ()) << 32);   // Most significant (random)
  }
  while (!map_insert_data (Continuators.map, continuator));     // continuator is inserted in the map.
  uint64_t *p_uid = malloc (sizeof (*p_uid));
  continuator->p_uid = p_uid;
  *p_uid = continuator->uid;    // p_uid will not fade out and will survive the continuator.
  // p_uid will be passed to the timer handler threadpool_task_continuation_timeout_handler where it will be free'd.
  continuator->timeout_timer = timer_set (abs_timeout, threadpool_task_continuation_timeout_handler, p_uid);
  continuator->threadpool->nb_async_tasks++;
  return continuator->uid;
}

// ================= Monitoring =================
static int
threadpool_monitor_exec (struct threadpool *monitor, void *data)
{
  (void) monitor;
  const struct threadpool *threadpool = ((struct threadpool_monitor *) data)->threadpool;
  if (threadpool->monitor.displayer)
    threadpool->monitor.displayer (*(struct threadpool_monitor *) data, threadpool->monitor.argument);
  return 0;
}

static void
threadpool_monitor_call (struct threadpool *threadpool, int force)
{
  if (threadpool->monitor.displayer && threadpool->monitor.processor)
  {
    struct threadpool_monitor v = {.threadpool = threadpool,.closed = threadpool->concluding,
      .workers = {.nb_requested = threadpool->requested_nb_workers,.nb_max = threadpool->max_nb_workers,
                  .nb_idle = threadpool->nb_idle_workers,.nb_alive = threadpool->nb_alive_workers,},
      .tasks = {.nb_submitted = threadpool->nb_submitted_tasks,
                .nb_processing = threadpool->nb_processing_tasks,.nb_asynchronous = threadpool->nb_async_tasks,
                .nb_succeeded = threadpool->nb_succeeded_tasks,.nb_failed = threadpool->nb_failed_tasks,
                .nb_pending = threadpool->nb_pending_tasks,.nb_canceled = threadpool->nb_canceled_tasks,},
    };
    struct timespec t;
    timespec_get (&t, TIME_UTC);        // C standard function, returns now.
    v.time = difftime (t.tv_sec, threadpool->monitor.t0.tv_sec) // type of tv_sec is time_t, difftime does not overflow
      + 1.e-9 * (double) (t.tv_nsec - threadpool->monitor.t0.tv_nsec);  // tv_nsec is signed.
    if (!threadpool->monitor.filter)
      force = 0;
    if (!force && threadpool->monitor.filter && !threadpool->monitor.filter (v))
      return;
    struct threadpool_monitor *p = malloc (sizeof (*p));
    if (p)
    {
      *p = v;
      threadpool_add_task (threadpool->monitor.processor, threadpool_monitor_exec, p, free);    // p will be free'd at task termination (see note (*)) by a call to free.
    }
  }
}

void
threadpool_monitor (struct threadpool *threadpool)
{
  if (!threadpool->monitor.filter)
    threadpool_monitor_call (threadpool, 1);
}

void
threadpool_set_monitor (struct threadpool *threadpool, threadpool_monitor_handler new, void *a, int (*filter) (struct threadpool_monitor d))
{
  thrd_honored (mtx_lock (&threadpool->mutex));
  threadpool->monitor.displayer = new;
  threadpool->monitor.argument = a;
  threadpool->monitor.filter = filter;
  if (new && !threadpool->monitor.processor)
    threadpool->monitor.processor = threadpool_create_and_start (TP_WORKER_SEQUENTIAL, &threadpool->monitor.last_time, TP_RUN_ALL_TASKS);
  threadpool_monitor_call (threadpool, 0);
  thrd_honored (mtx_unlock (&threadpool->mutex));
}

static once_flag I18N_INIT = ONCE_FLAG_INIT;
static void
threadpool_i18n_init (void)
{
  i18n_init;
}

void
threadpool_monitor_to_terminal (struct threadpool_monitor data, void *FILE_stream)
{
  call_once (&I18N_INIT, threadpool_i18n_init);
  struct
  {
    size_t upper;
    char c;
  } datas[] = { {data.tasks.nb_succeeded, '='}, {data.tasks.nb_failed, 'X'}, {data.tasks.nb_asynchronous, '?'}, {data.tasks.nb_processing, '*'},
  {data.tasks.nb_pending, '.'}, {data.tasks.nb_canceled, '/'},
  //{data.workers.nb_idle, '~'},
  };
  static FILE *f = 0;
  if (!f && !(f = FILE_stream))
    f = stderr;
  static atomic int legend = 0;
  if (!legend)
  {
    fprintf (f, "%s\n", _("[Thread pool UID][Elapsed seconds][Thread pool state (Nb alive workers/Nb allocated workers)][Nb submitted tasks] Tasks..."));
    fprintf (f, "     %s\n", _("Thread pool states: (R) running, (I) idle, (S) stopped."));
    fprintf (f, "     %s\n", _("Tasks             : (=) succeeded, (X) failed, (?) asynchronous, (*) processing, (.) pending, (/) canceled."));
    legend = 1;
  }
  fprintf (f, "[%p][% 10.4fs][%c (%zu/%zu)][%4zu] ", data.threadpool, data.time,
           data.tasks.nb_processing ? 'R' : data.workers.nb_idle ? 'I' : 'S', data.workers.nb_alive, data.workers.nb_max, data.tasks.nb_submitted);
  for (size_t j = 0; j < sizeof (datas) / sizeof (*datas); j++)
    for (size_t i = 0; i < datas[j].upper; i++)
      fprintf (f, "%c", datas[j].c);
  fprintf (f, "\n");
  fflush (f);
}

int
threadpool_monitor_every_100ms (struct threadpool_monitor d)
{
  static const double ms = 100; // 100 ms
  double *last_time = d.threadpool->monitor.processor->global_data;
  assert (last_time);
  if (d.workers.nb_alive == 0 || d.time > *last_time + ms / 1000.)
  {
    *last_time = d.time;
    return 1;
  }
  return 0;
}

// ================= Worker crew =================
static void
threadpool_clear_on_exit (void)
{
  map_traverse (Continuators.map, MAP_REMOVE_ALL, 0, continuator_data_clear_on_exit);
  map_destroy (Continuators.map);
}

static void
threadpool_init (void)          // Called once.
{
  Continuators.map = map_create (continuator_data_get_key, continuator_data_cmp_key, 0, 1);
  atexit (threadpool_clear_on_exit);
}

struct threadpool *
threadpool_create_and_start (size_t nb_workers, void *global_data, tp_property_t property)
{
  call_once (&THREADPOOL_INIT, threadpool_init);
  struct threadpool *threadpool = calloc (1, sizeof (*threadpool));     // All attributes are set to 0 (including pointers).
  if (!threadpool)
    goto on_error;
  if (nb_workers == 0)
#ifdef __GLIBC__
    if (!(nb_workers = (size_t) get_nprocs ()))
#endif
      goto on_error;
  threadpool->property = property;
  threadpool->requested_nb_workers = nb_workers;
  if (!(threadpool->worker_id = malloc (threadpool->requested_nb_workers * sizeof (*threadpool->worker_id))))
    goto on_error;
  if (!(threadpool->active_worker_id = calloc (threadpool->requested_nb_workers, sizeof (*threadpool->active_worker_id))))      // All set to 0.
    goto on_error;
  thrd_honored (mtx_init (&threadpool->mutex, mtx_plain));
  thrd_honored (cnd_init (&threadpool->proceed_or_conclude_or_runoff));
  threadpool->global_data = global_data;
  threadpool->worker_local_data_manager.make = 0;
  threadpool->worker_local_data_manager.destroy = 0;
  threadpool->in = threadpool->out = 0;
  threadpool->concluding = 0;
  threadpool->max_nb_workers = threadpool->nb_alive_workers = threadpool->nb_idle_workers = 0;
  threadpool->nb_created_tasks = threadpool->nb_processing_tasks = threadpool->nb_succeeded_tasks =
    threadpool->nb_async_tasks = threadpool->nb_failed_tasks = threadpool->nb_pending_tasks = threadpool->nb_submitted_tasks = threadpool->nb_canceled_tasks = 0;
  threadpool->idle_timeout = 0.1;       // seconds.
  threadpool->resource.data = 0;
  threadpool->resource.allocator = 0;
  threadpool->resource.deallocator = 0;
  threadpool->monitor.displayer = 0;
  threadpool->monitor.argument = 0;
  threadpool->monitor.processor = 0;
  threadpool->monitor.last_time = 0;
  timespec_get (&threadpool->monitor.t0, TIME_UTC);     // C standard function, returns now.
  return threadpool;

on_error:
  fprintf (stderr, "%s: %s\n", __func__, _("Out of memory."));
  errno = ENOMEM;
  if (threadpool)
  {
    if (threadpool->worker_id)
      free (threadpool->worker_id);
    if (threadpool->active_worker_id)
      free (threadpool->active_worker_id);
    free (threadpool);
  }
  return 0;
}

static int
thread_worker_runner (void *args)
{
  thrd_detach (thrd_current ());        // Asks for disposing of any resources allocated to the worker thread when it terminates.
  struct threadpool *threadpool = args;
  Worker_context.threadpool = threadpool;       // Thread local variable
  thrd_honored (mtx_lock (&threadpool->mutex));
  Worker_context.local_data = threadpool->worker_local_data_manager.make ? threadpool->worker_local_data_manager.make () : 0;   // Call to threadpool->worker_local_data.make is thread-safe.
  while (1)                     // Looping on tasks (concurrently with other workers)
  {
    struct timespec timeout = delay_to_abs_timespec (threadpool->idle_timeout); // from timers.h
    threadpool->nb_idle_workers++;
    while (!threadpool_something_to_process_predicate (threadpool) && !threadpool_is_done_predicate (threadpool))       // Predicate is not fulfilled: wait in idle state.
    {
      threadpool_monitor_call (threadpool, 0);
      int cnd;
      if (threadpool->nb_async_tasks)
        thrd_honored (cnd_wait (&threadpool->proceed_or_conclude_or_runoff, &threadpool->mutex));       // Wait for continuators to be processed (threadpool_task_continue) or to timeout (threadpool_task_continuation_timeout_handler).
      else if ((cnd = cnd_timedwait (&threadpool->proceed_or_conclude_or_runoff, &threadpool->mutex, &timeout)) == thrd_timedout)       // Wait for condition to be signalled or until after the TIME_UTC-based calendar time pointed to by &timeout
        break;                  // Timeout: time to end the worker.
      else
        thrd_honored (cnd);
    }                           // while (!threadpool_something_to_process_predicate (threadpool) && !threadpool_is_done_predicate (threadpool))
    assert (threadpool->nb_idle_workers--);
    if (threadpool_something_to_process_predicate (threadpool)) // First condition of the predicate is true (both conditions can't be true at the same time by design.)
    {
      struct elem *old_elem = threadpool->out;
      if (threadpool->in == threadpool->out)
        threadpool->in = threadpool->out = 0;   // The first condition of predicate becomes false: no need to signal it.
      else
        threadpool->out = threadpool->out->next;        // The first condition of the predicate remains true: no need to signal it.
      if (old_elem->task.work)
      {
        assert (threadpool->nb_pending_tasks--);
        threadpool->nb_processing_tasks++;      // The extracted data has to be processed somewhere.
        threadpool_monitor_call (threadpool, 0);        // Processing worker
        Worker_context.current_task = &old_elem->task;
        thrd_honored (mtx_unlock (&threadpool->mutex)); // Unlock
        int ret = old_elem->task.work (threadpool, old_elem->task.job.data);    //<<<<<<<<<< work <<<<<<<<<<< (N.B.: work could itself add tasks by calling 'threadpool_add_task').
        if (old_elem->task.to_be_continued)
          /* Nothing */ ;
        else if ((threadpool->property == TP_RUN_ALL_SUCCESSFUL_TASKS && ret != EXIT_SUCCESS) || (threadpool->property == TP_RUN_ONE_SUCCESSFUL_TASK && ret == EXIT_SUCCESS))
          threadpool_cancel_task (threadpool, TP_CANCEL_ALL_PENDING_TASKS);     // Cancel automatically other already submitted tasks.
        thrd_honored (mtx_lock (&threadpool->mutex));   // Relock
        Worker_context.current_task = 0;
        assert (threadpool->nb_processing_tasks--);
        if (old_elem->task.to_be_continued)
          /* Nothing */ ;
        else if (ret != EXIT_SUCCESS)
          threadpool->nb_failed_tasks++;
        else
          threadpool->nb_succeeded_tasks++;
      }
      threadpool_monitor_call (threadpool, 0);
      if (!old_elem->task.to_be_continued && old_elem->task.job.data_delete)    // Call to task.job.data_delete is MT-safe (guarded by threadpool->mutex)
        old_elem->task.job.data_delete (old_elem->task.job.data);       // Note (*): get rid of job after use (and if it is not scheduled in a continuation).
      free (old_elem);
      continue;                 // while (1) 
    }                           // if (threadpool_something_to_process_predicate (threadpool))
    else if (threadpool_is_done_predicate (threadpool)) // Second condition of the predicate is true: 
      thrd_honored (cnd_broadcast (&threadpool->proceed_or_conclude_or_runoff));        // broadcast it to unblock and finish all pending threads.
    break;                      // Work is done or the predicate was not fulfilled due to timeout. Quit.
  }                             // while (1)
  void *localdata = threadpool_worker_local_data ();
  Worker_context.local_data = 0;
  if (threadpool->worker_local_data_manager.destroy)
    threadpool->worker_local_data_manager.destroy (localdata);
  for (size_t i = 0; i < threadpool->requested_nb_workers; i++)
    if (threadpool->active_worker_id[i] && thrd_equal (thrd_current (), *threadpool->active_worker_id[i]))
    {
      threadpool->active_worker_id[i] = 0;      // Unregister active worker.
      assert (threadpool->nb_alive_workers--);
      threadpool_monitor_call (threadpool, 0);
      if (threadpool->nb_alive_workers == 0 && threadpool->resource.deallocator)
      {
        threadpool->resource.deallocator (threadpool->resource.data);
        threadpool->resource.data = 0;
        threadpool_monitor_call (threadpool, 0);
      }
      if (threadpool_runoff_predicate (threadpool))     // The last worker is quitting:
        thrd_honored (cnd_signal (&threadpool->proceed_or_conclude_or_runoff)); //  signals it.
      break;
    }
  Worker_context.threadpool = 0;
  thrd_honored (mtx_unlock (&threadpool->mutex));
  return 1;
}

static size_t
threadpool_create_task (struct threadpool *threadpool, int (*work) (struct threadpool *threadpool, void *job), void *job, void (*job_delete) (void *job), int is_continuation)
{
  thrd_honored (mtx_lock (&threadpool->mutex));
  if (!is_continuation)
    if ((threadpool->property == TP_RUN_ONE_SUCCESSFUL_TASK && threadpool->nb_succeeded_tasks)
        || (threadpool->property == TP_RUN_ALL_SUCCESSFUL_TASKS && threadpool->nb_failed_tasks))
      work = 0;                 // Cancel automatically new submitted tasks.
  if (!work)                    // Cancel task immediately.
  {
    threadpool->nb_submitted_tasks++;
    threadpool->nb_canceled_tasks++;
    if (job_delete)
      job_delete (job);
    if (++threadpool->nb_created_tasks == TP_CANCEL_ALL_PENDING_TASKS)
      threadpool->nb_created_tasks = 1; // Overflow. Wrap around.
    size_t id = threadpool->nb_created_tasks;   // task.id starts from 1.
    thrd_honored (mtx_unlock (&threadpool->mutex));
    return id;
  }

  struct elem *new_elem = malloc (sizeof (*new_elem));
  if (!new_elem)
  {
    thrd_honored (mtx_unlock (&threadpool->mutex));
    fprintf (stderr, "%s: %s\n", __func__, _("Out of memory."));
    errno = ENOMEM;
    return 0;
  }
  struct task task = {.job.data = job,.work = work,.job.data_delete = job_delete,.to_be_continued = 0,.is_continuation = is_continuation };
  new_elem->task = task;
  new_elem->next = 0;
  if (!threadpool->in)
    threadpool->in = threadpool->out = new_elem;
  else
  {
    threadpool->in->next = new_elem;
    threadpool->in = new_elem;
  }
  if (++threadpool->nb_created_tasks == TP_CANCEL_ALL_PENDING_TASKS)
    threadpool->nb_created_tasks = 1;   // Overflow. Wrap around.
  size_t id = new_elem->task.id = threadpool->nb_created_tasks; // task.id starts from 1.
  if (!is_continuation)
    threadpool->nb_submitted_tasks++;
  threadpool->nb_pending_tasks++;
  if (threadpool->nb_idle_workers)      // A job has been added to the thread pool of workers and at least one worker is idle and available:
    thrd_honored (cnd_signal (&threadpool->proceed_or_conclude_or_runoff));     // Signal it to wake up one of the idle workers.
  else if (threadpool->nb_alive_workers < threadpool->requested_nb_workers)     // No workers are idle and available to process this new task at once:
    for (size_t i = 0; i < threadpool->requested_nb_workers; i++)       // Search for a non-running worker and start it.
      if (!threadpool->active_worker_id[i] && thrd_create (&threadpool->worker_id[i], thread_worker_runner, threadpool) == thrd_success)        // Create a new worker.
      {
        threadpool->active_worker_id[i] = &threadpool->worker_id[i];    // Register active worker.
        // Note: a new worker thread has been created by thrd_create, but thread_worker_runner might not be launched right away.
        // Anyway, the worker has to be taken into consideration by the predicate threadpool_runoff_predicate with threadpool->nb_alive_workers++ to
        // let the thread pool know a new worker in on its way. This can not be deferred at the beginning of thread_worker_runner.
        if (threadpool->nb_alive_workers == 0 && threadpool->resource.allocator && !threadpool->resource.data)
        {
          threadpool_monitor_call (threadpool, 0);
          threadpool->resource.data = threadpool->resource.allocator (threadpool->global_data);
        }
        threadpool->nb_alive_workers++;
        if (threadpool->max_nb_workers < threadpool->nb_alive_workers)
          threadpool->max_nb_workers = threadpool->nb_alive_workers;
        break;
      }
  threadpool_monitor_call (threadpool, 0);
  thrd_honored (mtx_unlock (&threadpool->mutex));
  return id;
}

size_t
threadpool_add_task (struct threadpool *threadpool, int (*work) (struct threadpool *threadpool, void *job), void *job, void (*job_delete) (void *job))
{
  return threadpool_create_task (threadpool, work, job, job_delete, 0);
}

void
threadpool_wait_and_destroy (struct threadpool *threadpool)
{
  thrd_honored (mtx_lock (&threadpool->mutex));
  threadpool_monitor_call (threadpool, 1);
  threadpool->concluding = 1;   // Declares that no more tasks will be added into the FIFO by the caller of 'threadpool_wait_and_destroy' (processing workers can still add tasks).
  // The predicate is modified to true (concluding set to 1):
  if (threadpool_is_done_predicate (threadpool))        // No running tasks (asynchronous or not)
    thrd_honored (cnd_broadcast (&threadpool->proceed_or_conclude_or_runoff));  // broadcast it to unblock and finish all pending threads.
  while (!threadpool_runoff_predicate (threadpool))     // Wait for all tasks (either virtual or not) to be processed and all running workers to terminate properly.
    thrd_honored (cnd_wait (&threadpool->proceed_or_conclude_or_runoff, &threadpool->mutex));
  threadpool_monitor_call (threadpool, 1);
  if (threadpool->monitor.processor)
    threadpool_wait_and_destroy (threadpool->monitor.processor);        // Barrier to wait for all monitoring processes to finish.
  thrd_honored (mtx_unlock (&threadpool->mutex));

  free (threadpool->worker_id);
  free (threadpool->active_worker_id);
  mtx_destroy (&threadpool->mutex);
  cnd_destroy (&threadpool->proceed_or_conclude_or_runoff);
  free (threadpool);
}

void *
threadpool_worker_local_data (void)
{
  return Worker_context.local_data;
}

void *
threadpool_global_data (void)
{
  if (Worker_context.threadpool)
    return Worker_context.threadpool->global_data;
  else
    return 0;
}

size_t
threadpool_cancel_task (struct threadpool *threadpool, size_t task_id)
{
  size_t ret = 0;
  thrd_honored (mtx_lock (&threadpool->mutex));
  if (task_id == TP_CANCEL_LAST_PENDING_TASK)
  {
    struct elem *last = 0;
    for (struct elem * e = threadpool->out; e; e = e->next)
      if (e->task.work)
        last = e;
    if (last)
    {
      last->task.work = 0;      // The job won't be processed by thread_worker_runner.
      ret++;
    }
  }
  else
    for (struct elem * e = threadpool->out; e; e = e->next)
    {
      if (!((task_id == TP_CANCEL_NEXT_PENDING_TASK && e->task.work) || e->task.id == task_id || task_id == TP_CANCEL_ALL_PENDING_TASKS))
        continue;
      if (e->task.work)
        ret++;
      e->task.work = 0;         // The job won't be processed by thread_worker_runner.
      if (task_id == TP_CANCEL_NEXT_PENDING_TASK || e->task.id == task_id)
        break;
    }
  if (ret)
  {
    threadpool->nb_canceled_tasks += ret;
    assert (threadpool->nb_pending_tasks >= ret);
    threadpool->nb_pending_tasks -= ret;
    threadpool_monitor_call (threadpool, 0);
  }
  thrd_honored (mtx_unlock (&threadpool->mutex));
  return ret;
}

void
threadpool_set_idle_timeout (struct threadpool *threadpool, double delay)
{
  static double inifinity = 120 * 24 * 3600 /* seconds */ ;     // 120 UTC days.
  if (delay > inifinity)
    delay = inifinity;
  if (delay >= 0.)
  {
    thrd_honored (mtx_lock (&threadpool->mutex));
    threadpool->idle_timeout = delay;
    thrd_honored (mtx_unlock (&threadpool->mutex));
  }
  else
    errno = EINVAL;
}

void
threadpool_set_global_resource_manager (struct threadpool *threadpool, void *(*allocator) (void *global_data), void (*deallocator) (void *resource))
{
  thrd_honored (mtx_lock (&threadpool->mutex));
  if (threadpool->nb_alive_workers || threadpool->resource.data)
  {
    call_once (&I18N_INIT, threadpool_i18n_init);
    fprintf (stderr, "%s: %s\n", __func__, _("Operation not permitted."));
    errno = EPERM;
  }
  else
  {
    threadpool->resource.allocator = allocator;
    threadpool->resource.deallocator = deallocator;
  }
  thrd_honored (mtx_unlock (&threadpool->mutex));
}

void *
threadpool_global_resource (void)
{
  if (Worker_context.threadpool)
    return Worker_context.threadpool->resource.data;
  else
    return 0;
}

void
threadpool_set_worker_local_data_manager (struct threadpool *threadpool, void *(*make_local) (void), void (*delete_local) (void *local_data))
{
  thrd_honored (mtx_lock (&threadpool->mutex));
  if (threadpool->nb_alive_workers)
  {
    call_once (&I18N_INIT, threadpool_i18n_init);
    fprintf (stderr, "%s: %s\n", __func__, _("Operation not permitted."));
    errno = EPERM;
  }
  else
  {
    threadpool->worker_local_data_manager.make = make_local;
    threadpool->worker_local_data_manager.destroy = delete_local;
  }
  thrd_honored (mtx_unlock (&threadpool->mutex));
}
