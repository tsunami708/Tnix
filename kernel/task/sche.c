#include "config.h"
#include "task.h"
#include "cpu.h"

extern struct task task_queue[NPROC];

extern void context_switch(struct context* old, struct context* new);

void
yield()
{
  struct task* t = mytask();
  acquire_spin(&t->lock); //``
  t->state = READY;
  context_switch(&t->ctx, &mycpu()->ctx);
  release_spin(&t->lock); //*
}

void
task_schedule()
{
  while (1) {
    for (int i = 0; i < NPROC; ++i) {
      struct task* t = task_queue + i;
      struct cpu*  c = mycpu();
      acquire_spin(&t->lock); //*
      if (t->state == READY) {
        t->state      = RUN;
        c->cur_task   = t;
        c->cur_kstack = t->kstack;
        c->cur_ptb    = t->pagetable;
        context_switch(&c->ctx, &t->ctx);
      }
      release_spin(&t->lock); //``
    }
  }
}