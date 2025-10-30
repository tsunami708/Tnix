#include "config.h"
#include "task/task.h"
#include "mem/vm.h"
#include "syscall/usys.h"

// desc: systemd的执行逻辑,此函数在U模式下执行
__attribute__((section(".init.text"))) void
systemd_main(void)
{
  int r = fork1();
  (void)r;
  while (1)
    ;
  exit1(0);
}

extern char init[];
extern char einit[];
#define UINIT      ((u64)(init))
#define UINIT_SIZE (align_up((u64)(einit), PGSIZE) - UINIT)

void
init_systemd(void)
{
  extern void first_sched(void);

  struct task* t = alloc_task(NULL);
  t->tname = "systemd";
  t->lock.lname = "systemd-lock";
  task_vmmap(t, UINIT, UINIT, UINIT_SIZE, PTE_R | PTE_X | PTE_U, S_PAGE);
  t->entry = (u64)systemd_main;
  t->ctx.ra = (u64)first_sched;
  t->ctx.sp = t->kstack;
  t->state = READY;
}
