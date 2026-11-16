#include "multilevel_queue_policy.h"

static RoundRobinQueue *
mlq_queue_for_class(MultiLevelQueue *mlq, ProcessClass type)
{
  switch(type) {
  case SYSTEM_PROCESS:
    return &mlq->high_priority;
  case INTERACTIVE_PROCESS:
    return &mlq->medium_priority;
  case BATCH_PROCESS:
    return &mlq->low_priority;
  }
  return 0;
}

void
mlq_init(MultiLevelQueue *mlq, int high_quantum, int medium_quantum,
         int low_quantum)
{
  rr_queue_init(&mlq->high_priority, high_quantum);
  rr_queue_init(&mlq->medium_priority, medium_quantum);
  rr_queue_init(&mlq->low_priority, low_quantum);
}

int
mlq_assign_process(MultiLevelQueue *mlq, Process *p, ProcessClass type)
{
  RoundRobinQueue *rq = mlq_queue_for_class(mlq, type);

  if(rq == 0)
    return -1;
  return rr_enqueue(rq, p);
}

Process *
mlq_dispatch_next(MultiLevelQueue *mlq)
{
  Process *p;

  p = rr_dequeue(&mlq->high_priority);
  if(p != 0)
    return p;

  p = rr_dequeue(&mlq->medium_priority);
  if(p != 0)
    return p;

  return rr_dequeue(&mlq->low_priority);
}

int
mlq_migrate_process(MultiLevelQueue *mlq, Process *p,
                    ProcessClass from, ProcessClass to)
{
  RoundRobinQueue *source = mlq_queue_for_class(mlq, from);
  RoundRobinQueue *destination = mlq_queue_for_class(mlq, to);

  if(source == 0 || destination == 0 || source == destination)
    return -1;
  if(rr_queue_full(destination))
    return -1;
  if(rr_remove(source, p) < 0)
    return -1;
  return rr_enqueue(destination, p);
}
