// # Map me !
// **A unprecedented MT-safe implementation of a map library that can manage maps, sets, ordered and unordered lists that can do it all with a minimalist interface.**
// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#ifndef  __MAP_H__
#  define __MAP_H__
typedef struct map map;         // A map (internally modeled as a sorted binary tree).

// ## Creates a new map.
// The type of the user-defined function that should return a pointer to the the part of `data` that contains the key of the map.
typedef const void *(*map_key_extractor) (void *data);

// The type of the user-defined function that compares two keys of elements of a map.
// A comparison function must return an integer less than, equal to, or greater than zero
// if the first argument is considered to be respectively less than, equal to, or greater than the second.
typedef int (*map_key_comparator) (const void *key_a, const void *key_b, void *arg);

map *map_create (map_key_extractor get_key, map_key_comparator cmp_key, void *arg, int property);
// Returns `0` if the map could not be allocated (and `errno` set to `ENOMEM`).
// Otherwise, returns a pointer to the created map.
// If not `0`, the comparison function `cmp_key` must return an integer less than, equal to, or greater than zero
// if the first argument is considered to be respectively less than, equal to, or greater than the second.
// `cmp_key` is applied on `get_key (data)`.
// `get_key` and `cmp_key` should be either both non-null or both null.
// A pointer is passed to the comparison function `cmp_key` (as third argument) via `arg` (`arg` can be `0`).

// `property` is one of the values below and dictates the behavior in case two data with equal key are inserted.
// `property` is `MAP_NONE` (or `0`), `MAP_UNIQUENESS` or `MAP_STABLE`.
// Elements are unique in the map if and only if `property` is equal to `MAP_UNIQUENESS`.
// Equal elements are ordered in the order they were inserted if and only if `property` is equal to `MAP_STABLE`.
extern int MAP_UNIQUENESS;      // The second data is not inserted.
extern int MAP_STABLE;          // The second data is inserted AFTER the first data with the identical key.
extern int MAP_NONE;            // The second data is inserted either (randomly) before or after the first data with the identical key (keeps the binary tree more balanced).

extern map_key_extractor MAP_KEY_IS_DATA;       // Helper function to be used as a key extractor for sets and ordered lists (see below).

/* 7 possible uses:

| Use            | Property             | `get_key`         | Comment                                                                                                                    |
| -              | -                    | -                 | -                                                                                                                          |
| Ordered map    | `MAP_UNIQUENESS`     | Non-zero          | Each key is unique in the map. `get_key` and `cmp_key` must be set.                                                        |
| Dictionary     | not `MAP_UNIQUENESS` | Non-zero          | Keys can have multiple entries in the map. `get_key` and `cmp_key` must be set.                                            |
| Set            | `MAP_UNIQUENESS`     | `MAP_KEY_IS_DATA` | Elements are unique. `cmp_key` must be set.                                                                                |
| Ordered list   | `MAP_STABLE`         | `MAP_KEY_IS_DATA` | Equal elements are ordered in the order they were inserted. `cmp_key` must be set.                                         |
| Unordered list | `MAP_NONE`           | `0`               | `cmp_key` must be 0 as well.                                                                                               |
| FIFO           | `MAP_STABLE`         | `0`               | Elements are appended after the last element. Use `map_traverse (m, MAP_REMOVE, sel, &data)` to remove an element.         |
| LIFO           | `MAP_STABLE`         | `0`               | Elements are appended after the last element. Use `map_traverse_backward (m, MAP_REMOVE, sel, &data)` to remove an element.|

(*) If `cmp_key` or `get_key` is `0` and property is `MAP_STABLE`, complexity is reduced by a factor log n.

*/

// ## Destroy a map.
int map_destroy (map *);
// Destroys an EMPTY and previously created map. Returns `EXIT_FAILURE` (and `errno` set to `EPERM`) if the map is not empty (and the map is NOT destroyed), `EXIT_SUCCESS` otherwise.

