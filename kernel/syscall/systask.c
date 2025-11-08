#include "syscall/syscall.h"
#include "trap/pt_reg.h"
#include "util/string.h"
#include "util/printf.h"
#include "task/task.h"
#include "task/sche.h"
#include "mem/vm.h"
#include "fs/file.h"
#include "fs/elf.h"

extern void context_switch(struct context* old, struct context* new);
extern void first_sched(void);


u64
sys_fork(struct pt_regs*)
{
  extern void dump_context(struct context * ctx);

  struct task* p = mytask();
  struct task* c = alloc_task(p);
  list_pushback(&p->childs, &c->self);
  memcpy((void*)c->kstack - PGSIZE, (void*)p->kstack - PGSIZE, PGSIZE);
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
  struct task* t = mytask();
  t->exit_code = pt->a0;

  clean_source(t);

  t->state = EXIT;
  wakeup(&t->parent->childs);

  print("process %d done\n", t->pid);

  acquire_spin(&t->lock);
  context_switch(&t->ctx, &mycpu()->ctx);
  return 0;
}

u64
sys_exec(struct pt_regs* pt)
{
  if (pt->a0) {
    char path[MAX_PATH_LENGTH] = { 0 };
    if (argstr(pt->a0, path) == false)
      return -1;
    struct elfhdr eh;
    struct file* f = read_elfhdr(path, &eh);
    if (f == NULL)
      return -1;

    struct task* t = mytask();
    reset_vma(t);
    load_segment(t, f, &eh);
    t->entry = eh.entry;
    t->ctx.ra = (u64)first_sched;
    t->ctx.sp = t->kstack;
    t->state = READY;
    acquire_spin(&t->lock);
    context_switch(NULL, &mycpu()->ctx);
  }
  return -1;
}

u64
sys_wait(struct pt_regs* pt)
{
  struct task* t = mytask();
  if (t->childs.next == &t->childs)
    return -1;

  while (1) {
    struct list_node* child = t->childs.next;
    while (child != &t->childs) {
      struct task* c = container_of(child, struct task, self);
      if (c->state == EXIT) {
        u16 cpid = c->pid;
        list_remove(&c->self);
        if (pt->a0)
          copy_to_user((void*)pt->a0, &c->exit_code, sizeof(c->exit_code));
        c->state = FREE;
        return cpid;
      }
      child = child->next;
    }
    sleep(&t->childs, NULL);
  }
}