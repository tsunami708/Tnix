#include "task.h"
#include "alloc.h"
#include "vm.h"

struct task task_queue[NPROC] = { { .pid    = 1,
                                    .tid    = 1,
                                    .kstack = 0,
                                    .state  = INIT,
                                    .lock   = { .lname = "systemd_lock", .locked = false, .cpu = NULL },
                                    .pages  = { .next = &task_queue[0].pages, .prev = &task_queue[0].pages },
                                    .tname  = "systemd" } };

extern char trampoline[];
extern void first_sched();

// systemd的执行逻辑,此函数在U模式下执行
__attribute__((section(".init.text"))) void
systemd_main()
{
  while (1)
    ;
}

void
init_systemd()
{
  struct task* t   = task_queue;
  struct page* kst = kalloc();
  t->kstack        = pha(kst) + PGSIZE; // 为1号task分配内核栈
  insert_list(&t->pages, &kst->page_node);

  struct page* ust = kalloc();
  t->ustack        = pha(ust);
  insert_list(&t->pages, &ust->page_node);

  struct page* root_pgt = kalloc();
  t->pagetable          = (pagetable_t)pha(root_pgt); // 为1号task分配根页表
  insert_list(&t->pages, &root_pgt->page_node);


  // 这页代码内核和用户都可以执行,目的是trap时安全地切换页表
  vmmap(t->pagetable, UTRAMPOLINE, (u64)trampoline, PGSIZE, PTE_X | PTE_R | PTE_U, S_PAGE);

  vmmap(t->pagetable, USTACK, ust->paddr, PGSIZE, PTE_R | PTE_U | PTE_W, S_PAGE);

  t->entry  = (u64)systemd_main;
  t->ctx.ra = (u64)first_sched;
  t->state  = READY;
}
