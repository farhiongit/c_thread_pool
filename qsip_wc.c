//=========================================
// Multi-threaded Quick sort in place
// (c) L. Farhi, 2024
// Language: C
// Compile options for algorithm:
// -DFIXED_PIVOT: use a fixed pivot, in the middle of the array to sort (otherwise, a random pivot is used by default, recommended).
// -DDEBUG: For debugging purpose only.
// Compile options for algorithm:
// ================= Quick sort in place =================
#include "wc.h"
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

static void *
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
  atomic_size_t nb_swaps;       // For debugging purpose only. MT safe.
} GlobalData;

typedef struct                  // Thread specific local data
{
  void *temp;                   // Will be used as temporary data for swap
  size_t nb_swaps;              // For debugging purpose only.
#ifndef FIXED_PIVOT
  struct random_data rpg_state; // thread random generator state 
#endif
} LocalData;

static void *
local_data_create (void *vg)    // Called at worker initialization
{
  GlobalData *g = vg;
  LocalData *l = calloc (1, sizeof (*l));       // calloc, for rpg_state to be initialized to 0 (see initstate_r).
  EXEC_OR_ABORT (l);
  l->nb_swaps = 0;
  l->temp = malloc (g->elem_size);
  EXEC_OR_ABORT (l->temp);
#ifndef FIXED_PIVOT
  errno = 0;
  initstate_r ((unsigned int) (intptr_t) l, random_statebuf, sizeof (random_statebuf) / sizeof (*random_statebuf), &l->rpg_state);
  EXEC_OR_ABORT (!errno);
#endif
  return l;
}

static void
local_data_delete (void *vl, void *vg)  // Called at worker termination
{
  LocalData *l = vl;
  GlobalData *g = vg;
  atomic_fetch_add (&g->nb_swaps, l->nb_swaps);
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

static void *
work2 (Job *job, GlobalData *g, LocalData *l)
{
  int32_t result = job->nmemb / 2;      // Magical number 2 :(
#ifndef FIXED_PIVOT
  random_r (&l->rpg_state, &result);
#endif
  void *pivot = job->base + (g->elem_size * (result % job->nmemb));     // Select the pivot
  void *next_to_pivot = pivot + g->elem_size;
  for (void *elem = job->base; elem < pivot; elem += g->elem_size)
    if (g->elem_lt (pivot, elem, g->elem_lt_arg))       // *pivot < *elem
    {
      swap (pivot - g->elem_size, elem, g->elem_size, l->temp);
      swap (pivot, pivot - g->elem_size, g->elem_size, l->temp);
      l->nb_swaps += 2;
      pivot -= g->elem_size;
      elem -= g->elem_size;
    }
  for (void *elem = next_to_pivot; elem < job->base + (g->elem_size * job->nmemb); elem += g->elem_size)
    if (g->elem_lt (elem, pivot, g->elem_lt_arg))       // *pivot > *elem
    {
      swap (pivot + g->elem_size, elem, g->elem_size, l->temp);
      swap (pivot, pivot + g->elem_size, g->elem_size, l->temp);
      l->nb_swaps += 2;
      pivot += g->elem_size;
    }
  // elements equal to pivot (*pivot == *elem) are left where they are.
  return pivot;
}

static void
work (struct threadpool *threadpool, void *j)
{
  Job *job = j;
  DPRINTF ("Job         (%1$p, %2$'zu) is being processed...\n", job->base, job->nmemb);
  if (job->nmemb >= 2)
  {
    LocalData *l = threadpool_worker_local_data (threadpool);
    GlobalData *g = threadpool_global_data (threadpool);
    void *pivot = work2 (job, g, l);    //<<<<<<<<<<<<<<<<<<<<<<<<<<<
    Job *new_job1 = job_create ((Job) {
                                .base = job->base,.nmemb = (pivot - job->base) / g->elem_size,
                                });
    EXEC_OR_ABORT (new_job1);
    DPRINTF ("Job         (%1$p, %2$'zu) to be added to jobs ...\n", new_job1->base, new_job1->nmemb);
    threadpool_add_task (threadpool, work, new_job1, free);
    Job *new_job2 = job_create ((Job) {
                                .base = pivot + g->elem_size,.nmemb = job->nmemb - 1 - ((pivot - job->base) / g->elem_size),
                                });
    EXEC_OR_ABORT (new_job2);
    DPRINTF ("Job         (%1$p, %2$'zu) to be added to jobs ...\n", new_job2->base, new_job2->nmemb);
    threadpool_add_task (threadpool, work, new_job2, free);
    DPRINTF ("Job         (%1$p, %2$'zu) made %3$'zu swaps.\n", job->base, job->nmemb, l->nb_swaps);
  }                             // if (data.nmemb >= 2)
  DPRINTF ("Job         (%1$p, %2$'zu) processed.\n", job->base, job->nmemb);
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

#ifdef FIXED_PIVOT
  DPRINTF (_(" [Fixed pivot (in the middle)]\n"));
#else
  DPRINTF (_(" [Random pivot]\n"));
  memcpy (random_statebuf, base, MIN (sizeof (random_statebuf), size * nmemb));
#endif
  // Initialize the pool of thread workers.
  GlobalData global_data = {.elem_size = size,.elem_lt = lt,.elem_lt_arg = 0 };
  atomic_init (&global_data.nb_swaps, 0);
  struct threadpool *ThreadPool = threadpool_create_and_start (NB_CPU, &global_data, local_data_create, local_data_delete);

  // Feed thread workers.
  Job initial_job = {
    .base = base,.nmemb = nmemb,
  };
  threadpool_add_task (ThreadPool, work, &initial_job, 0);

  // Wait for all threads to be finished.
  threadpool_wait_and_destroy (ThreadPool);
  DPRINTF ("%'zu swaps were made.\n", atomic_load (&global_data.nb_swaps));

  return EXIT_SUCCESS;
}
