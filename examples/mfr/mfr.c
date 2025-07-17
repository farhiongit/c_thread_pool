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

// ------- Map, filter, reduce pattern framework ------------
struct stream
{
  size_t nb_mappers;
  struct mapper                 // Mapper or filter
  {
    tp_result_t (*f) (void *job, void *arg);    // Function
    void *arg;                  // Argument
  } *mappers;
  struct reducer                // Reducer, called MT safely
  {
    void *aggregate;            // Result of the stream after aggregation
    void (*aggregator) (void *aggregate, void *job);    // Job aggregator, aggregate 'job' into 'aggregate'
  } reducer;
  void (*job_deletor) (void *); //  Job cleaner (after aggregation). Freeing job should be done here if necessary.
};

static tp_result_t
mapfilter (void *job)
{
  struct stream *stream = threadpool_global_data ();
  if (!stream)
    return TP_JOB_FAILURE;
  tp_result_t ret = TP_JOB_SUCCESS;
  for (size_t i = 0; i < stream->nb_mappers && ret == TP_JOB_SUCCESS; i++)
    if (stream->mappers[i].f)
      ret = stream->mappers[i].f (job, stream->mappers[i].arg); // Map or filter
  return ret;
}

static void
reduce (void *job, tp_result_t ret)
{
  if (ret == TP_JOB_SUCCESS)
  {
    struct stream *stream = threadpool_global_data ();
    if (stream->reducer.aggregator && stream->reducer.aggregate)
      stream->reducer.aggregator (stream->reducer.aggregate, job);      // Reduce
    if (stream->job_deletor)
      stream->job_deletor (job);        // Delete job after use
  }
}

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
  struct threadpool *threadpool = threadpool_create_and_start (TP_WORKER_NB_CPU, &stream, TP_RUN_ALL_TASKS);
  threadpool_set_monitor (threadpool, threadpool_monitor_to_terminal, 0, 0);
  for (size_t i = 0; i < sizeof (numbers) / sizeof (*numbers); i++)
    threadpool_add_task (threadpool, mapfilter, numbers + i, reduce);
  threadpool_wait_and_destroy (threadpool);
  fprintf (stdout, "%zu\n", *(size_t *) stream.reducer.aggregate);
  // Freeing aggregate stream.reducer.aggregate should be done here if necessary.
}
