# Another simple, easy to use, effective but fully featured thread pool API for C language

> This API implements a task parallelization mechanism based on the thread pool pattern.

![alt text](ocean-pool-Giola-Lagoon.jpg "A beautiful, fully featured sea pool")

## Usage

The thread pool pattern allows to run parallel tasks easily without the burden of thread management.

The user:

1. declares a thread pool and chooses the maximum number of parallel workers in charge of the execution of tasks (`threadpool_create_and_start ()`) ;
2. submits tasks to the thread pool that will pass it to one of the available workers (`threadpool_add_task ()`) ;
3. waits for all the tasks to be completed (`threadpool_wait_and_destroy ()`).

## Implementation

The API is implemented in C11 (file `wqm.c`).
It is highly inspired from the great book "Programming with POSIX Threads" written by David R. Butenhof, 21st ed., 2008, Addison Wesley.

## Features

Compared to Butenhof's, it yields extra features:

1. It uses the standard (minimalist) thread C11 library, rather the POSIX threads.
1. Global data can be defined and accessed (via `threadpool_global_data ()`) by all tasks.
1. Local data can be defined and accessed (via `threadpool_worker_local_data ()`) for each worker of the thread pool.
1. The data passed to tasks can be accessed and released in a thread-safe manner after completion of the task (via the user defined function `job_delete ()`).
1. Workers will stay alive for a short idle time, ready to process new submitted tasks, even though `threadpool_wait_and_destroy ()` has already been called and no tasks are available, in case another task still being processed would create new tasks dynamically.
1. The activity of the thread pool can be monitored and displayed by a front-end user defined function.

## Management of workers

Workers are started automatically when needed, that is when a new task is submitted whereas all workers are already booked. 
Idle workers are kept ready for new tasks for a short time and are then stopped automatically to release system resources.

For instance, say a task requires 2 seconds to be processed and the maximum idle delay for a worker is half a second:

- if the task is repeatedly submitted every 3 seconds to the thread pool, one worker will be created and activated on submission, and stopped after completion of the task and an idle time (of half a second).
- if the task is submitted every 2.4 seconds to the thread pool, one worker will be created and activated on submission, kept idle and ready for the next task.
- if the task is submitted every 2 seconds, one worker will be active to process the tasks.
- if the task is submitted every second, two workers will be active to process the tasks.
- if the rate of submitted tasks is very high, the maximum number of workers (as passed to `threadpool_create_and_start ()`) will be active.

Therefore, the number for workers automatically adapts to the rate and duration for tasks.

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
- `NB_CPU` can be used as first argument to fit to the number of processors currently available in the system (as returned by `get_nprocs ()`).
- `SEQUENTIAL` can be used as first argument to create a sequential thread pool: tasks will be executed asynchronously, one at a time, and in the order there were submitted. 

The maximum number of workers can be defined higher than the number of CPUs as workers will be started only when solicited and will be released when unused after an idle time.

###### Options

- The second argument `global_data`, if not null, is a pointer to global data.
  This pointer can next be retrieved by tasks with the function `threadpool_global_data ()`.
- The third argument `make_local`, if not null, is a user defined function that returns a pointer to data for a local usage by a worker.
  This pointer can next be retrieved by tasks with the function `threadpool_worker_local_data ()`.
  `make_local` is called in a thread-safe manner at the initialization of a worker and is passed the pointer to global data.
  `make_local` can therefore safely access (and update) the content of `global_data` if needed.
- The fourth argument `delete_local`, if not null, is a user defined function that is executed to release and destroys the local data used by each worker (passed as an argument) when a worker stops.
  `delete_local` is called in a thread-safe manner at the termination of a worker and is passed the pointer to global data.
  `delete_local` can therefore safely access (and update) the content of `global_data` if needed (to gather results or statistics for instance).

### 2. Submit a task

```c
int threadpool_add_task (struct threadpool *threadpool,
                         void (*work) (struct threadpool * threadpool, void *job),
                         void *job,
                         void (*job_delete) (void *job))
```

A task is submitted to the thread pool with a call to `threadpool_add_task ()`.

- The first argument `threadpool` is a thread pool returned by a previous call to `threadpool_create_and_start ()`.
- The second argument `work`, mandatory, is a user defined function executed by a worker of the thread pool (in a parallel manner).
  This function receives the thread pool and a pointer to a job as arguments.
  Therefore, `work` can itself call `threadpool_add_task`, `threadpool_global_data` or `threadpool_worker_local_data` if needed.
- The third argument `job` is the data to be used by the task.

Each worker can process a task at a time. A task is processed as soon as a worker is available. 
A worker stops when all submitted tasks have been processed or after an idle time (half a second).

###### Options

- The fourth argument `job_delete`, if not null, is a user defined function called at termination of the task.
  This function receives the job of the task as an argument.
  It is called in a thread-safe manner and can therefore safely aggregate results to those of previous tasks for instance. 

