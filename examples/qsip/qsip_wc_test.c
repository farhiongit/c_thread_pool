//====================== Unit test ================================
// Compile options for unit test:
// -DTIMES=n: number of unit tests (default is 100).
// -DSIZE=n: size of array to sort, for unit testing (default is 1000000).
// -DREPRODUCTIBLE: get reproductible unit test data (with one thread) (by default, unit tests are randomized).
#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <locale.h>
#include <assert.h>
#include <unistd.h>
#define ELEM_SIZE(arr)  (sizeof(*arr))
#ifndef SIZE
#  define SIZE 1000000
#endif
#ifndef TIMES
#  define TIMES 100
#endif
#define _(s) (s)
#include "qsip_wc.h"
#include "wqm.h"

//typedef unsigned char SortableType;
typedef long long int SortableType;

static int
lti (const void *a, const void *b, void *arg)
{
  (void) arg;
  return ((*(const SortableType *) a)) < ((*(const SortableType *) b));
}

static int
cmpi (const void *a, const void *b, void *arg)
{
  if (lti (a, b, arg))
    return -1;
  if (lti (b, a, arg))
    return 1;
  return 0;
}

struct gd
{
  size_t size, elem_size;
  char *tags;
};

static void *
tag (void)
{
  void *global = threadpool_global_data ();
  return ((struct gd *) global)->tags++;
}

static void
untag (void *local)
{
  (void) local;
  void *global = threadpool_global_data ();
  ((struct gd *) global)->tags--;
}

static int
worker (struct threadpool *threadpool, void *base)
{
  (void) threadpool;
  int ret = 0;
  (void) cmpi;                  // Avoid ‘cmpi’ defined but not used
#if 1
  struct gd *gd = threadpool_global_data ();
#  ifndef QSORT
  qsip (base, gd->size, gd->elem_size, lti, 0);
#  else
  qsort_r (base, gd->size, gd->elem_size, cmpi, 0);     // The single threaded algorithm is much faster than the multi-threaded (due to threading overhead).
#  endif
  // Check base[i] <= base[i + 1]
  SortableType *st = base;
  for (size_t i = 0; i < gd->size - 1 && ret == 0; i++)
    if (lti (&st[i + 1], &st[i], 0))
      ret = 1;
#else
  sleep (1);
#endif
  char *tag = threadpool_worker_local_data ();
  (void) tag;
  return ret;
}

int
main ()
{
  setlocale (LC_ALL, "");
  /*alignas(128) */
  SortableType *base = malloc (((size_t) TIMES) * ((size_t) SIZE) * sizeof (*base));    // Beware of false sharing.
  assert (base);
  fprintf (stdout, _("Sorting %1$'zu elements (multi-threaded quick sort in place), %2$'i times:\n"), (size_t) SIZE, TIMES);
#ifndef REPRODUCTIBLE
  struct timespec ts;
  clock_gettime (CLOCK_MONOTONIC, &ts);
  srandom ((unsigned int) ts.tv_nsec);
#else
  fprintf (stdout, _(" [Reproductible]\n"));
#endif
  fprintf (stdout, _("Initializing %'zu random numbers...\n"), (size_t) (TIMES * SIZE));
  for (size_t i = 0; i < (size_t) TIMES; i++)
    for (size_t j = 0; j < (size_t) SIZE; j++)
      base[i * (size_t) SIZE + j] = ((1LL << 31) * random () + random ()) % (1LL << i);
  char threads_tags[] = "1234567";      // 7 workers
  struct gd gd = { (size_t) SIZE, ELEM_SIZE (base), threads_tags };
  size_t nb_workers = strlen (threads_tags);
  fprintf (stdout, _("%zu workers requested and processing...\n"), nb_workers);
  struct threadpool *tp = threadpool_create_and_start (nb_workers, &gd, tag, untag);    // Start 7 workers
  threadpool_set_monitor (tp, threadpool_monitor_to_terminal, 0);
  size_t i = 0;
  size_t task_id;
  for (; i < ((size_t) TIMES) / 2; i++)
    task_id = threadpool_add_task (tp, worker, base + (i * ((size_t) SIZE)), 0);        // Parallel work
  fprintf (stdout, _("Will go to sleep for %i seconds...\n"), TIMES / 6);
  sleep (TIMES / 6);
  fprintf (stdout, _("Stop sleeping after %i seconds.\n"), TIMES / 6);
  for (; i < (size_t) TIMES; i++)
    task_id = threadpool_add_task (tp, worker, base + (i * ((size_t) SIZE)), 0);        // Parallel work
  sleep (1);
  threadpool_cancel_task (tp, task_id);
  threadpool_cancel_task (tp, task_id);
  sleep (1);
  threadpool_cancel_task (tp, LAST_TASK);
  threadpool_cancel_task (tp, LAST_TASK);
  sleep (1);
  threadpool_cancel_task (tp, NEXT_TASK);
  threadpool_cancel_task (tp, NEXT_TASK);
  sleep (1);
  threadpool_cancel_task (tp, ALL_TASKS);
  threadpool_wait_and_destroy (tp);
  fprintf (stdout, _("Done.\n"));
  free (base);
}