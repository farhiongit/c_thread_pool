// Map, filter, reduce pattern parallelised by a thread pool.
// (c) L. Farhi, 2025
#include "wqm.h"
#include "mfr.h"

// ------- Map, filter, reduce pattern framework ------------
static tp_result_t
mapfilter (void *job)
{
  struct stream *stream = threadpool_global_data ();
  if (!stream)
    return TP_JOB_FAILURE;
  tp_result_t ret = TP_JOB_SUCCESS;
  for (size_t i = 0; i < stream->nb_mappers && ret == TP_JOB_SUCCESS; i++)
    if (stream->mappers[i].f)
      ret = stream->mappers[i].f (job, stream->mappers[i].arg); // Map or filter
  return ret;
}

static void
reduce (void *job, tp_result_t ret)
{
  if (ret == TP_JOB_SUCCESS)
  {
    struct stream *stream = threadpool_global_data ();
    if (stream->reducer.aggregator && stream->reducer.aggregate)
      stream->reducer.aggregator (stream->reducer.aggregate, job);      // Reduce
    if (stream->job_deletor)
      stream->job_deletor (job);        // Delete job after use
  }
}

tp_task_t
threadpool_add_task_to_stream (struct threadpool *threadpool, void *job)
{
  return threadpool_add_task (threadpool, mapfilter, job, reduce);
}

struct threadpool *
threadpool_create_and_start_stream (size_t nb_workers, struct stream *stream, tp_property_t property)
{
  return threadpool_create_and_start (nb_workers, stream, property);
}
