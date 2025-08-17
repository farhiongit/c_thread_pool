# Another simple, easy to use, effective but fully featured thread pool API for C language

> This API implements a task parallelization mechanism based on the thread pool pattern.

![Image of the Giola Lagoon in Thassos](ocean-pool-Giola-Lagoon.jpg "A beautiful, fully featured sea pool")

## Files

| Interface | Implementation |
|- | - |
| `wqm.h` | `wqm.c` |

## Libraries

Create a static and dynamic library using :

```
$ make libs
cc -O -fPIC   -c -o wqm.o wqm.c
ar rcs libwqm.a wqm.o
libwqm.a:wqm.o:0000000000000b0c T threadpool_add_task
libwqm.a:wqm.o:000000000000183c T threadpool_cancel_task
libwqm.a:wqm.o:00000000000007f9 T threadpool_create_and_start
libwqm.a:wqm.o:0000000000002501 T threadpool_current
libwqm.a:wqm.o:0000000000001813 T threadpool_global_data
libwqm.a:wqm.o:00000000000024d5 T threadpool_global_resource
libwqm.a:wqm.o:00000000000000cc T threadpool_job_free_handler
libwqm.a:wqm.o:000000000000144d T threadpool_monitor
libwqm.a:wqm.o:0000000000000762 T threadpool_monitor_every_100ms
libwqm.a:wqm.o:00000000000004f7 T threadpool_monitor_to_terminal
libwqm.a:wqm.o:0000000000002390 T threadpool_set_global_resource_manager
libwqm.a:wqm.o:00000000000022a1 T threadpool_set_idle_timeout
libwqm.a:wqm.o:000000000000146f T threadpool_set_monitor
libwqm.a:wqm.o:0000000000002521 T threadpool_set_worker_local_data_manager
libwqm.a:wqm.o:00000000000002cb T threadpool_task_continuation
libwqm.a:wqm.o:000000000000027f T threadpool_task_continue
libwqm.a:wqm.o:0000000000001595 T threadpool_wait_and_destroy
libwqm.a:wqm.o:00000000000017f3 T threadpool_worker_local_data
libwqm.a:wqm.o:0000000000000238 R TP_CANCEL_ALL_PENDING_TASKS
libwqm.a:wqm.o:0000000000000228 R TP_CANCEL_LAST_PENDING_TASK
libwqm.a:wqm.o:0000000000000230 R TP_CANCEL_NEXT_PENDING_TASK
libwqm.a:wqm.o:0000000000000210 R TP_JOB_CANCELED
libwqm.a:wqm.o:0000000000000214 R TP_JOB_FAILURE
libwqm.a:wqm.o:0000000000000218 R TP_JOB_SUCCESS
libwqm.a:wqm.o:0000000000000220 R TP_RUN_ALL_SUCCESSFUL_TASKS
libwqm.a:wqm.o:0000000000000224 R TP_RUN_ALL_TASKS
libwqm.a:wqm.o:000000000000021c R TP_RUN_ONE_SUCCESSFUL_TASK
libwqm.a:wqm.o:0000000000000248 R TP_WORKER_NB_CPU
libwqm.a:wqm.o:0000000000000240 R TP_WORKER_SEQUENTIAL
cc -shared -o libwqm.so wqm.o
```

| Static library | Dynamic library |
|- | - |
| `libwqm.a` | `libwqm.so` |

## Application programming Interface

### Usage

The thread pool pattern allows to run parallel tasks easily without the burden of thread management.

The user:

1. declares a thread pool and chooses the maximum number of parallel workers in charge of the execution of tasks (`threadpool_create_and_start ()`) ;
2. submits tasks to the thread pool (`threadpool_add_task ()`) that will be passed to one of available workers and will be executed asynchronously ;
3. waits for all the tasks to be completed (`threadpool_wait_and_destroy ()`).

### Unique features

This C standard implementation of a thread pool brings unique features, not found anywhere else at the time of writing.

