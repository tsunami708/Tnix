#include "config.h"

#include "task/task.h"
#include "mem/alloc.h"
#include "mem/vm.h"

struct task task_queue[NPROC] = { { .pid = 1,
                                    .tid = 1,
                                    .kstack = 0,
                                    .state = INIT,
                                    .lock = { .lname = "systemd_lock", .locked = false, .cpu = NULL },
                                    .pages = { .next = &task_queue[0].pages, .prev = &task_queue[0].pages },
                                    .tname = "systemd" } };

extern char trampoline[];
extern void first_sched(void);

// systemd的执行逻辑,此函数在U模式下执行
__attribute__((section(".init.text"))) void
systemd_main(void)
{
  while (1)
    ;
}


extern char init[];
extern char einit[];
extern u64 kernel_satp;
#define UINIT      ((u64)(init))
#define UINIT_SIZE (align_up((u64)(einit), PGSIZE) - UINIT)
void
init_systemd(void)
{
  struct task* t = task_queue;
  struct page* kst = alloc_page();
  t->kstack = pha(kst) + PGSIZE; // 为1号task分配内核栈
  list_pushback(&t->pages, &kst->page_node);

  struct page* tf = alloc_page();                    // 为1号task分配trapframe只读页
  ((struct trapframe*)pha(tf))->ksatp = kernel_satp; // 目前只保存一个内核satp值
  list_pushback(&t->pages, &tf->page_node);

  struct page* ust = alloc_page();
  t->ustack = USTACK + PGSIZE;
  list_pushback(&t->pages, &ust->page_node);

  struct page* root_pgt = alloc_page();
  t->pagetable = (pagetable_t)pha(root_pgt); // 为1号task分配根页表
  list_pushback(&t->pages, &root_pgt->page_node);


  // TRAMPOLINE页必须在内核和用户的页表中虚拟地址必须相同
  vmmap(t->pagetable, TRAMPOLINE, (u64)trampoline, PGSIZE, PTE_X | PTE_R, S_PAGE);

  vmmap(t->pagetable, TRAPFRAME, pha(tf), PGSIZE, PTE_R, S_PAGE);


  vmmap(t->pagetable, USTACK, pha(ust), PGSIZE, PTE_R | PTE_W | PTE_U, S_PAGE);
  vmmap(t->pagetable, UINIT, UINIT, UINIT_SIZE, PTE_R | PTE_X | PTE_U, S_PAGE);

  t->entry = (u64)systemd_main;
  t->ctx.ra = (u64)first_sched;
  t->ctx.sp = t->kstack;
  t->state = READY;
}
