#include "task/task.h"
#include "mem/alloc.h"
#include "mem/vm.h"
#include "util/string.h"
#include "util/spinlock.h"
#include "util/printf.h"
#include "fs/inode.h"
#include "fs/pipe.h"


INIT_SPINLOCK(tq);
struct task task_queue[NPROC];

INIT_SPINLOCK(ti);
static u32 tidmap[1 + NPROC / sizeof(u32) / 8] = { 0 };
static void
free_tid(u16 tid)
{
  spin_get(&ti);
  tidmap[(tid - 1) / sizeof(u32)] &= ~(1U << (tid - 1) % sizeof(u32));
  spin_put(&ti);
}
static u16
alloc_tid(void)
{
  u16 r;
  spin_get(&ti);
  int i = 0;
  for (; i < sizeof(tidmap) / sizeof(u32); ++i)
    if (tidmap[i] != 0xFFFFFFFFU)
      break;
  for (int j = 0; j < 32; ++j)
    if ((tidmap[i] & (1U << j)) == 0) {
      r = i * sizeof(u32) + j + 1;
      tidmap[i] |= (1U << j);
      break;
    }
  spin_put(&ti);
  return r;
}

static void
copy_files(struct task* c, struct task* p)
{
  memcpy(&c->files, &p->files, sizeof(p->files));
  for (u32 i = 0; i < NFILE; ++i) {
    struct file* pf = c->files.f[i];
    if (pf) {
      struct file* f = falloc();
      f->type = pf->type;
      f->mode = pf->mode;
      f->inode = pf->inode; // union !
      if (f->type == INODE || f->type == DEVICE)
        iref(f->inode);
      else if (f->type == PIPE)
        pipeget(f->pipe);
    }
  }
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
  p = alloc_page_for_task(t);
  t->pagetable = (pagetable_t)p->paddr;
  t->ustack = USTACK + PGSIZE;

  if (parent) {
    t->parent = parent;
    t->cwd = parent->cwd;
    iget(t->cwd->sb, t->cwd->inum);
    copy_pagetable(t, parent);
    copy_files(t, parent);
  } else {
    //* 分配用户栈
    p = alloc_page_for_task(t);
    task_vmmap(t, USTACK, p->paddr, PGSIZE, PTE_R | PTE_W | PTE_U, STACK);
  }

  //* 分配内核栈
  p = alloc_page_for_task(t);
  t->kstack = p->paddr + PGSIZE;

  //* 分配trapframe页
  p = alloc_page_for_task(t);
  ((struct trapframe*)p->paddr)->ksatp = kernel_satp;
  svmmap(t->pagetable, TRAPFRAME, p->paddr, PGSIZE, PTE_R, t);

  //! 映射trampoline页 |  TRAMPOLINE页必须在内核和用户的页表中虚拟地址必须相同 | 该页所有task共享
  svmmap(t->pagetable, TRAMPOLINE, (u64)trampoline, PGSIZE, PTE_X | PTE_R, NULL);
}

struct task*
alloc_task(struct task* p)
{
  spin_get(&tq);
  for (int i = 0; i < NPROC; ++i) {
    spin_get(&task_queue[i].lock);
    if (task_queue[i].state == FREE) {
      task_queue[i].state = INIT;
      spin_put(&task_queue[i].lock);
      spin_put(&tq);
      task_init(task_queue + i, p);
      return task_queue + i;
    }
    spin_put(&task_queue[i].lock);
  }
  panic("alloc_task: task too much");
}

void
clean_source(struct task* t)
{
  iput(t->cwd);

  for (int i = 0; i < t->files.i; ++i)
    fclose(t->files.f[i]);
  t->files.i = 0;
  t->vmas.nvma = 0;

  struct list_node *cur = t->pages.next, *tmp;
  while (cur != &t->pages) {
    tmp = cur->next;
    free_page_for_task(container_of(cur, struct page, page_node));
    cur = tmp;
  }

  free_tid(t->tid);
}

// 清空代码段和数据段
void
reset_vma(struct task* t)
{
  int i = 0;
  while (i < t->vmas.nvma) {
    if (t->vmas.vmas[i].type == DATA || t->vmas.vmas[i].type == TEXT) {
      vmunmap(t->pagetable, t->vmas.vmas[i].va, t->vmas.vmas[i].len);
      free_page_for_task(page(t->vmas.vmas[i].pa));
      for (int j = i; j < t->vmas.nvma - 1; j++)
        t->vmas.vmas[j] = t->vmas.vmas[j + 1];
      t->vmas.nvma--;
    } else
      ++i;
  }
}