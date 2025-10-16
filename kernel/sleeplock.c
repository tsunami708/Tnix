#include "sleeplock.h"
#include "cpu.h"
#include "task.h"
#include "sche.h"
#include "printf.h"

static bool
holding_sleep(struct sleeplock* lock)
{
  return lock->locked == true && lock->task == mytask();
}

void
acquire_sleep(struct sleeplock* lock)
{
  if (holding_sleep(lock))
    panic("task %s repeat acquire lock %s", mytask()->tname, lock->lname);
  while (__sync_lock_test_and_set(&lock->locked, 1) != 0)
    sleep(lock, NULL);
  __sync_synchronize();
  lock->task = mytask();
}

void
release_sleep(struct sleeplock* lock)
{
  if (!holding_sleep(lock))
    panic("task %s illegal release lock %s", mytask()->tname, lock->lname);
  lock->task = NULL;
  __sync_synchronize();
  __sync_lock_release(&lock->locked);
  wakeup(lock);
}