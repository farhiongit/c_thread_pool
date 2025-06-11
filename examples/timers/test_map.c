// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#include <stdlib.h>
#include <assert.h>
#include <sys/resource.h>
#include <math.h>
#include <errno.h>
#include <threads.h>
#include <unistd.h>
#include <string.h>
#include "map.h"
#include "trace.h"
#define map_create(...)            TRACE_EXPRESSION(map_create (__VA_ARGS__))
#define map_destroy(...)           TRACE_EXPRESSION(map_destroy (__VA_ARGS__))
#define map_insert_data(...)       TRACE_EXPRESSION(map_insert_data (__VA_ARGS__))
#define map_traverse(...)          TRACE_EXPRESSION(map_traverse (__VA_ARGS__))
#define map_find_key(...)          TRACE_EXPRESSION(map_find_key (__VA_ARGS__))
#define map_traverse_backward(...) TRACE_EXPRESSION(map_traverse_backward (__VA_ARGS__))

static int
cmpstringp (const void *p1, const void *p2, void *arg)
{
  /* The actual arguments to this function are "pointers to
     pointers to char", but strcmp(3) arguments are "pointers
     to char", hence the following cast plus dereference. */

  (void) arg;
  return strcmp ((const char *) p1, (const char *) p2);
}

static int
print_data (void *data, void *res, int *remove)
{
  (void) res;
  fprintf (stdout, "%s ", (char *) data);
  *remove = 0;                  // Tells: do not remove the data from the map.
  return 1;                     // Tells: continue traversing.
}

static int
select_c (void *data, void *res)
{
  (void) res;
  return (*(char *) data == 'c');
}

static int
cmpip (const void *p1, const void *p2, void *arg)
{
  (void) arg;
  return *(const int*)p1 < *(const int*)p2 ? -1 : *(const int*)p1 > *(const int*)p2 ? 1 : 0; 
}

static int dbl (int i) { return 2 * i ; }
static int dec (int i) { return i - 1 ; }

static int
apply (void *data, void *res, int *remove)
{
  (void) remove;
  int (*f) (int) = res;
  *(int *)data = f (*(int *)data);
  return 1;                     // Tells: continue traversing.
}

struct rai_args { int (*f) (int) ; map *map ; };

static int
remove_apply_insert (void *data, void *res, int *remove)
{
  int (*f) (int) = ((struct rai_args *)res)->f;
  map *m = ((struct rai_args *)res)->map;
  int *pi = malloc (sizeof (*pi));
  *pi = f (*(int *)data);
  map_insert_data (m, pi);
  free (data);
  *remove = 1;                  // Tells: remove the data from the map.
  return 1;                     // Tells: continue traversing.
}

static int
print_pi (void *data, void *res, int *remove)
{
  (void) res;
  fprintf (stdout, "%i ", *(int *) data);
  *remove = 0;                  // Tells: do not remove the data from the map.
  return 1;                     // Tells: continue traversing.
}

int
main (void)
{
  srand ((unsigned int) time (0));
  for (int i = 1; i <= 4; i++)
  {
    map *li;
    puts ("============================================================");
    switch (i)
    {
      case 1:
        li = map_create (MAP_KEY_IS_DATA, cmpstringp, 0, MAP_UNIQUENESS);       // Set
        break;
      case 2:
        li = map_create (MAP_KEY_IS_DATA, cmpstringp, 0, MAP_STABLE);   // Ordered list
        break;
      case 3:
        li = map_create (0, 0, 0, MAP_STABLE);  // Chain (FIFO or LIFO)
        break;
      case 4:
        li = map_create (0, 0, 0, MAP_NONE);    // Unordered list
        break;
      default:
    }
    map_insert_data (li, "b");  // The map stores pointers to static data of type char[].
    map_insert_data (li, "a");
    map_insert_data (li, "d");
    map_insert_data (li, "c");
    map_insert_data (li, "c");
    map_insert_data (li, "a");
    map_insert_data (li, "aa");
    map_insert_data (li, "cc");
    map_insert_data (li, "d");
    fprintf (stdout, "%lu elements.\n", map_size (li));

    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");
    map_traverse_backward (li, print_data, 0, 0);
    fprintf (stdout, "\n");
    map_traverse (li, print_data, select_c, 0);
    fprintf (stdout, "\n");

    char *data = 0;
    if (map_traverse (li, MAP_REMOVE, 0, &data) && data)        // Remove the first found element from the map.
    {
      fprintf (stdout, "%s <-- ", data);
      map_traverse (li, print_data, 0, 0);
      fprintf (stdout, "<-- %s\n", data);
      map_insert_data (li, data);       // Reinsert after use.
      map_traverse (li, print_data, 0, 0);
      fprintf (stdout, "\n");
    }

    map_insert_data (li, "r");
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");
    map *lj = map_create (0, 0, 0, MAP_NONE);
    map_find_key (li, "r", MAP_MOVE_TO, lj);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");
    map_traverse (lj, print_data, 0, 0);
    fprintf (stdout, "\n");
    map_traverse (lj, MAP_REMOVE_ALL, 0, 0);
    map_destroy (lj);

    map_find_key (li, "c", MAP_REMOVE_ALL, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");
    fprintf (stdout, "%lu elements.\n", map_size (li));

    map_traverse (li, MAP_REMOVE, 0, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_find_key (li, "b", MAP_REMOVE, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_find_key (li, "d", MAP_REMOVE, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_traverse_backward (li, MAP_REMOVE, 0, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_traverse (li, MAP_REMOVE, 0, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_traverse (li, MAP_REMOVE_ALL, 0, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");
    fprintf (stdout, "%lu elements.\n", map_size (li));
    map_traverse (li, MAP_REMOVE_ALL, 0, 0);
    map_destroy (li);
    fprintf (stdout, "=======\n");
  }

  map* li = li = map_create (MAP_KEY_IS_DATA, cmpip, 0, MAP_STABLE);   // Ordered list
  for (size_t i = 0 ; i < 10 ; i++)
  {
    int *pi = malloc (sizeof (*pi));
    *pi = rand () % 40 + 10;
    map_insert_data (li, pi);
  }
  map_traverse (li, print_pi, 0, 0);
  fprintf (stdout, "\n");
  map_traverse (li, apply, 0, dec);
  map_traverse (li, print_pi, 0, 0);
  fprintf (stdout, "\n");
  map_traverse (li, apply, 0, dbl);
  map_traverse (li, print_pi, 0, 0);
  fprintf (stdout, "\n");
  struct rai_args args;
  args = (struct rai_args ){ dec, li };
  map_traverse (li, remove_apply_insert, 0, &args);
  map_traverse (li, print_pi, 0, 0);
  fprintf (stdout, "\n");
  args = (struct rai_args ){ dbl, li };
  map_traverse (li, remove_apply_insert, 0, &args);  // Integers are removed, doubled and pushed back forward into the ordered list and traversed repeatedly, till they overflow to lower negative numbers (and are therefore pushed backward).
  map_traverse (li, print_pi, 0, 0);
  fprintf (stdout, "\n");
  map_traverse (li, MAP_REMOVE_ALL, 0, free);
  map_traverse (li, print_pi, 0, 0);
  fprintf (stdout, "\n");
  map_destroy (li);
}
