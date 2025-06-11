/********************** # Map me ! *********************/
// **A unprecedented MT-safe implementation of a map library that can manage maps, sets, ordered and unordered lists that can do it all with a minimalist interface.**
//
// (c) L. Farhi, 2024.
// Language: C (C11 or higher).

/* The interface has only 7 functions to do everything:

- `map_create`
- `map_destroy`
- `map_size` (MT-safe)
- `map_insert_data` (MT-safe)
- `map_find_key` (MT-safe)
- `map_traverse` (MT-safe)
- `map_traverse_backward` (MT-safe)
*/

// ## Type definitions
#ifndef  __MAP_H__
#  define __MAP_H__

// ### Map
// A map as an opaque Abstract Data Type (internally modelled as a sorted binary tree):
typedef struct map map;
/* The map stores pointers to allocated data:

  void *data;
*/

// ### Key
// The type of the user-defined function that should return a pointer to the the part of `data` that contains the key of the map.
typedef const void *(*map_key_extractor) (void *data);
// Functions of type `map_key_extractor` should not allocate memory dynamically though.
/* Example:

  struct entry
  {
    char* word;
    char* definition;
  }

  const void* get_word (void* data)
  {
    return ((struct entry *)data)->word;
  }

*/

// ### Key comparator
// The type of a user-defined function that compares two keys of elements of a map.
typedef int (*map_key_comparator) (const void *key_a, const void *key_b, void *arg);
// `key_a` and `key_b` are pointers to keys, as they would be returned by a function of type `map_key_extractor`.
// A comparison function must return an integer less than, equal to, or greater than zero if the first argument is considered to be respectively less than, equal to, or greater than the second.
// The third argument `arg` receives the pointer that was passed to `map_create`.

// ### Selector on elements of the map
// The type of a user-defined function that selects elements while traversing a map with `map_traverse` or `map_traverse_backward`. 
typedef int (*map_selector) (void *data, void *context);
// The data of the element of the map is passed as the first argument of the map_selector.
// The second argument `context` receives the pointer passed to `map_traverse` and `map_traverse_backward` (as last argument).
// Should return `1` if the `data` conforms to the user-defined conditions (and should be selected by `map_traverse` or `map_traverse_backward`), `0` otherwise.

// ### Operator on elements of the map
// The type of a user-defined function that operates on (and optionally removes) an element of a map picked by `map_traverse`, `map_traverse_backward` or `map_find_key`.
typedef int (*map_operator) (void *data, void *context, int *remove);
// The data of the element of the map is passed as the first argument of the `map_operator`.
// The second argument `context` receives the pointer passed to `map_traverse`, `map_traverse_backward` and `map_find_key` (as last argument).
// The third argument `remove` receives a non-null pointer for which `*remove` is set to `0`.
// If (and only if) the operator sets `*remove` to a non-zero value,
//
//   - the element will be removed from the map thread-safely ;
//   - the operator **should** keep track and ultimately free the data passed to it if it was allocated dynamically (otherwise data would be lost in memory leaks).
// The `map_operator` should return `1` if the operator should be applied on further elements of the map, `0` otherwise.
// In other words, as soon as the operator returns `0`, it stops `map_traverse`, `map_traverse_backward` or `map_find_key`. 

// #### Helper map operator to retrieve and remove one element
// This map operator simply retrieves and removes one element from the map.
extern map_operator MAP_REMOVE;
// > Its use is **not recommended** though. Actions on an element should better be directly integrated in the `map_operator` function.
// The helper operator `MAP_REMOVE` removes and retrieves an element found by `map_find_key`, `map_traverse` or `map_traverse_backward`
// and, if the parameter `context` of `map_find_key`, `map_traverse` or `map_traverse_backward` is a non null pointer,
// it sets the pointer `context` to the data of this element.
// `context` **should be** `0` or the address of a pointer to type T, where `context` is the argument passed to `map_find_key`, `map_traverse` or `map_traverse_backward`.
/* Example

If `m` is a map of elements of type T and `sel` a map_selector, the following piece of code will remove and retrieve the data of the first element selected by `sel`:

  T *data = 0;  // `data` is a *pointer* to the type stored in the map.
  if (map_traverse (m, MAP_REMOVE, sel, &data) && data)  // A *pointer to the pointer* `data` is passed to map_traverse.
  {
    // `data` can thread-safely be used to work with.
    ...
    // If needed, it can be reinserted in the map after use.
    map_insert_data (m, data);
  }
*/

// #### Helper map operator to remove all elements
// This map operator removes all the element from the map.
extern map_operator MAP_REMOVE_ALL;
// the parameter `context` of `map_find_key`, `map_traverse` or `map_traverse_backward` should be `0` or a pointer to a destructor function with signature void (*)(void * ptr).
// This destructor is applied to each element of the map.

// #### Helper map operator to move elements from one map to another
// This map operator moves an element of the map to another **different** map passed in the argument `context` of `map_find_key`, `map_traverse` or `map_traverse_backward`.
// N.B.: A destination map identical to the source map would **deadly lock** the calling thread.
extern map_operator MAP_MOVE_TO;

// ## Interface

