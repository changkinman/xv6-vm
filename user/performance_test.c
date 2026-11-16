#include "kernel/types.h"
#include "user/user.h"
#include "priority_policy.h"
#include "round_robin_policy.h"
#include "multilevel_queue_policy.h"

#define TEST_PROCESS_COUNT 5
#define NO_MIGRATION (-1)

enum test_process_state {
  TEST_NOT_ARRIVED,
  TEST_READY,
  TEST_RUNNING,
  TEST_FINISHED,
};

// The experiment-required record. Arrival time is based on an uptime()
// baseline; the scheduler then advances a reproducible simulated tick clock.
struct Process {
  int pid;
  int arrival_time;
  int burst_time;
  int start_time;
  int completion_time;
  int waiting_time;
  int turnaround_time;
  int response_time;
};

typedef struct {
  int pid;
  int arrival_offset;
  int burst_time;
  int priority;
  ProcessClass process_class;
  int migration_offset;
  ProcessClass migration_target;
} TestDefinition;

typedef struct {
  int remaining_time;
  int state;
  int priority;
  ProcessClass process_class;
  int migration_offset;
  ProcessClass migration_target;
  int migrated;
} ProcessRuntime;

typedef struct {
  ProcessRuntime *runtime;
  int start_tick;
  int finish_tick;
} RoundRobinContext;

static RoundRobinContext rr_context;

static TestDefinition cpu_intensive[TEST_PROCESS_COUNT] = {
  {1, 0, 4, 3, BATCH_PROCESS, NO_MIGRATION, BATCH_PROCESS},
  {2, 0, 2, 1, BATCH_PROCESS, NO_MIGRATION, BATCH_PROCESS},
  {3, 0, 5, 4, BATCH_PROCESS, NO_MIGRATION, BATCH_PROCESS},
  {4, 0, 3, 2, BATCH_PROCESS, NO_MIGRATION, BATCH_PROCESS},
  {5, 0, 1, 5, BATCH_PROCESS, NO_MIGRATION, BATCH_PROCESS},
};

static TestDefinition staggered_arrivals[TEST_PROCESS_COUNT] = {
  {1, 0, 4, 4, BATCH_PROCESS, NO_MIGRATION, BATCH_PROCESS},
  {2, 1, 2, 1, INTERACTIVE_PROCESS, NO_MIGRATION, INTERACTIVE_PROCESS},
  {3, 2, 5, 3, BATCH_PROCESS, NO_MIGRATION, BATCH_PROCESS},
  {4, 3, 3, 2, SYSTEM_PROCESS, NO_MIGRATION, SYSTEM_PROCESS},
  {5, 4, 1, 5, INTERACTIVE_PROCESS, NO_MIGRATION, INTERACTIVE_PROCESS},
};

static TestDefinition mixed_workload[TEST_PROCESS_COUNT] = {
  {1, 0, 2, 2, SYSTEM_PROCESS, NO_MIGRATION, SYSTEM_PROCESS},
  {2, 1, 3, 1, INTERACTIVE_PROCESS, NO_MIGRATION, INTERACTIVE_PROCESS},
  {3, 0, 5, 4, BATCH_PROCESS, NO_MIGRATION, BATCH_PROCESS},
  {4, 3, 2, 3, INTERACTIVE_PROCESS, NO_MIGRATION, INTERACTIVE_PROCESS},
  {5, 2, 4, 5, BATCH_PROCESS, 5, INTERACTIVE_PROCESS},
};

static void
reset_processes(Process processes[], ProcessRuntime runtime[],
                TestDefinition definitions[], int count, int start_tick)
{
  int i;

  for(i = 0; i < count; i++) {
    processes[i].pid = definitions[i].pid;
    processes[i].arrival_time = start_tick + definitions[i].arrival_offset;
    processes[i].burst_time = definitions[i].burst_time;
    processes[i].start_time = -1;
    processes[i].completion_time = -1;
    processes[i].waiting_time = 0;
    processes[i].turnaround_time = 0;
    processes[i].response_time = -1;

    runtime[i].remaining_time = definitions[i].burst_time;
    runtime[i].state = TEST_NOT_ARRIVED;
    runtime[i].priority = definitions[i].priority;
    runtime[i].process_class = definitions[i].process_class;
    runtime[i].migration_offset = definitions[i].migration_offset;
    runtime[i].migration_target = definitions[i].migration_target;
    runtime[i].migrated = 0;
  }
}

static int
all_finished(ProcessRuntime runtime[], int count)
{
  int i;

  for(i = 0; i < count; i++) {
    if(runtime[i].state != TEST_FINISHED)
      return 0;
  }
  return 1;
}

