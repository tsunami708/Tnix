#include "task/task.h"
#include "mem/alloc.h"
#include "mem/vm.h"
#include "util/string.h"
#include "util/spinlock.h"
#include "util/printf.h"
#include "fs/inode.h"


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
copy_files(struct task* c, struct task* p)
{
  memcpy(&c->files, &p->files, sizeof(p->files));
}

static void
task_init(struct task* t, struct task* parent)
{
  extern u64 kernel_satp;
  extern char trampoline[];
  struct page* p;

  t->tname = "task";
  t->lock.lname = "task-lock";
  t->pid = t->tid = alloc_tid();
  list_init(&t->pages);
  list_init(&t->childs);

  //* 分配页表
  p = alloc_page();
  list_pushback(&t->pages, &p->page_node);
  t->pagetable = (pagetable_t)pha(p);
  t->ustack = USTACK + PGSIZE;

  if (parent) {
    t->parent = parent;
    t->cwd = parent->cwd;
    iget(t->cwd->sb, t->cwd->inum);
    copy_pagetable(t, parent);
    copy_files(t, parent);
  } else {
    //* 分配用户栈
    p = alloc_page();
    list_pushback(&t->pages, &p->page_node);
    task_vmmap(t, USTACK, pha(p), PGSIZE, PTE_R | PTE_W | PTE_U, S_PAGE);
  }

  //* 分配内核栈
  p = alloc_page();
  list_pushback(&t->pages, &p->page_node);
  t->kstack = pha(p) + PGSIZE;

  //* 分配trapframe页
  p = alloc_page();
  ((struct trapframe*)pha(p))->ksatp = kernel_satp;
  list_pushback(&t->pages, &p->page_node);
  vmmap(t->pagetable, TRAPFRAME, pha(p), PGSIZE, PTE_R, S_PAGE, t);

  //! 映射trampoline页 |  TRAMPOLINE页必须在内核和用户的页表中虚拟地址必须相同 | 该页所有task共享
  vmmap(t->pagetable, TRAMPOLINE, (u64)trampoline, PGSIZE, PTE_X | PTE_R, S_PAGE, NULL);
}

struct task*
alloc_task(struct task* p)
{
  acquire_spin(&tq);
  for (int i = 0; i < NPROC; ++i) {
    acquire_spin(&task_queue[i].lock);
    if (task_queue[i].state == FREE) {
      task_queue[i].state = INIT;
      release_spin(&task_queue[i].lock);
      release_spin(&tq);
      task_init(task_queue + i, p);
      return task_queue + i;
    }
    release_spin(&task_queue[i].lock);
  }
  panic("task too much");
}

void
clean_source(struct task* t)
{
  iput(t->cwd);

  for (int i = 0; i < t->files.i; ++i)
    fclose(&t->files.f[i]);
  t->files.i = 0;

  t->vmas.nvma = 0;

  struct list_node *cur = t->pages.next, *tmp;
  while (cur != &t->pages) {
    tmp = cur->next;
    free_page(container_of(cur, struct page, page_node));
    cur = tmp;
  }
}