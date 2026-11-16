#include "priority_policy.h"

void
priority_slot_init(PrioritySlot *slot, int pid)
{
  slot->pid = pid;
  slot->priority = PRIORITY_DEFAULT;
  slot->wait_ticks = 0;
  slot->ready = 0;
}

int
priority_set(PrioritySlot *slot, int priority)
{
  if(priority < PRIORITY_MIN || priority > PRIORITY_MAX)
    return -1;

  slot->priority = priority;
  slot->wait_ticks = 0;
  return 0;
}

void
priority_set_ready(PrioritySlot *slot, int ready)
{
  slot->ready = ready;
  if(!ready)
    slot->wait_ticks = 0;
}

int
priority_effective(const PrioritySlot *slot)
{
  int effective = slot->priority -
                  slot->wait_ticks / PRIORITY_AGING_INTERVAL;

  if(effective < PRIORITY_MIN)
    return PRIORITY_MIN;
  return effective;
}

void
priority_age_ready(PrioritySlot slots[], int count, int running_pid)
{
  int i;
  int max_wait = (PRIORITY_MAX - PRIORITY_MIN) * PRIORITY_AGING_INTERVAL;

  for(i = 0; i < count; i++) {
    if(slots[i].ready && slots[i].pid != running_pid &&
       slots[i].wait_ticks < max_wait)
      slots[i].wait_ticks++;
  }
}

int
priority_select_next(const PrioritySlot slots[], int count)
{
  int best = -1;
  int i;

  for(i = 0; i < count; i++) {
    if(!slots[i].ready)
      continue;

    if(best < 0 || priority_effective(&slots[i]) < priority_effective(&slots[best]) ||
       (priority_effective(&slots[i]) == priority_effective(&slots[best]) &&
        slots[i].pid < slots[best].pid))
      best = i;
  }
  return best;
}

int
priority_should_preempt(const PrioritySlot *current,
                        const PrioritySlot *candidate)
{
  return priority_effective(candidate) < priority_effective(current);
}

void
priority_record_dispatch(PrioritySlot *slot)
{
  slot->wait_ticks = 0;
}
