typedef struct {
  int *queue;
  int capacity;
  int front;
  int rear;
  int count;
  int time_quantum;
} RoundRobinQueue;

int rr_queue_init(RoundRobinQueue *rq, int storage[], int capacity,
                  int time_quantum);
int rr_queue_empty(const RoundRobinQueue *rq);
int rr_queue_full(const RoundRobinQueue *rq);
int rr_enqueue(RoundRobinQueue *rq, int pid);
int rr_dequeue(RoundRobinQueue *rq);
int rr_dispatch_next(RoundRobinQueue *rq);
int rr_requeue_after_quantum(RoundRobinQueue *rq, int pid, int still_ready);