1. **Standard C:** It uses the standard (minimalist) C11 thread library <threads.h> (§7.28 of ISO/IEC C programming language), rather the POSIX threads. It can therefore be ported more easily to systems other than unix-like systems.
1. **Data management:** The data passed to tasks (via `threadpool_add_task ()`) can be accessed, retrieved  and released multi-thread-safely after completion of the task (via the user-defined function `job_delete ()`), allowing collecting data at task termination.
1. **Global data management:** Global data can be defined and accessed (via `threadpool_global_data ()`) by all tasks.
1. **Worker data management:** Local data can be defined  (via `threadpool_set_worker_local_data_manager ()`) and accessed (via `threadpool_worker_local_data ()`) for each worker of the thread pool.
1. **Resource management:** Global resources can be allocated and deallocated (via `threadpool_set_global_resource_manager ()`) and accessed (via `threadpool_global_resource ()`) for the thread pool.
1. **Worker life-time management:** Workers will stay alive for a short idle time, ready to process new submitted tasks, even though `threadpool_wait_and_destroy ()` has already been called and no tasks are available, as long as some other tasks are still being processed and could therefore create new tasks dynamically.
1. **Monitoring facility:** The activity of the thread pool can be monitored and displayed by a front-end user defined function (via `threadpool_set_monitor ()`).
1. **Task cancellation:** Pending tasks can be cancelled after submission (via `threadpool_cancel_task ()`).
1. **Virtual tasks:** The thread pool can wait for asynchronous calls without blocking workers (via `threadpool_task_continuation ()` and `threadpool_task_continue ()`).

Those features are detailed below.

### Basic functionalities

| Function | Description |
| - | - |
| `threadpool_create_and_start` | Creates and starts a new pool of workers |
| `threadpool_add_task` | Adds a task to the pool of workers |
| `threadpool_wait_and_destroy` | Waits for all the tasks to be done and destroy the pool of workers |

Those features are detailed below.

### Data management

| Function | Description |
| - | - |
| `threadpool_global_data` | Gives access to the user defined shared global data of the pool of workers |
| `threadpool_set_worker_local_data_manager` | Defines the workers' local data manager functions |
| `threadpool_worker_local_data` | Gives access to the user defined local data of a worker |
| `threadpool_set_global_resource_manager` | Defines the resource manager functions |
| `threadpool_global_resource` | Gives access to the global resource of the thread pool |

Those features are detailed below.

### Virtual tasks management

| Function | Description |
| - | - |
| `threadpool_task_continuation` | Defines a virtual task to be processed after the response of an asynchronous call |
| `threadpool_task_continue` | Callback function to be called by the callback function of an asynchronous call to proceed a virtual task |

Those features are detailed below.

### Other functionalities

| Function | Description |
| - | - |
| `threadpool_current` | Gives access to the current threadpool |
| `threadpool_cancel_task` | Cancels either all pending tasks, or the last, or the next submitted task, or a specific task |
| `threadpool_set_monitor` | Sets a user-defined function to retrieve and display monitoring information of the thread pool activity |
| `threadpool_set_idle_timeout` | Modifies the idle time out (default is 0.1 s) before an idle worker terminates |

Those features are detailed below.

## Detailed API

The API is defined in file `wqm.h`.

### Create a thread pool

```c
struct threadpool *threadpool_create_and_start (size_t nb_workers,
                                                void *global_data,
                                                tp_property_t property)
```

A thread pool is declared and started with a call to `threadpool_create_and_start ()`.

The first argument `nb_workers` is the number of requested workers, that is the maximum number of tasks that should be executed in parallel by the system.
- `TP_WORKER_NB_CPU` can be used as first argument to fit to the number of processors currently available in the system (as returned by `get_nprocs ()` with GNU C standard library).
- `TP_WORKER_SEQUENTIAL` can be used as first argument to create a sequential thread pool: tasks will be executed asynchronously, one at a time, and in the order there were submitted.

The maximum number of workers can be defined to a higher value than the number of CPUs as workers will be started only when solicited and will be released when unused after an idle time.

Nevertheless, the actual number of workers will be limited by the operating system to a lower value than `nb_workers`.

The third argument is either :

- `TP_RUN_ALL_TASKS` : Run all submitted tasks (usual expected standard behaviour).
- `TP_RUN_ALL_SUCCESSFUL_TASKS` : Run submitted tasks until at least one fails (returns `TP_JOB_FAILURE`). Cancel automatically other (already or to be) submitted tasks.
- `TP_RUN_ONE_SUCCESSFUL_TASK` : Run submitted tasks until at least one succeeds (returns `TP_JOB_SUCCESS`). Cancel automatically other (already or to be) submitted tasks.

###### Get the current thread pool

