/*
    - map digit to text
    - filter text with length equal to 3
    - count the number of elements found
 */
#include "wqm.h"
#undef NDEBUG
#include <assert.h>
#include <wchar.h>
#include <stdlib.h>

static unsigned int
itos (unsigned int number)
{
  unsigned int sum = 0;
  for (; number; number /= 10)
    sum += number % 10;
  return sum;
}

static int
work2 (void *job)
{
  unsigned int number = *(unsigned int *) job;
  unsigned int sum = itos (number);     // Map
  *(unsigned int *) job = sum;
  return sum == 5 ? TP_JOB_SUCCESS : TP_JOB_FAILURE;    // Filter
}

static int
work (void *job)
{
  unsigned int number = *(unsigned int *) job;
  unsigned int sum = itos (number);     // Map
  *(unsigned int *) job = sum;
  return sum % 10 == 0 ? work2 (job) : TP_JOB_FAILURE;  // Filter
}

static void
work_finalyze (void *job, tp_result_t result)
{
  (void) job;
  if (result == TP_JOB_SUCCESS)
    *(size_t *) (threadpool_worker_local_data ()) += 1; // Reduce on worker basis
}

static void *
make_worker_local_data (void)
{
  size_t *count = malloc (sizeof (*count));     // Worker local aggregator
  *count = 0;
  return count;
}

static void
delete_worker_local_data (void *worker_local_data)
{
  *(size_t *) (threadpool_global_data ()) += *(size_t *) worker_local_data;     // Reduce on thread pool basis
  free (worker_local_data);
}

int
main (void)
{
  unsigned int numbers[15];
  for (size_t i = 0; i < sizeof (numbers) / sizeof (*numbers); i++)
  {
    numbers[i] = (unsigned int) rand ();
    fprintf (stdout, "%u (%u) ; ", numbers[i], itos (numbers[i]));
  }
  fprintf (stdout, "\n");

  size_t count = 0;             // Global aggregator
  struct threadpool *threadpool = threadpool_create_and_start (TP_WORKER_NB_CPU, &count, TP_RUN_ALL_TASKS);
  threadpool_set_worker_local_data_manager (threadpool, make_worker_local_data, delete_worker_local_data);
  threadpool_set_monitor (threadpool, threadpool_monitor_to_terminal, 0, 0);
  for (size_t i = 0; i < sizeof (numbers) / sizeof (*numbers); i++)
    threadpool_add_task (threadpool, work, numbers + i, work_finalyze);

  threadpool_wait_and_destroy (threadpool);
  fprintf (stdout, "%zu\n", count);
}
