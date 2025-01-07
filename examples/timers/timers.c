#define _POSIX_C_SOURCE 199309L
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/resource.h>
#include <math.h>
#include <assert.h>
#include <errno.h>
#include <threads.h>
#include "wqm.h"

/*============== timers.h ================*/
struct timespec delay_to_abs_timespec (double seconds);
void timer_set (struct timespec timeout, int (*callback) (void *arg), void *arg);       // See below.

/*============== timers.c ================*/
struct timer_elem
{
  struct timespec timeout;
  int (*callback) (void *arg);
  void *arg;
  struct timer_elem *lt;
  struct timer_elem *upper;
  struct timer_elem *ge;
};

static int
timespec_cmp (struct timespec a, struct timespec b)
{
  return (a.tv_sec < b.tv_sec ? -1 : a.tv_sec > b.tv_sec ? 1 : a.tv_nsec < b.tv_nsec ? -1 : a.tv_nsec > b.tv_nsec ? 1 : 0);
}

static struct                   // Ordered list of struct timer_elem stored in an ordered binary tree.
{
  thrd_t thread;
  mtx_t mutex;
  cnd_t condition;
  struct timer_elem *earliest;
  struct timer_elem *root;
} Timers = { 0 };

static int
Timers_cmp (const struct timer_elem *a, struct timer_elem *b)
{
  return timespec_cmp (a->timeout, b->timeout);
}

static struct timer_elem *
Timers_get_earliest (void)
{
  return Timers.earliest;
}

static void
Timers_add (struct timespec timeout, int (*callback) (void *arg), void *arg)
{
  struct timer_elem *new = calloc (1, sizeof (*new));
  new->timeout = timeout;
  new->callback = callback;
  new->arg = arg;

  struct timer_elem *upper;
  if (!(upper = Timers.root))
    Timers.root = Timers.earliest = new;
  else
    while (1)
      if (Timers_cmp (new, upper) < 0)
      {
        if (upper->lt)
          upper = upper->lt;
        else
        {
          if (((upper->lt = new)->upper = upper) == Timers.earliest)
            Timers.earliest = new;
          break;
        }
      }
      else if (upper->ge)       // && (new >= upper)
        upper = upper->ge;
      else                      // (!upper->ge) && (new >= upper)
      {
        (upper->ge = new)->upper = upper;
        break;
      }
}

static void
Timers_remove_earliest (void)
{
  if (!Timers.earliest)
    return;
  struct timer_elem *earliest = Timers.earliest;
  if (Timers.earliest->ge)      /* Timers.earliest->lt == 0 */
  {
    if (!(Timers.earliest->ge->upper = Timers.earliest->upper))
      Timers.root = Timers.earliest->ge;
    else
      Timers.earliest->upper->lt = Timers.earliest->ge;
    for (Timers.earliest = Timers.earliest->ge; Timers.earliest->lt; Timers.earliest = Timers.earliest->lt) /* nothing */ ;
  }
  else if (!(Timers.earliest = Timers.earliest->upper))
    Timers.root = 0;
  else
    Timers.earliest->lt = 0;
  free (earliest);
}

static int
timers_loop (void *)
{
  struct timer_elem *earliest;
  mtx_lock (&Timers.mutex);
  while (1)                     // Infinite loop, never ends.
    if (!(earliest = Timers_get_earliest ()))
      cnd_wait (&Timers.condition, &Timers.mutex);
    else if (cnd_timedwait (&Timers.condition, &Timers.mutex, &earliest->timeout) != thrd_timedout) /* nothing */ ;
    else if (earliest != Timers_get_earliest ()) /* nothing */ ;        // Timers_head could have changed while waiting.
    else
    {
      if (earliest->callback)
        earliest->callback (earliest->arg);
      Timers_remove_earliest ();
    }
  mtx_unlock (&Timers.mutex);
  return 0;
}

static once_flag TIMERS_INIT = ONCE_FLAG_INIT;
static void
timers_init (void)              // Called once.
{
  mtx_init (&Timers.mutex, mtx_plain);  // Won't be destroyed.
  cnd_init (&Timers.condition); // Won't be destroyed.
  thrd_create (&Timers.thread, timers_loop, 0); // Won't be destroyed. The thread will be ended with the caller to timer_set.
}

