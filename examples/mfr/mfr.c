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

// ------- Map, filter, reduce pattern ------------
struct stream
{
  size_t nb_mappers;
  struct mapper                 // Mapper or filter
  {
    tp_result_t (*f) (void *job, void *arg);    // Function
    void *arg;                  // Argument
  } *mappers;
  struct
  {
    void *aggregate;            // Result of the stream after aggregation
    void *(*id) (void);         // Aggregate initialiser
    void (*aggregator) (void *aggregate, void *job);    // Job aggregator
    void (*job_delete) (void *);        //  Job cleaner after aggregation
    void (*merger) (void *agg1, void *agg2);    // Worker merger that allows to merge 2 aggregates of workers into 1
  } reducer;
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
  void *aggregate;
  if (ret == TP_JOB_SUCCESS && stream->reducer.merger && stream->reducer.aggregator && (aggregate = threadpool_worker_local_data ()))
    stream->reducer.aggregator (aggregate, job);        // Reduce on worker basis
  if (stream->reducer.job_delete)
    stream->reducer.job_delete (job);
  return ret;
}

static void
reduce (void *job, tp_result_t ret)
{
  if (ret == TP_JOB_SUCCESS)
  {
    struct stream *stream = threadpool_global_data ();
    void *aggregate = threadpool_worker_local_data ();
    if ((!stream->reducer.merger || !aggregate) && stream->reducer.aggregator && (aggregate = stream->reducer.aggregate))
      stream->reducer.aggregator (aggregate, job);      // Reduce on thread pool basis
  }
}

// ------------------ Job -------
struct job
{
  unsigned int data;
};

static void
job_delete (void *job)
{
  // Freeing job should be done here if necessary.
  (void) job;
}

// ------- Stream mappers and filters ------------
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

// ------- Stream reducer into aggregate ------------
struct aggregate
{
  size_t data;
};

static void *
id (void)
{
  static struct aggregate zero = { 0 };
  return &zero;                 // Identity
}

static void
increment (void *a, void *job)
{
  (void) job;
  struct aggregate *c = a;
  c->data += 1;                 // Aggregator
}

// ------------- Worker local aggregator ---------
static void
add (void *a, void *b)
{
  struct aggregate *ca = a;
  struct aggregate *cb = b;
  ca->data += cb->data;         // Merger of local aggregates
}

static void *
make_worker_local_data (void)
{
  struct stream *stream = threadpool_global_data ();
  struct aggregate *count = 0;
  if (stream->reducer.merger && (count = malloc (sizeof (*count))))
    count->data = ((struct aggregate *) stream->reducer.id ())->data;   // Initialise the worker local aggregator
  return count;
}

static void
aggregate_and_delete_worker_local_data (void *worker_local_data)
{
  struct stream *stream = threadpool_global_data ();
  if (stream->reducer.merger && worker_local_data)
    stream->reducer.merger (stream->reducer.aggregate, worker_local_data);      // Merge worker aggregates into the thread pool aggregated result
  // Freeing aggregate should be done here if necessary.
  free (worker_local_data);
}

// -----------------------------------------
int
main (void)
{
  static const int WORKER_LOCAL_AGGREGATION_IS_ACTIVE = 1;
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
  struct aggregate counter = {.data = *(size_t *) id () };      // Aggreagte
  (void) add;
  struct stream stream = {.nb_mappers = sizeof (mappers) / sizeof (*mappers),.mappers = mappers,.reducer = {.aggregate = &counter,.id = id,.aggregator = increment,.job_delete =
                                                                                                            job_delete,.merger = (WORKER_LOCAL_AGGREGATION_IS_ACTIVE ? add : 0)}
  };
  struct threadpool *threadpool = threadpool_create_and_start (TP_WORKER_NB_CPU, &stream, TP_RUN_ALL_TASKS);
  threadpool_set_worker_local_data_manager (threadpool, make_worker_local_data, aggregate_and_delete_worker_local_data);
  threadpool_set_monitor (threadpool, threadpool_monitor_to_terminal, 0, 0);
  for (size_t i = 0; i < sizeof (numbers) / sizeof (*numbers); i++)
    threadpool_add_task (threadpool, mapfilter, numbers + i, reduce);
  threadpool_wait_and_destroy (threadpool);
  fprintf (stdout, "%zu\n", *(size_t *) stream.reducer.aggregate);
  // Freeing aggregate stream.reducer.aggregate should be done here if necessary.
}
