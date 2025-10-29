#include "syscall/syscall.h"
#include "trap/pt_reg.h"
#include "util/printf.h"
#include "mem/vm.h"
#include "task/task.h"

static syscall_t syscalls[] = {
  [SYS_FORK] sys_fork,
  [SYS_EXIT] sys_exit,
  [SYS_EXEC] sys_exec,
};

void
do_syscall(struct pt_regs* pt)
{
  int sysno = pt->a7;
  if (sysno < 0 || sysno > sizeof(syscalls) / sizeof(syscall_t) - 1)
    panic("illegal syscall");
  pt->a0 = syscalls[sysno]();
}

u64
sys_fork(void)
{
  scan_pagetable(mytask()->pagetable);
  struct task* t = alloc_task();
  copy_pagetable(t, mytask());
  print("\n");
  scan_pagetable(t->pagetable);
  return 100;
}
u64
sys_exit(void)
{
  return 1;
}
u64
sys_exec(void)
{
  return 1;
}