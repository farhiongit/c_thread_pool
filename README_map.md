# Map me !
**A unprecedented MT-safe implementation of a map library that can manage maps, sets, ordered and unordered lists that can do it all with a minimalist interface.**

(c) L. Farhi, 2024.


Language: C (C11 or higher).


The interface has only 7 functions to do everything:

- `map_create`
- `map_destroy`
- `map_size` (MT-safe)
- `map_insert_data` (MT-safe)
- `map_find_key` (MT-safe)
- `map_traverse` (MT-safe)
- `map_traverse_backward` (MT-safe)

## Type definitions

| Define | Value |
| - | - |
| \_\_MAP\_H\_\_ |

### Map
A map as an opaque Abstract Data Type (internally modelled as a sorted binary tree):

| Type definition |
| - |
| struct map map |

The map stores pointers to allocated data:

	  void *data;
	
### Key
The key of the map is extracted from the data stored in it (generally but not necessarily a subset of it). A user-defined function of type `map_key_extractor` (passed to `map_create`) can be used to extract this subset.


`map_key_extractor` is the type of the user-defined function that should return a pointer to the the part of `data` that contains the key of the map.



| Type definition |
| - |
| const void \*(\*map\_key\_extractor) (void \*data) |

> Functions of type `map_key_extractor` should not allocate memory dynamically.


Example:

	  enum class { NOUN, VERB, ADJECTIVE, ADVERB, PRONOUN, DETERMINER, PREPOSITION, CONJUNCTION, INTERJECTION };
	  enum gender { MASCULINE, FEMININE, NEUTER };
	  struct entry  // The type of the data stored in the map
	  {
	    struct word { char *spelling ; enum class class ; } word;
	    enum gender gender ;
	    char* definition;
	  };

	  static const void* get_word (void* data)       // 'data' is supposed to be a pointer to 'struct entry'
	  {
	    return &((struct entry *)data)->word;  // 'word' is declared as the subset of the 'data' that defines the key of the map.
	  }


### Key comparator
The type of a user-defined function that compares two keys of elements of a map.



| Type definition |
| - |
| int (\*map\_key\_comparator) (const void \*key\_a, const void \*key\_b, void \*arg) |

`key_a` and `key_b` are pointers to keys, as they would be returned by a function of type `map_key_extractor`.


A comparison function must return an integer less than, equal to, or greater than zero if the first argument is considered to be respectively less than, equal to, or greater than the second.


The third argument `arg` receives the pointer that was passed to `map_create`.


Example:

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


### Selector on elements of the map
The type of a user-defined function that selects elements while traversing a map with `map_traverse` or `map_traverse_backward`.



| Type definition |
| - |
| int (\*map\_selector) (void \*data, void \*context) |

The data of the element of the map is passed as the first argument of the map_selector.


The second argument `context` receives the pointer passed to `map_traverse` and `map_traverse_backward` (as last argument).


Should return `1` if the `data` conforms to the user-defined conditions (and should be selected by `map_traverse` or `map_traverse_backward`), `0` otherwise.


### Operator on elements of the map
The type of a user-defined function that operates on (and optionally removes) an element of a map picked by `map_traverse`, `map_traverse_backward` or `map_find_key`.



| Type definition |
| - |
| int (\*map\_operator) (void \*data, void \*context, int \*remove) |

The data of the element of the map is passed as the first argument of the `map_operator`.


The second argument `context` receives the pointer passed to `map_traverse`, `map_traverse_backward` and `map_find_key` (as last argument).


The third argument `remove` receives a non-null pointer for which `*remove` is set to `0`.


If (and only if) the operator sets `*remove` to a non-zero value,

  - the element will be removed from the map thread-safely ;
  - the operator **should** keep track and ultimately free the data passed to it if it was allocated dynamically (otherwise data would be lost in memory leaks).


The `map_operator` should return `1` if the operator should be applied on further elements of the map, `0` otherwise.


In other words, as soon as the operator returns `0`, it stops `map_traverse`, `map_traverse_backward` or `map_find_key`.


## Interface
### Create a map
```c
map *map_create (map_key_extractor get_key, map_key_comparator cmp_key, void *arg, int property);
```
Returns `0` if the map could not be allocated (and `errno` set to `ENOMEM`).


Otherwise, returns a pointer to the created map.


If not `0`, the comparison function `cmp_key` must return an integer less than, equal to, or greater than zero
if the first argument is considered to be respectively less than, equal to, or greater than the second.


