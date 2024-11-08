// Multi-threaded work queue manager
// (c) L. Farhi, 2024
// Language: C (C11)
#include <stdlib.h>
#include <threads.h>
#include <sys/sysinfo.h>        // for get_nprocs
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include "wqm.h"
#define thrd_honored(cond) do { if ((cond) != thrd_success) abort () ; } while (0)      // returns thrd_success on success or thrd_error if the request could not be honored.
#define threadpool_something_to_process_predicate(threadpool)   ((threadpool)->out)     // Indicates that the FIFO is not empty.
#define threadpool_is_done_predicate(threadpool)   ( (threadpool)->nb_processing_tasks == 0 && \
                                                     !threadpool_something_to_process_predicate (threadpool) && \
                                                     (threadpool)->concluding ) // The FIFO is empty and there is not work in progress or new task that could ever fill it (all expected tasks have been processed).
// N.B.: Once done, a FIFO cannot be undone by design: there aren't any data being processed left, that could have called 'threadpool_add_task' and refilled the empty FIFO (see loop in 'thread_worker_starter').
#define threadpool_runoff_predicate(threadpool) (threadpool_is_done_predicate(threadpool) && (threadpool)->nb_running_workers == 0)

#ifdef __GLIBC__
size_t const NB_CPU = 0;
#endif
size_t const SEQUENTIAL = 1;
size_t const ALL_TASKS = SIZE_MAX - 2;
size_t const NEXT_TASK = SIZE_MAX - 1;
size_t const LAST_TASK = SIZE_MAX;
struct threadpool
{
  size_t max_nb_workers;
  thrd_t *worker_id /* [max_nb_workers] */ ;
  mtx_t mutex;
  void *global_data;
  struct local_data             // Thread specific local data
  {
    tss_t reference;
    void *(*make) (void);
    void (*destroy) (void *local_data);
  } worker_local_data;
  size_t nb_running_workers, nb_idle_workers, nb_processing_tasks, nb_succeeded_tasks, nb_failed_tasks, nb_pending_tasks, nb_submitted_tasks, nb_canceled_tasks;
  thrd_t **running_worker_id /* [max_nb_workers] */ ;
  struct elem                   // Elements in FIFO.
  {
    struct elem *next;
    struct task                 // Task to be processed by a worker.
    {
      void *job;
      int (*work) (struct threadpool * threadpool, void *job);
      void (*job_delete) (void *job);
      size_t id;
    } task;
  } *in, *out;
  int concluding;               // Indicates that 'threadpool_wait_and_destroy' has been called. Only workers can now add tasks (in 'thread_worker_starter').
  cnd_t proceed_or_conclude_or_runoff;  // Associated with 3 exclusive predicates.
  // Monitoring
  struct
  {
    void (*f) (struct threadpool_monitor, void *a);
    void *a;
  } monitor;
  struct threadpool *monitoring;
  struct timespec t0;
};

static tss_t TSS_THREADPOOL;    // thread-specific storage of the threadpool in which a worker is running.
static once_flag TSS_THREADPOOL_CREATED = ONCE_FLAG_INIT;

// ================= Monitoring =================
static int
threadpool_monitor_exec (struct threadpool *monitoring, void *data)
{
  (void) monitoring;
  struct threadpool *threadpool = threadpool_global_data ();
  if (threadpool->monitor.f)
    threadpool->monitor.f (*(struct threadpool_monitor *) data, threadpool->monitor.a);
  return 0;
}

static void
threadpool_monitor_call (struct threadpool *threadpool)
{
  if (threadpool->monitor.f && threadpool->monitoring)
  {
    struct threadpool_monitor *p = malloc (sizeof (*p));
    if (p)
    {
      *p = (struct threadpool_monitor)
      {.threadpool = threadpool,.workers = {.max_nb = threadpool->max_nb_workers,.nb_idle = threadpool->nb_idle_workers,},
      .tasks = {.nb_submitted = threadpool->nb_submitted_tasks,
                .nb_processing = threadpool->nb_processing_tasks,
                .nb_succeeded = threadpool->nb_succeeded_tasks,.nb_failed = threadpool->nb_failed_tasks,
                .nb_pending = threadpool->nb_pending_tasks,.nb_canceled = threadpool->nb_canceled_tasks,},
      };
      struct timespec t;
      timespec_get (&t, TIME_UTC);
      if (t.tv_nsec < threadpool->t0.tv_nsec)
      {
        t.tv_sec--;             // -1s
        t.tv_nsec += 1000000000;        // +1s
      }
      t.tv_sec -= threadpool->t0.tv_sec;
      t.tv_nsec -= threadpool->t0.tv_nsec;
      p->time = (double) t.tv_sec + (double) t.tv_nsec / 1e9;
      threadpool_add_task (threadpool->monitoring, threadpool_monitor_exec, p, free);
    }
  }
}

