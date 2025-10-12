#include "task.h"
#include "alloc.h"
#include "vm.h"
#include "config.h"

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


extern char init[];
extern char einit[];
extern u64  kernel_satp;
#define UINIT      ((u64)(init))
#define UINIT_SIZE (align_up((u64)(einit), PGSIZE) - UINIT)
void
init_systemd()
{
  struct task* t   = task_queue;
  struct page* kst = kalloc();
  t->kstack        = pha(kst) + PGSIZE; // 为1号task分配内核栈
  insert_list(&t->pages, &kst->page_node);

  struct page* tf                     = kalloc();    // 为1号task分配trapframe只读页
  ((struct trapframe*)pha(tf))->ksatp = kernel_satp; // 目前只保存一个内核satp值
  insert_list(&t->pages, &tf->page_node);

  struct page* ust = kalloc();
  t->ustack        = USTACK + PGSIZE;
  insert_list(&t->pages, &ust->page_node);

  struct page* root_pgt = kalloc();
  t->pagetable          = (pagetable_t)pha(root_pgt); // 为1号task分配根页表
  insert_list(&t->pages, &root_pgt->page_node);


  // TRAMPOLINE页必须在内核和用户的页表中虚拟地址必须相同
  vmmap(t->pagetable, TRAMPOLINE, (u64)trampoline, PGSIZE, PTE_X | PTE_R, S_PAGE);

  vmmap(t->pagetable, TRAPFRAME, pha(tf), PGSIZE, PTE_R, S_PAGE);


  vmmap(t->pagetable, USTACK, pha(ust), PGSIZE, PTE_R | PTE_W | PTE_U, S_PAGE);
  vmmap(t->pagetable, UINIT, UINIT, UINIT_SIZE, PTE_R | PTE_X | PTE_U, S_PAGE);

  t->entry  = (u64)systemd_main;
  t->ctx.ra = (u64)first_sched;
  t->ctx.sp = t->kstack;
  t->state  = READY;
}
