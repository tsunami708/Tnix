#include "util/spinlock.h"
#include "util/printf.h"
#include "task/cpu.h"

static bool
holding_spin(struct spinlock* lock)
{
  return lock->locked == true && lock->cpu == mycpu();
}

void
acquire_spin(struct spinlock* lock)
{
  push_intr(); // 防止因中断导致锁重入
  if (holding_spin(lock))
    panic("cpu%d repeat acquire %s", cpuid(), lock->lname);

  while (__sync_lock_test_and_set(&lock->locked, true) != 0)
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
  __sync_lock_release(&lock->locked, false);
  pop_intr();
}

/*
  note: cpu可能在某个执行流中获取多个自旋锁,在临界区内必须保证不被中断,武断地acquire~cli,release~sti会
  ! 导致一个锁被释放而开中断,但cpu可能还处在另一个锁的临界区中,容易死锁
*/