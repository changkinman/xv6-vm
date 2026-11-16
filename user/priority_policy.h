#define PRIORITY_MIN 0
#define PRIORITY_MAX 31
#define PRIORITY_DEFAULT 15
#define PRIORITY_AGING_INTERVAL 10

// PrioritySlot is scheduling metadata only. It is not the Process record
// required by the task 7 performance-test program.
typedef struct {
  int pid;
  int priority;
  int wait_ticks;
  int ready;
} PrioritySlot;

void priority_slot_init(PrioritySlot *slot, int pid);
int priority_set(PrioritySlot *slot, int priority);
void priority_set_ready(PrioritySlot *slot, int ready);
void priority_age_ready(PrioritySlot slots[], int count, int running_pid);
int priority_effective(const PrioritySlot *slot);
int priority_select_next(const PrioritySlot slots[], int count);
int priority_should_preempt(const PrioritySlot *current,
                            const PrioritySlot *candidate);
void priority_record_dispatch(PrioritySlot *slot);
