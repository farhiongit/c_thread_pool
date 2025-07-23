// Map, filter, reduce pattern parallelised by a thread pool.
// (c) L. Farhi, 2025
#ifndef __MFR_H__
#  include "wqm.h"

// ------- Map, filter, reduce pattern framework ------------
struct stream
{
  size_t nb_mappers;
  struct mapper                 // Mapper or filter
  {
    tp_result_t (*f) (void *job, void *arg);    // Function
    void *arg;                  // Argument
  } *mappers;
  struct reducer                // Reducer, called MT safely
  {
    void *aggregate;            // Result of the stream after aggregation
      tp_result_t (*aggregator) (void *aggregate, void *job);   // Job aggregator, aggregate 'job' into 'aggregate'
  } reducer;
    tp_result_t (*job_deletor) (void *);        //  Job cleaner (after aggregation). Freeing job should be done here if necessary.
  int rejecting;                // Should be left unset to 0 at initialisation.
};

tp_task_t threadpool_add_task_to_stream (struct threadpool *threadpool, void *job);
struct threadpool *threadpool_create_and_start_stream (size_t nb_workers, struct stream *stream);

#endif
