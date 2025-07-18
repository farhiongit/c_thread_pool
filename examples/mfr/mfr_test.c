/*
    - map digit to text
    - filter text with length equal to 3
    - count the number of elements found
 */
#include "wqm.h"
#include "mfr.h"
#undef NDEBUG
#include <assert.h>
#include <wchar.h>
#include <stdlib.h>

// ----------------- User defines stream ----------
// Job
struct job
{
  unsigned int data;
};

// Stream mappers and filters
static unsigned int
itos (unsigned int number)
{
  unsigned int sum = 0;
  for (; number; number /= 10)
    sum += number % 10;
  return sum;
}

static tp_result_t
adddigits (void *job, void *arg)
{
  (void) arg;
  unsigned int number = ((struct job *) job)->data;
  unsigned int sum = itos (number);     // Map
  ((struct job *) job)->data = sum;
  return TP_JOB_SUCCESS;
}

static tp_result_t
multipleof (void *job, void *arg)
{
  unsigned int number = ((struct job *) job)->data;
  unsigned int div = *(unsigned int *) arg;
  return number % div == 0 ? TP_JOB_SUCCESS : TP_JOB_FAILURE;   // Filter
}

static tp_result_t
equals (void *job, void *arg)
{
  unsigned int number = ((struct job *) job)->data;
  unsigned int val = *(unsigned int *) arg;
  return number == val ? TP_JOB_SUCCESS : TP_JOB_FAILURE;       // Filter
}

// Stream reducer into aggregate
struct aggregate
{
  size_t data;
};

static void
increment (void *a, void *job)
{
  (void) job;
  struct aggregate *c = a;
  c->data += 1;                 // Aggregator
}

// -----------------------------------------
int
main (void)
{
  struct job numbers[15];
  for (size_t i = 0; i < sizeof (numbers) / sizeof (*numbers); i++)
  {
    numbers[i].data = (unsigned int) rand ();
    fprintf (stdout, "%u (%u) ; ", numbers[i].data, itos (numbers[i].data));
  }
  fprintf (stdout, "\n");
  static unsigned int f_arg_1 = 10;
  static unsigned int f_arg_2 = 5;
  static struct mapper mappers[] = { {.f = adddigits}, {.f = multipleof,.arg = &f_arg_1}, {.f = adddigits}, {.f = equals,.arg = &f_arg_2}, };
  struct aggregate counter = { 0 };     // Initialise aggreagte
  struct stream stream = {.nb_mappers = sizeof (mappers) / sizeof (*mappers),.mappers = mappers,        // Mappers and filters
    .reducer = {.aggregate = &counter,.aggregator = increment}, // Reducer
  };
  struct threadpool *threadpool = threadpool_create_and_start_stream (TP_WORKER_NB_CPU, &stream, TP_RUN_ALL_TASKS);
  threadpool_set_monitor (threadpool, threadpool_monitor_to_terminal, 0, 0);
  for (size_t i = 0; i < sizeof (numbers) / sizeof (*numbers); i++)
    threadpool_add_stream (threadpool, numbers + i);
  threadpool_wait_and_destroy (threadpool);
  fprintf (stdout, "%zu\n", *(size_t *) stream.reducer.aggregate);
  // Freeing aggregate stream.reducer.aggregate should be done here if necessary.
}
