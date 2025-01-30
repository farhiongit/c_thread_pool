// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#include <threads.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "map.h"

int MAP_NONE = 0;
int MAP_UNIQUENESS = 1;
int MAP_STABLE = 2;

struct smap;
struct map_elem
{
  struct map_elem *lt, *upper, *ge;
  void *data;
  int rand;
  struct smap *map;
};

struct smap
{
  struct map_elem *first, *last, *root;
  mtx_t mutex;
  int (*cmp_key) (void *, void *, void *);
  void * (*get_key) (void *);
  void *arg;
  int uniqueness, stable;       // Properties.
};

struct smap *
map_create (void * (*get_key) (void *data), int (*cmp_key) (void *key_a, void *key_bb, void *arg), void *arg, int property)
{
  if (!get_key)
  {
    errno = EINVAL;
    return 0;
  }
  struct smap *l = calloc (1, sizeof (*l));
  if (!l)
  {
    errno = ENOMEM;
    fprintf (stderr, "%s: %s\n", __func__, "Out of memory.");
    return 0;
  }
  l->get_key = get_key;
  l->cmp_key = cmp_key;
  l->arg = arg;
  l->uniqueness = property & MAP_UNIQUENESS;
  l->stable = (property & MAP_UNIQUENESS) || (property & MAP_STABLE);
  if (mtx_init (&l->mutex, mtx_plain) != thrd_success)
  {
    if (l)
      free (l);
    l = 0;
    errno = ENOMEM;
    fprintf (stderr, "%s: %s\n", __func__, "Out of memory.");
  }
  return l;
}

int
map_destroy (struct smap *l)
{
  if (!l)
  {
    errno = EINVAL;
    return EXIT_FAILURE;
  }
  if (map_first (l))
  {
    errno = EPERM;
    return EXIT_FAILURE;
  }
  mtx_destroy (&l->mutex);
  free (l);
  return EXIT_SUCCESS;
}

void *
map_get_data (struct map_elem *e)
{
  if (!e)
  {
    errno = EINVAL;
    return 0;
  }
  return e->data;
}

struct map_elem *
map_insert_data (struct smap *l, void *data)
{
  if (!l)
  {
    errno = EINVAL;
    return 0;
  }
  struct map_elem *new = calloc (1, sizeof (*new));
  if (!new)
  {
    errno = ENOMEM;
    fprintf (stderr, "%s: %s\n", __func__, "Out of memory.");
    return 0;
  }
  new->data = data;
  new->map = l;
  new->rand = (l->uniqueness || l->stable) ? 0 : rand ();
  mtx_lock (&l->mutex);
  struct map_elem *upper;
  struct map_elem *ret = new;
  int cmp;
  if (!(upper = l->root))
    l->root = l->first = l->last = new;
  else
    while (1)
      if ((cmp = l->cmp_key ? l->cmp_key (l->get_key (new->data), l->get_key (upper->data), l->arg) : 0) < 0
          || (cmp == 0 && new->rand < upper->rand))
      {
        if (upper->lt)
          upper = upper->lt;
        else
        {
          if (((upper->lt = new)->upper = upper) == l->first)
            l->first = new;
          break;
        }
      }
      else if (l->uniqueness && cmp == 0)
      {
        errno = EPERM;
        ret = 0;
        break;
      }
      else if (upper->ge)       // && (new >= upper)
        upper = upper->ge;
      else                      // (!upper->ge) && (new >= upper)
      {
        if (((upper->ge = new)->upper = upper) == l->last)
          l->last = new;
        break;
      }
  mtx_unlock (&l->mutex);
  return ret;
}