// ## Retrieve the number of elements in a map.
size_t map_size (map *);
// Returns the number of elements in a map.
// Note: if the map is used by several threads, `map_size` should better not be used since the size of the map can be modified any time.
// Complexity : 1. MT-safe.

// ## Add an element into a map.
int map_insert_data (map *, void *data);
// Adds a previously allocated data into map and returns `1` if the element was added, `0` otherwise.
// Complexity : log n (see (*) above). MT-safe. Non-recursive.

// ## Retrieve and remove elements from the map.

// Note: `map_find_key`, `map_traverse`, `map_traverse_backward` and `map_insert_data` can call each other (the map should be passed in the `context` argument though).

// ### Signature of a user-defined selector on elements of the map.
typedef int (*map_selector) (void *data, void *context);
// The data of the element of the map is passed as the first argument of the map_selector.
// The second argument `context` is the pointer passed to `map_traverse` and `map_traverse_backward` (as last argument).
// Should return `1` if the `data` conforms to the user-defined conditions, `0` otherwise.

// ### Signature of a user-defined operator on elements of the map.
typedef int (*map_operator) (void *data, void *context, int *remove);
// The data of the element of the map is passed as the first argument of the `map_operator`.
// The second argument `context` is the pointer passed to `map_traverse`, `map_traverse_backward` and `map_find_key` (as last argument).
// The third argument `remove` is a non-null pointer. If (and only if) the operator sets `*remove` to a non-zero value,
//   - the element will be removed from the map thread-safely ;
//   - the operator SHOULD free the data passed to it if it was allocated dynamically (otherwise it would be lost).
// Should return `1` if the operator should be applied on other elements of the map, `0` otherwise.

// ### Find an element from its key.
size_t map_find_key (struct map *l, const void *key, map_operator op, void *context);
// If `get_key` and `cmp_key` are not null, applies `operator` on the data of the elements in the map that matches the key (for which `cmp_key` returns `0`), as long as `op` returns non-zero.
// `context` is passed as the second argument of operator `op`.
// Returns the number of elements on which the operator `op` has been applied.
// Complexity : log n (see (*)). MT-safe. Non-recursive.

// ### Traverse a map.
size_t map_traverse (map *, map_operator op, map_selector sel, void *context);
size_t map_traverse_backward (map *, map_operator op, map_selector sel, void *context);
// Applies the operator `op` on all the data stored in the map as long as the operator `op` returns non-zero, from the first element to the last or the other way round.
// If `sel` is not null, `op` is applied only to `data` for which the selector `sel` returns non-zero. `map_traverse` then behaves as if the operator `op` would start with: `if (!sel (data, context)) return 1;`.
// `context` is passed as the second argument of operator `op` and selector `sel`.
// Returns the number of elements of the map on which the operator `op` has been applied.
// Complexity : n * log n (see (*)). MT-safe. Non-recursive.

// ### Helper map operator to retrieve an element.
extern map_operator MAP_REMOVE;
// If the parameter `context` of `map_find_key`, `map_traverse` or `map_traverse_backward` is a pointer,
// the helper operator `MAP_REMOVE` removes and retrieves the first element found by `map_find_key`, `map_traverse` or `map_traverse_backward`
// and sets the pointer `context` to the data of this element. Otherwise `context` is left unchanged.
// `context` SHOULD BE the address of a pointer to type T, where `context` is the argument passed to `map_find_key`, `map_traverse` or `map_traverse_backward`.
/* Example
If `m` is a map of elements of type T and `sel` a map_selector, the following piece of code will remove and retrieve the data of the first element selected by `sel`:

  T \*data = 0;  // `data` is a *pointer* to the type stored in the map.
  if (map_traverse (m, MAP_REMOVE, sel, &data))  // A *pointer to the pointer* `data` is passed to map_traverse.
  {

    // `data` can thread-safely be used to work with.
    ...
    // If needed, it can be reinserted in the map after use.
    map_insert_data (m, data);

  }
*/
#endif
