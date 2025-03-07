#include <stdlib.h>
#include <stdio.h>
#include <locale.h>
#include <wchar.h>
#include <wctype.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>
#include "wqm.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define _(s) (s)

// ----- Thread pool 2
struct tp2_local
{
  size_t length;
  unsigned long int *array;
};

static void *
tp2_make_local ()
{
  return calloc (1, sizeof (struct tp2_local)); // 0-length array.
}

static void
tp2_delete_local (void *local_data)
{
  free (((struct tp2_local *) local_data)->array);
  free (local_data);
}

struct tp2_global
{
  const wchar_t *match;
  unsigned long int dmatch;
};

struct tp2_job
{
  struct
  {
    const wchar_t *word;
    const wchar_t *realword;
    const wchar_t *fuzzyword;
  } input;
  struct
  {
    unsigned long int d;
    const wchar_t *match;
  } result;
};

static void
tp2_job_free (void *arg)
{
  struct tp2_global *tp2_global = threadpool_global_data ();
  struct tp2_job *tp2_job = arg;
  if (tp2_job->result.d <= tp2_global->dmatch)  // Aggregation for job results.
  {
    tp2_global->match = tp2_job->result.match;
    tp2_global->dmatch = tp2_job->result.d;
  }
  free (arg);
}

// https://en.wikipedia.org/wiki/Damerau%E2%80%93Levenshtein_distance
static unsigned long int
dld (size_t lwa, const wchar_t *wa, size_t lwb, const wchar_t *wb, int transpose)
{
  struct tp2_local *d = threadpool_worker_local_data ();
  if (d->length < (lwa + 1) * (lwb + 1))
    d->array = realloc (d->array, (d->length = (lwa + 1) * (lwb + 1)) * sizeof (*d->array));
  for (size_t ia = 0; ia <= lwa; ia++)
    d->array[ia * (lwb + 1)] = ia;
  for (size_t ib = 1; ib <= lwb; ib++)
    d->array[ib] = ib;

  static const size_t INSERTION = 2;
  static const size_t DELETION = 4;
  static const size_t MISMATCH = 5;     // A mismatch is less than a deletion followed by an insertion
  static const size_t TRANSPOSITION = 1;
  for (size_t ia = 1; ia <= lwa; ia++)
    for (size_t ib = 1; ib <= lwb; ib++)
    {
      unsigned int cost = (wa[ia - 1] != wb[ib - 1] ? 1 : 0);
      // Levenshtein distance
      d->array[ia * (lwb + 1) + ib] =
        MIN (MIN (d->array[(ia - 1) * (lwb + 1) + ib] + INSERTION, d->array[ia * (lwb + 1) + (ib - 1)] + DELETION), d->array[(ia - 1) * (lwb + 1) + (ib - 1)] + cost * MISMATCH);
      // Damerau–Levenshtein distance (transposition of two adjacent characters)
      if (transpose && ia > 1 && ib > 1 && wa[ia - 2] == wb[ib - 1] && wa[ia - 1] == wb[ib - 2])
        d->array[ia * (lwb + 1) + ib] = MIN (d->array[ia * (lwb + 1) + ib], d->array[(ia - 2) * (lwb + 1) + (ib - 2)] + cost * TRANSPOSITION);
    }
  unsigned long int dld = d->array[(lwa + 1) * (lwb + 1) - 1];
  return dld;
}

static int
tp2_worker (struct threadpool *threadpool, void *arg)
{
  (void) threadpool;
  struct tp2_job *tp2 = arg;
  tp2->result.d = dld (wcslen (tp2->input.word), tp2->input.word, wcslen (tp2->input.fuzzyword), tp2->input.fuzzyword, 1);
  tp2->result.match = tp2->input.realword;
  return EXIT_SUCCESS;
}

// ----- Thread pool 1
struct tp1_global
{
  const char *listofwords;
};

struct tp1_resource
{
  size_t nb_lines;
  wchar_t (*lines)[100];        // Pointer to type const wchar_t[100]
  wchar_t **colllines;
};

static void *
tp1_res_alloc (void *global_data)
{
  static struct tp1_resource res;

  res.lines = 0;
#ifdef COLLATE
  fprintf (stderr, _("Collating with locale %s\n"), setlocale (LC_COLLATE, 0));
  res.colllines = 0;
#endif
  const char *listofwords = ((struct tp1_global *) global_data)->listofwords;
  fprintf (stderr, _("Reading the french dictionnary of words %s...\n"), listofwords);
  res.nb_lines = 0;
  FILE *f;
  assert ((f = fopen (listofwords, "r,ccs=UTF-8")));    // File is encoded in UTF-8.
  for (wchar_t line[100]; fgetws (line, 100, f);)       // Reads a string of at most n-1 wide characters, and adds a terminating null wide character
    res.nb_lines++;
  res.lines = malloc (res.nb_lines * sizeof (*res.lines));
#ifdef COLLATE
  res.colllines = malloc (res.nb_lines * sizeof (*res.colllines));
#endif
  f = freopen (0, "r,ccs=UTF-8", f);    // File is encoded in UTF-8.
  res.nb_lines = 0;
  for (wchar_t line[100]; fgetws (line, 100, f);)       // Reads a string of at most n-1 wide characters, and adds a terminating null wide character
  {
    if (wcslen (line) && line[wcslen (line) - 1] == L'\n')
      line[wcslen (line) - 1] = L'\0';
    res.nb_lines++;
    wcsncpy (res.lines[res.nb_lines - 1], line, wcslen (line) + 1);     // Copies at most n wide characters, including the terminating null wide character.
#ifdef COLLATE
    size_t lcollline = wcsxfrm (0, line, 0);    // Returns the number of bytes required to store the transformed string, excluding the terminating null byte.
    wchar_t *collline = calloc ((lcollline + 1), sizeof (*collline));
    wcsxfrm (collline, line, lcollline);        // Transforms at most n bytes.
    res.colllines[res.nb_lines - 1] = collline; // Copies at most n wide characters, including the terminating null wide character.
#endif
  }
  fclose (f);

  return &res;
}

