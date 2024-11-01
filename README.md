# Another simple, easy to use, effective but fully featured thread pool API for C language

> This API implements a task parallelization mechanism based on the thread pool pattern.

![alt text](ocean-pool-Giola-Lagoon.jpg "A beautiful, fully featured sea pool")

## Usage

The thread pool pattern allows to run parallel tasks easily without the burden of thread management.

The user:

1. declares a thread pool and chooses the maximum number of parallel workers in charge of the execution of tasks (`threadpool_create_and_start ()`) ;
2. submits tasks to the thread pool that will pass it to one of available workers (`threadpool_add_task ()`) and will be executed asynchronously;
3. waits for all the tasks to be completed (`threadpool_wait_and_destroy ()`).

### Files

| Interface | Implementation |
|- | - |
| `wqm.h` | `wqm.c` |

### Interface

#### Basic functionalities

| Function | Description |
| - | - |
| `threadpool_create_and_start` | Create and start a new pool of workers |
| `threadpool_add_task` | Add a task to the pool of workers |
| `threadpool_cancel_task` | Cancel all pending tasks, the last or next submitted task, or a specific task |
| `threadpool_wait_and_destroy` | Wait for all the tasks to be done and destroy the pool of workers |

Those features are detailed below.

#### Advanced functionalities

| Function | Description |
| - | - |
| `threadpool_global_data` | Access the user defined shared global data of the pool of workers |
| `threadpool_worker_local_data` | Access the user defined local data of a worker |
| `threadpool_set_monitor` | Set a user-defined function to retrieve and display monitoring information |

Those features are detailed below.

## Unique features

This implementation of a thread pool brings unique features, not found anywhere else at the time of writing.

1. It uses the standard (minimalist) C11 thread library <threads.h>, rather the POSIX threads. It can therefore be ported more easily to systems other than unix-like systems.
1. The data passed to tasks (via `threadpool_add_task ()`) can be accessed, retrieved  and released multi-thread-safely after completion of the task (via the user defined function `job_delete ()`), allowing collecting data at task termination.
1. Global data can be defined and accessed (via `threadpool_global_data ()`) by all tasks.
1. Local data can be defined and accessed (via `threadpool_worker_local_data ()`) for each worker of the thread pool.
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
                                                void *(*make_local) (void *global_data),
                                                void (*delete_local) (void *local_data, void *global_data))
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
  This function receives the thread pool `threadpool` and the job `job` as arguments, as they were passed to `threadpool_add_task`.
  Therefore, `work` can itself (multi-thread-safely) call `threadpool_add_task` or `threadpool_global_data` if needed.
- The third argument `job` is a pointer to the data to be used by the task and that will be processed by `work`.
  The data pointed to by `job` should not be deallocated before `threadpool_wait_and_destroy` is called.

The function `threadpool_add_task` returns a unique id of the submitted task, or 0 on error (with `errno` set to `ENOMEM`).

###### Options