The current thread pool can further be retrieved in a working context,
inside user-defined functions `work` and `job_delete` (as passed to `threadpool_add_task`) and `work` (as passed to `threadpool_task_continuation`),
with:

```c
threadpool_current (void)
```

###### Options

- The second argument `global_data`, if not null, is a pointer to global data.
  This pointer can next be retrieved by tasks with the function `threadpool_global_data ()`.

### Submit a task

```c
tp_task_t threadpool_add_task (struct threadpool *threadpool,
                               tp_result_t (*work) (void *job),
                               void *job,
                               void (*job_delete) (void *job, tp_result_t result))
```

A task is submitted to the thread pool with a call to `threadpool_add_task ()`.

- The first argument `threadpool` is a thread pool returned by a previous call to `threadpool_create_and_start ()`.
- The second argument `work` is a user defined function to be executed by a worker of the thread pool on `job`.
  This function `work` should return `TP_JOB_SUCCESS` on success, `TP_JOB_FAILURE` otherwise.
  `work` should be made thread-safe as several `work` will be running in parallel (that's the whole point of the thread pool).
  This function receives the job `job` as argument, as it was passed to `threadpool_add_task`.
  If needed, `work` can itself (multi-thread-safely) call `threadpool_add_task` (on the current thread pool retrieved with `threadpool_current`).
- The third argument `job` is a pointer to the data to be used by the task and that will be processed by `work`.
  It should be fully initialised before it is passed to `threadpool_add_task`.
  It can be allocated automatically, statically or dynamically, as long it survives until the job is done by `work`.

The function `threadpool_add_task` returns a unique id of the submitted task, or 0 on error (with `errno` set to `ENOMEM`).

###### Options

- The fourth argument `job_delete`, if not null, is a user defined function called at termination of the task (after processing or [cancellation](#cancel-tasks).)
  This function receives the `job` of the task and the result of `work` as arguments.

`job_delete` should be used if the job was allocated dynamically in order to release and destroy the data after `work` is done and avoid memory leaks.

`job_delete` is called in a multi-thread-safe manner and can therefore safely aggregate results to those of previous tasks for instance
(in a map and reduce pattern for instance). See [below](#multi-thread-safe-task-post-processing).

> `job_delete` could as well be called manually (rather than passed as an argument to `threadpool_add_task`) at the very end of `work ()`, but it then would not be executed multi-thread-safely, forbidding any aggregation.
> If a part of a task needs to be synchronised, `threadpool_guard_begin ()` and `threadpool_guard_end ()` could be used to guard some sections of the task. Nevertheless, these functions SHOULD generally NOT BE USED. Calls to `job_delete` are synchronised and should respond to ususal cases.

- A handler is provided for convenience.
```c
void threadpool_job_free_handler (void *job, tp_result_t result);
```
  It calls `free (job)`, whatever the value of `result`.
  Therefore, if `job` was allocated with a single `malloc ()` (or affiliated functions), `threadpool_job_free_handler` is a possible choice for `job_delete`.

###### Multi-thread safe task post-processing

Besides simply releasing data `job`, the user-defined `job_delete` can also be used as a callback function, called after the task is done or cancelled, to retrieve the used and possibly modified `job` and take actions (displaying, computing, aggregating, ...).

For instance, a user-defined type `job_t` could be declared as a structure containing:

```c
typedef struct {
  struct { ... } input;
  struct { ... } result;
} job_t;
```
A `job` of type `job_t` would then be passed to `threadpool_add_task`.

The task post-processing pattern stands in 4 steps :

1. A `job`, of type `job_t`, is allocated and initialised:
   - `input` data is set ;
1. The `job` is passed is passed to `threadpool_add_task`.
1. The `job` is processed in the user-defined function `work`:
   - the previously set `input` data is used to compute the `result` data of the task ;
1. In the user-defined function `job_delete`:
   - if the result of `work` is `TP_JOB_SUCCESS`, the `result` can multi-thread-safely update an aggregated result of all the tasks ;
     `global_data`, passed to `threadpool_create_and_start` and retrievable by `threadpool_global_data`, is a possible choice to hold the aggregated result ;
   - any required deallocation of `job` is done.

See below [fuzzy words](#fuzzy-words) and [map, filter, reduce](#map-filter-and-reduce) for examples of such a pattern.

### Access to global and local thread data

Global and local data of threads can be retrieved and updated safely in the context of working threads.

The global data of a thread pool (`global_data` as passed to `threadpool_create_and_start`):

- should be fully initialised before `threadpool_create_and_start` is called ;
- should survive after `threadpool_wait_and_destroy` is called ;
- can be accessed inside user-defined functions `make_local`, `delete_local` (as passed to `threadpool_create_and_start`), `work` and `job_delete` (as passed to `threadpool_add_task`), `work` (as passed to `threadpool_task_continuation`) and `deallocator` (as passed to `threadpool_set_resource_manager`) with :

```c
void *threadpool_global_data (void)
```

> `threadpool_global_data` can _not_ be called in the resource `allocator` set by `threadpool_set_resource_manager`.

The local data of a worker (created and returned by `make_local` and destroyed by `delete_local`, as passed to `threadpool_set_worker_local_data_manager`) can be accessed inside user-defined functions `work` and `job_delete` (as passed to `threadpool_add_task`) and `work` (as passed to `threadpool_task_continuation`)  with :

```c
void threadpool_set_worker_local_data_manager (struct threadpool *threadpool, void *(*make_local) (void), void (*delete_local) (void *local_data))
```

```c
void *threadpool_worker_local_data (void)
```

> These two functions must be called in the context of a running thread, otherwise they return 0.

### Cancel tasks

```c
size_t threadpool_cancel_task (struct threadpool *threadpool, tp_task_t task_id)
```

Previously submitted and still pending tasks can be cancelled.
`task_id` is :

- either a unique id returned by a previous call to `threadpool_add_task` ;
- or `TP_CANCEL_ALL_PENDING_TASKS` to cancel all still pending tasks ;
- or `TP_CANCEL_NEXT_PENDING_TASK` to cancel the next still pending submitted task (it can be used several times in a row) ;
- or `TP_CANCEL_LAST_PENDING_TASK` to cancel the last still pending submitted task (it can be used several times in a row).

Canceled tasks won't be processed, but `job_delete`, as optionally passed to `threadpool_add_task`, will be called though
(`TP_JOB_CANCELED` will be passed to `job_delete`).

The function returns the number of cancelled tasks, if any, or 0 if there are not any left pending task to be cancelled.

### Wait for all submitted tasks to be completed

```c
void threadpool_wait_and_destroy (struct threadpool *threadpool)
```

The single argument `threadpool` is a thread pool returned by a previous call to `threadpool_create_and_start ()`.

This function declares that all the tasks have been submitted.
It then waits for all the tasks to be completed by workers.

`threadpool` should not be used after a call to `threadpool_wait_and_destroy ()`.

### Monitor the thread pool activity

A monitoring of the thread pool activity can optionally be activated by calling
```c
void threadpool_set_monitor (struct threadpool *threadpool, threadpool_monitor_handler handler, void *arg, threadpool_monitor_filter filter)
```

A user-defined `handler` function is passed as second argument, with signature
```c
void (*threadpool_monitor_handler) (struct threadpool_monitor, void *arg)
```

`0` or a user-defined `filter` function can be passed as fourth argument, with signature
```c
int (*threadpool_monitor_filter) (struct threadpool_monitor d)
```

The `handler` will be called to retrieve and display information about the activity of the thread pool.
It :

- will be called whenever the state of the thread pool changes and, if `filter` is not null, whenever `filter` returns non-zero. `filter` should be set to 0 to monitor every change of the thread pool state.
- will be passed the argument `arg` (which can be `0`) previously passed to `threadpool_set_monitor` as third argument,
- will be called asynchronously, without interfering with the execution of workers (actually, a sequential dedicated asynchronous thread pool is used for monitoring),
- will be executed multi-thread-safely,
- should not be called after `threadpool_wait_and_destroy` has been called.

The monitoring data are passed to the handler function in a structure `threadpool_monitor` which contains:

- `struct threadpool *threadpool`: the thread pool for which the monitoring handler is called ;
- `double time`: the elapsed seconds since the creation of the thread pool (`threadpool_create_and_start`) ;
- `int closed` : 1 if the thread pool has been closed (by a call to `threadpool_wait_and_destroy`), 0 otherwise ;
- `size_t workers.nb_requested`: the requested number of workers, as defined at the creation of the thread pool ;
- `size_t workers.nb_max`: the maximum number of workers granted by the operating system (<= `workers.nb_requested`) ;
- `size_t workers.nb_alive`: the number of alive workers, either running (`tasks.nb_processing`) or waiting (`workers.nb_idle`) ;
- `size_t workers.nb_idle`: the number of idle worker, i.e. waiting (some time) for a task to process ;
- `size_t tasks.nb_pending`: the number of tasks submitted to the thread pool and not yet processed or being processed ;
- `size_t tasks.nb_processing`: the number of running workers, i.e. processing a task ;
- `size_t tasks.nb_asynchronous`: the number of asynchronous (virtual) tasks ;
- `size_t tasks.nb_succeeded`: the number of already processed and succeeded tasks by the thread pool
  (a task is considered successful when `work`, the function passed to `threadpool_add_task`, returns 0) ;
- `size_t tasks.nb_failed`: the number of already processed and failed tasks by the thread pool
  (a task is considered failed when `work`, the function passed to `threadpool_add_task`, does not return 0) ;
- `size_t tasks.nb_canceled`: the number of cancelled tasks ;
- `size_t tasks.nb_submitted` : the number of submitted tasks (either pending, processing, succeeded, failed or cancelled).

A handler `threadpool_monitor_to_terminal` is available for convenience:

- It can be used as the second argument of `threadpool_set_monitor`.
- It displays monitoring data as text sent to a stream of type `FILE *`, passed as the third argument of `threadpool_set_monitor`.
  `stderr` will be used by default if this third argument is `NULL`.

A filter `threadpool_monitor_every_100ms` is available for convenience:

- It can be used as the fourth argument of `threadpool_set_monitor`.
- It permits to monitor not more often than every 100 ms.

Even though the `handler` is automatically called when relevant, it can be called manually with `threadpool_monitor`.

### Manage data

Data used in the context of a thread pool can be managed globally or locally with four different ways, depending on the scope and life-cycle of the data.

| Scope | Access | Management |
|- | - | - |
| Thread pool | `threadpool_global_data` | `threadpool_create_and_start`, see [Manage global data](#manage-global-data) |
| Active thread pool (running workers) | `threadpool_global_resource` | `threadpool_set_global_resource_manager`, see [Manage global resources](#manage-global-resources) |
| Worker | `threadpool_worker_local_data` | `threadpool_set_worker_local_data_manager`, see [Manage worker local data](#manage-worker-local-data) |
| Task | `job` in `work` | `threadpool_add_task` and `threadpool_task_continuation`, see [Manage task data](#manage-task-data) |

#### Manage global data

A global context data of a thread pool can be passed as second argument when it is created with `threadpool_create_and_start`.

This data should be allocated (statically, automatically or dynamically) before calling `threadpool_create_and_start` and respectively deallocated after calling `threadpool_wait_and_destroy`.

This global data of a thread pool can be accessed inside:

- user-defined functions `allocator` and `deallocator` passed to `threadpool_set_global_resource_manager` ;
- user-defined functions `make_local` and `delete_local` (as passed to `threadpool_set_worker_local_data_manager`) ;
- user-defined functions `work` and `job_delete` (as passed to `threadpool_add_task`) ;
- user-defined functions `work` (as passed to `threadpool_task_continuation`) ;

with :

```c
void *threadpool_global_data (void)
```

#### Manage global resources

In case external resources should be allocated for tasks processing (for instance a shared connection to a database), user-defined functions `allocator` and `deallocator` can be set with:

```c
void threadpool_set_global_resource_manager (struct threadpool *threadpool, void *(*allocator) (void *global_data), void (*deallocator) (void *resource))
```

> This function should be called after `threadpool_create_and_start` and before adding tasks to the thread pool with `threadpool_add_task`.
> Otherwise, it would have no effect (as workers are already running) and `errno` would be set to `ECANCELED`.

- The user-defined function `allocator`:
    - should fully initialise and return the global resource of the thread pool ;
    - is passed the `global_data` of the thread pool as an argument ;
    - will generally be called once per thread pool, before processing the very first task.
- The user-defined function `deallocator`:
    - should fully release the global resource of the thread pool ;
    - is passed the resource to deallocate, as previously returned by `allocator`, as an argument ;
    - will generally be called once per thread pool, after all tasks have been processed or cancelled.

Moreover, if the thread pool remains idle (waiting for tasks to process) for too long (see [below](#timeout-delay-of-idle-workers)), resources will be deallocated automatically, and will be reallocated automatically when the thread pool gets alive again.

The global resource of a thread pool (as returned by the `allocator` passed to `threadpool_set_global_resource_manager`) can be accessed inside user-defined functions `make_local`, `delete_local` (as passed to `threadpool_set_worker_local_data_manager`), `work` and `job_delete` (as passed to `threadpool_add_task`),
`work` (as passed to `threadpool_task_continuation`) with :

```c
void *threadpool_global_resource (void)
```

##### Timeout delay of idle workers

The timeout delay after which thread pool internal and external resources are deallocated when a thread pool has been kept idle for too long can be modified with:

```c
void threadpool_set_idle_timeout (struct threadpool *threadpool, double delay)
```

`delay`, in seconds, should be a non negative value, otherwise it is ignored and `errno` is set to `EINVAL`.
It can not exceed 10,000,000 seconds.

> This delay would be set to a value larger than the time required to allocate global resources for tasks. It should nevertheless be kept as low as possible to release unused scarce resources when workers are kept idle for a long period.

The default delay is 0.1 seconds when a thread pool is created by `threadpool_create_and_start`.

The delay is set to 10,000,000 seconds by default after `threadpool_set_global_resource_manager` is called : resources will not be deallocated by default if the thread pool is idle.
`threadpool_set_idle_timeout` should be called _after_ `threadpool_set_global_resource_manager` to lower the delay in order to deallocate scarce resources after a specified idle delay.

#### Manage worker local data

In case resources should be allocated for each worker (for instance a connection to a database), user-defined functions `make_local` and `delete_local` can be set with:

```c
void threadpool_set_worker_local_data_manager (struct threadpool *threadpool, void *(*make_local) (void), void (*delete_local) (void *local_data));
```

> This function should be called after `threadpool_create_and_start` and before adding tasks to the thread poll with `threadpool_add_task`.
> Otherwise, it would have no effect (as workers are already running) and `errno` would be set to `ECANCELED`.

`make_local` will be called when a worker is created and `delete_local` when it is terminated:

- The argument `make_local`, if not null, is a user defined function that returns a pointer to data for a local usage by a worker.
  This pointer can next be retrieved by tasks with the function `threadpool_worker_local_data ()`.
  `make_local` is called in a multi-thread-safe manner at the initialisation of a worker.
  `make_local` can safely access (and update) the content of `global_data` (with `threadpool_global_data`) if needed.
- The argument `delete_local`, if not null, is a user defined function that is executed to release and destroys the local data used by each worker (passed as an argument) when a worker stops.
  `delete_local` is called in a multi-thread-safe manner at the termination of a worker.
  `delete_local` can safely access (and update) the content of `global_data` (with `threadpool_global_data`) if needed (to gather results or statistics for instance).

The local data of a worker (as returned by the `make_local` passed to `threadpool_set_worker_local_data_manager`) can be accessed inside user-defined functions `work` and `job_delete` (as passed to `threadpool_add_task`) and `work` (as passed to `threadpool_task_continuation`) with :

```c
void *threadpool_worker_local_data (void)
```

#### Manage task data

The data of a `job` is passed to a task when it is added to a thread pool with `threadpool_add_task`.

It can be accessed inside user-defined functions `work` and `job_delete` (as passed to `threadpool_add_task`) and `work` (as passed to `threadpool_task_continuation`).

See [Submit a task](#submit-a-task) and below.

### Manage asynchronous calls (virtual tasks)

In case a task would need to use an asynchronous call, the continuation of the task `work_continuator` can be specified by calling

```c
uint64_t threadpool_task_continuation (tp_result_t (*work_continuator) (void *data), double seconds)
```

just *before* the asynchronous call.

The function `work_continuator` indicates the function to be called when the asynchronous call notifies its completion.
`work_continuator` should return `TP_JOB_SUCCESS` on success, `TP_JOB_FAILURE` otherwise.

- `threadpool_task_continuation` returns 0 and sets `errno` to `EINVAL` if `work_continuator` is null.
- `threadpool_task_continuation` returns 0 and sets `errno` to `EPERM` if it is called outside of:
    - a function `work` of a task scheduled with `threadpool_add_task` ;
    - a function `work_continuator` of a task scheduled with `threadpool_task_continuation` ;
- otherwise `threadpool_task_continuation` returns a non-zero unique ID that should be passed to the asynchronous call.

`seconds` is a timeout delay, in seconds, for the asynchronous response. After this delay, the asynchronous response will be ignored (see below).

In the callback function of the asynchronous call (when the asynchronous call notifies its completion), the function

```c
tp_result_t threadpool_task_continue (uint64_t uid)
```

should be called with the ID previously retrieved to continue to process the initial task.

The function `work_continuator` previously declared by `threadpool_task_continuation` will then be automatically called:

- If `threadpool_task_continue` is called before the timeout delay `seconds` has elapsed, `work_continuator` will be scheduled by the thread pool and `TP_JOB_SUCCESS` will be returned.
- If `threadpool_task_continue` is called after the timeout delay `seconds` has elapsed, `threadpool_task_continue` will have no effect, will return `TP_JOB_FAILURE` and `errno` will be set to `ETIMEOUT`.

The task does not block any worker between the calls to `threadpool_task_continuation` and `threadpool_task_continue`.
Workers are available to process any other tasks (either asynchronous or not): the tasks using `threadpool_task_continuation` and `threadpool_task_continue` behave like virtual tasks.

If needed, `work_continuator` can itself (multi-thread-safely) call `threadpool_add_task` (on the current thread pool retrieved with `threadpool_current`).

> This features was inspired from Java virtual threads (see https://openjdk.org/jeps/444).

## Examples

Type `make` to compile and run the examples (in sub-folder [examples](examples)).

### Quick sort in place

An example of the usage of thread pool is given in files `qsip_wc.c` and `qsip_wc_test.c` [here](examples/qsip).

It sorts 2 bunches of 50 lists of 1.000.000 numbers. 

Two encapsulated thread pools are used : one to distribute 100 tasks over 7 monitored threads, each task sorting 1000000 numbers distributed over the CPU threads.

- `qsip_wc.c` is an attempt to implement a parallelised version of the quick sort algorithm (using a thread pool);

    - It uses features such as global data, worker local data, dynamic creation and deletion of jobs.
    - It reveals that a parallelised quick sort is inefficient due to thread management overhead.

- `qsip_wc_test.c` is an example of a thread pool that sorts several arrays using the above parallelised version of the quick sort algorithm.

    - It uses features such as global data, worker local data, task cancellation, (fake) resource management and monitoring.

Run this example with

```
$ make qsip_wc_test
```

### Fuzzy words

This [example](examples/fuzzyword) matches a list of french fuzzy words against the french dictionary.

Run it with:

```
$ make fuzzyword
```

Two encapsulated thread pools are used : one to distribute the list of words on one monitored single thread (words are processed sequentially),
each word being compared to the entries (distributed over the CPU threads) of the dictionary.

It uses `job_delete` as a callback function for [task post-processing](#multi-thread-safe-task-post-processing) and `threadpool_set_global_resource_manager` for [global resource management](#manage-global-resources).

### Intensive

This [example](examples/intensive) requests more workers than what the system permits.

Run it with:

```
$ make intensive
```

### Continuations (virtual tasks)

This [example](examples/continuations) uses `threadpool_task_continuation` and `threadpool_task_continue` to create asynchronous virtual tasks.

Asynchronous tasks (here a pause of one second) can be processed with a thread pool of one worker only: those tasks behave like virtual tasks which do not block the worker (awesome !)

Run it with:

```
$ make timers
```

### Map, filter and reduce

This [example](examples/mfr) shows how to implement a map, filter, reduce pattern with parallelisation.

Results of each job are aggregated in the worker local data and then in the thread pool global data.

Run it with:

```
$ make mfr
```

## Implementation insights

The API is implemented in C11 (file `wqm.c`) using the standard C thread library <threads.h>.
It is highly inspired from the great book "Programming with POSIX Threads" written by David R. Butenhof, 21st ed., 2008, Addison Wesley.

It has been heavily tested, but bugs are still possible. Please don't hesitate to report them to me.

It makes use of `libmap` and `libtimer` implemented in [minimaps](https://github.com/farhiongit/minimaps) :

- `map.h` and `map.c` define an unprecedented MT-safe implementation of a map library that can manage maps, sets, ordered and unordered lists that can do it all with a minimalist interface.
- `timer.h` and `timer.c` define a OS-independent (as compared to POSIX `timer_settime`) timer.

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
