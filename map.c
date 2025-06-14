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

struct map;
struct map_elem
{
  struct map_elem *lt, *upper, *ge;
  void *data;
  int rand;
  struct map *map;
};

struct map
{
  struct map_elem *first, *last, *root;
  mtx_t mutex;
  int (*cmp_key) (const void *, const void *, void *);
  const void *(*get_key) (const void *);
  void *arg;
  int uniqueness, stable;       // Properties.
  size_t nb_elem;
};

static void *
_map_remove (struct map_elem *old)
{
  struct map_elem *e = old;
  struct map *l = e->map;
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
  else                          // (e == e->upper->ge)
    e->upper->ge = child;

  l->nb_elem--;
  void *data = e->data;
  free (e);
  return data;
}

static struct map_elem *
_map_previous (struct map_elem *e)
{
  struct map_elem *ret = e;
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
  return ret;
}

static struct map_elem *
_map_next (struct map_elem *e)
{
  struct map_elem *ret = e;
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
  return ret;
}

static size_t
_map_traverse (map *m, map_operator op, map_selector sel, void *res, int backward)
{
  if (!m)
  {
    errno = EINVAL;
    return 0;
  }
  mtx_lock (&m->mutex);
  size_t nb_op = 0;
  for (struct map_elem * e = backward ? m->last : m->first; e;)
  {
    int remove = 0;
    int go_on = 1;
    if (!sel || sel (e->data, res))
    {
      go_on = op ? op (e->data, res, &remove) : 1;
      nb_op++;
    }
    struct map_elem *n = backward ? _map_previous (e) : _map_next (e);
    if (remove)
      _map_remove (e);
    if (!go_on)
      break;
    e = n;
  }
  mtx_unlock (&m->mutex);
  return nb_op;
}

static const void *
_MAP_KEY_IS_DATA (const void *data)
{
  return data;
}

struct map *
map_create (map_key_extractor get_key, map_key_comparator cmp_key, void *arg, int property)
{
  if (!get_key && cmp_key)
    get_key = _MAP_KEY_IS_DATA;
  if (((property & MAP_UNIQUENESS) || get_key) && !cmp_key)
  {
    errno = EINVAL;
    fprintf (stderr, "%s: %s\n", __func__, "Undefined key comparator.");
    return 0;
  }
  struct map *l = calloc (1, sizeof (*l));
  if (!l)
  {
    errno = ENOMEM;
    fprintf (stderr, "%s: %s\n", __func__, "Out of memory.");
    return 0;
  }
  l->get_key = get_key;
  l->cmp_key = cmp_key;
  l->uniqueness = (property & MAP_UNIQUENESS) ? 1 : 0;
  l->arg = arg;
  l->stable = l->uniqueness || (property & MAP_STABLE);
  // mtx_recursive : the SAME thread can lock (and unlock) the mutex several times. See https://en.wikipedia.org/wiki/Reentrant_mutex for more.
  // Therefore, map_find_key, map_traverse, map_traverse_backward and map_insert_data can call each other.
  if (mtx_init (&l->mutex, mtx_recursive) != thrd_success)
  {
    free (l);
    errno = ENOMEM;
    fprintf (stderr, "%s: %s\n", __func__, "Out of memory.");
    return 0;
  }
  return l;
}

int
map_destroy (struct map *l)
{
  if (!l)
  {
    errno = EINVAL;
    return EXIT_FAILURE;
  }
  mtx_lock (&l->mutex);
  if (l->first)
  {
    errno = EPERM;
    fprintf (stderr, "%s: %s\n", __func__, "Not empty.");
    mtx_unlock (&l->mutex);
    return EXIT_FAILURE;
  }
  mtx_unlock (&l->mutex);
  mtx_destroy (&l->mutex);
  free (l);
  return EXIT_SUCCESS;
}

int
map_insert_data (struct map *l, void *data)
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
  int cmp;
  unsigned int ret = 1;
  if (!(upper = l->root))
    l->root = l->first = l->last = new;
  else if (!l->cmp_key && l->stable)
  {
    (l->last->ge = new)->upper = l->last;
    l->last = new;
  }
  else
    while (1)
      if ((cmp = l->cmp_key ? l->cmp_key (l->get_key (new->data), l->get_key (upper->data), l->arg) : 0) < 0 || (cmp == 0 && new->rand < upper->rand))
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
        free (new);             // new is not inserted.
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
  l->nb_elem += ret;
  mtx_unlock (&l->mutex);
  return (int) ret;
}

size_t
map_traverse (map *m, map_operator op, map_selector sel, void *res)
{
  return _map_traverse (m, op, sel, res, 0);
}

size_t
map_traverse_backward (map *m, map_operator op, map_selector sel, void *res)
{
  return _map_traverse (m, op, sel, res, 1);
}

size_t
map_find_key (struct map *l, const void *key, map_operator op, void *res)
{
  if (!l->cmp_key)
  {
    fprintf (stderr, "%s: %s\n", __func__, "Undefined key.");
    errno = EPERM;
    return 0;
  }
  if (!l || !key)
  {
    errno = EINVAL;
    return 0;
  }
  mtx_lock (&l->mutex);
  size_t nb_op = 0;
  int cmp_key;
  struct map_elem *upper = l->root;
  while (upper)
    if ((cmp_key = l->cmp_key (key, l->get_key (upper->data), l->arg)) < 0)
      upper = upper->lt;
    else if (cmp_key == 0)
    {
      int remove = 0;
      int ret = op ? op (upper->data, res, &remove) : 1;
      struct map_elem *next = upper->ge;
      nb_op++;
      if (remove)
        _map_remove (upper);
      if (ret && next && l->cmp_key (key, l->get_key (next->data), l->arg) == 0)
        upper = next;
      else
        break;
    }
    else                        // && (new >= upper)
      upper = upper->ge;
  mtx_unlock (&l->mutex);
  return nb_op;
}

static int
_MAP_REMOVE (void *data, void *res, int *remove)
{
  if (res)
    *(void **) res = data;      // *res is supposed to be a pointer here.
  *remove = 1;
  return 0;
}

map_operator MAP_REMOVE_ONE = _MAP_REMOVE;

static int
_MAP_REMOVE_ALL (void *data, void *res, int *remove)
{
  if (res)
  {
    void (*dtor) (void *) = res;
    dtor (data);      // *res is supposed to be a pointer to a destructor here.
  }
  *remove = 1;
  return 1;
}

map_operator MAP_REMOVE_ALL = _MAP_REMOVE_ALL;

static int
_MAP_MOVE (void *data, void *res, int *remove)
{
  if (!res)
  {
    fprintf (stderr, "%s: %s\n", "MAP_MOVE", "Context must not be a null pointer.");
    errno = EINVAL;
    return 0;
  }
  // if res is the same as the map containing data, i.e. (map *)res is already locked, ...
  return (*remove = map_insert_data (res, data));
}

map_operator MAP_MOVE_TO = _MAP_MOVE;

size_t
map_size (map *m)
{
  mtx_lock (&m->mutex);
  size_t ret = m->nb_elem;
  mtx_unlock (&m->mutex);
  return ret;
}
