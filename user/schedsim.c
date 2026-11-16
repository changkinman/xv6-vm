#include "kernel/types.h"
#include "user/user.h"

#define NPROCESS 5
#define PRIORITY_MIN 0
#define PRIORITY_MAX 31
#define PRIORITY_AGING_INTERVAL 10

// Policy identifiers used by the Makefile's SCHED_POLICY setting.
#define SCHED_PRIORITY 1
#define SCHED_RR       2
#define SCHED_MLFQ     3

#ifndef SCHED_POLICY
#define SCHED_POLICY SCHED_PRIORITY
#endif

#if SCHED_POLICY != SCHED_PRIORITY
#error "Task 4 implements only SCHED_PRIORITY; tasks 5 and 6 add the other policies"
#endif

enum process_state {
  NOT_ARRIVED,
  READY,
  RUNNING,
  FINISHED,
};

// This is the experiment's simulated-process record. It is deliberately
// independent of xv6's kernel struct proc.
typedef struct {
  int pid;
  int priority;        // Base priority: 0 is highest.
  int arrival_time;    // Arrival tick, recorded with uptime().
  int burst_time;      // Theoretical CPU burst, in simulated ticks.
  int start_time;      // First simulated dispatch time.
  int completion_time; // Simulated completion time.
  int waiting_time;
  int turnaround_time;
  int response_time;
  int remaining_time;  // Internal state needed by a preemptive simulation.
  int wait_ticks;      // Internal aging counter while READY.
  int state;
} Process;

static int
effective_priority(Process *p)
{
  int priority = p->priority - p->wait_ticks / PRIORITY_AGING_INTERVAL;

  if(priority < PRIORITY_MIN)
    return PRIORITY_MIN;
  return priority;
}

static void
record_arrivals(Process processes[])
{
  // A tick is 10 ms in xv6. The configured gaps create different arrival
  // times while uptime() records the values required by the experiment.
  int arrival_gap[NPROCESS] = {0, 1, 2, 3, 4};
  int burst_ticks[NPROCESS] = {4, 2, 5, 3, 1};
  int priority[NPROCESS] = {4, 1, 3, 2, 5};
  int experiment_start = uptime();
  int i;

  for(i = 0; i < NPROCESS; i++) {
    while(uptime() - experiment_start < arrival_gap[i])
      ;

    processes[i].pid = i + 1;
    processes[i].priority = priority[i];
    processes[i].arrival_time = uptime() - experiment_start;
    processes[i].burst_time = burst_ticks[i];
    processes[i].start_time = -1;
    processes[i].completion_time = -1;
    processes[i].waiting_time = 0;
    processes[i].turnaround_time = 0;
    processes[i].response_time = -1;
    processes[i].remaining_time = burst_ticks[i];
    processes[i].wait_ticks = 0;
    processes[i].state = NOT_ARRIVED;
  }
}

static void
admit_arrivals(Process processes[], int now)
{
  int i;

  for(i = 0; i < NPROCESS; i++) {
    if(processes[i].state == NOT_ARRIVED && processes[i].arrival_time <= now)
      processes[i].state = READY;
  }
}

static int
pick_highest_priority(Process processes[])
{
  int best = -1;
  int i;

  for(i = 0; i < NPROCESS; i++) {
    if(processes[i].state != READY)
      continue;

    if(best < 0 || effective_priority(&processes[i]) < effective_priority(&processes[best]) ||
       (effective_priority(&processes[i]) == effective_priority(&processes[best]) &&
        processes[i].arrival_time < processes[best].arrival_time) ||
       (effective_priority(&processes[i]) == effective_priority(&processes[best]) &&
        processes[i].arrival_time == processes[best].arrival_time &&
        processes[i].pid < processes[best].pid))
      best = i;
  }
  return best;
}

static void
age_waiting_processes(Process processes[], int running)
{
  int i;

  for(i = 0; i < NPROCESS; i++) {
    if(i != running && processes[i].state == READY)
      processes[i].wait_ticks++;
  }
}

static int
all_finished(Process processes[])
{
  int i;

  for(i = 0; i < NPROCESS; i++) {
    if(processes[i].state != FINISHED)
      return 0;
  }
  return 1;
}

static void
print_results(Process processes[])
{
  int i;
  int total_waiting = 0;
  int total_turnaround = 0;
  int total_response = 0;

  printf("\npid pri arrival burst start complete wait turnaround response\n");
  for(i = 0; i < NPROCESS; i++) {
    printf("%d   %d   %d       %d     %d     %d        %d    %d          %d\n",
           processes[i].pid, processes[i].priority, processes[i].arrival_time,
           processes[i].burst_time, processes[i].start_time,
           processes[i].completion_time, processes[i].waiting_time,
           processes[i].turnaround_time, processes[i].response_time);
    total_waiting += processes[i].waiting_time;
    total_turnaround += processes[i].turnaround_time;
    total_response += processes[i].response_time;
  }

  printf("average waiting time: %d/%d ticks\n", total_waiting, NPROCESS);
  printf("average turnaround time: %d/%d ticks\n", total_turnaround, NPROCESS);
  printf("average response time: %d/%d ticks\n", total_response, NPROCESS);
}

static void
priority_schedule(Process processes[])
{
  int now = 0;

  printf("priority scheduling: lower number means higher priority\n");
  while(!all_finished(processes)) {
    int running;
    Process *p;

    admit_arrivals(processes, now);
    running = pick_highest_priority(processes);
    if(running < 0) {
      now++;
      continue;
    }

    p = &processes[running];
    p->state = RUNNING;
    if(p->start_time < 0) {
      p->start_time = now;
      p->response_time = p->start_time - p->arrival_time;
    }

    printf("tick %d: run P%d (base=%d, effective=%d)\n",
           now, p->pid, p->priority, effective_priority(p));
    p->wait_ticks = 0;
    p->remaining_time--;
    now++;

    age_waiting_processes(processes, running);
    if(p->remaining_time == 0) {
      p->state = FINISHED;
      p->completion_time = now;
      p->turnaround_time = p->completion_time - p->arrival_time;
      p->waiting_time = p->turnaround_time - p->burst_time;
    } else {
      p->state = READY;
    }
  }
}

int
main(void)
{
  Process processes[NPROCESS];

  record_arrivals(processes);
  priority_schedule(processes);
  print_results(processes);
  exit(0);
}
