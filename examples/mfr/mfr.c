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

struct stream
{
  struct
  {
    size_t nb_ops;
      tp_result_t (**op) (void *, void *);      // Mapper or filter
  } mf;
  struct
  {
    void *aggregate;            // Global aggregator
    void *(*id) (void);         // Aggregate initialiser
    void *(*aggregator) (void *, void *);       // Job aggregator
    void *(*merger) (void *, void *);   // Worker merger
  } r;
};

static unsigned int
itos (unsigned int number)
{
  unsigned int sum = 0;
  for (; number; number /= 10)
    sum += number % 10;
  return sum;
}

static tp_result_t
adddigits (void *in, void *out)
{
  unsigned int number = *(unsigned int *) in;
  unsigned int sum = itos (number);     // Map
  *(unsigned int *) out = sum;
  return TP_JOB_SUCCESS;
}

static tp_result_t
is_a_tenth (void *job, void *)
{
  unsigned int number = *(unsigned int *) job;
  return number % 10 == 0 ? TP_JOB_SUCCESS : TP_JOB_FAILURE;    // Filter
}

static tp_result_t
is_five (void *job, void *)
{
  unsigned int number = *(unsigned int *) job;
  return number == 5 ? TP_JOB_SUCCESS : TP_JOB_FAILURE; // Filter
}

static void *
id (void)
{
  static size_t zero = 0;
  return &zero;
}

static void *
increment (void *a, void *)
{
  size_t *c = a;
  *c += 1;
  return a;
}

static void *
add (void *a, void *b)
{
  size_t *ca = a;
  size_t *cb = b;
  *ca += *cb;
  return a;
}

static tp_result_t
mapfilter (void *job)
{
  struct stream *stream = threadpool_global_data ();
  tp_result_t ret = TP_JOB_SUCCESS;
  for (size_t i = 0; i < stream->mf.nb_ops && ret == TP_JOB_SUCCESS; i++)
    ret = (stream->mf.op[i]) (job, job);        // Map or filter
  return ret;
}

static void
reduce (void *job, tp_result_t ret)
{
  if (ret == TP_JOB_SUCCESS)
  {
    struct stream *stream = threadpool_global_data ();
    size_t *count = threadpool_worker_local_data ();
    if ((stream->r.merger && count) || (count = stream->r.aggregate))
      *count = *(size_t *) stream->r.aggregator (count, job);   // Reduce
  }
}

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
delete_worker_local_data (void *worker_local_data)
{
  struct stream *stream = threadpool_global_data ();
  if (stream->r.merger && worker_local_data)
    *(size_t *) stream->r.aggregate = *(size_t *) stream->r.merger (stream->r.aggregate, worker_local_data);    // Reduce on thread pool basis
  free (worker_local_data);
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
  size_t counter = *(size_t *) id ();
  static tp_result_t (*ops[]) (void *, void *) = { adddigits, is_a_tenth, adddigits, is_five, };
  (void) add;
  struct stream stream = { {sizeof (ops) / sizeof (*ops), ops}, {&counter, id, increment, WORKER_LOCAL_AGGREGATION ? add : 0}
  };
  struct threadpool *threadpool = threadpool_create_and_start (TP_WORKER_NB_CPU, &stream, TP_RUN_ALL_TASKS);
  threadpool_set_worker_local_data_manager (threadpool, make_worker_local_data, delete_worker_local_data);
  threadpool_set_monitor (threadpool, threadpool_monitor_to_terminal, 0, 0);
  for (size_t i = 0; i < sizeof (numbers) / sizeof (*numbers); i++)
    threadpool_add_task (threadpool, mapfilter, numbers + i, reduce);

  threadpool_wait_and_destroy (threadpool);
  fprintf (stdout, "%zu\n", *(size_t *) stream.r.aggregate);
}
