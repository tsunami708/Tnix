#include "task/task.h"
#include "mem/vm.h"

void
init_systemd(void)
{
  extern void first_sched(void);

  struct task* t = alloc_task(NULL);
  t->tname = "systemd";
  t->lock.lname = "systemd-lock";
  t->ctx.ra = (u64)first_sched;
  t->ctx.sp = t->kstack;
  t->state = READY;
}
