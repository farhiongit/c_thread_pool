/*
    - map digit to text
    - filter text with length equal to 3
    - count the number of elements found
 */
#ifndef __MFR_H__
  #include "wqm.h"

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
    void (*aggregator) (void *aggregate, void *job);    // Job aggregator, aggregate 'job' into 'aggregate'
  } reducer;
  void (*job_deletor) (void *); //  Job cleaner (after aggregation). Freeing job should be done here if necessary.
};

tp_task_t threadpool_add_stream (struct threadpool *threadpool, void *job);
struct threadpool *threadpool_create_and_start_stream (size_t nb_workers, struct stream *stream, tp_property_t property);

#endif
