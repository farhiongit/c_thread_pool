// (c) L. Farhi, 2024
// Language: C (C11 or higher)
#ifndef  __MAP_H__
#  define __MAP_H__
typedef struct smap map;
typedef struct map_elem map_elem;

// Behavior in case two data with equal key are inserted
extern int MAP_UNIQUENESS;  // The second data is not inserted.
extern int MAP_STABLE;      // The second data is inserted AFTER the first data with the identical key (first added, first removed).
extern int MAP_NONE;        // The second data is inserted either (randomly) before or after the first data with the identical key (keeps the binary tree more balanced).

// Creates a new map with a mandatory comparator cmp.
// property is MAP_NONE, MAP_UNIQUENESS or MAP_STABLE.
// Elements are unique in the map if and only if property is equal to MAP_UNIQUENESS.
// Otherwise, equal elements are ordered in the order they were inserted if and only if property is equal to MAP_STABLE (first added, first removed).
// Returns 0 if mandatory get_key is null (and errno set to EINVAL) or the map could not be allocated (and errno set to ENOMEM).
// The comparison function cmp_key must return an integer less than, equal to, or greater than zero.
// if the first argument is considered to be respectively less than, equal to, or greater than the second. cmp_key is applied on get_key (void *data).
// A pointer is passed (as third argument) to the comparison function cmp_key via arg (arg can be 0).
map *map_create (void *(*get_key) (void *data), int (*cmp_key) (void *key_a, void *key_b, void *arg), void *arg, int property);

// Destroys an EMPTY and previously created map. Returns EXIT_FAILURE (and errno set to EPERM) if the map is not empty (and the map is NOT destroyed), EXIT_SUCCESS otherwise.
int map_destroy (map * l);

// Adds a previously allocated data into map and returns the added element.
map_elem *map_insert_data (map * l, void *data);

// Returns the first element of the map matching key (for which cmp_key returns 0).
map_elem *map_find_key (map * l, void *key);

// Removes an element elem from the map and returns a pointer to the data that was stored in the removed element.
void *map_remove (map_elem * e);

// Returns the first (smallest with respect to cmp_key) element of the map.
map_elem *map_first (map * l);
// Returns the last (greatest with respect to cmp_key) element of the map.
map_elem *map_last (map * l);

// Returns the element preceding (with respect to cmp_key) the element elem (or 0 if none).
map_elem *map_previous (map_elem * e);
// Returns the element following (with respect to cmp_key) the element elem (or 0 if none).
map_elem *map_next (map_elem * e);

// Returns a pointer to the data that was stored in the element.
void *map_get_data (map_elem * e);
#endif
