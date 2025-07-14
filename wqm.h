// Multi-threaded work crew
// (c) L. Farhi, 2024
// Language: C
#ifndef __THREADPOOL_H__
#  define __THREADPOOL_H__
#  include <stddef.h>           // for size_t
#  include <stdio.h>            // for __GLIBC__
#  include <stdint.h>

struct threadpool;              // Abstract data type : opaque record of a thread pool

// 'threadpool_create_and_start' creates a threadpool of 'nb_workers' workers.
// If 'nb_workers' is 0 or 'NB_CPU', the number of workers is set equal to the number of available CPUs.
// Arguments 'global_data' can hold a global context of the thread pool and is optional.
// Returns 0 (with errno = ENOMEM) on error, a pointer to the created threadpool otherwise (with errno = ENOMEM if not all required workers could be created).
#  ifdef __GLIBC__
extern const size_t TP_WORKER_NB_CPU;
#  endif
extern const size_t TP_WORKER_SEQUENTIAL;

typedef int tp_property_t;
extern const tp_property_t TP_RUN_ALL_TASKS;    // Runs all submitted tasks.
extern const tp_property_t TP_RUN_ALL_SUCCESSFUL_TASKS; // Runs submitted tasks until one fails. Cancel automatically other (already or to be) submitted tasks.
extern const tp_property_t TP_RUN_ONE_SUCCESSFUL_TASK;  // Runs submitted tasks until one succeeds. Cancel automatically other (already or to be) submitted tasks.
struct threadpool *threadpool_create_and_start (size_t nb_workers, void *global_data, tp_property_t property);

// Returns the current thread pool.
void *threadpool_current (void);

// Global data pointed to by 'global_data' will be accessible through a call to 'threadpool_global_data'.
void *threadpool_global_data (void);

typedef int tp_result_t;
extern const tp_result_t TP_JOB_SUCCESS;
extern const tp_result_t TP_JOB_FAILURE;
extern const tp_result_t TP_JOB_CANCELED;

// Call to 'threadpool_add_task' is MT-safe.
// Tasks can be submitted to workers. They will be processed in parallel distributed over workers of the threadpool.
// The submitted work is the user defined function 'work'. 'work' should be made MT-safe. `work' should return 0 on success, non 0 otherwise.
// The submitted job is defined by 'job'.
//   - 'job' will be passed to 'work' when processed by a worker.
//   - 'job' will be destroyed by a call to 'job_delete' once the job has been processed by the worker.
// 'threadpool' is passed to 'work' to give it access to 'threadpool_add_task' if needed.
// Therefore, a worker can also create and submit tasks on his own.
// Argument 'job_delete' is optional (see below).
// Returns 0 on error, a unique id of the submitted task otherwise.
// Set errno to ENOMEM on error (out of memory).
typedef size_t tp_task_t;
tp_task_t threadpool_add_task (struct threadpool *threadpool, tp_result_t (*work) (void *job), void *job, void (*job_delete) (void *job, tp_result_t result));

// A handler is provided for convenience. It calls 'free' on 'job', whatever the value of 'result'.
void threadpool_job_free_handler (void *job, tp_result_t result);

// ** Options for 'threadpool_add_job' **
// Call to 'job_delete' is MT-safe and, if not null, is done once per job (no less no more) right after the job has been completed by 'worker'.
// 'job_delete' is passed, as argument, the 'job' added by 'threadpool_add_job', as well as its result.
// 'job_delete' can be used if the 'job' passed to 'threadpool_add_job' has been allocated dynamically and needs to be free'd after use.

