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
void timer_set (double seconds, int (*callback) (void *arg), void *arg);        // See below.

#define NB_TIMERS (30000)
#define MAXDELAY (2)
#define TIMEOUT (0.9 * (MAXDELAY))      // Timeout (seconds), shorten a little bit, for the purpose of the example.
//#define SIGTIMER SIGUSR1        // POSIX signal based timer. Seems to be limited by the OS.
#define THREADTIMER             // POSIX thread based timer.

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
#else
  double delay;
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
#else // User-defined thread timer (see below).
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
  timer->itimerspec.it_value.tv_sec = lround (trunc (seconds)); // C standard function
  timer->itimerspec.it_value.tv_nsec = lround ((seconds - trunc (seconds)) * 1000 * 1000 * 1000);
#else
  timer->delay = seconds;
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
#else
  timer_set (timer->delay, timer_handler, &timer->uid);
#endif

  return EXIT_SUCCESS;
}

static void
monitor_handler (struct threadpool_monitor d, void *)
{
  fprintf (stdout, "t=%1$f s: %2$'zu workers have been active (over %3$'zu requested). %4$'zu virtual tasks have succeeded, %5$'zu have timed out (over %6$'zu submitted)).\n",
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

/*============== Timers ================*/
struct timer_elem
{
  struct timespec timeout;
  int (*callback) (void *arg);
  void *arg;
  struct timer_elem *next;
};

static thrd_t Timers_thread;
static mtx_t Timers_mutex;
static cnd_t Timers_condition;
static struct timer_elem *Timers_head;
static once_flag TIMERS_INIT = ONCE_FLAG_INIT;

static int
timespec_cmp (struct timespec a, struct timespec b)
{
  return (a.tv_sec < b.tv_sec ? -1 : a.tv_sec > b.tv_sec ? 1 : a.tv_nsec < b.tv_nsec ? -1 : a.tv_nsec > b.tv_nsec ? 1 : 0);
}

static struct timespec
delay_to_abs_timespec (double seconds)
{
  long sec = lround (trunc (seconds));  // C standard function
  long nsec = lround ((seconds - trunc (seconds)) * 1000 * 1000 * 1000);
  struct timespec t;
  timespec_get (&t, TIME_UTC);  // C standard function, returns now. UTC since cnd_timedwait is UTC-based.
  t.tv_sec += sec + (t.tv_nsec + nsec) / (1000 * 1000 * 1000);
  t.tv_nsec = (t.tv_nsec + nsec) % (1000 * 1000 * 1000);
  return t;
}

static int
timers_loop (void *)
{
  mtx_lock (&Timers_mutex);
  while (1)                     // Infinite loop
  {
    struct timer_elem *head = Timers_head;
    if (head)
    {
      struct timespec timeout = head->timeout;
      if (cnd_timedwait (&Timers_condition, &Timers_mutex, &timeout) != thrd_timedout || head != Timers_head)   // Timers_head could have changed while waiting.
        continue /* loop */ ;
      if (head->callback)
        head->callback (Timers_head->arg);
      Timers_head = Timers_head->next;
      free (head);
    }
    else                        // if (!head)
      cnd_wait (&Timers_condition, &Timers_mutex);
  }
  mtx_unlock (&Timers_mutex);

  return 0;
}

static void
timers_init (void)
{
  thrd_create (&Timers_thread, timers_loop, 0); // Won't be detroyed
  mtx_init (&Timers_mutex, mtx_plain);  // Won't be detroyed
  cnd_init (&Timers_condition); // Won't be detroyed
}

void
timer_set (double seconds, int (*callback) (void *arg), void *arg)
{
  mtx_lock (&Timers_mutex);
  struct timer_elem *new = calloc (1, sizeof (*new));
  new->timeout = delay_to_abs_timespec (seconds);
  new->callback = callback;
  new->arg = arg;

  if (!Timers_head)
    Timers_head = new;
  else if (timespec_cmp (new->timeout, Timers_head->timeout) <= 0)
  {
    new->next = Timers_head;
    Timers_head = new;
  }
  else
  {
    struct timer_elem *prev;
    for (prev = Timers_head; prev->next && timespec_cmp (new->timeout, prev->next->timeout) > 0; prev = prev->next)
      /* nothing */ ;
    new->next = prev->next;
    prev->next = new;
  }
  cnd_signal (&Timers_condition);
  mtx_unlock (&Timers_mutex);
  call_once (&TIMERS_INIT, timers_init);
}
