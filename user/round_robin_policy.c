#include "round_robin_policy.h"

void
rr_queue_init(RoundRobinQueue *rq, int time_quantum)
{
  rq->front = 0;
  rq->rear = 0;
  rq->time_quantum = time_quantum;
}

int
rr_queue_empty(const RoundRobinQueue *rq)
{
  return rq->front == rq->rear;
}

int
rr_queue_full(const RoundRobinQueue *rq)
{
  return (rq->rear + 1) % MAX_PROCESSES == rq->front;
}

int
rr_enqueue(RoundRobinQueue *rq, Process *p)
{
  if(rr_queue_full(rq))
    return -1;

  rq->queue[rq->rear] = p;
  rq->rear = (rq->rear + 1) % MAX_PROCESSES;
  return 0;
}

Process *
rr_dequeue(RoundRobinQueue *rq)
{
  Process *p;

  if(rr_queue_empty(rq))
    return 0;

  p = rq->queue[rq->front];
  rq->front = (rq->front + 1) % MAX_PROCESSES;
  return p;
}

int
rr_remove(RoundRobinQueue *rq, Process *p)
{
  int read = rq->front;
  int write = rq->front;
  int removed = 0;

  while(read != rq->rear) {
    Process *current = rq->queue[read];

    if(!removed && current == p) {
      removed = 1;
    } else {
      rq->queue[write] = current;
      write = (write + 1) % MAX_PROCESSES;
    }
    read = (read + 1) % MAX_PROCESSES;
  }

  if(!removed)
    return -1;

  rq->rear = write;
  return 0;
}

