# Another simple, easy to use, effective but fully featured thread pool API for C language

> This API implements a task parallelization mechanism based on the thread pool pattern.

![Image of the Giola Lagoon in Thassos](ocean-pool-Giola-Lagoon.jpg "A beautiful, fully featured sea pool")

## Usage

The thread pool pattern allows to run parallel tasks easily without the burden of thread management.

The user:

1. declares a thread pool and chooses the maximum number of parallel workers in charge of the execution of tasks (`threadpool_create_and_start ()`) ;
2. submits tasks to the thread pool (`threadpool_add_task ()`) that will be passed to one of available workers and will be executed asynchronously ;
3. waits for all the tasks to be completed (`threadpool_wait_and_destroy ()`).

### Files

| Interface | Implementation |
|- | - |
| `wqm.h` | `wqm.c` |

### Libraries

Create a static and dynamic library using :

```
$ make libs
cc -O -fPIC   -c -o wqm.o wqm.c
ar rcs libwqm.a wqm.o
libwqm.a:wqm.o:0000000000000010 R ALL_TASKS
libwqm.a:wqm.o:0000000000000020 R LAST_TASK
libwqm.a:wqm.o:0000000000000000 R NB_CPU
libwqm.a:wqm.o:0000000000000018 R NEXT_TASK
libwqm.a:wqm.o:0000000000000008 R SEQUENTIAL
libwqm.a:wqm.o:0000000000000eda T threadpool_add_task
libwqm.a:wqm.o:0000000000001405 T threadpool_cancel_task
libwqm.a:wqm.o:0000000000000570 T threadpool_create_and_start
libwqm.a:wqm.o:00000000000013d0 T threadpool_global_data
libwqm.a:wqm.o:00000000000016d6 T threadpool_global_resource
libwqm.a:wqm.o:0000000000000386 T threadpool_monitor_to_terminal
libwqm.a:wqm.o:00000000000015ac T threadpool_set_idle_timeout
libwqm.a:wqm.o:00000000000002d2 T threadpool_set_monitor
libwqm.a:wqm.o:0000000000001641 T threadpool_set_resource_manager
libwqm.a:wqm.o:000000000000121c T threadpool_wait_and_destroy
libwqm.a:wqm.o:0000000000001395 T threadpool_worker_local_data
cc -shared -o libwqm.so wqm.o
```

| Static library | Dynamic library |
|- | - |
| `libwqm.a` | `libwqm.so` |

### Interface

#### Basic functionalities

| Function | Description |
| - | - |
| `threadpool_create_and_start` | Creates and starts a new pool of workers |
| `threadpool_add_task` | Adds a task to the pool of workers |
| `threadpool_wait_and_destroy` | Waits for all the tasks to be done and destroy the pool of workers |

Those features are detailed below.

#### Optional advanced functionalities

| Function | Description |
| - | - |
| `threadpool_cancel_task` | Cancels all pending tasks, the last or next submitted task, or a specific task |
| `threadpool_global_data` | Gives access to the user defined shared global data of the pool of workers |
| `threadpool_worker_local_data` | Gives access to the user defined local data of a worker |
| `threadpool_set_resource_manager` | Defines the resource manager functions |
| `threadpool_global_resource` | Gives access to the global resource of the thread pool |
| `threadpool_set_idle_timeout` | Modifies the idle time out (default is 0.1 s) |
| `threadpool_set_monitor` | Sets a user-defined function to retrieve and display monitoring information |

Those features are detailed below.

## Unique features

This implementation of a thread pool brings unique features, not found anywhere else at the time of writing.

1. It uses the standard (minimalist) C11 thread library <threads.h>, rather the POSIX threads. It can therefore be ported more easily to systems other than unix-like systems.
1. The data passed to tasks (via `threadpool_add_task ()`) can be accessed, retrieved  and released multi-thread-safely after completion of the task (via the user defined function `job_delete ()`), allowing collecting data at task termination.
1. Global data can be defined and accessed (via `threadpool_global_data ()`) by all tasks.
1. Local data can be defined and accessed (via `threadpool_worker_local_data ()`) for each worker of the thread pool.
1. Global resources can be allocated and deallocated for all tasks (via `threadpool_set_resource_manager`).
1. Workers will stay alive for a short idle time, ready to process new submitted tasks, even though `threadpool_wait_and_destroy ()` has already been called and no tasks are available, as long as some other tasks are still being processed and could therefore create new tasks dynamically.
1. The activity of the thread pool can be monitored and displayed by a front-end user defined function (via `threadpool_set_monitor ()`).
1. Pending tasks can be canceled after submission (via `threadpool_cancel_task ()`).

