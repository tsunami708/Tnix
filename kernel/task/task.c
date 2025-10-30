#include "task/task.h"
#include "mem/alloc.h"
#include "mem/vm.h"
#include "util/spinlock.h"
#include "util/printf.h"


INIT_SPINLOCK(tq);
struct task task_queue[NPROC];

INIT_SPINLOCK(ti);
static u16 tid = 1;

static u16
alloc_tid()
{
  u16 r;
  acquire_spin(&ti);
  r = tid++;
  release_spin(&ti);
  return r;
}

static void
task_init(struct task* t)
{
  extern u64 kernel_satp;
  extern char trampoline[];

  t->pid = t->tid = alloc_tid();
  list_init(&t->pages);

  //* 分配页表
  struct page* p = alloc_page();
  list_pushback(&t->pages, &p->page_node);
  t->pagetable = (pagetable_t)pha(p);

  //* 分配用户栈
  p = alloc_page();
  list_pushback(&t->pages, &p->page_node);
  t->ustack = USTACK + PGSIZE;
  task_vmmap(t, USTACK, pha(p), PGSIZE, PTE_R | PTE_W | PTE_U, S_PAGE);


  //* 分配内核栈
  p = alloc_page();
  list_pushback(&t->pages, &p->page_node);
  t->kstack = pha(p) + PGSIZE;

  //* 分配trapframe页
  p = alloc_page();
  ((struct trapframe*)pha(p))->ksatp = kernel_satp;
  list_pushback(&t->pages, &p->page_node);
  vmmap(t->pagetable, TRAPFRAME, pha(p), PGSIZE, PTE_R, S_PAGE, t);

  //! 映射trampoline页 |  TRAMPOLINE页必须在内核和用户的页表中虚拟地址必须相同
  vmmap(t->pagetable, TRAMPOLINE, (u64)trampoline, PGSIZE, PTE_X | PTE_R, S_PAGE, t);
}

struct task*
alloc_task()
{
  acquire_spin(&tq);
  for (int i = 0; i < NPROC; ++i) {
    acquire_spin(&task_queue[i].lock);
    if (task_queue[i].state == FREE) {
      task_queue[i].state = INIT;
      release_spin(&task_queue[i].lock);
      release_spin(&tq);
      task_init(task_queue + i);
      return task_queue + i;
    }
    release_spin(&task_queue[i].lock);
  }
  panic("task too much");
}