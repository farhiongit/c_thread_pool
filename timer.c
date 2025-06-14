// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <threads.h>
#include "map.h"
#include "timer.h"

struct timer_elem
{
  struct timespec timeout;
  int (*callback) (void *arg);
  void *arg;
};

static int
timespec_cmp (const struct timespec *a, const struct timespec *b)
{
  return (a->tv_sec < b->tv_sec ? -1 : a->tv_sec > b->tv_sec ? 1 : a->tv_nsec < b->tv_nsec ? -1 : a->tv_nsec > b->tv_nsec ? 1 : 0);
}

static struct                   // Ordered list of struct timer_elem stored in an ordered binary tree.
{
  thrd_t thread;
  mtx_t mutex;
  cnd_t condition;
  map *map;
} Timers = { 0 };

static const void *
Timers_get_key (const void *pa)
{
  const struct timer_elem *a = pa;
  return &a->timeout;
}

static int
Timers_cmp_key (const void *pa, const void *pb, void *arg)
{
  (void) arg;
  const struct timespec *a = pa;
  const struct timespec *b = pb;
  return timespec_cmp (a, b);
}

static int
Timers_get_earliest (void *data, void *res, int *remove)
{
  *(struct timer_elem **) res = data;
  *remove = 0;
  return 0;
}

static void *
Timers_add (struct timespec timeout, int (*callback) (void *arg), void *arg)
{
  struct timer_elem *new = calloc (1, sizeof (*new));
  if (!new)
    return 0;
  new->timeout = timeout;
  new->callback = callback;
  new->arg = arg;

  map_insert_data (Timers.map, new);
  return new;
}

static int
Timers_remove_earliest (void *data, void *res, int *remove)
{
  (void) data;
  (void) res;
  *remove = 1;
  return 0;
}

static int
timers_loop (void *)
{
  mtx_lock (&Timers.mutex);
  while (1)                     // Infinite loop, never ends.
  {
    struct timer_elem *earliest = 0;
    map_traverse (Timers.map, Timers_get_earliest, 0, &earliest);
    if (!earliest)
      cnd_wait (&Timers.condition, &Timers.mutex);
    else if (cnd_timedwait (&Timers.condition, &Timers.mutex, &earliest->timeout) != thrd_timedout) /* nothing, loop again */ ;
    else
    {
      struct timer_elem *earliest2 = 0;
      map_traverse (Timers.map, Timers_get_earliest, 0, (void **) &earliest2);
      if (earliest == earliest2)        // Timers_head could have changed while waiting.
      {
        if (earliest->callback)
          earliest->callback (earliest->arg);
        map_traverse (Timers.map, Timers_remove_earliest, 0, 0);
      }
    }
  }
  mtx_unlock (&Timers.mutex);
  return 0;
}

static once_flag TIMERS_INIT = ONCE_FLAG_INIT;
static void
timers_init (void)              // Called once.
{
  Timers.map = map_create (Timers_get_key, Timers_cmp_key, 0, MAP_NONE);        // Timers.map won't be destroyed.
  mtx_init (&Timers.mutex, mtx_plain);  // Timers.mutex won't be destroyed.
  cnd_init (&Timers.condition); // Timers.condition won't be destroyed.
  thrd_create (&Timers.thread, timers_loop, 0); // The thread won't be destroyed and will never end. It will be stopped when the thread of the caller to timer_set will end.
}

struct timespec
delay_to_abs_timespec (double seconds)
{
  long sec = lround (trunc (seconds));  // C standard function.
  long nsec = lround ((seconds - trunc (seconds)) * 1000 * 1000 * 1000);
  struct timespec t;
  timespec_get (&t, TIME_UTC);  // C standard function, returns now. UTC since cnd_timedwait is UTC-based.
  t.tv_sec += sec + (t.tv_nsec + nsec) / (1000 * 1000 * 1000);
  t.tv_nsec = (t.tv_nsec + nsec) % (1000 * 1000 * 1000);
  return t;
}

void *
timer_set (struct timespec timeout, int (*callback) (void *arg), void *arg)
{
  call_once (&TIMERS_INIT, timers_init);
  void *timer = Timers_add (timeout, callback, arg);
  cnd_broadcast (&Timers.condition);
  return timer;
}

static int
timer_remover (void *data, void *res, int *remove)
{
  return (*remove = (data == res) ? 1 : 0) ? 0 : 1;
}

void
timer_unset (void *timer)
{
  if (map_traverse (Timers.map, timer_remover, 0, timer))
  {
    call_once (&TIMERS_INIT, timers_init);
    cnd_broadcast (&Timers.condition);
  }
}
