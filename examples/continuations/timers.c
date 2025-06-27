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

#define NB_TIMERS (10 * 1000)
#define MAXDELAY (2.)
#define TIMEOUT (0.7 * (MAXDELAY))      // Timeout (seconds) (shortened a little bit, for the purpose of the example.)

struct async_task_wrapper
{
  uint64_t uid;
  struct timespec end_time;
  void* timer;
};

static struct async_task_wrapper *
async_task_create (double seconds)
{
  (void) seconds;
  struct async_task_wrapper *task = calloc (1, sizeof (*task));
  assert (task);
  static struct timespec duration = { 0, 7 /* microseconds */  * 1000 };
  nanosleep (&duration, 0);     // Artificial duration of creation (as for timer_create).
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
  struct async_task_wrapper *task = arg;
  assert (threadpool_task_continue (task->uid) == EXIT_SUCCESS || errno == ETIMEDOUT);  // Trigger continuation.
  async_task_delete (task);
}

static int
resume (struct threadpool * /* tp */ , void * /* arg */ )
{
  return EXIT_SUCCESS;
}

static int
wait (struct threadpool * /* tp */ , void * /*arg */)
{
  struct async_task_wrapper *task = async_task_create (1. * MAXDELAY * rand () / RAND_MAX);
  assert ((task->uid = threadpool_task_continuation (resume, TIMEOUT)));       // Declare continuation (resume).
  task->timer = timer_set (task->end_time, timer_handler, task);      // Trigger continuation (timer_handler) after a delay.
  return EXIT_SUCCESS;
}

static void
monitor_handler (struct threadpool_monitor d, void *)
{
  fprintf (stdout, "t=%6.2f s: %'zu workers. %'zu virtual tasks have succeeded, %'zu have timed out (over %'zu submitted).\n",
           d.time, d.workers.nb_alive, d.tasks.nb_succeeded, d.tasks.nb_failed, d.tasks.nb_submitted);
}

int
main (void)
{
#define getrlimit(resource) do { struct rlimit resource; getrlimit (RLIMIT_##resource, &resource); fprintf (stdout, "getrlimit (" #resource ") = %'jd\n", (intmax_t) resource.rlim_cur); } while (0)
  getrlimit (SIGPENDING);       // the number of timers is limited by the RLIMIT_SIGPENDING
  fprintf (stdout, "Running %d virtual tasks (asynchronous timers of at most %g seconds) on a single worker (timeout %g seconds).\n", NB_TIMERS, 1. * MAXDELAY, 1. * TIMEOUT);
  struct threadpool *tp = threadpool_create_and_start (TP_WORKER_SEQUENTIAL, 0, TP_RUN_ALL_TASKS);
  threadpool_set_monitor (tp, monitor_handler, 0, threadpool_monitor_every_100ms);
  for (size_t i = 0; i < NB_TIMERS; i++)
    threadpool_add_task (tp, wait, 0, 0);
  static struct timespec duration = { MAXDELAY + 2, 0 };
  nanosleep (&duration, 0);
  threadpool_wait_and_destroy (tp);
  fprintf (stdout, "=======\n");
}
