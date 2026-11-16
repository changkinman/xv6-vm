#include "round_robin_policy.h"

int
rr_queue_init(RoundRobinQueue *rq, int storage[], int capacity,
              int time_quantum)
{
  if(capacity <= 0 || time_quantum <= 0)
    return -1;

  rq->queue = storage;
  rq->capacity = capacity;
  rq->front = 0;
  rq->rear = 0;
  rq->count = 0;
  rq->time_quantum = time_quantum;
  return 0;
}

int
rr_queue_empty(const RoundRobinQueue *rq)
{
  return rq->count == 0;
}

int
rr_queue_full(const RoundRobinQueue *rq)
{
  return rq->count == rq->capacity;
}

int
rr_enqueue(RoundRobinQueue *rq, int pid)
{
  if(rr_queue_full(rq))
    return -1;

  rq->queue[rq->rear] = pid;
  rq->rear = (rq->rear + 1) % rq->capacity;
  rq->count++;
  return 0;
}

int
rr_dequeue(RoundRobinQueue *rq)
{
  int pid;

  if(rr_queue_empty(rq))
    return -1;

  pid = rq->queue[rq->front];
  rq->front = (rq->front + 1) % rq->capacity;
  rq->count--;
  return pid;
}

int
rr_dispatch_next(RoundRobinQueue *rq)
{
  return rr_dequeue(rq);
}

int
rr_requeue_after_quantum(RoundRobinQueue *rq, int pid, int still_ready)
{
  if(!still_ready)
    return 0;
  return rr_enqueue(rq, pid);
}
