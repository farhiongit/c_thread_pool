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
static const double MAXDELAY = 2.;
static const double TIMEOUT = 0.7 * MAXDELAY;   // Timeout (seconds) (shortened a little bit, for the purpose of the example.)
static size_t Nb_timers_done = 0;

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
  assert (threadpool_task_continue (task->uid) == EXIT_SUCCESS || errno == ETIMEDOUT);  // Trigger continuation.
  async_task_delete (task);
}

static int
resume (void *)
{
  // Do whatever wanted here at asynchronous task termination;
  return EXIT_SUCCESS;
}

static int
wait (void *)
{
  struct async_task_wrapper *task = async_task_create (1. * MAXDELAY * rand () / RAND_MAX);
  assert ((task->uid = threadpool_task_continuation (resume, TIMEOUT)));        // Declare continuation (resume).
  task->timer = timer_set (task->end_time, timer_handler, task);        // Trigger continuation (timer_handler) after a delay.
  return EXIT_SUCCESS;
}

static void
monitor_handler (struct threadpool_monitor d, void *)
{
  fprintf (stdout, "t=%6.2fs: %'zu active worker, %'zu processing virtual tasks, "
           "%'zu virtual tasks have succeeded, %'zu will definitely be out of time (over %'zu submitted).\n",
           d.time, d.workers.nb_alive, d.tasks.nb_asynchronous, d.tasks.nb_succeeded, d.tasks.nb_failed, d.tasks.nb_submitted);
  fflush (stdout);
}

int
main (void)
{
  fprintf (stdout, "Running %zu virtual tasks (asynchronous timers of at most %g seconds) on a single worker "
           "(virtual tasks will time out after %g seconds).\n", NB_TIMERS, 1. * MAXDELAY, 1. * TIMEOUT);
  fprintf (stdout, "Creating the thread pool...\n");
  struct timespec t0;
  timespec_get (&t0, TIME_UTC);
  struct threadpool *tp = threadpool_create_and_start (TP_WORKER_SEQUENTIAL, 0, TP_RUN_ALL_TASKS);
  threadpool_set_monitor (tp, monitor_handler, 0, threadpool_monitor_every_100ms);
  fprintf (stdout, "Submiting virtual tasks...\n");
  for (size_t i = 0; i < NB_TIMERS; i++)
    threadpool_add_task (tp, wait, 0, 0);
  fprintf (stdout, "Waiting for the threads to end...\n");
  threadpool_wait_and_destroy (tp);
  fprintf (stdout, "The thread pool has been destroyed.\n");
  fprintf (stdout, "Waiting for the remaining out of time, late, thus disregarded, virtual tasks to time out...\n");
  static const struct timespec duration = { 0, 100000000 };     /* 100 ms */
  while (Nb_timers_done <= NB_TIMERS)
  {
    struct timespec now;
    timespec_get (&now, TIME_UTC);
    fprintf (stdout, "t=%6.2fs: %'zu virtual tasks have now finisihed.\n", difftime (now.tv_sec, t0.tv_sec) + 1e-9 * (double) (now.tv_nsec - t0.tv_nsec), Nb_timers_done);
    fflush (stdout);
    if (Nb_timers_done >= NB_TIMERS)
      break;
    nanosleep (&duration, 0);
  }
  fprintf (stdout, "=======\n");
}
