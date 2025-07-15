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
  } *mapper;
  struct
  {
    void *aggregate;            // Global aggregator
    void *(*id) (void);         // Aggregate initialiser
    void (*aggregator) (void *aggregate, void *job);    // Job aggregator and job cleaner
    void (*job_delete) (void *);
    void (*merger) (void *agg1, void *agg2);    // Worker merger
  } r;
};

static tp_result_t
mapfilter (void *job)
{
  struct stream *stream = threadpool_global_data ();
  tp_result_t ret = TP_JOB_SUCCESS;
  for (size_t i = 0; i < stream->nb_mappers && ret == TP_JOB_SUCCESS; i++)
    ret = (stream->mapper[i].f) (job, stream->mapper[i].arg);   // Map or filter
  void *aggregate;
  if (ret == TP_JOB_SUCCESS && stream->r.merger && stream->r.aggregator && (aggregate = threadpool_worker_local_data ()))
    stream->r.aggregator (aggregate, job);      // Reduce on worker basis
  stream->r.job_delete (job);
  return ret;
}

static void
reduce (void *job, tp_result_t ret)
{
  if (ret == TP_JOB_SUCCESS)
  {
    struct stream *stream = threadpool_global_data ();
    void *aggregate = threadpool_worker_local_data ();
    if ((!stream->r.merger || !aggregate) && stream->r.aggregator && (aggregate = stream->r.aggregate))
      stream->r.aggregator (aggregate, job);    // Reduce on thread pool basis
  }
}

// ------------- Worker local aggregator ---------
static void *
make_worker_local_data (void)
{
  struct stream *stream = threadpool_global_data ();
  size_t *count = 0;
  if (stream->r.merger && (count = malloc (sizeof (*count))))
    *count = *(size_t *) stream->r.id ();       // Worker local aggregator
  return count;
}

static void
job_delete (void *job)
{
  // Freeing job should be done here if necessary.
  (void) job;
}

static void
aggregate_delete (void *aggregate)
{
  free (aggregate);
}

static void
delete_worker_local_data (void *worker_local_data)
{
  struct stream *stream = threadpool_global_data ();
  if (stream->r.merger && worker_local_data)
    stream->r.merger (stream->r.aggregate, worker_local_data);  // Reduce on thread pool basis
  // Freeing aggregate should be done here if necessary.
  aggregate_delete (worker_local_data);
}

// ------- Stream operators ------------
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
  unsigned int number = *(unsigned int *) job;
  unsigned int sum = itos (number);     // Map
  *(unsigned int *) job = sum;
  return TP_JOB_SUCCESS;
}

static tp_result_t
multipleof (void *job, void *arg)
{
  unsigned int number = *(unsigned int *) job;
  unsigned int div = *(unsigned int *) arg;
  return number % div == 0 ? TP_JOB_SUCCESS : TP_JOB_FAILURE;   // Filter
}

static tp_result_t
equals (void *job, void *arg)
{
  unsigned int number = *(unsigned int *) job;
  unsigned int val = *(unsigned int *) arg;
  return number == val ? TP_JOB_SUCCESS : TP_JOB_FAILURE;       // Filter
}

static void *
id (void)
{
  static size_t zero = 0;
  return &zero;                 // Identity
}

static void
increment (void *a, void *job)
{
  (void) job;
  size_t *c = a;
  *c += 1;                      // Aggregator
}

static void
add (void *a, void *b)
{
  size_t *ca = a;
  size_t *cb = b;
  *ca += *cb;                   // Merger
}

int
main (void)
{
  static const int WORKER_LOCAL_AGGREGATION = 1;
  unsigned int numbers[15];
  for (size_t i = 0; i < sizeof (numbers) / sizeof (*numbers); i++)
  {
    numbers[i] = (unsigned int) rand ();
    fprintf (stdout, "%u (%u) ; ", numbers[i], itos (numbers[i]));
  }
  fprintf (stdout, "\n");
  static unsigned int f_arg_1 = 10;
  static unsigned int f_arg_2 = 5;
  static struct mapper mappers[] = { {adddigits, 0}, {multipleof, &f_arg_1}, {adddigits, 0}, {equals, &f_arg_2}, };
  size_t counter = *(size_t *) id ();   // Aggreagte
  (void) add;
  struct stream stream = { sizeof (mappers) / sizeof (*mappers), mappers, {&counter, id, increment, job_delete, WORKER_LOCAL_AGGREGATION ? add : 0}
  };
  struct threadpool *threadpool = threadpool_create_and_start (TP_WORKER_NB_CPU, &stream, TP_RUN_ALL_TASKS);
  threadpool_set_worker_local_data_manager (threadpool, make_worker_local_data, delete_worker_local_data);
  threadpool_set_monitor (threadpool, threadpool_monitor_to_terminal, 0, 0);
  for (size_t i = 0; i < sizeof (numbers) / sizeof (*numbers); i++)
    threadpool_add_task (threadpool, mapfilter, numbers + i, reduce);
  threadpool_wait_and_destroy (threadpool);
  fprintf (stdout, "%zu\n", *(size_t *) stream.r.aggregate);
  // Freeing aggregate should be done here if necessary.
}
