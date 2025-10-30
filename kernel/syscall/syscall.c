#include "syscall/syscall.h"
#include "trap/pt_reg.h"
#include "util/printf.h"
#include "util/string.h"
#include "task/task.h"
#include "mem/vm.h"


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
  u64 ret = syscalls[sysno](pt);
  mypt()->a0 = ret; //* 对于fork的妥协,必须使用mypt()获取pt
}

u64
sys_fork(struct pt_regs* pt)
{
  extern void dump_context(struct context * ctx);

  struct task* p = mytask();
  struct task* c = alloc_task(p);
  memcpy1((void*)c->kstack - PGSIZE, (void*)p->kstack - PGSIZE, PGSIZE);
  dump_context(&c->ctx);
  if (mytask() == p) {
    u64 sp;
    asm volatile("mv %0, sp" : "=r"(sp));
    c->ctx.sp = c->kstack - (p->kstack - sp);
    c->state = READY;
    return c->pid;
  } else {
    release_spin(&mytask()->lock);
    return 0;
  }
}

u64
sys_exit(struct pt_regs* pt)
{
  return 1;
}
u64
sys_exec(struct pt_regs* pt)
{
  return 1;
}