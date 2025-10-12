#include "config.h"
#include "task.h"
#include "cpu.h"
#include "printf.h"

extern struct task task_queue[NPROC];

extern char trampoline[];
extern char utrap_entry[];
extern char run_new_task[];

extern void context_switch(struct context* old, struct context* new);

void
first_sched()
{
  struct task* t = mytask();
  release_spin(&t->lock);
  u64 addr = (u64)run_new_task - (u64)trampoline + TRAMPOLINE;

  cli();
  /*
    需要在进入用户态之前设置trap向量表寄存器stvec
    在将stvec设置为utrap_entry和sret这段代码期间不可以发生中断
  */

  w_stvec((u64)utrap_entry - (u64)trampoline + TRAMPOLINE);
  w_sstatus((r_sstatus() & ~SSTATUS_SPP) | SSTATUS_SPIE);
  w_sepc(t->entry);
  print("CPU %u first-sche systemd\n", cpuid());
  ((void (*)(u64, u64))addr)(mycpu()->cur_satp, t->ustack);
}

void
yield()
{
  print("CPU %u yield systemd\n", cpuid());
  struct task* t = mytask();
  acquire_spin(&t->lock); //``
  t->state = READY;
  context_switch(&t->ctx, &mycpu()->ctx);
  release_spin(&t->lock); //*
  print("CPU %u re-sche systemd\n", cpuid());
}

void
task_schedule()
{
  while (1) {
    for (int i = 0; i < 2; ++i) {
      struct task* t = task_queue + i;
      struct cpu*  c = mycpu();
      acquire_spin(&t->lock); //*
      if (t->state == READY) {
        t->state      = RUN;
        c->cur_task   = t;
        c->cur_kstack = t->kstack;
        c->cur_satp   = SATP_MODE | ((u64)t->pagetable >> 12);
        context_switch(&c->ctx, &t->ctx);
      }
      release_spin(&t->lock); //``
    }
  }
}