void
threadpool_set_monitor (struct threadpool *threadpool, threadpool_monitor_handler new, void *a)
{
  thrd_honored (mtx_lock (&threadpool->mutex));
  threadpool->monitor.f = new;
  threadpool->monitor.a = a;
  if (new && !threadpool->monitoring)
    threadpool->monitoring = threadpool_create_and_start (SEQUENTIAL, threadpool, 0, 0);
  threadpool_monitor_call (threadpool);
  thrd_honored (mtx_unlock (&threadpool->mutex));
}

void
threadpool_monitor_to_terminal (struct threadpool_monitor data, void *FILE_stream)
{
  struct
  {
    size_t upper;
    char c;
  } datas[] = { {data.tasks.nb_succeeded, '='}, {data.tasks.nb_failed, 'X'}, {data.tasks.nb_processing, '*'},
  {data.tasks.nb_pending, '.'}, {data.tasks.nb_canceled, '/'}, {data.workers.nb_idle, '-'},
  };
  static FILE *f = 0;
  if (!f)
    f = FILE_stream ? FILE_stream : stderr;
  static int legend = 0;
  if (!legend)
    legend = fprintf (f, "(=) succeeded tasks, (X) failed tasks, (*) processing tasks, (.) pending tasks, (/) canceled tasks, (-) idle workers.\n");
  fprintf (f, "[%p (%zu)][% 10.4fs][%4zu] ", data.threadpool, data.workers.max_nb, data.time, data.tasks.nb_submitted);
  for (size_t j = 0; j < sizeof (datas) / sizeof (*datas); j++)
    for (size_t i = 0; i < datas[j].upper; i++)
      fprintf (f, "%c", datas[j].c);
  fprintf (f, "\n");
  fflush (f);
}

// ================= Worker crew =================
static void
tss_threadpool_create (void)
{
  thrd_honored (tss_create (&TSS_THREADPOOL, 0));       // No tss_delete will be called.
}

struct threadpool *
threadpool_create_and_start (size_t nb_workers, void *global_data, void *(*make_local) (void), void (*delete_local) (void *local_data))
{
  struct threadpool *threadpool = calloc (1, sizeof (*threadpool));     // All attributes are set to 0 (including pointers).
  if (!threadpool)
    goto on_error;
  if (nb_workers == 0)
#ifdef __GLIBC__
    if (!(nb_workers = (size_t) get_nprocs ()))
#endif
      goto on_error;
  threadpool->max_nb_workers = nb_workers;
  if (!(threadpool->worker_id = malloc (threadpool->max_nb_workers * sizeof (*threadpool->worker_id))))
    goto on_error;
  if (!(threadpool->running_worker_id = calloc (threadpool->max_nb_workers, sizeof (*threadpool->running_worker_id))))  // All set to 0.
    goto on_error;
  if (tss_create
      (&threadpool->worker_local_data.reference, 0 /* The destructor of the thread-specific storage will be handled manually (see thread_worker_runner). */ ) != thrd_success)
    goto on_error;
  thrd_honored (mtx_init (&threadpool->mutex, mtx_plain));
  thrd_honored (cnd_init (&threadpool->proceed_or_conclude_or_runoff));
  threadpool->global_data = global_data;
  threadpool->worker_local_data.make = make_local;
  threadpool->worker_local_data.destroy = delete_local;
  threadpool->in = threadpool->out = 0;
  threadpool->concluding = 0;
  threadpool->nb_running_workers = threadpool->nb_idle_workers = threadpool->nb_processing_tasks = threadpool->nb_succeeded_tasks =
    threadpool->nb_failed_tasks = threadpool->nb_pending_tasks = threadpool->nb_submitted_tasks = threadpool->nb_canceled_tasks = 0;
  threadpool->monitor.f = 0;
  threadpool->monitor.a = 0;
  threadpool->monitoring = 0;
  timespec_get (&threadpool->t0, TIME_UTC);
  call_once (&TSS_THREADPOOL_CREATED, tss_threadpool_create);
  return threadpool;

on_error:
  errno = ENOMEM;
  if (threadpool)
  {
    if (threadpool->worker_id)
      free (threadpool->worker_id);
    if (threadpool->running_worker_id)
      free (threadpool->running_worker_id);
    free (threadpool);
  }
  return 0;
}

