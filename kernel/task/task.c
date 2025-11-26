#include "task/task.h"
#include "mem/alloc.h"
#include "mem/vm.h"
#include "mem/slot.h"
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
      c->files.f[i] = f;
    }
  }
}

static void
task_info_init(struct task* t, struct task* p)
{
  strcpy(t->tname, "task");
  t->lock.lname = "task-lock";
  t->pid = t->tid = alloc_tid();
  t->parent = p;
  list_init(&t->childs);
}

static void
task_mm_init(struct task* t, struct task* p)
{
  struct mm_struct *tm = alloc_mm_struct_slot(), *pm = p ? p->mm_struct : NULL;
  t->mm_struct = tm;
  list_init(&tm->vma_head);
  list_init(&tm->page_head);
  tm->next_heap = p ? pm->next_heap : 0;

  // 分配页表
  struct page* page = alloc_page_for_task(t);
  t->pagetable = (pagetable_t)page->paddr;
  t->ustack = USTACK + PGSIZE;

  if (p) {
    copy_pagetable(t, p);
  } else {
    page = alloc_page_for_task(t);
    task_vmmap(t, USTACK, page->paddr, PGSIZE, PTE_R | PTE_W | PTE_U, STACK);
  }

  // 分配内核栈
  page = alloc_page_for_task(t);
  t->kstack = page->paddr + PGSIZE;

  extern u64 kernel_satp;
  page = alloc_page_for_task(t);
  ((struct trapframe*)page->paddr)->ksatp = kernel_satp;
  svmmap(t->pagetable, TRAPFRAME, page->paddr, PGSIZE, PTE_R, t);

  // 映射trampoline页 |  TRAMPOLINE页必须在内核和用户的页表中虚拟地址必须相同 | 该页所有task共享
  extern char trampoline[];
  svmmap(t->pagetable, TRAMPOLINE, (u64)trampoline, PGSIZE, PTE_X | PTE_R, NULL);
}

static void
task_fs_init(struct task* t, struct task* p)
{
  if (p == NULL)
    return;
  iref(p->cwd);
  t->cwd = p->cwd;
  copy_files(t, p);
}

static void
task_init(struct task* t, struct task* p)
{
  task_info_init(t, p);
  task_mm_init(t, p);
  task_fs_init(t, p);
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

  struct list_node* node = t->mm_struct->page_head.next;
  while (node != &t->mm_struct->page_head) {
    struct page* p = container_of(node, struct page, page_node);
    node = node->next;
    free_page_for_task(p);
  }
  node = t->mm_struct->vma_head.next;
  while (node != &t->mm_struct->vma_head) {
    struct vma* vma = container_of(node, struct vma, node);
    node = node->next;
    list_remove(&vma->node);
    free_vma_slot(vma);
  }
  free_mm_struct_slot(t->mm_struct);

  free_tid(t->tid);
}

// 清空代码段,数据段和堆区
void
reset_vma(struct task* t)
{
  struct list_node* node = t->mm_struct->vma_head.next;
  while (node != &t->mm_struct->vma_head) {
    struct vma* vma = container_of(node, struct vma, node);
    node = node->next;
    if (vma->type != STACK) {
      list_remove(&vma->node);
      free_vma_slot(vma);
    }
  }
}

void
dump_all_task(void)
{
  print("\npid tid state name\n");
  for (int i = 0; i < NPROC; ++i) {
    struct task* t = &task_queue[i];
    spin_get(&t->lock);
    if (t->state == READY || t->state == RUN || t->state == SLEEP)
      print("%d  %d  %s %s\n", t->pid, t->tid, t->state == READY ? "READY" : (t->state == RUN ? "RUN" : "SLEEP"),
            t->tname);
    spin_put(&t->lock);
  }
}