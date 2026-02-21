// Map, filter, reduce pattern parallelised by a thread pool.
// (c) L. Farhi, 2025
#undef NDEBUG
#include "wqm.h"
#include "mfr.h"
#include <assert.h>

// ------- Map, filter, reduce pattern framework ------------

// Returns the status of the last unsuccessful mapper or filter.
// A mapper or a filter function f can return either:
//   - TP_JOB_SUCCESS to continue process the stream ;
//   - TP_JOB_FAILURE to ignore the stream ;
//   - TP_JOB_CANCELED to interrupt the threadpool.
static tp_result_t
mapfilter (void *job)
{
  struct stream *stream = threadpool_global_data ();
  assert (stream);
  tp_result_t ret = TP_JOB_SUCCESS;
  for (size_t i = 0; i < stream->nb_mappers && ret == TP_JOB_SUCCESS; i++)
    if (stream->mappers[i].f)
      ret = stream->mappers[i].f (job, stream->mappers[i].arg); // Map or filter
  return ret;
}

// Returns, by decreasing priority:
//   - TP_JOB_CANCELED if the threadpool was interrupted.
//   - if all mappers and filters were processed successfully,
//     - the result of the deletor, if it is defined and unsuccessful,
//     - or the result of the aggregator (if it is defined)
//     - or TP_JOB_SUCCESS
//   - otherwise, the result of the deletor, if it is defined and unsuccessful,
//   - otherwise, TP_JOB_SUCCESS.
// If TP_JOB_FAILURE is returned, the threadpool is interrupted (all subsequent tasks are cancelled).
// Therefore, if the aggregator returns either TP_JOB_CANCELED or TP_JOB_FAILURE, the threadpool is interrupted.
static tp_result_t
reduce (void *job, tp_result_t ret)
{
  struct stream *stream = threadpool_global_data ();
  assert (stream);
  if (ret == TP_JOB_CANCELED)
  {
    if (stream->deletor)
      stream->deletor (job);
    stream->rejecting = 1;
    return TP_JOB_FAILURE;      // Interrupt the threadpool.
  }
  if (ret == TP_JOB_SUCCESS)
  {
    if (stream->reducer.aggregator && stream->reducer.aggregate)
      ret = stream->reducer.aggregator (stream->reducer.aggregate, job);        // Reduce
    if (ret != TP_JOB_SUCCESS)  // The aggregator can return either TP_JOB_CANCELED or TP_JOB_FAILURE to interrupt the threadpool.
      ret = TP_JOB_FAILURE;     // Interrupt the threadpool.
  }
  else
    ret = TP_JOB_SUCCESS;       // A failing job is not aggregated and does not interrupt the threadpool.
  if (stream->deletor)          // Delete job after use, being it reduced or not.
    stream->deletor (job);
  if (stream->rejecting)        // Test AFTER aggregation.
    ret = TP_JOB_FAILURE;       // Interrupt the threadpool.
  else if (ret != TP_JOB_SUCCESS)
    stream->rejecting = 1;      // Interrupt the threadpool.
  return ret;
}

static tp_result_t
breakorcontinue (void *job, void *arg, tp_result_t res)
{
  void **args = arg;
  tp_result_t (*filter) (void *job, void *arg) = args[0];
  void *filter_arg = args[1];
  if (filter && filter (job, filter_arg) != TP_JOB_SUCCESS)
    return res;
  else
    return TP_JOB_SUCCESS;
}

// ------- User interface and helpers ------------
tp_result_t
interrupt (void *job, void *arg)
{
  struct stream *stream = threadpool_global_data ();
  assert (stream);
  void **args = arg;
  tp_result_t (*filter) (void *job, void *arg) = args[0];
  void *filter_arg = args[1];
  if (filter && filter (job, filter_arg) == TP_JOB_SUCCESS)
    stream->rejecting = 1;
  return TP_JOB_SUCCESS;
}

tp_result_t
takewhile (void *job, void *arg)
{
  return breakorcontinue (job, arg, TP_JOB_CANCELED);   // break
}

tp_result_t
dropuntil (void *job, void *arg)
{
  return breakorcontinue (job, arg, TP_JOB_FAILURE);    // continue
}

tp_task_t
threadpool_add_task_to_stream (struct threadpool *threadpool, void *job)
{
  return threadpool_add_task (threadpool, mapfilter, job, reduce);
}

struct threadpool *
threadpool_create_and_start_stream (size_t nb_workers, struct stream *stream)
{
  return threadpool_create_and_start (nb_workers, stream, TP_RUN_ALL_SUCCESSFUL_TASKS);
}
