#include <unistd.h>
#include <stdlib.h>
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
  return (d.workers.nb_active == 0);
}

static void
monitor_handler (struct threadpool_monitor d, void *f)
{
  int (*filter) (struct threadpool_monitor) = f;
  if (filter (d))
    fprintf (stdout, "t=%1$f s: %2$zu workers have been active (over %3$zu requested). %4$zu tasks have been processed.\n",
             d.time, d.workers.nb_max, d.workers.nb_requested, d.tasks.nb_succeeded);
  //threadpool_monitor_to_terminal (d, 0);
}

int
main (void)
{
  // Limit of 9212 threads on my system with default configuration.
  const size_t MAX_NB_THREADS = 9212;
  size_t nb_requested_workers = MAX_NB_THREADS + 100;   // Only MAX_NB_THREADS will be allocated.
  size_t nb_tasks = 4 * nb_requested_workers;

  struct threadpool *tp = threadpool_create_and_start (nb_requested_workers, 0);
  threadpool_set_monitor (tp, monitor_handler, monitor_start_and_stop);
  for (size_t i = 0; i < nb_tasks; i++)
    threadpool_add_task (tp, worker, 0, 0);
  threadpool_wait_and_destroy (tp);
}