// ### Create a map
map *map_create (map_key_extractor get_key, map_key_comparator cmp_key, void *arg, int property);
// Returns `0` if the map could not be allocated (and `errno` set to `ENOMEM`).
// Otherwise, returns a pointer to the created map.
// If not `0`, the comparison function `cmp_key` must return an integer less than, equal to, or greater than zero
// if the first argument is considered to be respectively less than, equal to, or greater than the second.
// `cmp_key` is applied on `get_key (data)`.
// `get_key` and `cmp_key` should be either both non-null or both null.
// The pointer `arg` (which can be `0`) is passed to the comparison function `cmp_key` (as third argument).

// `property` is one of the values below and dictates the behavior in case two data with equal key are inserted.
// `property` is `MAP_NONE` (or `0`), `MAP_UNIQUENESS` or `MAP_STABLE`.
// Elements are unique in the map if and only if `property` is equal to `MAP_UNIQUENESS`.
// Equal elements are ordered in the order they were inserted if and only if `property` is equal to `MAP_STABLE`.
extern int MAP_UNIQUENESS;      // The second data is not inserted.
extern int MAP_STABLE;          // The second data is inserted **after** the first data with the identical key.
extern int MAP_NONE;            // The second data is inserted either (randomly) before or after the first data with the identical key (keeps the binary tree more balanced).

extern map_key_extractor MAP_KEY_IS_DATA;       // Helper function to be used as a key extractor for sets and ordered lists (see below).

/* 7 possible uses, depending on `property` and `get_key`:

| Use            | `property`           | `get_key`         | Comment                                                                                                                    |
| -              | -                    | -                 | -                                                                                                                          |
| Ordered map    | `MAP_UNIQUENESS`     | Non-zero          | Each key is unique in the map. `get_key` and `cmp_key` must be set.                                                        |
| Dictionary     | not `MAP_UNIQUENESS` | Non-zero          | Keys can have multiple entries in the map. `get_key` and `cmp_key` must be set.                                            |
| Set            | `MAP_UNIQUENESS`     | `MAP_KEY_IS_DATA` | Elements are unique. `cmp_key` must be set.                                                                                |
| Ordered list   | `MAP_STABLE`         | `MAP_KEY_IS_DATA` | Equal elements are ordered in the order they were inserted. `cmp_key` must be set.                                         |
| Unordered list | `MAP_NONE`           | `0`               | `cmp_key` must be `0` as well.                                                                                             |
| FIFO           | `MAP_STABLE`         | `0`               | Elements are appended after the last element. Use `map_traverse (m, MAP_REMOVE, 0, &data)` to remove an element.           |
| LIFO           | `MAP_STABLE`         | `0`               | Elements are appended after the last element. Use `map_traverse_backward (m, MAP_REMOVE, 0, &data)` to remove an element.  |

> (*) If `cmp_key` or `get_key` is `0` and property is `MAP_STABLE`, complexity is reduced by a factor log n.

*/

// ### Destroy a map
int map_destroy (map *);
// Destroys an **empty** and previously created map.
// If the map is not empty, the map is not destroyed.
// Returns `EXIT_FAILURE` (and `errno` set to `EPERM`) if the map is not empty (and the map is NOT destroyed), `EXIT_SUCCESS` otherwise.

// ### Retrieve the number of elements in a map
size_t map_size (map *);
// Returns the number of elements in a map.
// Note: if the map is used by several threads, `map_size` should better not be used since the size of the map can be modified any time by other threads.
// Complexity : 1. MT-safe.

// ### Add an element into a map
int map_insert_data (map *, void *data);
// Adds a previously allocated data into map and returns `1` if the element was added, `0` otherwise.
// Complexity : log n (see (*) above). MT-safe. Non-recursive.

// ### Retrieve and remove elements from a map

// > `map_find_key`, `map_traverse`, `map_traverse_backward` and `map_insert_data` can call each other (the map should be passed in the `context` argument though).

// #### Find an element from its key
size_t map_find_key (struct map *l, const void *key, map_operator op, void *context);
// If `get_key` and `cmp_key` are not null, applies `operator` on the data of the elements in the map that matches the key (for which `cmp_key` returns `0`), as long as `op` returns non-zero.
// Elements can be removed from (when `*remove` is set to `1` in `op`) or inserted into (when `map_insert_data` is called in `op`) the map *by the same thread* while finding elements.
// `context` is passed as the second argument of operator `op`.
// Returns the number of elements on which the operator `op` has been applied.
// Complexity : log n (see (*)). MT-safe. Non-recursive.

// #### Traverse a map
size_t map_traverse (map *, map_operator op, map_selector sel, void *context);
size_t map_traverse_backward (map *, map_operator op, map_selector sel, void *context);
// Applies the operator `op` on all the data stored in the map as long as the operator `op` returns non-zero, from the first element to the last (resp. the other way round).
// Elements can be removed from (when `*remove` is set to `1` in `op`) or inserted into (when `map_insert_data` is called in `op`) the map *by the same thread* while traversing elements.
// If `sel` is not null, `op` is applied only to `data` for which the selector `sel` returns non-zero. `map_traverse` (resp.`map_traverse_backward`) then behaves as if the operator `op` would start with: `if (!sel (data, context)) return 1;`.
// `context` is passed as the second argument of operator `op` and selector `sel`.
// Returns the number of elements of the map on which the operator `op` has been applied.
// Complexity : n * log n (see (*)). MT-safe. Non-recursive.

#endif
