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
  size_t seq;
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

static tp_result_t
countdown (void *job, void *arg, tp_result_t respos, tp_result_t res0)
{
  (void) job;
  size_t *count = arg;
  tp_result_t ret = respos;
  threadpool_guard_begin ();
  if (*count)
    *count -= 1;
  else
    ret = res0;
  threadpool_guard_end ();
  return ret;
}

static tp_result_t
countuntil (void *job, void *arg)
{
  return countdown (job, arg, TP_JOB_FAILURE, TP_JOB_SUCCESS);
}

static tp_result_t
countwhile (void *job, void *arg)
{
  return countdown (job, arg, TP_JOB_SUCCESS, TP_JOB_FAILURE);
}

static struct job *
make_job (void)
{
  struct job *j = malloc (sizeof (*j));
  j->init = j->final = (unsigned int) rand ();
  j->seq = 1;
  return j;
}

static tp_result_t
printjob (void *job, void *arg)
{
  (void) job;
  (void) arg;
  struct job *j = job;
  fprintf (stdout, "#%zu: %u (%u)\n", j->seq, j->init, itos (j->init));
  return TP_JOB_SUCCESS;
}

static tp_result_t
iota (void *job, void *arg)
{
  (void) arg;
  struct job *j = make_job ();
  j->seq = ((struct job *) job)->seq + 1;
  struct threadpool *threadpool = threadpool_current ();
  threadpool_add_task_to_stream (threadpool, j);
  return TP_JOB_SUCCESS;
}

static tp_result_t
isnull (void *job, void *arg)
{
  (void) job;
  size_t *count = arg;
  return *count ? TP_JOB_FAILURE : TP_JOB_SUCCESS;
}

// Stream reducer into aggregate
struct aggregate
{
  size_t nb;
  unsigned int *init;
};

static tp_result_t
increment (void *aggregate, void *job)
{
  struct aggregate *c = aggregate;
  c->nb += 1;                   // Aggregator
  c->init = realloc (c->init, c->nb * sizeof (*c->init));
  c->init[c->nb - 1] = ((struct job *) job)->init;
  return TP_JOB_SUCCESS;
}

// -----------------------------------------
int
main (void)
{
  unsigned int f_arg_1 = 10;
  unsigned int f_arg_2 = 5;
  size_t drop = 13;
  size_t take = 2;
  struct mapper mappers[] = { {.f = printjob},
  {.f = iota},
  {.f = dropuntil,.arg = (void *[])
   {countuntil, &drop}},
  {.f = adddigits},
  {.f = multipleof,.arg = &f_arg_1},
  {.f = adddigits},
  {.f = equals,.arg = &f_arg_2},
  {.f = printjob},
  {.f = takewhile,.arg = (void *[])
   {countwhile, &take}},
  {.f = rejectif,.arg = (void *[])
   {isnull, &take}},
  };
  struct aggregate counter = { 0 };     // Initialise aggreagte
  struct stream stream = {.nb_mappers = sizeof (mappers) / sizeof (*mappers),.mappers = mappers,        // Mappers and filters
    .reducer = {.aggregate = &counter,.aggregator = increment}, // Reducer
    .deletor = free,            // Destructor
  };
  //size_t nb_cpu = TP_WORKER_SEQUENTIAL;
  size_t nb_cpu = TP_WORKER_NB_CPU;
  struct threadpool *threadpool = threadpool_create_and_start_stream (nb_cpu, &stream);
  //threadpool_set_monitor (threadpool, threadpool_monitor_to_terminal, 0, 0);
  threadpool_add_task_to_stream (threadpool, make_job ());
  threadpool_wait_and_destroy (threadpool);
  fprintf (stdout, "%zu\n", counter.nb);
  for (size_t i = 0; i < counter.nb; i++)
    fprintf (stdout, "%u ; ", counter.init[i]);
  fprintf (stdout, "\n");
  // Freeing aggregate stream.reducer.aggregate should be done here if necessary.
  free (counter.init);
}
