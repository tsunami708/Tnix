#include "config.h"
#include "task/task.h"
#include "task/cpu.h"
#include "util/printf.h"
#include "task/elf.h"
#include "fs/file.h"

extern struct task task_queue[NPROC];

extern char trampoline[];
extern char utrap_entry[];
extern char run_new_task[];

extern void context_switch(struct context* old, struct context* new);
extern void fsinit(void);
void
first_sched(void)
{
  struct task* t = mytask();
  spin_put(&t->lock);
  u64 _start = (u64)run_new_task - (u64)trampoline + TRAMPOLINE; // run_new_task的高虚拟地址

  static bool first = true; // 第一次调度init
  if (first) {
    first = false;
    __sync_synchronize();
    fsinit();
    struct file* f;
    struct elfhdr eh;
    if ((f = read_elfhdr("/bin/init", &eh))) {
      load_segment(t, f, &eh);
      t->entry = eh.entry;
      fclose(f);
    }
  }
  cli();
  /*
      需要在进入用户态之前设置trap向量表寄存器stvec
    ! 在将stvec设置为utrap_entry和sret这段代码期间不可以发生中断
  */

  w_stvec((u64)utrap_entry - (u64)trampoline + TRAMPOLINE);
  w_sstatus((r_sstatus() & ~SSTATUS_SPP) | SSTATUS_SPIE);
  w_sepc(t->entry);
  ((void (*)(u64, u64, u64))_start)(mycpu()->cur_satp, t->ustack, t->ustack == USTACK + PGSIZE ? 0 : t->ustack);
  // t->ustack == USTACK + PGSIZE表示没有选项参数
}

void
yield(void)
{
  struct task* t = mytask();
  spin_get(&t->lock); //``
  t->state = READY;
  context_switch(&t->ctx, &mycpu()->ctx);
  spin_put(&t->lock); //*
}

void
sleep(void* chan, struct spinlock* lock) // 调用sleep时最多只能持有1个自旋锁
{
  struct task* t = mytask();

  spin_get(&t->lock);
  if (lock)
    spin_put(lock); //! lock必须在获得t->lock后再释放,防止唤醒丢失

  t->chan = chan;
  t->state = SLEEP;
  context_switch(&t->ctx, &mycpu()->ctx);
  t->chan = NULL;
  spin_put(&t->lock);
  if (lock)
    spin_get(lock); //! 重新申请因等待而被暂时释放的自旋锁
}


void
wakeup(void* chan)
{
  for (int i = 0; i < NPROC; ++i) {
    struct task* t = task_queue + i;
    if (t != mytask()) {
      spin_get(&t->lock);
      if (t->state == SLEEP && t->chan == chan)
        t->state = READY;
      spin_put(&t->lock);
    }
  }
}

void
kill(void)
{
  extern void context_switch(struct context * old, struct context * new);

  struct task* t = mytask();
  t->exit_code = 255;

  clean_source(t);

  t->state = EXIT;
  wakeup(&t->parent->childs);

  spin_get(&t->lock);
  context_switch(&t->ctx, &mycpu()->ctx);
}

void
task_schedule(void)
{
  while (1) {
    sti();
    cli();
    for (int i = 0; i < NPROC; ++i) {
      struct task* t = task_queue + i;
      struct cpu* c = mycpu();
      spin_get(&t->lock);
      if (t->state == READY) {
        t->state = RUN;
        c->cur_task = t;
        c->cur_kstack = t->kstack;
        c->cur_satp = SATP_MODE | ((u64)t->pagetable >> 12);
        context_switch(&c->ctx, &t->ctx);
      }
      c->cur_task = NULL; //! 不要在释放线程锁后置空,可能会被中断
      spin_put(&t->lock);
    }
  }
}