static void
record_first_dispatch(Process *p, int now)
{
  if(p->start_time >= 0)
    return;

  p->start_time = now;
  p->response_time = p->start_time - p->arrival_time;
}

static void
record_completion(Process *p, ProcessRuntime *runtime, int now)
{
  runtime->state = TEST_FINISHED;
  p->completion_time = now;
  p->turnaround_time = p->completion_time - p->arrival_time;
  p->waiting_time = p->turnaround_time - p->burst_time;
}

static void
print_average(const char *name, int total, int count)
{
  printf("average %s: %d.%d ticks\n", name, total / count,
         (total % count) * 10 / count);
}

static void
print_metrics(const char *case_name, const char *algorithm,
              Process processes[], int count)
{
  int total_waiting = 0;
  int total_turnaround = 0;
  int total_response = 0;
  int i;

  printf("\n%s - %s\n", case_name, algorithm);
  printf("pid arrival burst start complete wait turnaround response\n");
  for(i = 0; i < count; i++) {
    printf("%d   %d       %d     %d     %d        %d    %d          %d\n",
           processes[i].pid, processes[i].arrival_time, processes[i].burst_time,
           processes[i].start_time, processes[i].completion_time,
           processes[i].waiting_time, processes[i].turnaround_time,
           processes[i].response_time);
    total_waiting += processes[i].waiting_time;
    total_turnaround += processes[i].turnaround_time;
    total_response += processes[i].response_time;
  }

  print_average("waiting time", total_waiting, count);
  print_average("turnaround time", total_turnaround, count);
  print_average("response time", total_response, count);
}

static void
priority_admit_arrivals(Process processes[], ProcessRuntime runtime[],
                        PrioritySlot slots[], int count, int now)
{
  int i;

  for(i = 0; i < count; i++) {
    if(runtime[i].state == TEST_NOT_ARRIVED && processes[i].arrival_time <= now) {
      runtime[i].state = TEST_READY;
      priority_set_ready(&slots[i], 1);
    }
  }
}

static int
priority_dispatch(Process processes[], ProcessRuntime runtime[],
                  PrioritySlot slots[], int index, int now)
{
  priority_set_ready(&slots[index], 0);
  priority_record_dispatch(&slots[index]);
  runtime[index].state = TEST_RUNNING;
  record_first_dispatch(&processes[index], now);
  return index;
}

static void
priority_schedule(Process processes[], ProcessRuntime runtime[], int count,
                  int start_tick)
{
  PrioritySlot slots[MAX_PROCESSES];
  int now = start_tick;
  int running = -1;
  int i;

  for(i = 0; i < count; i++) {
    priority_slot_init(&slots[i], processes[i].pid);
    priority_set(&slots[i], runtime[i].priority);
  }

  while(!all_finished(runtime, count)) {
    int candidate;

    priority_admit_arrivals(processes, runtime, slots, count, now);
    candidate = priority_select_next(slots, count);

    if(running >= 0 && candidate >= 0 &&
       priority_should_preempt(&slots[running], &slots[candidate])) {
      runtime[running].state = TEST_READY;
      priority_set_ready(&slots[running], 1);
      running = priority_dispatch(processes, runtime, slots, candidate, now);
    }

    if(running < 0 && candidate >= 0)
      running = priority_dispatch(processes, runtime, slots, candidate, now);

    if(running < 0) {
      now++;
      continue;
    }

    runtime[running].remaining_time--;
    now++;
    priority_age_ready(slots, count, processes[running].pid);

    if(runtime[running].remaining_time == 0) {
      record_completion(&processes[running], &runtime[running], now);
      priority_set_ready(&slots[running], 0);
      running = -1;
    }
  }
}

static void
rr_admit_arrivals(Process processes[], ProcessRuntime runtime[],
                  RoundRobinQueue *rq, int count, int now)
{
  int i;

  for(i = 0; i < count; i++) {
    if(runtime[i].state == TEST_NOT_ARRIVED && processes[i].arrival_time <= now) {
      runtime[i].state = TEST_READY;
      rr_enqueue(rq, &processes[i]);
    }
  }
}

// The handout requires this exact function signature. Runtime metadata is
// supplied by the performance-test context prepared before the call.
void
round_robin_schedule(Process processes[], int n, int quantum)
{
  RoundRobinQueue rq;
  ProcessRuntime *runtime = rr_context.runtime;
  int now = rr_context.start_tick;

  rr_queue_init(&rq, quantum);
  while(!all_finished(runtime, n)) {
    Process *p;
    int index;
    int used;

    rr_admit_arrivals(processes, runtime, &rq, n, now);
    p = rr_dequeue(&rq);
    if(p == 0) {
      now++;
      continue;
    }

    index = (int)(p - processes);
    runtime[index].state = TEST_RUNNING;
    record_first_dispatch(p, now);

    for(used = 0; used < rq.time_quantum && runtime[index].remaining_time > 0;
        used++) {
      runtime[index].remaining_time--;
      now++;
      rr_admit_arrivals(processes, runtime, &rq, n, now);
    }

    if(runtime[index].remaining_time == 0) {
      record_completion(p, &runtime[index], now);
    } else {
      runtime[index].state = TEST_READY;
      rr_enqueue(&rq, p);
    }
  }
  rr_context.finish_tick = now;
}

