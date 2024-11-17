// Multi-threaded work crew
// (c) L. Farhi, 2024
// Language: C
#ifndef __THREADPOOL_H__
#  define __THREADPOOL_H__
#  include <stddef.h>           // for size_t
#  include <stdio.h>            // for __GLIBC__

struct threadpool;              // Abstract data type : opaque record of a thread pool

// 'threadpool_create_and_start' creates a threadpool of 'nb_workers' workers.
// If 'nb_workers' is 0 or 'NB_CPU', the number of workers is set equal to the number of available CPUs.
// Arguments 'global_data', 'make_local', 'delete_local' are optional (see below).
// Returns 0 (with errno = ENOMEM) on error, a pointer to the created threadpool otherwise (with errno = ENOMEM if not all required workers could be created).
#  ifdef __GLIBC__
extern const size_t NB_CPU;
#  endif
extern const size_t SEQUENTIAL;
struct threadpool *threadpool_create_and_start (size_t nb_workers, void *global_data, void *(*make_local) (void), void (*delete_local) (void *local_data));

// Call to 'threadpool_add_task' is MT-safe.
// Tasks can be submitted to workers. They will be processed in parallel distributed over workers of the threadpool.
// The submitted work is the user defined function 'work'. 'work' should be made MT-safe. `work' should return 0 on success, non 0 otherwise.
// The submitted job is defined by 'job'.
//   - 'job' will be passed to 'work' when processed by a worker.
//   - 'job' will be destroyed by a call to 'job_delete' once the job has been processed by the worker.
// 'threadpool' is passed to 'work' to give it access to 'threadpool_add_task' and 'threadpool_global_data' if needed.
// Therefore, a worker can also create and submit tasks on his own.
// Argument 'job_delete' is optional (see below).
// Returns 0 on error, a unique id of the submitted task otherwise.
// Set errno to ENOMEM on error (out of memory).
size_t threadpool_add_task (struct threadpool *threadpool, int (*work) (struct threadpool * threadpool, void *job), void *job, void (*job_delete) (void *job));

// Cancel a pending task identified by its unique id, as returned by threadpool_add_task, or all tasks if task_id is equal to ALL_TASKS, or the next submitted task if task_id is equal to NEXT_TASK, or the last if equal to LAST_TASK.
// Returns the number of canceled tasks.
extern const size_t ALL_TASKS;
extern const size_t NEXT_TASK;
extern const size_t LAST_TASK;
size_t threadpool_cancel_task (struct threadpool *threadpool, size_t task_id);

// Once all tasks have been submitted to the threadpool, 'threadpool_wait_and_destroy' waits for all the tasks to be finished and thereafter destroys the threadpool.
// 'threadpool' should not be used after a call to 'threadpool_wait_and_destroy'.
void threadpool_wait_and_destroy (struct threadpool *threadpool);

// ** Options for 'threadpool_create_and_start' **
// Global data pointed to by 'global_data' will be accessible to worker through a call to 'threadpool_global_data'.
void *threadpool_global_data (void);

// Workers local data constructed by 'make_local' and destroyed by 'delete_local' will be (MT-safely) accessible to worker through a call to 'threadpool_worker_local_data'.
// Call to 'make_local' is MT-safe and, if not null, is done once per worker thread (no less no more) at worker initialization.
// Call to 'delete_local' is MT-safe and, if not null, is done once per worker thread (no less no more) at worker termination.
// 'delete_local' is passed, as first argument, a value previously returned by 'make_local'.
// Global data (as if returned by threadpool_global_data) is passed as an argument to 'make_local' and second argument to 'delete_local'.
// 'make_local' and 'delete_local' can therefore access and update the content of 'global_data' safely if needed (to gather results or statistics for instance).
void *threadpool_worker_local_data (void);

// ** Options for 'threadpool_add_job' **
// Call to 'job_delete' is MT-safe and, if not null, is done once per job (no less no more) right after the job has been completed by 'worker'.
// 'job_delete' is passed, as argument, the 'job' added by 'threadpool_add_job'.
// 'job_delete' is useful if the 'job' passed to 'threadpool_add_job' has been allocated dynamically and needs to be free'd after use.
// This could alternatively (and less conveniently) be done manually at the end for 'worker'.

// Modify the idle timeout delay (in seconds, default is 0.1 s).
void threadpool_set_idle_timeout (struct threadpool *threadpool, double delay);

// Manage global resources for all tasks.
// allocator will be called before the first tasks is processed, deallocator after the last tasks has been processed.
// Resources will be deallocated and reallocated after idle timeout.
void threadpool_set_resource_manager (struct threadpool *threadpool, void *(*allocator) (void *global_data), void (*deallocator) (void *resource));

// Global data pointed to by 'global_data' will be accessible to worker through a call to 'threadpool_global_data'.
void *threadpool_global_resource (void);

struct threadpool_monitor
{
  const struct threadpool *threadpool;  // The monitored Thread pool.
  double time;                  // Elapsed seconds since thread pool creation.
  struct
  {
    size_t nb_max, nb_idle;
  } workers;                    // Monitoring workers.
  struct
  {
    size_t nb_submitted, nb_pending, nb_processing, nb_succeeded, nb_failed, nb_canceled;
  } tasks;                      // Monitoring tasks.
};
typedef void (*threadpool_monitor_handler) (struct threadpool_monitor, void *arg);
// Set monitor handler.
// Monitor handler will be called asynchronously (without interfering with the execution of workers) and executed thread-safely and not after `threadpool_wait_and_destroy` has been called.
void threadpool_set_monitor (struct threadpool *threadpool, threadpool_monitor_handler new, void *arg);

// A monitor handler to FILE stream.
void threadpool_monitor_to_terminal (struct threadpool_monitor data, void *FILE_stream);
#endif
