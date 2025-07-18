/*
    Map, filter, reduce pattern example.
    - Map an integer to the sum of its digits.
    - Select only those multiple of 10.
    - Map again to the sum of its digits.
    - Select only those equal to 5.
    - Count the number of selected elements.
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
  unsigned int init, final;
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
  unsigned int number = ((struct job *) job)->final;
  unsigned int sum = itos (number);     // Map
  ((struct job *) job)->final = sum;
  return TP_JOB_SUCCESS;
}

static tp_result_t
multipleof (void *job, void *arg)
{
  unsigned int number = ((struct job *) job)->final;
  unsigned int div = *(unsigned int *) arg;
  return number % div == 0 ? TP_JOB_SUCCESS : TP_JOB_FAILURE;   // Filter
}

static tp_result_t
equals (void *job, void *arg)
{
  unsigned int number = ((struct job *) job)->final;
  unsigned int val = *(unsigned int *) arg;
  return number == val ? TP_JOB_SUCCESS : TP_JOB_FAILURE;       // Filter
}

// Stream reducer into aggregate
struct aggregate
{
  size_t nb;
  unsigned int *init;
};

static void
increment (void *a, void *job)
{
  struct aggregate *c = a;
  c->nb += 1;                   // Aggregator
  c->init = realloc (c->init, c->nb * sizeof (*c->init));
  c->init[c->nb - 1] = ((struct job *) job)->init;
}

// -----------------------------------------
int
main (void)
{
  struct job numbers[15];
  for (size_t i = 0; i < sizeof (numbers) / sizeof (*numbers); i++)
  {
    numbers[i].init = numbers[i].final = (unsigned int) rand ();
    fprintf (stdout, "%u (%u) ; ", numbers[i].init, itos (numbers[i].init));
  }
  fprintf (stdout, "\n");
  static unsigned int f_arg_1 = 10;
  static unsigned int f_arg_2 = 5;
  static struct mapper mappers[] = { {.f = adddigits}, {.f = multipleof,.arg = &f_arg_1}, {.f = adddigits}, {.f = equals,.arg = &f_arg_2}, };
  struct aggregate counter = { 0 };  // Initialise aggreagte
  struct stream stream = {.nb_mappers = sizeof (mappers) / sizeof (*mappers),.mappers = mappers,        // Mappers and filters
    .reducer = {.aggregate = &counter,.aggregator = increment}, // Reducer
  };
  struct threadpool *threadpool = threadpool_create_and_start_stream (TP_WORKER_NB_CPU, &stream, TP_RUN_ALL_TASKS);
  threadpool_set_monitor (threadpool, threadpool_monitor_to_terminal, 0, 0);
  for (size_t i = 0; i < sizeof (numbers) / sizeof (*numbers); i++)
    threadpool_add_task_to_stream (threadpool, numbers + i);
  threadpool_wait_and_destroy (threadpool);
  fprintf (stdout, "%zu\n", counter.nb);
  for (size_t i = 0; i < counter.nb; i++)
    fprintf (stdout, "%u ; ", counter.init[i]);
  fprintf (stdout, "\n");
  // Freeing aggregate stream.reducer.aggregate should be done here if necessary.
  free (counter.init);
}
