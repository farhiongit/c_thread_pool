//=========================================
// Multi-threaded Quick sort in place
// (c) L. Farhi, 2024
// Language: C
// Compile options for algorithm:
// -DFIXED_PIVOT: use a fixed pivot, in the middle of the array to sort (otherwise, a random pivot is used by default, recommended).
// -DDEBUG: For debugging purpose only.
// Compile options for algorithm:
// ================= Quick sort in place =================
#define _DEFAULT_SOURCE         // For initstate_r
#include "wqm.h"
#include "qsip_wc.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#ifndef FIXED_PIVOT
#  define MIN(a,b) ((a) < (b) ? (a) : (b))
static char random_statebuf[128] = { 0 };
#endif
#ifdef DEBUG
#  include <unistd.h>
#endif
#define EXEC_OR_ABORT(cond) do { if (!(cond)) abort () ; } while (0)
#ifdef DEBUG
#  include <stdio.h>
#  define DPRINTF(...) do { fprintf(stderr, __VA_ARGS__); } while (0)
#  define TEST_OR_ABORT(cond) do { if (!(cond)) abort () ; } while (0)
#else
#  define DPRINTF(...)
#  define TEST_OR_ABORT(cond)
#endif
#define _(s) (s)

typedef struct
{
  void *base;
  size_t nmemb;
} Job;                          // Chunk of an array of elements.

static Job *
job_create (Job j)
{
  Job *pj = malloc (sizeof (*pj));
  EXEC_OR_ABORT (pj);
  *pj = j;
  return pj;
}

typedef struct                  // Threadpool specific global data
{
  const size_t elem_size;       // Size of elements of type of *base
  int (*const elem_lt) (const void *, const void *, void *);    // Elements comparator
  void *const elem_lt_arg;      // elem_lt third argument
  atomic_size_t nb_swaps, nb_cmp;       // For debugging purpose only. MT safe.
} GlobalData;

typedef struct                  // Thread specific local data
{
  void *temp;                   // Will be used as temporary data for swap
  size_t nb_swaps, nb_cmp;      // For debugging purpose only.
#ifndef FIXED_PIVOT
  struct random_data rpg_state; // thread random generator state 
#endif
} LocalData;

static void *
local_data_create (void)        // Called at worker initialization
{
  GlobalData *g = threadpool_global_data ();
  LocalData *l = malloc (sizeof (*l));
  EXEC_OR_ABORT (l);
  l->nb_swaps = l->nb_cmp = 0;
  l->temp = malloc (g->elem_size);
  EXEC_OR_ABORT (l->temp);
#ifndef FIXED_PIVOT
  errno = 0;
  l->rpg_state.state = 0;       // the buf.state field must be initialized to 0 before use (see initstate_r).
  initstate_r ((unsigned int) (intptr_t) l, random_statebuf, sizeof (random_statebuf) / sizeof (*random_statebuf), &l->rpg_state);
  EXEC_OR_ABORT (!errno);
#endif
  return l;
}

static void
local_data_delete (void *vl)    // Called at worker termination
{
  LocalData *l = vl;
  GlobalData *g = threadpool_global_data ();
  atomic_fetch_add (&g->nb_swaps, l->nb_swaps);
  atomic_fetch_add (&g->nb_cmp, l->nb_cmp);
  free (l->temp);
  free (l);
}

// ================= Work =================
static void
swap (void *a, void *b, size_t size, void *temp)
{
  if (a == b)
    return;
  memcpy (temp, a, size);
  memcpy (a, b, size);
  memcpy (b, temp, size);
}

static void
lomuto (Job *job, GlobalData *g, LocalData *l, void **lt, void **gt)
{
  int32_t result = (int32_t) (job->nmemb / 2);  // Magical number 2 :(
#ifndef FIXED_PIVOT
  random_r (&l->rpg_state, &result);
#endif
  void *pi = job->base + (g->elem_size * ((size_t) result % job->nmemb));       // Select the pivot
  swap (job->base, pi, g->elem_size, l->temp);
  pi = job->base;
  l->nb_swaps++;
  *lt = job->base + g->elem_size;
  *gt = job->base + g->elem_size * job->nmemb;
  // |p|l            i ?               |g
  // |p|    <    |l  i ?    |g    >    |
  for (void *i = *lt; i < *gt; i += g->elem_size)
    if (g->elem_lt (i, pi, g->elem_lt_arg))     // *p > *i
    {
      l->nb_cmp++;
      swap (i, *lt, g->elem_size, l->temp);
      l->nb_swaps++;
      *lt += g->elem_size;
    }
    else if (g->elem_lt (pi, i, g->elem_lt_arg))        // *p < *i
    {
      l->nb_cmp++;
      *gt -= g->elem_size;
      swap (i, *gt, g->elem_size, l->temp);
      l->nb_swaps++;
      i -= g->elem_size;
    }
  // |p|    <    |l    =    |g    >    |
  *lt -= g->elem_size;
  swap (pi, *lt, g->elem_size, l->temp);
  l->nb_swaps++;
  *gt -= g->elem_size;
  // |      <   |l     =   g|     >    |
}

