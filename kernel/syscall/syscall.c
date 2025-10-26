#include "syscall/syscall.h"
#include "trap/pt_reg.h"
#include "util/printf.h"

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