- The fourth argument `job_delete`, if not null, is a user defined function called at termination of the task (after processing or [cancellation](#3-cancel-tasks).)
  This function receives the `job` of the task as an argument.

`job_delete` should be used if the job was allocated dynamically in order to release and destroy the data after use.

`job_delete` is called in a multi-thread-safe manner and can therefore safely aggregate results to those of previous tasks for instance
(in a map and reduce pattern for instance). See [below](#task-post-processing).

`free ()` is a possible choice for `job_delete`, if `job` was allocated with `malloc ()` and affiliated functions.

###### Task post-processing

`job_delete` can also be used to do more than simply release data `job` after the task is done (or canceled):
it can be used to retrieve data used and possibly modified by the processed task in a multi-thread-safe manner.

For instance, a type `job_t` could be declared as a structure containing the `input` data of the task and its `result`
and a pointer to `job_t` passed as the `job` argument of `threadpool_add_task`.

```c
typedef struct {
  struct { ... } input;
  struct { ... } result;
} job_t;
```

The `job->result` can then be retrieved inside the user defined function `job_delete` in a multi-thread-safe manner
(allowing aggregation into a global output for instance), before any required deallocation of `job`.

> `job_delete` could as well be called manually (rather than passed as an argument to `threadpool_add_task`) at the very end of `work ()`, but it then would not be executed multi-thread-safely, forbidding any aggregation.

### 3. Cancel tasks

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

### 4. Wait for all submitted tasks to be completed

```c
void threadpool_wait_and_destroy (struct threadpool *threadpool)
```

The single argument `threadpool` is a thread pool returned by a previous call to `threadpool_create_and_start ()`.
This function declares that all the tasks have been submitted.
It then waits for all the tasks to be completed by workers.
`threadpool` should not be used after a call to `threadpool_wait_and_destroy ()`.

### 5. Monitor the thread pool activity

A monitoring of the thread pool activity can optionally be activated by calling
```c
void threadpool_set_monitor (struct threadpool *threadpool, threadpool_monitor_handler new, void *arg)
```

A user-defined handler function is passed as second argument, with signature
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
- `size_t max_nb_workers`: the maximum number of workers, as defined at the creation of the thread pool ;
- `size_t nb_processing_tasks`: the number of active workers, i.e. processing a task ;
- `size_t nb_idle_workers`: the number of idle worker, i.e. waiting (some time) for a task to process ;
- `size_t nb_pending_tasks`: the number of tasks submitted to the thread pool and not yet processed or being processed ;
- `size_t nb_succeeded_tasks`: the number of already processed and succeeded tasks by the thread pool
  (a task is considered successful when `work`, the function passed to `threadpool_add_task`, returns 0) ;
- `size_t nb_failed_tasks`: the number of already processed and failed tasks by the thread pool
  (a task is considered failed when `work`, the function passed to `threadpool_add_task`, does not return 0) ;
- `size_t nb_canceled_tasks`: the number of canceled tasks.

A handler `threadpool_monitor_to_terminal` is available for convenience:

- It can be passed as the second argument of `threadpool_set_monitor`.
- It displays monitoring data as text sent to a stream of type `FILE *`, passed as the third argument of `threadpool_set_monitor`.
  `stderr` will used by default if this third argument is `NULL`.

## Examples

An example of the usage of thread pool is given in files `qsip_wc.c` and `qsip_wc_test.c`.

- `qsip_wc.c` is an attempt to implement a parallelized version of the quick sort algorithm (using a thread pool);

    - It uses features such as global data, worker local data, dynamic creation and deletion of jobs.
    - It reveals that parallelizing quick sort is inefficient due to thread management overhead.

- `qsip_wc_test.c` is an example of a thread pool that sorts several arrays using the above parallelized version of the quick sort algorithm.

    - It uses features such as global data, worker local data, task cancellation and monitoring.

Type `make` to compile and run the example.
Running this example yields:
```
$ make
cc -O   -c -o wqm.o wqm.c
cc -O   -c -o qsip_wc.o qsip_wc.c
cc -O    qsip_wc_test.c qsip_wc.o wqm.o   -o qsip_wc_test
wqm.o:0000000000000010 R ALL_TASKS
wqm.o:0000000000000000 R LAST_TASK
wqm.o:0000000000000020 R NB_CPU
wqm.o:0000000000000008 R NEXT_TASK
wqm.o:0000000000000018 R SEQUENTIAL
wqm.o:00000000000001f2 T threadpool_add_task
wqm.o:0000000000000926 T threadpool_cancel_task
wqm.o:000000000000004f T threadpool_create_and_start
wqm.o:000000000000091d T threadpool_global_data
wqm.o:00000000000004b5 T threadpool_set_monitor
wqm.o:0000000000000542 T threadpool_wait_and_destroy
wqm.o:000000000000062d T threadpool_worker_local_data
qsip_wc.o:000000000000031a T qsip
./qsip_wc_test
Sorting 1,000,000 elements (multi-threaded quick sort in place), 100 times:
Initializing 100,000,000 random numbers...
7 workers requested and processing...
(=) succeeded tasks, (X) failed tasks, (*) processing tasks, (.) pending tasks, (/) canceled tasks, (-) idle workers.
[0x613036dea860][    0.0000s]
[0x613036dea860][    0.0002s] .
[0x613036dea860][    0.0003s] ..
[0x613036dea860][    0.0003s] ...
.
.
.
[0x613036dea860][   20.2429s] ========================================================================**//////////////////////////---
[0x613036dea860][   20.2509s] ========================================================================**//////////////////////////--
[0x613036dea860][   20.2683s] =========================================================================*//////////////////////////---
[0x613036dea860][   20.2862s] =========================================================================*//////////////////////////--
[0x613036dea860][   20.2976s] ==========================================================================//////////////////////////--
[0x613036dea860][   20.2976s] ==========================================================================//////////////////////////-
[0x613036dea860][   20.2976s] ==========================================================================//////////////////////////
Done.
```

## Implementation

The API is implemented in C11 (file `wqm.c`) using the standard C thread library <threads.h>.
It is highly inspired from the great book "Programming with POSIX Threads" written by David R. Butenhof, 21st ed., 2008, Addison Wesley.

It has been heavily tested, but bugs are still possible. Please don't hesitate to report them to me.

### Management of workers

Workers are started automatically when needed, that is when a new task is submitted whereas all workers are already booked.
Workers are running in parallel, asynchronously.
Each worker can process a task at a time. A task is processed as soon as a worker is available.

A worker stops when all submitted tasks have been processed or after an idle time (half a second).
Idle workers are kept ready for new tasks for a short time and are then stopped automatically to release system resources.

For instance, say a task requires 2 seconds to be processed and the maximum idle delay for a worker is half a second:

- if the task is repeatedly submitted every 3 seconds to the thread pool, one worker will be created and activated on submission, and stopped after completion of the task and an idle time (of half a second).
- if the task is submitted every 2.4 seconds to the thread pool, one worker will be created and activated on submission, kept idle and ready for the next task.
- if the task is submitted every 2 seconds, one worker will be active to process the tasks.
- if the task is submitted every second, two workers will be active to process the tasks.
- if the rate of submitted tasks is very high, the maximum number of workers (as passed to `threadpool_create_and_start ()`) will be active.

Therefore, the number for workers automatically adapts to the rate and duration for tasks.

## That's it. Have fun and let me know!

> Zed is dead, but C is not.
