#include "task/task.h"
#include "mem/alloc.h"
#include "util/spinlock.h"
#include "util/printf.h"
extern struct spinlock tq;
extern struct task task_queue[NPROC];

struct task*
alloc_task()
{
  acquire_spin(&tq);
  for (int i = 0; i < NPROC; ++i)
    if (task_queue[i].state == FREE) {
      task_queue[i].state = INIT;
      release_spin(&tq);
      task_queue[i].lock.lname = "task";
      list_init(&task_queue[i].pages);
      struct page* p = alloc_page();
      list_pushback(&(task_queue + i)->pages, &p->page_node);
      task_queue[i].pagetable = (pagetable_t)pha(p);
      return task_queue + i;
    }
  panic("task too much");
}