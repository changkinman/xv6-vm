#ifndef ROUND_ROBIN_POLICY_H
#define ROUND_ROBIN_POLICY_H

#ifndef MAX_PROCESSES
#define MAX_PROCESSES 64
#endif

typedef struct Process Process;

typedef struct {
  Process *queue[MAX_PROCESSES];
  int front;
  int rear;
  int time_quantum;
} RoundRobinQueue;

void rr_queue_init(RoundRobinQueue *rq, int time_quantum);
int rr_queue_empty(const RoundRobinQueue *rq);
int rr_queue_full(const RoundRobinQueue *rq);
int rr_enqueue(RoundRobinQueue *rq, Process *p);
Process *rr_dequeue(RoundRobinQueue *rq);
int rr_remove(RoundRobinQueue *rq, Process *p);
// A parameter declared as Process processes[] is adjusted by C to Process *.
// The pointer spelling is required here because Process is completed only by
// the task 7 performance-test program after this header is included.
void round_robin_schedule(Process *processes, int n, int quantum);

#endif