Those features are detailed below.

## Detailed API

The API is defined in file `wqm.h`.

### 1. Create a thread pool

```c
struct threadpool *threadpool_create_and_start (size_t nb_workers,
                                                void *global_data,
                                                void *(*make_local) (void),
                                                void (*delete_local) (void *local_data))
```

A thread pool is declared and started with a call to `threadpool_create_and_start ()`.

The first argument `nb_workers` is the number of required workers, that is the maximum number of tasks that can be executed in parallel by the system.
- `NB_CPU` can be used as first argument to fit to the number of processors currently available in the system (as returned by `get_nprocs ()` with GNU C standard library).
- `SEQUENTIAL` can be used as first argument to create a sequential thread pool: tasks will be executed asynchronously, one at a time, and in the order there were submitted. 

The maximum number of workers can be defined higher than the number of CPUs as workers will be started only when solicited and will be released when unused after an idle time.

###### Options

- The second argument `global_data`, if not null, is a pointer to global data.
  This pointer can next be retrieved by tasks with the function `threadpool_global_data ()`.
- The third argument `make_local`, if not null, is a user defined function that returns a pointer to data for a local usage by a worker.
  This pointer can next be retrieved by tasks with the function `threadpool_worker_local_data ()`.
  `make_local` is called in a multi-thread-safe manner at the initialization of a worker and is passed the pointer to global data.
  `make_local` can therefore safely access (and update) the content of `global_data` if needed.
- The fourth argument `delete_local`, if not null, is a user defined function that is executed to release and destroys the local data used by each worker (passed as an argument) when a worker stops.
  `delete_local` is called in a multi-thread-safe manner at the termination of a worker and is passed the pointer to global data.
  `delete_local` can therefore safely access (and update) the content of `global_data` if needed (to gather results or statistics for instance).

### 2. Submit a task

```c
size_t threadpool_add_task (struct threadpool *threadpool,
                            int (*work) (struct threadpool * threadpool, void *job),
                            void *job,
                            void (*job_delete) (void *job))
```

A task is submitted to the thread pool with a call to `threadpool_add_task ()`.