static int
runtime_index(Process processes[], Process *p)
{
  return (int)(p - processes);
}

static void
mlq_admit_arrivals(Process processes[], ProcessRuntime runtime[],
                   MultiLevelQueue *mlq, int count, int now)
{
  int i;

  for(i = 0; i < count; i++) {
    if(runtime[i].state == TEST_NOT_ARRIVED && processes[i].arrival_time <= now) {
      runtime[i].state = TEST_READY;
      mlq_assign_process(mlq, &processes[i], runtime[i].process_class);
    }
  }
}

static void
mlq_apply_migrations(Process processes[], ProcessRuntime runtime[],
                     MultiLevelQueue *mlq, int count, int now)
{
  int i;

  for(i = 0; i < count; i++) {
    if(runtime[i].state == TEST_READY && !runtime[i].migrated &&
       runtime[i].migration_offset != NO_MIGRATION &&
       now >= processes[i].arrival_time + runtime[i].migration_offset) {
      if(mlq_migrate_process(mlq, &processes[i], runtime[i].process_class,
                             runtime[i].migration_target) == 0) {
        runtime[i].process_class = runtime[i].migration_target;
        runtime[i].migrated = 1;
      }
    }
  }
}

static int
mlq_quantum_for_class(MultiLevelQueue *mlq, ProcessClass type)
{
  if(type == SYSTEM_PROCESS)
    return mlq->high_priority.time_quantum;
  if(type == INTERACTIVE_PROCESS)
    return mlq->medium_priority.time_quantum;
  return mlq->low_priority.time_quantum;
}

static void
multilevel_queue_schedule(Process processes[], ProcessRuntime runtime[],
                          int count, int start_tick)
{
  MultiLevelQueue mlq;
  int now = start_tick;

  mlq_init(&mlq, 1, 2, 4);
  while(!all_finished(runtime, count)) {
    Process *p;
    int index;
    int used;
    int quantum;

    mlq_admit_arrivals(processes, runtime, &mlq, count, now);
    mlq_apply_migrations(processes, runtime, &mlq, count, now);
    p = mlq_dispatch_next(&mlq);
    if(p == 0) {
      now++;
      continue;
    }

    index = runtime_index(processes, p);
    runtime[index].state = TEST_RUNNING;
    record_first_dispatch(p, now);
    quantum = mlq_quantum_for_class(&mlq, runtime[index].process_class);

    for(used = 0; used < quantum && runtime[index].remaining_time > 0; used++) {
      runtime[index].remaining_time--;
      now++;
      mlq_admit_arrivals(processes, runtime, &mlq, count, now);
      mlq_apply_migrations(processes, runtime, &mlq, count, now);
    }

    if(runtime[index].remaining_time == 0) {
      record_completion(p, &runtime[index], now);
    } else {
      runtime[index].state = TEST_READY;
      mlq_assign_process(&mlq, p, runtime[index].process_class);
    }
  }
}

static void
run_case(const char *case_name, TestDefinition definitions[], int count)
{
  Process processes[TEST_PROCESS_COUNT];
  ProcessRuntime runtime[TEST_PROCESS_COUNT];
  int start_tick = uptime();

  reset_processes(processes, runtime, definitions, count, start_tick);
  priority_schedule(processes, runtime, count, start_tick);
  print_metrics(case_name, "priority scheduling", processes, count);

  reset_processes(processes, runtime, definitions, count, start_tick);
  rr_context.runtime = runtime;
  rr_context.start_tick = start_tick;
  round_robin_schedule(processes, count, 2);
  print_metrics(case_name, "round robin", processes, count);

  reset_processes(processes, runtime, definitions, count, start_tick);
  multilevel_queue_schedule(processes, runtime, count, start_tick);
  print_metrics(case_name, "multilevel queue", processes, count);
}

int
main(void)
{
  printf("Experiment 4 scheduling performance test\n");
  printf("All burst times are theoretical CPU ticks (1 tick = about 10 ms).\n");

  run_case("CPU-intensive workload", cpu_intensive, TEST_PROCESS_COUNT);
  run_case("staggered-arrival workload", staggered_arrivals, TEST_PROCESS_COUNT);
  run_case("mixed workload", mixed_workload, TEST_PROCESS_COUNT);
  exit(0);
}