`cmp_key` is applied on `get_key (data)` if `get_key` is not `0` ; otherwise, `cmp_key` is applied on `data` (where `data` is a pointer inserted by `map_insert_data`).


`cmp_key` must be set if `get_key` is set.


The pointer `arg` (which can be `0`) is passed to the comparison function `cmp_key` (as third argument).


`property` is one of the values below and dictates the behaviour in case two data with equal key are inserted.


`property` is `MAP_NONE` (or `0`), `MAP_UNIQUENESS` or `MAP_STABLE`.


Elements are unique in the map if and only if `property` is equal to `MAP_UNIQUENESS`.


Equal elements are ordered in the order they were inserted if `property` is equal to `MAP_STABLE`.


The second data is not inserted (uniqueness).


```c
extern int MAP_UNIQUENESS;      
```
The second data is inserted **after** the first data with the identical key (stability).


```c
extern int MAP_STABLE;          
```
The second data is inserted either (randomly) before or after the first data with the identical key (keeps the binary tree more balanced).


```c
extern int MAP_NONE;            
```
7 possible uses, depending on `property`, `cmp_key` and `get_key`:

| Use            | `property`           | `cmp_key` | `get_key` | Comment                                                                                                                       |
| -              | -                    | -         | -         | -                                                                                                                             |
| Ordered map    | `MAP_UNIQUENESS`     | Non-zero  | Non-zero  | Each key is unique in the map.                                                                                                |
| Dictionary     | not `MAP_UNIQUENESS` | Non-zero  | Non-zero  | Keys can have multiple entries in the map.                                                                                    |
| Set            | `MAP_UNIQUENESS`     | Non-zero  | `0`       | Elements are unique. `cmp_key` applies to inserted `data` (the `data` is the key).                                            |
| Ordered list   | `MAP_STABLE`         | Non-zero  | `0`       | Equal elements are ordered in the order they were inserted. `cmp_key` applies to inserted `data` (the `data` is the key).     |
| Unordered list | not `MAP_UNIQUENESS` | `0`       | `0`       |                                                                                                                               |
| FIFO           | `MAP_STABLE`         | `0`       | `0`       | Elements are appended after the last element. Use `map_traverse (m, MAP_REMOVE_ONE, 0, &data)` to remove an element.          |
| LIFO           | `MAP_STABLE`         | `0`       | `0`       | Elements are appended after the last element. Use `map_traverse_backward (m, MAP_REMOVE_ONE, 0, &data)` to remove an element. |

> (*) If `cmp_key` or `get_key` is `0` and property is `MAP_STABLE`, complexity is reduced by a factor log n.




### Destroy a map
```c
int map_destroy (map *);
```
Destroys an **empty** and previously created map.


If the map is not empty, the map is not destroyed.


Returns `EXIT_FAILURE` (and `errno` set to `EPERM`) if the map is not empty (and the map is NOT destroyed), `EXIT_SUCCESS` otherwise.


### Retrieve the number of elements in a map
```c
size_t map_size (map *);
```
Returns the number of elements in a map.


Note: if the map is used by several threads, `map_size` should better not be used since the size of the map can be modified any time by other threads.


Complexity : 1. MT-safe.


### Add an element into a map
```c
int map_insert_data (map *, void *data);
```
Adds a previously allocated data into map and returns `1` if the element was added, `0` otherwise.


Complexity : log n (see (*) above). MT-safe. Non-recursive.


### Retrieve and remove elements from a map
#### Find an element from its key
```c
size_t map_find_key (struct map *map, const void *key, map_operator op, void *context);
```
If `get_key` is not null, applies `op` on the data of the elements in the map that matches the key (for which `cmp_key (get_key (data))` returns `0`), as long as `op` returns non-zero.


If `get_key` is null, applies `operator` on the data of the elements in the map that matches the data (for which `cmp_key (data)` returns `0`), as long as `op` returns non-zero.


If `op` is null, all the matching elements are found (and counted).


> `cmp_key` should have been previously set by `map_create`.


`context` is passed as the second argument of operator `op`.


Returns the number of elements on which the operator `op` has been applied.


> If `op` is null, `map_find_key` simply counts and returns the number of matching elements with the `key`.


Complexity : log n (see (*)). MT-safe. Non-recursive.


