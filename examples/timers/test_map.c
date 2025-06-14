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
#define map_size(...)              TRACE_EXPRESSION(map_size (__VA_ARGS__))

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
select_c (const void *data, void *res)
{
  (void) res;
  return (*(const char *) data == 'c');
}

static int
cmpip (const void *p1, const void *p2, void *arg)
{
  (void) arg;
  return *(const int*)p1 < *(const int*)p2 ? -1 : *(const int*)p1 > *(const int*)p2 ? 1 : 0; 
}

static int nop (int i) { return i ; }
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

enum class { NOUN, VERB, ADJECTIVE, ADVERB, PRONOUN, DETERMINER, PREPOSITION, CONJUNCTION, INTERJECTION };
enum gender { MASCULINE, FEMININE, NEUTER, NONE };
struct entry  // The type of the data stored in the map
{
  struct word { char *spelling ; enum class class ; } word;
  enum gender gender ;
  char* definition;
};

static const void*
get_word (const void* data)       // 'data' is supposed to be a pointer to 'struct entry'
{
  return &((const struct entry *)data)->word;  // 'word' is declared as the subset of the 'data' that defines the key of the map.
}

static int
cmp_word (const void *p1, const void *p2, void *arg)
{
  (void) arg;
  const struct word *w1 = p1;
  const struct word *w2 = p2;
  int ret = strcoll (w1->spelling, w2->spelling);
  if (!ret)
    ret = w1->class > w2->class ? 1 : w1->class < w2->class ? -1 : 0;
  return ret;
}

static int
sel_noun_masculine (const void *data, void *context)
{
  (void) context;
  const struct entry *e = data;
  return e->word.class == NOUN && e->gender == MASCULINE;
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
        li = map_create (0, cmpstringp, 0, MAP_UNIQUENESS);       // Set
        break;
      case 2:
        li = map_create (0, cmpstringp, 0, MAP_STABLE);   // Ordered list
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
    if (map_traverse (li, MAP_REMOVE_ONE, 0, &data) && data)        // Remove the first found element from the map.
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

    map_traverse (li, MAP_REMOVE_ONE, 0, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_find_key (li, "b", MAP_REMOVE_ONE, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_find_key (li, "d", MAP_REMOVE_ONE, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_traverse_backward (li, MAP_REMOVE_ONE, 0, 0);
    map_traverse (li, print_data, 0, 0);
    fprintf (stdout, "\n");

    map_traverse (li, MAP_REMOVE_ONE, 0, 0);
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

  puts ("============================================================");
  map* li = map_create (0, cmpip, 0, MAP_STABLE);   // Ordered list
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
  args = (struct rai_args ){ nop, li };
  map_traverse_backward (li, remove_apply_insert, 0, &args);
  map_traverse (li, print_pi, 0, 0);
  fprintf (stdout, "\n");
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

  puts ("============================================================");
  map* dictionary = map_create (get_word, cmp_word, 0, MAP_STABLE);   // Dictionary. A word can have several definitions and therefore appear several times in the map.
  map_insert_data (dictionary, &(struct entry){{"Orange", NOUN}, FEMININE, "Fruit"});
  map_insert_data (dictionary, &(struct entry){{"Abricot", NOUN}, MASCULINE, "Fruit"});
  map_insert_data (dictionary, &(struct entry){{"Orange", NOUN}, MASCULINE, "Colour"});
  map_insert_data (dictionary, &(struct entry){{"Orange", ADJECTIVE}, NONE, "Colour"});
  fprintf (stdout, "%lu element(s).\n", map_size (dictionary));
  fprintf (stdout, "%lu element(s).\n", map_traverse (dictionary, 0, 0, 0));
  fprintf (stdout, "%lu element(s) found.\n", map_traverse (dictionary, 0, sel_noun_masculine, 0));
  fprintf (stdout, "%lu element(s) found.\n", map_find_key (dictionary, &(struct word){"Orange", NOUN}, 0, 0));
  fprintf (stdout, "%lu element(s) found.\n", map_find_key (dictionary, &(struct word){"Orange", ADJECTIVE}, 0, 0));
  fprintf (stdout, "%lu element(s) found.\n", map_find_key (dictionary, &(struct word){"Orange", VERB}, 0, 0));
  map_traverse (dictionary, MAP_REMOVE_ALL, 0, 0);
  map_destroy (dictionary);
}