static int
thread_worker_runner (void *args)
{
  thrd_detach (thrd_current ());        // dispose of any resources allocated to the thread when it terminates.
  struct threadpool *threadpool = args;
  thrd_honored (tss_set (TSS_THREADPOOL, threadpool));
  thrd_honored (mtx_lock (&threadpool->mutex));
  thrd_honored (tss_set (threadpool->worker_local_data.reference, threadpool->worker_local_data.make ? threadpool->worker_local_data.make () : 0));     // Call to threadpool->worker_local_data.make is thread-safe.
  while (1)                     // Looping on tasks (concurrently with other workers)
  {
    static const struct timespec timeout_delay = {.tv_sec = 0,.tv_nsec = 100 * 1000 * 1000 };   // Idle time (0.1 s)
    struct timespec timeout;
    timespec_get (&timeout, TIME_UTC);
    timeout.tv_sec += timeout_delay.tv_sec + (timeout.tv_nsec + timeout_delay.tv_nsec) / (1000 * 1000 * 1000);
    timeout.tv_nsec = (timeout.tv_nsec + timeout_delay.tv_nsec) % (1000 * 1000 * 1000);
    threadpool->nb_idle_workers++;
    while (!threadpool_something_to_process_predicate (threadpool) && !threadpool_is_done_predicate (threadpool))       // Predicate is not fulfilled.
    {
      threadpool_monitor_call (threadpool);     // Idle
      if (cnd_timedwait (&threadpool->proceed_or_conclude_or_runoff, &threadpool->mutex, &timeout) == thrd_timedout)    // Wait until after the TIME_UTC-based calendar time pointed to by &timeout
        break;                  // Timeout
    }
    threadpool->nb_idle_workers--;
    if (threadpool_something_to_process_predicate (threadpool)) // First condition of the predicate is true (both conditions can't be true at the same time by design.)
    {
      struct elem *old_elem = threadpool->out;
      if (threadpool->in == threadpool->out)
        threadpool->in = threadpool->out = 0;   // The first condition of predicate becomes false: no need to signal it.
      else
        threadpool->out = threadpool->out->next;        // The first condition of the predicate remains true: no need to signal it.
      if (old_elem->task.work)
      {
        threadpool->nb_pending_tasks--;
        threadpool->nb_processing_tasks++;      // The extracted data has to be processed somewhere.
        threadpool_monitor_call (threadpool);   // Processing worker
        thrd_honored (mtx_unlock (&threadpool->mutex));
        int ret = old_elem->task.work (threadpool, old_elem->task.job); //<<<<<<<<<< work <<<<<<<<<<< (N.B.: workers can add tasks by calling 'threadpool_add_task').
        thrd_honored (mtx_lock (&threadpool->mutex));
        threadpool->nb_processing_tasks--;
        if (ret)
          threadpool->nb_failed_tasks++;
        else
          threadpool->nb_succeeded_tasks++;
      }
      if (old_elem->task.job_delete)    // MT-safe
        old_elem->task.job_delete (old_elem->task.job); // Get rid of job after use.
      free (old_elem);
      continue;                 // while (1) 
    }
    else if (threadpool_is_done_predicate (threadpool)) // Second condition of the predicate is true: 
      thrd_honored (cnd_broadcast (&threadpool->proceed_or_conclude_or_runoff));        // broadcast it to unblock and finish all pending threads.
    break;                      // Work is done or the predicate was not fulfilled due to timeout. Quit.
  }                             // while (1)
  void *localdata = threadpool_worker_local_data ();
  thrd_honored (tss_set (threadpool->worker_local_data.reference, 0));  // tss_set does not invoke the destructor associated with the key on the value being replaced.
  if (threadpool->worker_local_data.destroy)
    // The destructor of the thread-specific storage is called manually in a thread-safe manner
    // and the before the thread is deallocated, allowing access to global data with threadpool_global_data.
    threadpool->worker_local_data.destroy (localdata);
  for (size_t i = 0; i < threadpool->max_nb_workers; i++)
    if (threadpool->running_worker_id[i] && thrd_equal (thrd_current (), *threadpool->running_worker_id[i]))
    {
      threadpool->running_worker_id[i] = 0;     // Unregister running worker.
      threadpool->nb_running_workers--;
      threadpool_monitor_call (threadpool);
      if (threadpool_runoff_predicate (threadpool))     // The last worker is quitting:
        thrd_honored (cnd_signal (&threadpool->proceed_or_conclude_or_runoff)); //  signals it.
      break;
    }
  thrd_honored (mtx_unlock (&threadpool->mutex));
  return 1;
}

