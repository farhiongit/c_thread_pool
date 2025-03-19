#include <unistd.h>
#include <stdlib.h>
#include <locale.h>
#include <sys/resource.h>
#include <stdint.h>
#include "wqm.h"

static int
worker (struct threadpool *, void *)
{
  sleep (1);
  return EXIT_SUCCESS;
}

static int
monitor_start_and_stop (struct threadpool_monitor d)
{
  return (d.workers.nb_alive == 0);
}

static void
monitor_handler (struct threadpool_monitor d, void *f)
{
  int (*filter) (struct threadpool_monitor) = f;
  if (filter (d))
    fprintf (stdout, "t=%1$f s: %2$'zu workers have been active (over %3$'zu requested). %4$'zu tasks have been processed (over %5$'zu submitted)).\n",
             d.time, d.workers.nb_max, d.workers.nb_requested, d.tasks.nb_succeeded, d.tasks.nb_submitted);
}

int
main (void)
{
  setlocale (LC_ALL, "");
#define getrlimit(resource) do { struct rlimit resource; getrlimit (RLIMIT_##resource, &resource); fprintf (stdout, "getrlimit (" #resource ") = %'jd\n", (intmax_t) resource.rlim_cur); } while (0)
  getrlimit (NPROC);
  getrlimit (AS);
  getrlimit (STACK);
  getrlimit (MEMLOCK);
  // Limit of 9212 threads on my system with default configuration.
  const size_t MAX_NB_THREADS = 9500;
  size_t nb_requested_workers = MAX_NB_THREADS; // Only MAX_NB_THREADS will be allocated.
  size_t nb_tasks = 4 * nb_requested_workers;

  struct threadpool *tp = threadpool_create_and_start (nb_requested_workers, 0, TP_RUN_ALL_TASKS);
  (void) monitor_handler;
  (void) monitor_start_and_stop;
  threadpool_set_monitor (tp, monitor_handler, monitor_start_and_stop, 0);
  for (size_t i = 0; i < nb_tasks; i++)
    threadpool_add_task (tp, worker, 0, 0);
  threadpool_wait_and_destroy (tp);
}