static void *
hoare (Job *job, GlobalData *g, LocalData *l)
{
  int32_t result = (int32_t) (job->nmemb / 2);  // Magical number 2 :(
#ifndef FIXED_PIVOT
  random_r (&l->rpg_state, &result);
#endif
  void *pi, *pj, *pn, *a;
  a = job->base;
  pi = a + (g->elem_size * ((size_t) result % job->nmemb));     // Select the pivot
  swap (a, pi, g->elem_size, l->temp);
  l->nb_swaps++;
  pi = a;
  pj = pn = a + job->nmemb * g->elem_size;
  for (;;)
  {
    do
    {
      pi += g->elem_size;
      l->nb_cmp++;
    }
    while (pi < pn && g->elem_lt (pi, a, g->elem_lt_arg));
    do
    {
      pj -= g->elem_size;
      l->nb_cmp++;
    }
    while (g->elem_lt (a, pj, g->elem_lt_arg));
    l->nb_cmp -= 2;
    if (pj < pi)
      break;
    swap (pi, pj, g->elem_size, l->temp);
    l->nb_swaps++;
  }
  swap (a, pj, g->elem_size, l->temp);
  l->nb_swaps++;
  return pj;
}

static int
work (struct threadpool *threadpool, void *j)
{
  Job *job = j;
  DPRINTF ("Job         (%1$p, %2$'zu) is being processed...\n", job->base, job->nmemb);
  if (job->nmemb >= 2)
  {
    LocalData *l = threadpool_worker_local_data ();
    GlobalData *g = threadpool_global_data ();
    void *p1, *p2;
    (void) hoare;               //p1 = p2 = hoare (job, g, l);
    lomuto (job, g, l, &p1, &p2);
    TEST_OR_ABORT (p1 >= job->base && p1 < job->base + job->nmemb * g->elem_size);
    TEST_OR_ABORT (p2 >= job->base && p2 < job->base + job->nmemb * g->elem_size);
    Job *new_job1 = job_create ((Job) {
                                .base = job->base,.nmemb = (typeof (job->nmemb)) (p1 - job->base) / g->elem_size,
                                });
    EXEC_OR_ABORT (new_job1);
    DPRINTF ("Job         (%1$p, %2$'zu) to be added to jobs ...\n", new_job1->base, new_job1->nmemb);
    threadpool_add_task (threadpool, work, new_job1, free);
    Job *new_job2 = job_create ((Job) {
                                .base = p2 + g->elem_size,.nmemb = job->nmemb - 1 - ((typeof (job->nmemb)) (p2 - job->base) / g->elem_size),
                                });
    EXEC_OR_ABORT (new_job2);
    DPRINTF ("Job         (%1$p, %2$'zu) to be added to jobs ...\n", new_job2->base, new_job2->nmemb);
    threadpool_add_task (threadpool, work, new_job2, free);
    DPRINTF ("Job         (%1$p, %2$'zu) made %3$'zu swaps.\n", job->base, job->nmemb, l->nb_swaps);
  }                             // if (data.nmemb >= 2)
  DPRINTF ("Job         (%1$p, %2$'zu) processed.\n", job->base, job->nmemb);
  return 0;
}

// ================= Entry point =================
int
qsip (void *base, size_t nmemb, size_t size, int (*lt) (const void *, const void *, void *), void *arg)
{
  if (!lt || !size || (!base && nmemb) || (base && !nmemb))
  {
    errno = EINVAL;
    return EXIT_FAILURE;
  }
  if (!base)
    return EXIT_SUCCESS;

  (void) arg;

#ifdef FIXED_PIVOT
  DPRINTF (_(" [Fixed pivot (in the middle)]\n"));
#else
  DPRINTF (_(" [Random pivot]\n"));
  memcpy (random_statebuf, base, MIN (sizeof (random_statebuf), size * nmemb));
#endif
  // Initialize the pool of thread workers.
  GlobalData global_data = {.elem_size = size,.elem_lt = lt,.elem_lt_arg = 0 };
  atomic_init (&global_data.nb_swaps, 0);
  atomic_init (&global_data.nb_cmp, 0);
  struct threadpool *ThreadPool = threadpool_create_and_start (NB_CPU, &global_data);
  threadpool_set_worker_local_data_manager (ThreadPool, local_data_create, local_data_delete);

  // Feed thread workers.
  Job initial_job = {
    .base = base,.nmemb = nmemb,
  };
  threadpool_add_task (ThreadPool, work, &initial_job, 0);

  // Wait for all threads to be finished.
  threadpool_wait_and_destroy (ThreadPool);
  DPRINTF ("%'zu swaps were made.\n", atomic_load (&global_data.nb_swaps));
  DPRINTF ("%'zu comparisons were made.\n", atomic_load (&global_data.nb_cmp));

  return EXIT_SUCCESS;
}
