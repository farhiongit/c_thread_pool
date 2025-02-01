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

#define NB_TIMERS (300 * 1000)
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

#if 1
static int
done (void *arg)
{
  static size_t i = 0;
  double *j = arg;
  fprintf (stdout, "%zu = %f s\n", ++i, *j);
  free (j);
  return EXIT_SUCCESS;
}

static int
cmpstringp (const void *p1, const void *p2, void *arg)
{
  /* The actual arguments to this function are "pointers to
     pointers to char", but strcmp(3) arguments are "pointers
     to char", hence the following cast plus dereference. */

  (void) arg;
  return strcmp ((const char *) p1, (const char *) p2);
}

static int
remove_one_data (void *data, void *res, int *remove)
{
  (void) data;
  (void) res;
  *remove = 1;                  // Tells: remove the data from the map.
  return 0;                     // Tells: stop traversing.
}

static int
remove_all_data (void *data, void *res, int *remove)
{
  (void) data;
  (void) res;
  *remove = 1;                  // Tells: remove the data from the map.
  return 1;                     // Tells: continue traversing.
}

static int
print_data (void *data, void *res, int *remove)
{
  (void) res;
  fprintf (stdout, "%s ", (char *) data);
  *remove = 0;                  // Tells: do not remove the data from the map.
  return 1;                     // Tells: continue traversing.
}

static int
select_c (void *data, void *res)
{
  (void) res;
  return (*(char *) data == 'c');
}
#endif

int
main (void)
{
#if 1
  srand ((unsigned int) time (0));
  map *li;
  for (int i = 1; i <= 4; i++)
  {
    switch (i)
    {
      case 1:
        li = map_create (MAP_KEY_IS_DATA, cmpstringp, 0, MAP_UNIQUENESS);       // Set
        break;
      case 2:
        li = map_create (MAP_KEY_IS_DATA, cmpstringp, 0, MAP_STABLE);   // Ordered list
        break;
      case 3:
        li = map_create (0, 0, 0, MAP_STABLE);  // Chain (FIFO or LIFO)
        break;
      case 4:
        li = map_create (0, 0, 0, MAP_NONE);    // Unordered list
        break;
      default:
    }
    map_insert_data (li, "b");  // The map stores pointers to static data of type char[].
    map_insert_data (li, "a");
    map_insert_data (li, "d");
    map_insert_data (li, "c");
    map_insert_data (li, "c");
    map_insert_data (li, "a");
    map_insert_data (li, "aa");
    map_insert_data (li, "cc");
    map_insert_data (li, "d");

    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");
    map_traverse_backward (li, print_data, 0, 0);
    fprintf (stdout, "\n");
    map_traverse (li, print_data, select_c, 0);
    fprintf (stdout, "\n");

    char *data;
    if (map_traverse (li, MAP_REMOVE_FIRST, 0, &data))  // Remove the first found element from the map.
    {
      fprintf (stdout, "%s <-- ", data);
      map_traverse (li, print_data, 0, 0);
      fprintf (stdout, "<-- %s\n", data);
      map_insert_data (li, data);       // Reinsert after use.
      map_traverse (li, print_data, 0, 0);
      fprintf (stdout, "\n");
    }

    map_find_key (li, "c", remove_all_data, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_traverse (li, remove_one_data, 0, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_find_key (li, "b", remove_one_data, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_find_key (li, "d", remove_one_data, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_traverse_backward (li, remove_one_data, 0, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_traverse (li, remove_one_data, 0, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_traverse (li, remove_all_data, 0, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");
    map_traverse (li, remove_all_data, 0, 0);
    map_destroy (li);
    fprintf (stdout, "=======\n");
  }

  fprintf (stdout, "Set\n");
  void *timer = 0;
  for (size_t i = 0; i < 10; i++)
  {
    double *j = malloc (sizeof (*j));
    timer = timer_set (delay_to_abs_timespec ((*j = 1. * MAXDELAY * rand () / RAND_MAX)), done, j);
  }
  timer_unset (timer);
  fprintf (stdout, "Wait\n");
  sleep (MAXDELAY);             // Wait for all timers to finish.
  fprintf (stdout, "=======\n");
#endif
#define getrlimit(resource) do { struct rlimit resource; getrlimit (RLIMIT_##resource, &resource); fprintf (stdout, "getrlimit (" #resource ") = %'jd\n", (intmax_t) resource.rlim_cur); } while (0)
  getrlimit (SIGPENDING);       // the number of timers is limited by the RLIMIT_SIGPENDING
  fprintf (stdout, "Running %d virtual tasks (asynchronous timers of at most %g seconds) on a single worker (timeout %g seconds).\n", NB_TIMERS, 1. * MAXDELAY, 1. * TIMEOUT);
  struct threadpool *tp = threadpool_create_and_start (SEQUENTIAL, 0);
  threadpool_set_monitor (tp, monitor_handler, 0, threadpool_monitor_every_100ms);
  for (size_t i = 0; i < NB_TIMERS; i++)
    threadpool_add_task (tp, wait, job_create (1. * MAXDELAY * rand () / RAND_MAX), job_delete);
  threadpool_wait_and_destroy (tp);
  fprintf (stdout, "=======\n");
}
