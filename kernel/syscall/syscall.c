#include "syscall/syscall.h"
#include "trap/pt_reg.h"
#include "util/printf.h"
#include "util/string.h"
#include "task/task.h"
#include "task/sche.h"
#include "mem/vm.h"
#include "fs/file.h"
#include "fs/elf.h"

extern void context_switch(struct context* old, struct context* new);
extern void first_sched(void);

static syscall_t syscalls[] = {
  [SYS_FORK] sys_fork,
  [SYS_EXIT] sys_exit,
  [SYS_EXEC] sys_exec,
  [SYS_WAIT] sys_wait,
};

static const char* sysfunc[] = {
  [SYS_FORK] "fork",
  [SYS_EXIT] "exit",
  [SYS_EXEC] "exec",
  [SYS_WAIT] "wait",
};

void
do_syscall(struct pt_regs* pt)
{
  int sysno = pt->a7;
  print("cpu%d call syscall - %s\n", cpuid(), sysfunc[sysno]);
  if (sysno < 0 || sysno > sizeof(syscalls) / sizeof(syscall_t) - 1)
    panic("illegal syscall");
  u64 ret = syscalls[sysno](pt);
  mypt()->a0 = ret; //* 对于fork的妥协,必须使用mypt()获取pt
}

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
    pte_t* pte;
    u64 paddr = va_to_pa(mytask()->pagetable, pt->a0, &pte);
    if (paddr == 0 || ((*pte) & (PTE_U | PTE_R)) != (PTE_U | PTE_R))
      kill();
    char path[128] = { 0 };
    int len = strlen((char*)paddr);
    if (len + 1 > sizeof(path))
      goto bad;
    memcpy(path, (void*)paddr, len);
    struct elfhdr eh;
    struct file* f = read_elfhdr(path, &eh);
    if (f == NULL)
      goto bad;

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
bad:
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