struct timespec
delay_to_abs_timespec (double seconds)
{
  long sec = lround (trunc (seconds));  // C standard function.
  long nsec = lround ((seconds - trunc (seconds)) * 1000 * 1000 * 1000);
  struct timespec t;
  timespec_get (&t, TIME_UTC);  // C standard function, returns now. UTC since cnd_timedwait is UTC-based.
  t.tv_sec += sec + (t.tv_nsec + nsec) / (1000 * 1000 * 1000);
  t.tv_nsec = (t.tv_nsec + nsec) % (1000 * 1000 * 1000);
  return t;
}

void
timer_set (struct timespec timeout, int (*callback) (void *arg), void *arg)
{
  call_once (&TIMERS_INIT, timers_init);
  mtx_lock (&Timers.mutex);
  Timers_add (timeout, callback, arg);
  cnd_signal (&Timers.condition);
  mtx_unlock (&Timers.mutex);
}

/*====================================== Example ================================================*/
#define NB_TIMERS (60000)
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
  struct itimerspec itimerspec;
  struct sigevent sev;
  struct sigaction sa;
#elifdef THREADTIMER
  timer_t timerid;
  struct itimerspec itimerspec;
  struct sigevent sev;
#elifdef USERTIMER
  struct timespec timeout;
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
  assert (threadpool_task_continue (*(uint64_t *) arg) == EXIT_SUCCESS || errno == ETIMEDOUT);
  return EXIT_SUCCESS;
}
#endif

static struct job *
job_create (double seconds)
{
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
  timer->itimerspec.it_value.tv_sec = lround (trunc (seconds)); // C standard function.
  timer->itimerspec.it_value.tv_nsec = lround ((seconds - trunc (seconds)) * 1000 * 1000 * 1000);
#elifdef USERTIMER
  struct timespec duration = { 0, 75 * 1000 };  // 75 microseconds.
  nanosleep (&duration, 0);     // Artificial duration of creation (as for timer_create).
  timer->timeout = delay_to_abs_timespec (seconds);
#else
  (void) seconds;
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
pause (struct threadpool * /* tp */ , void *arg)
{
  struct job *timer = arg;
  assert ((timer->uid = threadpool_task_continuation (resume, TIMEOUT)));
#if defined (SIGTIMER) || defined (THREADTIMER)
  assert (timer_settime (timer->timerid, 0, &timer->itimerspec, 0) == 0);
#elifdef USERTIMER
  timer_set (timer->timeout, timer_handler, &timer->uid);
#endif

  return EXIT_SUCCESS;
}

static void
monitor_handler (struct threadpool_monitor d, void *)
{
  fprintf (stdout, "t=%1$f s: %2$'zu workers have been active (over %3$'zu requested). %4$'zu virtual tasks have succeeded, %5$'zu have timed out (over %6$'zu submitted).\n",
           d.time, d.workers.nb_max, d.workers.nb_requested, d.tasks.nb_succeeded, d.tasks.nb_failed, d.tasks.nb_submitted);
}

int
main (void)
{
#define getrlimit(resource) do { struct rlimit resource; getrlimit (RLIMIT_##resource, &resource); fprintf (stdout, "getrlimit (" #resource ") = %'jd\n", (intmax_t) resource.rlim_cur); } while (0)
  getrlimit (SIGPENDING);       // the number of timers is limited by the RLIMIT_SIGPENDING
  fprintf (stdout, "Running %d virtual tasks (asynchronous timers of at most %g seconds) on a single worker (timeout %g s).\n", NB_TIMERS, 1. * MAXDELAY, 1. * TIMEOUT);
  struct threadpool *tp = threadpool_create_and_start (SEQUENTIAL, 0);
  threadpool_set_monitor (tp, monitor_handler, 0, threadpool_monitor_every_100ms);
  for (size_t i = 0; i < NB_TIMERS; i++)
    threadpool_add_task (tp, pause, job_create (1. * MAXDELAY * rand () / RAND_MAX), job_delete);
  threadpool_wait_and_destroy (tp);
}