static void
tp1_res_dealloc (void *p)
{
  struct tp1_resource *res = p;
  free (res->lines);
#ifdef COLLATE
  for (size_t i = 0; i < res->nb_lines; i++)
    free (res->colllines[i]);
  free (res->colllines);
#endif
}

struct tp1_job
{
  struct
  {
    wchar_t *wa;                // fuzzy word
  } input;
  struct
  {
    const wchar_t *match_ref;   // Best match
  } result;
};

static void
tp1_job_free (void *arg)
{
  struct tp1_job *ta = arg;
  if (ta->result.match_ref)
    fprintf (stdout, "\"%1$ls\" => \"%2$ls\"\n", ta->input.wa, ta->result.match_ref);   // Job post-processing.

  free (ta->input.wa);
  free (ta);
}

#undef COLLATE
#define COLLATE

static const wchar_t *
get_match (wchar_t *wa, size_t nb_lines, const wchar_t (*const lines)[100], wchar_t *const *const colllines)
{
  size_t start = ((size_t) rand ()) % nb_lines; // To avoid false-sharing.
  struct tp2_global tp2_global = {.match = 0,.dmatch = ULONG_MAX };
  for (size_t i = 0; !tp2_global.match && i < nb_lines; i++)
    if (!wcscmp (wa, lines[(i + start) % nb_lines]))
      tp2_global.match = lines[(i + start) % nb_lines]; // Perfect match found.

  if (!tp2_global.match)        // Search for an approximate match.
  {
    wchar_t *fuzzyword = wa;

#ifdef COLLATE
    size_t lcollwa = wcsxfrm (0, wa, 0);        // Returns the number of bytes required to store the transformed string, excluding the terminating null byte.
    wchar_t *collwa = calloc ((lcollwa + 1), sizeof (*collwa));
    wcsxfrm (collwa, wa, lcollwa);      // Transforms at most n bytes.
    fuzzyword = collwa;
#endif

    struct threadpool *tp2 = threadpool_create_and_start (NB_CPU, &tp2_global, ALL_TASKS);
    threadpool_set_worker_local_data_manager (tp2, tp2_make_local, tp2_delete_local);
    for (size_t i = 0; tp2_global.dmatch && i < nb_lines; i++)
    {
      const wchar_t *realword = lines[(i + start) % nb_lines];  // To avoid false-sharing.
      const wchar_t *word = realword;
#ifdef COLLATE
      word = colllines[(i + start) % nb_lines]; // To avoid false-sharing.
#endif
      struct tp2_job *job = malloc (sizeof (*job));
      *job = (struct tp2_job)
      {
        {word, realword, fuzzyword},
        {0, 0},
      };
      threadpool_add_task (tp2, tp2_worker, job, tp2_job_free);
    }                           // for (size_t i = 0; dmatch && i < nb_lines; i++)
    threadpool_wait_and_destroy (tp2);
#ifdef COLLATE
    free (collwa);
#endif
  }                             //  if (!match)
  return tp2_global.match;
}

static int
tp1_worker (struct threadpool *threadpool, void *arg)
{
  (void) threadpool;
  struct tp1_job *ta = arg;
  struct tp1_resource *tp1_resource = threadpool_global_resource ();
  ta->result.match_ref = get_match (ta->input.wa, tp1_resource->nb_lines, tp1_resource->lines, tp1_resource->colllines);
  return EXIT_SUCCESS;
}

int
main (int argc, char *argv[])
{
  setlocale (LC_ALL, "fr_FR.UTF-8");    // File listofwords is a list of french words.

  struct tp1_global tp1_global = { "liste.de.mots.francais.frgut.txt" };
  struct threadpool *tp1 = threadpool_create_and_start (SEQUENTIAL, &tp1_global, ALL_TASKS);
  threadpool_set_global_resource_manager (tp1, tp1_res_alloc, tp1_res_dealloc);
  threadpool_set_monitor (tp1, threadpool_monitor_to_terminal, stderr, 0);
  fprintf (stderr, "Searching for matching words...\n");
  for (int iarg = 1; iarg < argc; iarg++)
  {
    const char *a = argv[iarg];
    mbstate_t s = { };
    size_t lwa = mbsrtowcs (0, &a, 0, &s);      // Returns the number of wide characters that make up the converted part of the wide-character string, not including the terminating null wide character.
    if (lwa == (size_t) -1)
      return EXIT_FAILURE;
    wchar_t *wa = calloc ((lwa + 1), sizeof (*wa));
    mbsrtowcs (wa, &a, lwa, &s);        // At most n wide characters are written.
    wa[lwa] = 0;
    for (size_t i = 0; i < wcslen (wa) && i < lwa; i++)
      wa[i] = (wchar_t) towlower ((wint_t) wa[i]);

    struct tp1_job *job = malloc (sizeof (*job));
    *job = (struct tp1_job)
    {
      {wa},
      {0}
    };
    threadpool_add_task (tp1, tp1_worker, job, tp1_job_free);
  }                             // for (int iarg = 1; iarg < argc; iarg++)
  threadpool_wait_and_destroy (tp1);

  fprintf (stderr, "Done.\n");
}
