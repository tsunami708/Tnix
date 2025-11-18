#include "task/task.h"
#include "mem/vm.h"
#include "util/string.h"
void
init_proc1(void)
{
  extern void first_sched(void);

  struct task* t = alloc_task(NULL);
  strcpy(t->tname, "init");
  t->lock.lname = "systemd-lock";
  t->ctx.ra = (u64)first_sched;
  t->ctx.sp = t->kstack;
  t->state = READY;
}
