#include "spinlock.h"
#include "cpu.h"
#include "printf.h"

static bool
holding_spin(struct spinlock* lock)
{
  return lock->locked == true && lock->cpu == mycpu();
}

void
acquire_spin(struct spinlock* lock)
{
  cli(); // 防止因中断导致锁重入
  if (holding_spin(lock))
    panic("cpu%d repeat acquire %s", cpuid(), lock->lname);

  while (__sync_lock_test_and_set(&lock->locked, 1) != 0)
    ;
  __sync_synchronize();
  lock->cpu = mycpu();
}

void
release_spin(struct spinlock* lock)
{
  if (!holding_spin(lock))
    panic("cpu%d illegal release %s", cpuid(), lock->lname);
  lock->cpu = NULL;
  __sync_synchronize();
  __sync_lock_release(&lock->locked);
  sti();
}
