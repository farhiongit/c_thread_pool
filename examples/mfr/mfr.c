// Map, filter, reduce pattern parallelised by a thread pool.
// (c) L. Farhi, 2025
#include "wqm.h"
#include "mfr.h"
#undef NDEBUG
#include <assert.h>

// ------- Map, filter, reduce pattern framework ------------
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
//   - TP_JOB_CANCELED if the stream was interrupted.
//   - if all mappers and filters were processed successfully,
//     - the result of the deletor, if it is defined and unsuccessful,
//     - or the result of the aggregator (if it is defined)
//     - or TP_JOB_SUCCESS
//   - otherwise, the result of the deletor, if it is defined and unsuccessful,
//   - otherwise, TP_JOB_SUCCESS.
// If TP_JOB_FAILURE is returned, the stream is interrupted (all subsequent tasks are cancelled).
// Therefore, if the aggregator returns either TP_JOB_CANCELED or TP_JOB_FAILURE, the stream is interrupted.
static tp_result_t
reduce (void *job, tp_result_t ret)
{
  struct stream *stream = threadpool_global_data ();
  assert (stream);
  if (ret == TP_JOB_CANCELED || stream->rejecting)
  {
    if (stream->job_deletor)
      stream->job_deletor (job);
    return TP_JOB_CANCELED;
  }
  if (ret == TP_JOB_SUCCESS)
  {
    if (stream->reducer.aggregator && stream->reducer.aggregate)
      ret = stream->reducer.aggregator (stream->reducer.aggregate, job);        // Reduce
    if (ret == TP_JOB_CANCELED)
      ret = TP_JOB_FAILURE;     // The aggregator can return either TP_JOB_CANCELED or TP_JOB_FAILURE to interrupt the stream.
  }
  else
    ret = TP_JOB_SUCCESS;
  if (stream->job_deletor && stream->job_deletor (job) != TP_JOB_SUCCESS)       // Delete job after use, being it reduced or not.
    ret = TP_JOB_FAILURE;       // The deletor can return TP_JOB_FAILURE to interrupt the stream.
  if (ret == TP_JOB_FAILURE)
    stream->rejecting = 1;
  return ret;
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
