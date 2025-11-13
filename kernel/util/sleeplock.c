#include "util/sleeplock.h"
#include "util/printf.h"

#include "task/cpu.h"
#include "task/task.h"
#include "task/sche.h"

bool
sleep_holding(struct sleeplock* lock)
{
  return lock->locked == true && lock->task == mytask();
}

void
sleep_get(struct sleeplock* lock)
{
  if (sleep_holding(lock))
    panic("sleep_get: task %s ~ lock %s", mytask()->tname, lock->lname);
  while (__sync_lock_test_and_set(&lock->locked, true) != 0)
    sleep(lock, NULL);
  __sync_synchronize();
  lock->task = mytask();
}

void
sleep_put(struct sleeplock* lock)
{
  if (! sleep_holding(lock))
    panic("sleep_put: task %s ~ lock %s", mytask()->tname, lock->lname);
  lock->task = NULL;
  __sync_synchronize();
  __sync_lock_release(&lock->locked, false);
  wakeup(lock);
}