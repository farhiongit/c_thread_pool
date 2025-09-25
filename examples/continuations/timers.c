// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <assert.h>
#include <sys/resource.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>
#include "map.h"
#include "timer.h"
#include "wqm.h"

static const size_t NB_TIMERS = 4000;
static const double MAXDELAY = 1.;      // Seconds.
static const double RATIO = .4; // Timeout ratio (< 1, for the purpose of the example.)
static _Atomic size_t Nb_timers_done = 0;
static _Atomic size_t Nb_timers_started = 0;
typedef enum
{ PHASE_I, PHASE_II } phase;

struct async_task_wrapper
{
  uint64_t uid;
  struct timespec end_time;
  void *timer;
};

static struct async_task_wrapper *
async_task_create (double seconds)
{
  (void) seconds;
  struct async_task_wrapper *task = calloc (1, sizeof (*task));
  assert (task);
  task->end_time = delay_to_abs_timespec (seconds);
  return task;
}

static void
async_task_delete (struct async_task_wrapper *task)
{
  free (task);
}

static void
timer_handler (void *arg)
{
  errno = 0;
  Nb_timers_done++;
  struct async_task_wrapper *task = arg;
  assert (threadpool_task_continue (task->uid) == EXIT_SUCCESS || errno == ETIMEDOUT);  // Calls the continuation (resume) declared in wait.
  async_task_delete (task);
}

static int wait (void *);
static int
resume (void *j)
{
  // Do whatever wanted here at asynchronous task termination;
  if (*(phase *) j == PHASE_I)
  {
    *(phase *) j = PHASE_II;
    return wait (j);            // Phase II: the task continuation starts a new consecutive asynchronous call in a row.
  }
  else
    return EXIT_SUCCESS;
}

static tp_result_t
done (void *j, tp_result_t res)
{
  if (res == TP_JOB_SUCCESS)
  {
    size_t *counter = threadpool_global_data ();
    *counter += 1;
  }
  free (j);
  return res;
}

static int
wait (void *)
{
  struct async_task_wrapper *task = async_task_create (1. * MAXDELAY * rand () / RAND_MAX);     // Declares an asynchronous call.
  assert ((task->uid = threadpool_task_continuation (resume, RATIO * MAXDELAY)));       // Declares a continuation (resume) with time-out.
  task->timer = timer_set (task->end_time, timer_handler, task);        // This is the asynchronous call (a timer is used as an example). Will trigger continuation at termination, when the callback is called (timer_handler, at task->end_time, after a delay).
  Nb_timers_started++;
  return EXIT_SUCCESS;
}

static void
monitor_handler (struct threadpool_monitor d, void *)
{
  fprintf (stdout, "t=%6.2fs: %'zu active worker, %'zu processing virtual tasks, "
           "%'zu virtual tasks have succeeded, %'zu will definitely be out of time (over %'zu submitted).\n",
           d.time, d.workers.nb_alive, d.tasks.nb_asynchronous, d.tasks.nb_succeeded, d.tasks.nb_failed + d.tasks.nb_canceled, d.tasks.nb_submitted);
  fflush (stdout);
}

int
main (void)
{
  srand ((unsigned int) time (0));
  assert (RATIO >= 0. && RATIO <= 1.);
  struct timespec t0;
  timespec_get (&t0, TIME_UTC);
  size_t counter = 0;
  fprintf (stdout, "Creating the thread pool...\n");
  struct threadpool *tp = threadpool_create_and_start (TP_WORKER_SEQUENTIAL, &counter, TP_RUN_ALL_TASKS);       // One single worker will handle all the asynchronous calls.
  fprintf (stdout, "Running %zu virtual tasks (each task will run two consecutive asynchronous timers of at most %g seconds) on a %zu worker(s) "
           "(asynchronous calls will time out after %g seconds).\n", NB_TIMERS, 1. * MAXDELAY, threadpool_nb_workers (tp), 1. * RATIO * MAXDELAY);
  fprintf (stdout, " - %zu (phase I) then about %zu (phase II) asynchronous calls will be started.\n", NB_TIMERS, (size_t) (RATIO * (double) NB_TIMERS));
  fprintf (stdout, "   - Phase I : About %zu asynchronous calls will fall short due to time-out of the continuation.\n", (size_t) ((1. - RATIO) * (double) NB_TIMERS));
  fprintf (stdout, "   - Phase II : Then about %zu asynchronous calls will fall short due to time-out of the continuation.\n",
           (size_t) ((1. - RATIO) * RATIO * (double) NB_TIMERS));
  fprintf (stdout, "About %zu virtual tasks should succeed.\n", (size_t) (RATIO * RATIO * (double) NB_TIMERS));
  threadpool_set_monitor (tp, monitor_handler, 0, threadpool_monitor_every_100ms);
  fprintf (stdout, "Submiting %zu virtual tasks...\n", NB_TIMERS);
  for (size_t i = 0; i < NB_TIMERS; i++)
  {
    phase *j = malloc (sizeof (*j));
    *j = PHASE_I;
    threadpool_add_task (tp, wait, j, done);    // Phase I: starts an asynchronous call.
  }
  fprintf (stdout, "Waiting for the threads to end...\n");
  threadpool_wait_and_destroy (tp);
  fprintf (stdout, "The thread pool has been destroyed.\n");
  fprintf (stdout, "%zu virtual tasks have succeeded (vs around %zu expected).\n", counter, (size_t) (RATIO * RATIO * (double) NB_TIMERS));
  fprintf (stdout, "Waiting for the remaining out of time, late, thus disregarded, asynchronous calls to end...\n");
  static const struct timespec duration = { 0, 100000000 };     /* 100 ms */
  while (Nb_timers_done <= Nb_timers_started)
  {
    struct timespec now;
    timespec_get (&now, TIME_UTC);
    fprintf (stdout, "t=%6.2fs: %'zu asynchronous calls have now finisihed.\n", difftime (now.tv_sec, t0.tv_sec) + 1e-9 * (double) (now.tv_nsec - t0.tv_nsec), Nb_timers_done);
    fflush (stdout);
    if (Nb_timers_done == Nb_timers_started)
      break;
    nanosleep (&duration, 0);
  }
  fprintf (stdout, "=======\n");
}