size_t
threadpool_add_task (struct threadpool *threadpool, int (*work) (struct threadpool *threadpool, void *job), void *job, void (*job_delete) (void *job))
{
  thrd_honored (mtx_lock (&threadpool->mutex));
  struct elem *new_elem = malloc (sizeof (*new_elem));
  if (!new_elem)
  {
    thrd_honored (mtx_unlock (&threadpool->mutex));
    errno = ENOMEM;
    return 0;
  }
  struct task task = {.job = job,.work = work,.job_delete = job_delete };
  new_elem->task = task;
  new_elem->next = 0;
  if (!threadpool->in)
    threadpool->in = threadpool->out = new_elem;
  else
  {
    threadpool->in->next = new_elem;
    threadpool->in = new_elem;
  }
  if (work)
    threadpool->nb_pending_tasks++;
  else
    threadpool->nb_canceled_tasks++;
  if (++threadpool->nb_submitted_tasks == ALL_TASKS)
    threadpool->nb_submitted_tasks = 1;
  new_elem->task.id = threadpool->nb_submitted_tasks;
  if (threadpool->nb_idle_workers)      // A job has been added to the thread pool of workers and at least one worker is idle and available:
    thrd_honored (cnd_signal (&threadpool->proceed_or_conclude_or_runoff));     // Signal it to wake one of the pending workers.
  else if (threadpool->nb_running_workers < threadpool->max_nb_workers) // No worker are idle and available to process this new task at once:
    for (size_t i = 0; i < threadpool->max_nb_workers; i++)
      if (!threadpool->running_worker_id[i] && thrd_create (&threadpool->worker_id[i], thread_worker_runner, threadpool) == thrd_success)       // Create a new worker.
      {
        threadpool->running_worker_id[i] = &threadpool->worker_id[i];   // Register running worker.
        threadpool->nb_running_workers++;
        break;
      }
  threadpool_monitor_call (threadpool);
  thrd_honored (mtx_unlock (&threadpool->mutex));
  return new_elem->task.id;
}

void
threadpool_wait_and_destroy (struct threadpool *threadpool)
{
  thrd_honored (mtx_lock (&threadpool->mutex));
  threadpool->concluding = 1;   // Declares that no more tasks will be added into the FIFO by the caller of 'threadpool_wait_and_destroy' (processing workers can still add tasks).
  if (threadpool_is_done_predicate (threadpool))        // The predicate is modified to true:
    thrd_honored (cnd_broadcast (&threadpool->proceed_or_conclude_or_runoff));  // broadcast it to unblock and finish all pending threads.
  while (!threadpool_runoff_predicate (threadpool))     // Wait for all tasks to be processed and all running workers to terminate properly.
    thrd_honored (cnd_wait (&threadpool->proceed_or_conclude_or_runoff, &threadpool->mutex));
  thrd_honored (mtx_unlock (&threadpool->mutex));
  tss_delete (threadpool->worker_local_data.reference); // Does not invoke any destructors.
  free (threadpool->worker_id);
  free (threadpool->running_worker_id);
  mtx_destroy (&threadpool->mutex);
  cnd_destroy (&threadpool->proceed_or_conclude_or_runoff);
  if (threadpool->monitoring)
    threadpool_wait_and_destroy (threadpool->monitoring);       // Barrier to wait for all monitoring processes to finish. 
  free (threadpool);
}

void *
threadpool_worker_local_data (void)
{
  struct threadpool *threadpool = tss_get (TSS_THREADPOOL);
  if (threadpool)
    return tss_get (threadpool->worker_local_data.reference);
  else
    return 0;
}

void *
threadpool_global_data (void)
{
  struct threadpool *threadpool = tss_get (TSS_THREADPOOL);
  if (threadpool)
    return threadpool->global_data;
  else
    return 0;
}

size_t
threadpool_cancel_task (struct threadpool *threadpool, size_t task_id)
{
  size_t ret = 0;
  thrd_honored (mtx_lock (&threadpool->mutex));
  if (task_id == LAST_TASK)
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
      if (!((task_id == NEXT_TASK && e->task.work) || e->task.id == task_id || task_id == ALL_TASKS))
        continue;
      if (e->task.work)
        ret++;
      e->task.work = 0;         // The job won't be processed by thread_worker_runner.
      if (task_id == NEXT_TASK || e->task.id == task_id)
        break;
    }
  if (ret)
  {
    threadpool->nb_pending_tasks -= ret;
    threadpool->nb_canceled_tasks += ret;
    threadpool_monitor_call (threadpool);
  }
  thrd_honored (mtx_unlock (&threadpool->mutex));
  return ret;
}
