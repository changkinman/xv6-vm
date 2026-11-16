#ifndef MULTILEVEL_QUEUE_POLICY_H
#define MULTILEVEL_QUEUE_POLICY_H

#include "round_robin_policy.h"

typedef enum {
  SYSTEM_PROCESS,
  INTERACTIVE_PROCESS,
  BATCH_PROCESS,
} ProcessClass;

typedef struct {
  RoundRobinQueue high_priority;
  RoundRobinQueue medium_priority;
  RoundRobinQueue low_priority;
} MultiLevelQueue;

void mlq_init(MultiLevelQueue *mlq, int high_quantum, int medium_quantum,
              int low_quantum);
int mlq_assign_process(MultiLevelQueue *mlq, Process *p, ProcessClass type);
Process *mlq_dispatch_next(MultiLevelQueue *mlq);
int mlq_migrate_process(MultiLevelQueue *mlq, Process *p,
                        ProcessClass from, ProcessClass to);

#endif