- The first argument `threadpool` is a thread pool returned by a previous call to `threadpool_create_and_start ()`.
- The second argument `work` is a user defined function to be executed by a worker of the thread pool on `job`.
  This function `work` should return 0 on success, non 0 otherwise.
  `work` should be made thread-safe as several `work` will be running in parallel (that's the whole point of the thread pool).
  This function receives the thread pool `threadpool` and the job `job` as arguments, as they were passed to `threadpool_add_task`.
  Therefore, `work` can itself (multi-thread-safely) call `threadpool_add_task` if needed.
- The third argument `job` is a pointer to the data to be used by the task and that will be processed by `work`.
  It should be fully initialized before it is passed to `threadpool_add_task`.
  It can be allocated automatically, statically or dynamically, as long it survives until the job is done by `work`.

The function `threadpool_add_task` returns a unique id of the submitted task, or 0 on error (with `errno` set to `ENOMEM`).

###### Options

- The fourth argument `job_delete`, if not null, is a user defined function called at termination of the task (after processing or [cancellation](#4-cancel-tasks).)
  This function receives the `job` of the task as an argument.

`job_delete` should be used if the job was allocated dynamically in order to release and destroy the data after `work` is done and avoid memory leaks.

`job_delete` is called in a multi-thread-safe manner and can therefore safely aggregate results to those of previous tasks for instance
(in a map and reduce pattern for instance). See [below](#multi-thread-safe-task-post-processing).

> `job_delete` could as well be called manually (rather than passed as an argument to `threadpool_add_task`) at the very end of `work ()`, but it then would not be executed multi-thread-safely, forbidding any aggregation.

If `job` was allocated with a single `malloc ()` and affiliated functions, `free ()` is a possible choice for `job_delete`.

###### Multi-thread safe task post-processing

Besides simply releasing data `job`, the user-defined `job_delete` can also be used as a callback function, called after the task is done or canceled, to retrieve the used and possibly modified `job` and take actions (displaying, computing, aggregating, ...).

For instance, a user-defined type `job_t` could be declared as a structure containing:

```c
typedef struct {
  struct { ... } input;
  struct { ... } result;
  int done;
} job_t;
```
A `job` of type `job_t` would then be passed to `threadpool_add_task`.

The task post-processing pattern stands in 3 steps :

1. Before `job` is passed to `threadpool_add_task`:
   - `input` data is set ;
   - `done` is set to 0 ;
1. In the user-defined function `work`:
   - the previously set `input` data is used to compute the `result` data of the task ;
   - `done` is set to 1 (and therefore remains to 0 if the task was canceled by `threadpool_cancel_task`) ;
1. In the user-defined function `job_delete`:
   - if `done` is set to 1, an aggregated result of all the tasks can then be multi-thread-safely updated from `result` ;
     `global_data`, passed to `threadpool_create_and_start` and retrievable by `threadpool_global_data`, is a possible choice to hold the aggregated result ;
   - any required deallocation of `job` is done.

See below for the example [fuzzy words](#fuzzy-words) of such a pattern.

### 3. Access to global and local thread data

Global and local data of threads can be retrieved and updated safely in the context of working threads.

The global data of a thread pool (`global_data` as passed to `threadpool_create_and_start`):

- should be fully initialized before `threadpool_create_and_start` is called ;
- should survive after `threadpool_wait_and_destroy` is called ;
- can be accessed inside user-defined functions `make_local`, `delete_local` (as passed to `threadpool_create_and_start`), `work` and `job_delete` (as passed to `threadpool_add_task`), and `deallocator` (as passed to `threadpool_set_resource_manager`) with :

```c
void *threadpool_global_data (void)
```

> `threadpool_global_data` can not be called in the resource `allocator` set by `threadpool_set_resource_manager`.

The local data of a worker (created and returned by `make_local` and destroyed by `delete_local`, as passed to `threadpool_create_and_start`) can be accessed inside user-defined functions `work` and `job_delete` (as passed to `threadpool_add_task`) with :

```c
void *threadpool_worker_local_data (void)
```

> These two functions must be called in the context of a running thread, otherwise they return 0.

### 4. Cancel tasks

```c
size_t threadpool_cancel_task (struct threadpool *threadpool, size_t task_id)
```

Previously submitted and still pending tasks can be canceled.
`task_id` is:

- either a unique id returned by a previous call to `threadpool_add_task` ;
- or `ALL_TASKS` to cancel all still pending tasks ;
- or `NEXT_TASK` to cancel the next still pending submitted task (it can be used several times in a row) ;
- or `LAST_TASK` to cancel the last still pending submitted task (it can be used several times in a row).

Canceled tasks won't be processed, but `job_delete`, as optionally passed to `threadpool_add_task`, will be called though.

The function returns the number of canceled tasks, if any, or 0 if there are not any left pending task to be canceled.

### 5. Wait for all submitted tasks to be completed

```c
void threadpool_wait_and_destroy (struct threadpool *threadpool)
```

The single argument `threadpool` is a thread pool returned by a previous call to `threadpool_create_and_start ()`.

This function declares that all the tasks have been submitted.
It then waits for all the tasks to be completed by workers.

`threadpool` should not be used after a call to `threadpool_wait_and_destroy ()`.

### 6. Monitor the thread pool activity

A monitoring of the thread pool activity can optionally be activated by calling
```c
void threadpool_set_monitor (struct threadpool *threadpool, threadpool_monitor_handler handler, void *arg)
```

A user-defined `handler` function is passed as second argument, with signature
```c
void (*threadpool_monitor_handler) (struct threadpool_monitor, void *arg)
```

This handler will be called to retrieve and display information about the activity of the tread pool.
It :

- will be called whenever the state of the thread pool changes,
- will be passed the argument `arg` previously passed to `threadpool_set_monitor` as third (be it not null) argument,
- will be called asynchronously, without interfering with the execution of workers (actually, a sequential dedicated thread pool is used),
- will be executed multi-thread-safely,
- will not be called after `threadpool_wait_and_destroy` has been called.

The monitoring data are passed to the handler function in a structure `threadpool_monitor` which contains:

- `struct threadpool *threadpool`: the thread pool for which the monitoring handler is called ;
- `float time`: the elapsed seconds since the creation of the thread pool ;
- `size_t workers.max_nb`: the maximum number of workers, as defined at the creation of the thread pool ;
- `size_t workers.nb_idle`: the number of idle worker, i.e. waiting (some time) for a task to process ;
- `size_t tasks.nb_pending`: the number of tasks submitted to the thread pool and not yet processed or being processed ;
- `size_t tasks.nb_processing`: the number of active workers, i.e. processing a task ;
- `size_t tasks.nb_succeeded`: the number of already processed and succeeded tasks by the thread pool
  (a task is considered successful when `work`, the function passed to `threadpool_add_task`, returns 0) ;
- `size_t tasks.nb_failed`: the number of already processed and failed tasks by the thread pool
  (a task is considered failed when `work`, the function passed to `threadpool_add_task`, does not return 0) ;
- `size_t tasks.nb_canceled`: the number of canceled tasks ;
- `size_t tasks.nb_submitted` : the number of submitted tasks (either pending, processing, succeeded, failed or canceled).

A handler `threadpool_monitor_to_terminal` is available for convenience:

- It can be passed as the second argument of `threadpool_set_monitor`.
- It displays monitoring data as text sent to a stream of type `FILE *`, passed as the third argument of `threadpool_set_monitor`.
  `stderr` will be used by default if this third argument is `NULL`.

### 7. Manage global resources

#### Allocating and releasing global resources

In case external resources should be allocated for tasks processing (for instance a connection to a database), user-defined functions `allocator` and `deallocator` can be set with:

```c
void threadpool_set_resource_manager (struct threadpool *threadpool, void *(*allocator) (void *global_data), void (*deallocator) (void *resource))
```

> This function should be called after `threadpool_create_and_start` and before adding tasks to the thread poll as it has no effect if workers are already running (`errno` would be set to `ECANCELED`).

- The user-defined function `allocator`:
    - should fully initialize and return the global resource of the thread pool ;
    - is passed the `global_data` of the thread pool as an argument ;
    - will generally be called once per thread pool, before processing the very first task.
- The user-defined function `deallocator`:
    - should fully release the global resource of the thread pool ;
    - is passed the resource to deallocate, as previously returned by `allocator`, as an argument ;
    - will generally be called once per thread pool, after all tasks have been processed or canceled.

Moreover, if the thread pool remains idle (waiting for tasks to process) for too long (see [below](#timeout-delay-of-idle-workers)), resources will be deallocated automatically, and will be reallocated automatically when the thread pool gets active again.

The global resource of a thread pool (as returned by the `allocateor` passed to `threadpool_set_resource_manager`) can be accessed inside user-defined functions `make_local`, `delete_local` (as passed to `threadpool_create_and_start`), `work` and `job_delete` (as passed to `threadpool_add_task`) with :

```c
void *threadpool_global_resource (void)
```

#### Timeout delay of idle workers

The timeout delay before thread pool internal and external resources are deallocated when a thread pool is kept idle for too long can be modified with:

```c
void threadpool_set_idle_timeout (struct threadpool *threadpool, double delay)
```

`delay`, in seconds, should be a non negative value, otherwise it is ignored and `errno` is set to `EINVAL`.
It should neither greater then 10.000.000 seconds.

> This delay should be set to a larger value than the time required to allocate global resources for tasks. It should nevertheless be kept as low as possible to release unused scarce resources.

The default delay is 0.1 seconds when a thread pool is created by `threadpool_create_and_start`.

The delay is set to 10.000.000 seconds by default after `threadpool_set_resource_manager` is called :
    - resources will not be deallocated by default if the thread pool is idle ;
    - `threadpool_set_idle_timeout` can be called after `threadpool_set_resource_manager` to lower the delay in order to deallocate scarce resources after a specified idle delay.

## Examples

Type `make` to compile and run the examples (in sub-folder [examples](examples)).

### Quick sort in place

An example of the usage of thread pool is given in files `qsip_wc.c` and `qsip_wc_test.c` [here](examples/qsip).

It sorts 2 bunches of 50 lists of 1.000.000 numbers. 

Two encapsulated thread pools are used : one to distribute 100 tasks over 7 monitored threads, each task sorting 1000000 numbers distributed over the CPU threads.

- `qsip_wc.c` is an attempt to implement a parallelized version of the quick sort algorithm (using a thread pool);

    - It uses features such as global data, worker local data, dynamic creation and deletion of jobs.
    - It reveals that a parallelized quick sort is inefficient due to thread management overhead.

- `qsip_wc_test.c` is an example of a thread pool that sorts several arrays using the above parallelized version of the quick sort algorithm.

    - It uses features such as global data, worker local data, task cancellation, (fake) resource management and monitoring.

Running this example yields:
```
$ make qsip_wc_test
Sorting 1,000,000 elements (multi-threaded quick sort in place), 100 times:
Initializing 100,000,000 random numbers...
7 workers requested and processing...
Allocate resources...
(=) succeeded tasks, (X) failed tasks, (*) processing tasks, (.) pending tasks, (/) canceled tasks, (~) idle thread pool.
[0x5cbba68eb7d0 (7)][    0.0000s][   0] 
[0x5cbba68eb7d0 (7)][    0.0002s][   1] .
Resources allocated.
[0x5cbba68eb7d0 (7)][    2.0006s][   1] .
[0x5cbba68eb7d0 (7)][    2.0010s][   2] ..
Will go to sleep for 16 seconds...
[0x5cbba68eb7d0 (7)][    2.0012s][   3] ...
[0x5cbba68eb7d0 (7)][    2.0014s][   4] ....
...
[0x5cbba68eb7d0 (7)][   13.1516s][  50] =================================================*
[0x5cbba68eb7d0 (7)][   13.2413s][  50] ================================================== ~
[0x5cbba68eb7d0 (7)][   14.1519s][  50] ================================================== ~
Deallocate resources...
[0x5cbba68eb7d0 (7)][   14.2414s][  50] ==================================================
Resources deallocated.
[0x5cbba68eb7d0 (7)][   16.2417s][  50] ==================================================
Stop sleeping after 16 seconds.
Allocate resources...
[0x5cbba68eb7d0 (7)][   18.0050s][  51] ==================================================.
Resources allocated.
[0x5cbba68eb7d0 (7)][   20.0054s][  51] ==================================================.
[0x5cbba68eb7d0 (7)][   20.0056s][  52] ==================================================..
...
[0x5cbba68eb7d0 (7)][   24.6119s][ 100] ===============================================================*////////////////////////////////////
[0x5cbba68eb7d0 (7)][   24.7960s][ 100] ================================================================//////////////////////////////////// ~
Deallocate resources...
Resources deallocated.
[0x5cbba68eb7d0 (7)][   26.7966s][ 100] ================================================================////////////////////////////////////
Done.
```

### Fuzzy words

This [example](examples/fuzzyword) matches a list of french fuzzy words against the french dictionary.

Run it with:

```
$ make fuzzyword
```

Two encapsulated thread pools are used : one to distribute the list of words on one monitored single thread (words are processed sequentially),
each word being compared to the entries (distributed over the CPU threads) of the dictionary.

It uses `job_delete` as a callback function for [task post-processing](#multi-thread-safe-task-post-processing) and `threadpool_set_resource_manager` for [global resource management](#7-manage-global-resources).

## Implementation insights

The API is implemented in C11 (file `wqm.c`) using the standard C thread library <threads.h>.
It is highly inspired from the great book "Programming with POSIX Threads" written by David R. Butenhof, 21st ed., 2008, Addison Wesley.

It has been heavily tested, but bugs are still possible. Please don't hesitate to report them to me.

### Management of workers

Workers are started automatically when needed, that is when a new task is submitted whereas all workers are already booked.
Workers are running in parallel, asynchronously.
Each worker can process one task at a time. A task is processed as soon as a worker is available.

A worker stops when all submitted tasks have been processed or after an idle time.
Idle workers are kept ready for new tasks for a short time and are then stopped automatically to release system resources.

For instance, say a task requires 2 seconds to be processed and the maximum idle delay for a worker is half a second:

- if the task is repeatedly submitted every 3 seconds to the thread pool, one worker will be created and activated on submission, and stopped after completion of the task and an idle time (of half a second).
- if the task is submitted every 2.4 seconds to the thread pool, one worker will be created and activated on submission, kept idle and reactivated for the next task.
- if the task is submitted every 2 seconds, one worker will be active to process the tasks.
- if the task is submitted every second, two workers will be active to process the tasks.
- if the rate of submitted tasks is very high, the maximum number of workers (as passed to `threadpool_create_and_start ()`) will be active.

Therefore, the number for workers automatically adapts to the rate and duration for tasks.

## That's it. Have fun and let me know!

> Zed is dead, but C is not.