void *
map_remove (struct map_elem *old)
{
  if (!old)
  {
    errno = EINVAL;
    return 0;
  }

  struct map_elem *e = old;
  struct smap *l = e->map;
  mtx_lock (&l->mutex);
  if (e == l->first)
  {                             // l->first = map_next (l->first)
    if (l->first->ge)
      for (l->first = l->first->ge; l->first->lt; l->first = l->first->lt) /* nothing */ ;
    else
      l->first = l->first->upper;
  }
  if (e == l->last)
  {                             // l->last = map_previous (l->last)
    if (l->last->lt)
      for (l->last = l->last->lt; l->last->ge; l->last = l->last->ge) /* nothing */ ;
    else
      l->last = l->last->upper;
  }

  if (e->lt && e->ge)
  {
    if (rand () & 1)
    {
      struct map_elem *lt = e->lt;
      e->lt = 0;
      for (e = e->ge; e->lt; e = e->lt);
      (e->lt = lt)->upper = e;
    }
    else
    {
      struct map_elem *ge = e->ge;
      e->ge = 0;
      for (e = e->lt; e->ge; e = e->ge);
      (e->ge = ge)->upper = e;
    }
  }

  e = old;
  struct map_elem *child;
  if ((child = (e->lt ? e->lt : e->ge)))
    child->upper = e->upper;
  if (!e->upper)
    l->root = child;            // Update map->root
  else if (e == e->upper->lt)
    e->upper->lt = child;
  else  // (e == e->upper->ge)
    e->upper->ge = child;
  mtx_unlock (&l->mutex);

  void *data = e->data;
  free (e);
  return data;
}

struct map_elem *
map_first (struct smap *l)
{
  if (!l)
  {
    errno = EINVAL;
    return 0;
  }
  mtx_lock (&l->mutex);
  struct map_elem *ret = l->first;
  mtx_unlock (&l->mutex);
  return ret;
}

struct map_elem *
map_last (struct smap *l)
{
  if (!l)
  {
    errno = EINVAL;
    return 0;
  }
  mtx_lock (&l->mutex);
  struct map_elem *ret = l->last;
  mtx_unlock (&l->mutex);
  return ret;
}

struct map_elem *
map_previous (struct map_elem *e)
{
  if (!e)
  {
    errno = EINVAL;
    return 0;
  }
  struct map_elem *ret = e;
  struct smap *l = ret->map;
  mtx_lock (&l->mutex);
  if (ret->lt)
    for (ret = ret->lt; ret->ge; ret = ret->ge) /* nothing */ ;
  else if (ret->upper)
  {
    for (; ret && ret->upper && ret == ret->upper->lt; ret = ret->upper);
    if (ret)
      ret = ret->upper;
  }
  else
    ret = 0;
  mtx_unlock (&l->mutex);
  return ret;
}

struct map_elem *
map_next (struct map_elem *e)
{
  if (!e)
  {
    errno = EINVAL;
    return 0;
  }
  struct map_elem *ret = e;
  struct smap *l = ret->map;
  mtx_lock (&l->mutex);
  if (ret->ge)
    for (ret = ret->ge; ret->lt; ret = ret->lt) /* nothing */ ;
  else if (ret->upper)
  {
    for (; ret && ret->upper && ret == ret->upper->ge; ret = ret->upper);
    if (ret)
      ret = ret->upper;
  }
  else
    ret = 0;
  mtx_unlock (&l->mutex);
  return ret;
}

struct map_elem *
map_find_key (struct smap *l, void *key)
{
  if (!l || !key)
  {
    errno = EINVAL;
    return 0;
  }
  mtx_lock (&l->mutex);
  struct map_elem *upper;
  struct map_elem *ret = 0;
  int cmp_key;
  if ((upper = l->root))
  {
    while (1)
      if ((cmp_key = l->cmp_key ? l->cmp_key (key, l->get_key (upper->data), l->arg) : 0) < 0)
      {
        if (upper->lt)
          upper = upper->lt;
        else
          break;
      }
      else if (cmp_key == 0)
      {
        ret = upper;
        break;
      }
      else if (upper->ge)       // && (new >= upper)
        upper = upper->ge;
      else                      // (!upper->ge) && (new >= upper)
        break;
  }
  mtx_unlock (&l->mutex);
  return ret;
}
