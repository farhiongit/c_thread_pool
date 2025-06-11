// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#define _POSIX_C_SOURCE 199309L
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/resource.h>
#include <math.h>
#include <errno.h>
#include <threads.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "timer.h"
#include "wqm.h"
#include "map.h"
#include "trace.h"
#define map_create(...)            TRACE_EXPRESSION(map_create (__VA_ARGS__))
#define map_destroy(...)           TRACE_EXPRESSION(map_destroy (__VA_ARGS__))
#define map_insert_data(...)       TRACE_EXPRESSION(map_insert_data (__VA_ARGS__))
#define map_traverse(...)          TRACE_EXPRESSION(map_traverse (__VA_ARGS__))
#define map_find_key(...)          TRACE_EXPRESSION(map_find_key (__VA_ARGS__))
#define map_traverse_backward(...) TRACE_EXPRESSION(map_traverse_backward (__VA_ARGS__))

#define NB_TIMERS (10 * 1000)
#define MAXDELAY (2.)
#define TIMEOUT (0.9 * (MAXDELAY))      // Timeout (seconds) (shortened a little bit, for the purpose of the example.)
//#define SIGTIMER SIGUSR1        // POSIX signal based timer. Seems to be limited by the OS.
//#define THREADTIMER             // POSIX thread based timer.
#define USERTIMER               // User-defined timer.

struct job
{
  uint64_t uid;
#ifdef SIGTIMER
  timer_t timerid;
  struct itimerspec delay;
  struct sigevent sev;
  struct sigaction sa;
#elifdef THREADTIMER
  timer_t timerid;
  struct itimerspec delay;
  struct sigevent sev;
#elifdef USERTIMER
  struct timespec end_time;
#else
#  error Please choose a timer type (SIGTIMER, THREADTIMER or USERTIMER).
#endif
};

#ifdef SIGTIMER
static void                     // POSIX signal timer. At any point in time, at most one signal is queued to the process for a given timer.
signal_handler (int, siginfo_t *si, void *)
{
  errno = 0;
  assert (threadpool_task_continue (*(uint64_t *) si->si_value.sival_ptr) == EXIT_SUCCESS || errno == ETIMEDOUT);
}
#elifdef THREADTIMER            // POSIX thread-timer.
static void
sigev_handler (union sigval sig)
{
  errno = 0;
  assert (threadpool_task_continue (*(uint64_t *) sig.sival_ptr) == EXIT_SUCCESS || errno == ETIMEDOUT);
}
#elifdef USERTIMER              // User-defined thread timer (see below).
static int
timer_handler (void *arg)
{
  errno = 0;
  assert (threadpool_task_continue (*(uint64_t *) arg) == EXIT_SUCCESS || errno == ETIMEDOUT);  // Trigger continuation.
  return EXIT_SUCCESS;
}
#endif

static struct job *
job_create (double seconds)
{
  (void) seconds;
  struct job *timer = calloc (1, sizeof (*timer));
  assert (timer);
#ifdef SIGTIMER
  timer->sa.sa_flags = SA_SIGINFO;
  timer->sa.sa_sigaction = signal_handler;
  assert (sigaction (SIGTIMER, &timer->sa, 0) == 0);

  timer->sev.sigev_notify = SIGEV_SIGNAL;
  timer->sev.sigev_signo = SIGTIMER;
  timer->sev.sigev_value.sival_ptr = &timer->uid;
#elifdef THREADTIMER
  timer->sev.sigev_notify = SIGEV_THREAD;
  timer->sev.sigev_notify_function = sigev_handler;
  timer->sev.sigev_value.sival_ptr = &timer->uid;
#endif
#if defined (SIGTIMER) || defined (THREADTIMER)
  assert (timer_create (CLOCK_REALTIME, &timer->sev, &timer->timerid) == 0);
  timer->delay.it_value.tv_sec = lround (trunc (seconds));      // C standard function.
  timer->delay.it_value.tv_nsec = lround ((seconds - trunc (seconds)) * 1000 * 1000 * 1000);
#elifdef USERTIMER
  static struct timespec duration = { 0, 7 /* microseconds */  * 1000 };
  nanosleep (&duration, 0);     // Artificial duration of creation (as for timer_create).
  timer->end_time = delay_to_abs_timespec (seconds);
#endif

  return timer;
}

static void
job_delete (void *arg)
{
  struct job *timer = arg;
#if defined (SIGTIMER) || defined (THREADTIMER)
  timer_delete (timer->timerid);
#endif
  free (timer);
}

static int
resume (struct threadpool * /* tp */ , void * /* arg */ )
{
  return EXIT_SUCCESS;
}

static int
wait (struct threadpool * /* tp */ , void *arg)
{
  struct job *timer = arg;
  assert ((timer->uid = threadpool_task_continuation (resume, TIMEOUT)));       // Declare continuation (resume).
#if defined (SIGTIMER) || defined (THREADTIMER)
  assert (timer_settime (timer->timerid, 0, &timer->delay, 0) == 0);
#elifdef USERTIMER
  timer_set (timer->end_time, timer_handler, &timer->uid);      // Trigger continuation (timer_handler) after a delay.
#endif

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
    threadpool_add_task (tp, wait, job_create (1. * MAXDELAY * rand () / RAND_MAX), job_delete);
  threadpool_wait_and_destroy (tp);
  fprintf (stdout, "=======\n");
}