// Cancel a pending task identified by its unique id, as returned by threadpool_add_task, or all tasks if task_id is equal to ALL_PENDING_TASKS, or the next submitted task if task_id is equal to NEXT_PENDING_TASK, or the last if equal to LAST_PENDING_TASK.
// Returns the number of cancelled tasks.
extern const tp_task_t TP_CANCEL_ALL_PENDING_TASKS;     // Cancels all pending tasks
extern const tp_task_t TP_CANCEL_NEXT_PENDING_TASK;     // Cancels next pending task (in submission order)
extern const tp_task_t TP_CANCEL_LAST_PENDING_TASK;     // Cancels last pending tasks (in submission order)
size_t threadpool_cancel_task (struct threadpool *threadpool, tp_task_t task_id);

// Once all tasks have been submitted to the threadpool, 'threadpool_wait_and_destroy' waits for all the tasks to be finished and thereafter destroys the threadpool.
// 'threadpool' should not be used after a call to 'threadpool_wait_and_destroy'.
void threadpool_wait_and_destroy (struct threadpool *threadpool);

// Manage local data of workers.
// 'make_local' will be called when a worker is created and 'delete_local' when it is terminated.
// Call to 'make_local' is MT-safe and, if not null, is done once per worker thread (no less no more) at worker initialisation.
// Call to 'delete_local' is MT-safe and, if not null, is done once per worker thread (no less no more) at worker termination.
void threadpool_set_worker_local_data_manager (struct threadpool *threadpool, void *(*make_local) (void), void (*delete_local) (void *local_data));

// Workers local data constructed by 'make_local' and destroyed by 'delete_local' will be (MT-safely) accessible to worker through a call to 'threadpool_worker_local_data'.
// 'delete_local' is passed, as first argument, a value previously returned by 'make_local'.
void *threadpool_worker_local_data (void);

// Modify the idle timeout delay (in seconds, default is 0.1 s).
void threadpool_set_idle_timeout (struct threadpool *threadpool, double delay);

// Manage global resources for all tasks.
// allocator will be called before the first task is processed, deallocator after the last tasks has been processed.
// Resources will be deallocated and reallocated automatically after idle timeout.
void threadpool_set_global_resource_manager (struct threadpool *threadpool, void *(*allocator) (void *global_data), void (*deallocator) (void *resource));

// Global resources allocated with `allocator` will be accessible through a call to 'threadpool_global_resource'.
void *threadpool_global_resource (void);

struct threadpool_monitor
{
  const struct threadpool *threadpool;  // The monitored Thread pool.
  double time;                  // Elapsed seconds since thread pool creation.
  int closed;
  struct
  {
    size_t nb_requested, nb_max, nb_idle, nb_alive;
  } workers;                    // Monitoring workers.
  struct
  {
    size_t nb_submitted, nb_pending, nb_asynchronous, nb_processing, nb_succeeded, nb_failed, nb_canceled;
  } tasks;                      // Monitoring tasks.
};
typedef void (*threadpool_monitor_handler) (struct threadpool_monitor, void *arg);
// Set monitor handler.
// Monitor handler will be called asynchronously (without interfering with the execution of workers) and executed thread-safely and not after `threadpool_wait_and_destroy` has been called.
void threadpool_set_monitor (struct threadpool *threadpool, threadpool_monitor_handler displayer, void *arg, int (*filter) (struct threadpool_monitor d));

// A convenient monitor handler to monitor text to FILE stream.
void threadpool_monitor_to_terminal (struct threadpool_monitor data, void *FILE_stream);
// A convenient filter to monitor at most every 100 ms.
int threadpool_monitor_every_100ms (struct threadpool_monitor d);
// A function to call the monitor occasionally (shoulb be used seldom).
void threadpool_monitor (struct threadpool *threadpool);

// Virtual tasks (calling asynchronous jobs).
// Declare the task continuation and the time out, in seconds. Returns the UID of the continuator.
uint64_t threadpool_task_continuation (tp_result_t (*work) (void *data), double seconds);
// Call the task continuation. Returns TP_JOB_SUCCESS if the continuator UID was previously declared and has not timed out, TP_JOB_FAILURE (with errno set to ETIMEDOUT) otherwise.
tp_result_t threadpool_task_continue (uint64_t uid);
#endif