`job_delete` should be used if the job was allocated dynamically in order to release and destroy the data after use.
`free ()` is a possible choice for `job_delete`.

`job_delete` can also be used to do more than simply release jobs, for instance to retrieve data used and possibly modified by the processed task in a thread-safe manner. 
For instance, a job could be declared as a structure containing the input data of the task and its result.
The result could then be retrieved by the user defined function `job_delete` in a multi-thread safe manner and aggregated in a global result. 

```c
typedef struct {
  struct { ... } input;
  struct { ... } result;
} job;
```

### 3. Wait for all the submitted tasks to be completed

```c
void threadpool_wait_and_destroy (struct threadpool *threadpool)
```

The single argument `threadpool` is a thread pool returned by a previous call to `threadpool_create_and_start ()`.
This function declares that all the tasks have been submitted.
It then waits for all the tasks to be completed by workers.
`threadpool` should not be used after a call to `threadpool_wait_and_destroy ()`.

### 4. Monitoring of the thread pool activity

A monitoring of the thread pool can optionally be activated by calling
```c
threadpool_monitor_handler threadpool_set_monitor (struct threadpool *threadpool, threadpool_monitor_handler new)
```

A user-defined handler function is passed as second argument, with signature
```c
void (*threadpool_monitor_handler) (struct threadpool_monitor)
```

This handler will be called asynchronously (without interfering with the execution of workers) when the state of the thread pool changes, and will be executed thread-safely and not after `threadpool_wait_and_destroy` has been called (actually, a sequential dedicated thread pool is used).
The monitoring data are passed to the handler function in the structure `threadpool_monitor` which contains:

- `struct threadpool *threadpool`: the thread pool for which the monitoring handler is called ;
- `float time`: the elapsed seconds since the creation of the thread pool ;
- `size_t max_nb_workers`: the maximum number of workers, as defined at the creation of the thread pool ;
- `size_t nb_running_workers`: the number of running workers, either idle, or active ;
- `size_t nb_active_workers`: the number of active workers, i.e. processing a task ;
- `size_t nb_idle_workers`: the number of idle worker, i.e. waiting for a task to process ;
- `size_t nb_pending_tasks`: the number of tasks submitted to the thread pool and not yet processed or being processed ;
- `size_t nb_processed_tasks`: the number of already processed tasks by the thread pool.

## Examples

An example of the usage of thread pool is given in files `qsip_wc.c` and `qsip_wc_test.c`.

- `qsip_wc.c` is an attempt to implement a parallelized version of the quick sort algorithm (using a thread pool);

    - It uses global data, worker local data, dynamic creation and deletion of jobs.

- `qsip_wc_test.c` is an example of a thread pool that sorts several arrays using the above parallelized version of the quick sort algorithm.

    - It uses global data, worker local data, and monitoring feature.

Running this example yields:
```
$ CFLAGS='-DTIMES=4' make -B
clang -DTIMES=4 -Wall -O3   -c -o wqm.o wqm.c
clang -DTIMES=4 -Wall -O3   -c -o qsip_wc.o qsip_wc.c
clang -DTIMES=4 -Wall -O3    qsip_wc_test.c qsip_wc.o wqm.o   -o qsip_wc_test
wqm.o:0000000000000000 R NB_CPU
wqm.o:0000000000000008 R SEQUENTIAL
wqm.o:0000000000000300 T threadpool_add_task
wqm.o:0000000000000080 T threadpool_create_and_start
wqm.o:0000000000000900 T threadpool_global_data
wqm.o:0000000000000000 T threadpool_set_monitor
wqm.o:0000000000000800 T threadpool_wait_and_destroy
wqm.o:00000000000008f0 T threadpool_worker_local_data
qsip_wc.o:0000000000000000 T qsip

$ ./qsip_wc_test
Sorting 1,000,000 elements (multi-threaded quick sort in place), 4 times...
(X) processed tasks, (*) processing tasks, (-) idle workers, (.) pending tasks. 
[0x64c89c7c4860:     0.0000s]        
[0x64c89c7c4860:     0.0001s] .       
[0x64c89c7c4860:     0.0002s] ..       
[0x64c89c7c4860:     0.0002s] *.       
[0x64c89c7c4860:     0.0003s] **       
[0x64c89c7c4860:     0.0004s] **.       
[0x64c89c7c4860:     0.0005s] **..       
[0x64c89c7c4860:     0.0005s] ***.       
[0x64c89c7c4860:     0.0025s] ****       
[0x64c89c7c4860:     0.5893s] X***-       
[0x64c89c7c4860:     0.6510s] XX**--       
[0x64c89c7c4860:     0.6893s] XX**-       
[0x64c89c7c4860:     0.7149s] XXX*--       
[0x64c89c7c4860:     0.7511s] XXX*-       
[0x64c89c7c4860:     0.7756s] XXXX-       
[0x64c89c7c4860:     0.7756s] XXXX       
```

## That's it. Have fun and let me know!

> Zed is dead, but C is not.