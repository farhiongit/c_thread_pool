// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#ifndef  __MAP_H__
#  define __MAP_H__
typedef struct map map;
typedef struct map_elem map_elem;

// Creates a new map.
// 'property' is MAP_NONE (or 0), MAP_UNIQUENESS or MAP_STABLE.
// Returns 0 if the map could not be allocated (and errno set to ENOMEM).
// Otherwise, returns a pointer to the created map.
// If not 0, the comparison function 'cmp_key' must return an integer less than, equal to, or greater than zero.
// if the first argument is considered to be respectively less than, equal to, or greater than the second. 'cmp_key' is applied on 'get_key (data)'.
// A pointer is passed (as third argument) to the comparison function 'cmp_key' via 'arg' (arg can be 0).
map *map_create (const void *(*get_key) (void *data), int (*cmp_key) (const void *key_a, const void *key_b, void *arg), void *arg, int property);

// 'property' is one of the values below and dictates the behavior in case two data with equal key are inserted.
// Elements are unique in the map if and only if 'property' is equal to MAP_UNIQUENESS.
// Equal elements are ordered in the order they were inserted if and only if 'property' is equal to MAP_STABLE.
extern int MAP_UNIQUENESS;      // The second data is not inserted.
extern int MAP_STABLE;          // The second data is inserted AFTER the first data with the identical key.
extern int MAP_NONE;            // The second data is inserted either (randomly) before or after the first data with the identical key (keeps the binary tree more balanced).

const void *MAP_KEY_IS_DATA (void *);

/* 6 possible uses:

| Use            | Property       | 'get_key'       | Comment                                                                                              |
| -              | -              | -               | -                                                                                                    |
| Map            | MAP_UNIQUENESS | Non-zero        | Elements are unique. 'get_key' and 'cmp_key' must be set.                                            |
| Set            | MAP_UNIQUENESS | MAP_KEY_IS_DATA | Elements are unique. 'get_key' and 'cmp_key' must be set.                                            |
| Ordered list   | MAP_STABLE     | MAP_KEY_IS_DATA | Equal elements are ordered in the order they were inserted. 'cmp_key' must be set.                   |
| Unordered list | MAP_NONE       | 0               | 'cmp_key' is ignored.                                                                                |
| FIFO           | MAP_STABLE     | 0               | Elements are appended after the last element. Use map_remove (map_first (map *)) to remove elements. |
| LIFO           | MAP_STABLE     | 0               | Elements are appended after the last element. Use map_remove (map_last (map *)) to remove elements.  |

(*) If cmp_key or get_key is 0 and property is MAP_STABLE, complexity is reduced by a factor log n.

*/

// Destroys an EMPTY and previously created map. Returns EXIT_FAILURE (and errno set to EPERM) if the map is not empty (and the map is NOT destroyed), EXIT_SUCCESS otherwise.
int map_destroy (map *);

// Adds a previously allocated data into map and returns 1 if the element was added, 0 otherwise.
// Complexity : log n (see (*) above) -- MT-safe
int map_insert_data (map *, void *data);

// The data of the element of the map is passed as the first argument of the map_operator.
// The second argument 'context' is the pointer passed to map_traverse, map_traverse_backward and map_find_key.
// The third argument 'remove' is a non-null pointer. If (and only if) the operator sets '*remove' to a non-zero value,
//   - the element will be removed from the map thread-safely ;
//   - the operator SHOULD free the data passed to it if it was allocated dynamically (otherwise it would be lost).
typedef int (*map_operator) (void *data, void *context, int *remove);

// If 'get_key' and 'cmp_key' are not null, applies 'operator' on the data of the elements in the map that matches the key (for which 'cmp_key' returns 0), as long as 'op' returns non-zero.
// 'context' is passed as the second argument of operator 'op'.
// Complexity : log n (see (*)) -- MT-safe
void map_find_key (struct map *l, const void *key, map_operator op, void *context);

// Applies 'operator' on all the data stored in the map as long as 'op' returns non-zero, from the first element to the last or the other way round.
// 'context' is passed as the second argument of operator 'op'.
// Complexity : n * log n (see (*)) -- MT-safe
void map_traverse (map *, map_operator op, void *context);
void map_traverse_backward (map *, map_operator op, void *context);
#endif