> `map_find_key`, `map_traverse`, `map_traverse_backward` and `map_insert_data` can call each other *in the same thread* (the first argument `map` can be passed again through the `context` argument).


> Therefore, elements can be removed from (when `*remove` is set to `1` in `op`) or inserted into (when `map_insert_data` is called in `op`) the map *by the same thread* while finding elements.


#### Traverse a map
```c
size_t map_traverse (map *map, map_operator op, map_selector sel, void *context);
```
```c
size_t map_traverse_backward (map *map, map_operator op, map_selector sel, void *context);
```
Traverse the elements of the map.


If the operator `op` is not null, it is applied on the data stored in the map, from the first element to the last (resp. the other way round), as long as the operator `op` returns non-zero.


If `op` is null, all the elements are traversed.


If the selector `sel` is not null, elements for which `sel (data)` (where `data` is an element previously inserted into the map) returns `0` are ignored. `map_traverse` (resp.`map_traverse_backward`) behaves as if the operator `op` would start with: `if (!sel (data, context)) return 1;`.


`context` is passed as the second argument of operator `op` and selector `sel`.


Returns the number of elements of the map that match `sel` (if set) and on which the operator `op` (if set) has been applied.


> If `op` is null, `map_traverse` and `map_traverse_backward` simply count and return the number of matching elements with the selector `sel` (if set).


> If `op` and `sel `are null, `map_traverse` and `map_traverse_backward` simply count and return the number of elements.


Complexity : n * log n (see (*)). MT-safe. Non-recursive.


> `map_find_key`, `map_traverse`, `map_traverse_backward` and `map_insert_data` can call each other *in the same thread* (the first argument `map` can be passed again through the `context` argument).


> Therefore, elements can be removed from (when `*remove` is set to `1` in `op`) or inserted into (when `map_insert_data` is called in `op`) the map *by the same thread* while traversing elements.


> Insertion while traversing should be done with care since an infinite loop will occur if, in `op`, an element is removed and :
>
>  - while traversing forward, at least an equal or greater element is inserted ;
>  - while traversing backward, at least a lower element is inserted.


### Predefined operators for use with `map_find_key`, `map_traverse` and `map_traverse_backward`.


`map_operator` functions passed to `map_find_key`, `map_traverse` and `map_traverse_backward` should be user-defined.


But useful operators are provided below.


#### Map operator to retrieve and remove one element
This map operator simply retrieves and removes one element from the map.


```c
extern map_operator MAP_REMOVE_ONE;
```
> Its use is **not recommended** though. Actions on an element should better be directly integrated in the `map_operator` function.


The helper operator `MAP_REMOVE_ONE` removes and retrieves an element found by `map_find_key`, `map_traverse` or `map_traverse_backward`
and, if the parameter `context` of `map_find_key`, `map_traverse` or `map_traverse_backward` is a non null pointer,
it sets the pointer `context` to the data of this element.


`context` **should be** `0` or the address of a pointer to type T, where `context` is the argument passed to `map_find_key`, `map_traverse` or `map_traverse_backward`.


Example

If `m` is a map of elements of type T and `sel` a map_selector, the following piece of code will remove and retrieve the data of the first element selected by `sel`:

	  T *data = 0;  // `data` is a *pointer* to the type stored in the map.
	  if (map_traverse (m, MAP_REMOVE_ONE, sel, &data) && data)  // A *pointer to the pointer* `data` is passed to map_traverse.
	  {
	    // `data` can thread-safely be used to work with.
	    ...
	    // If needed, it can be reinserted in the map after use.
	    map_insert_data (m, data);
	  }
	
#### Map operator to remove all elements
This map operator removes all the element from the map.


```c
extern map_operator MAP_REMOVE_ALL;
```
the parameter `context` of `map_find_key`, `map_traverse` or `map_traverse_backward` should be `0` or a pointer to a destructor function with signature `void (*)(void * ptr)` (such as `free`).


This destructor is applied to each element selected by `map_find_key`, `map_traverse` or `map_traverse_backward`.


#### Map operator to move elements from one map to another
This map operator moves each element selected by `map_find_key`, `map_traverse` or `map_traverse_backward` to another **different** map passed in the argument `context` of `map_find_key`, `map_traverse` or `map_traverse_backward`.


N.B.: A destination map identical to the source map would **deadly lock** the calling thread.


```c
extern map_operator MAP_MOVE_TO;
```

-----
*This page was generated automatically from `map.h` by `h2md`.*
