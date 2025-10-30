#include "config.h"
#include "mem/alloc.h"
#include "mem/vm.h"
#include "util/printf.h"
#include "util/riscv.h"
#include "util/list.h"
#include "util/string.h"
#include "task/cpu.h"
#include "task/task.h"

// 2->1->0
#define va_level(va, level) (((va >> 12) >> (9 * level)) & 0x1FFUL)

pagetable_t kernel_pgt; // 内核根页表
u64 kernel_satp;

extern char etext[];
extern char trampoline[];
extern char brodata[];
extern char erodata[];
extern char end[];
#define KCODE_SIZE   (align_up((u64)etext, PGSIZE) - KBASE)
#define KRODATA      ((u64)brodata)
#define KRODATA_SIZE (align_up((u64)erodata, PGSIZE) - KRODATA)
#define KDATA        (KRODATA + KRODATA_SIZE)
#define KDATA_SIZE   ((align_up((u64)end, PGSIZE)) - KDATA)

/*
  将虚拟地址va映射到物理地址pa,范围size
  va,pa必须对齐到页大小
  attr为页设置属性
  gra页粒度
*/
void
vmmap(pagetable_t ptb, u64 va, u64 pa, u64 size, u16 attr, i8 gra, struct task* ut)
{
  if (va % PGSIZE || pa % PGSIZE)
    panic("vmmap not aligned");
  if (va + size > VA_TOP || pa + size > PHY_TOP)
    panic("out of range");
  u64 delta;
  switch (gra) {
  case S_PAGE:
    delta = PGSIZE;
    break;
  case M_PAGE:
    delta = MPGSIZE;
    break; // 2MB
  case G_PAGE:
    delta = GPGSIZE;
    break; // 1GB
  default:
    panic("vmmap unknown gra");
  }

  u64 bound = align_up(va + size, delta);
  for (; va < bound; va += delta, pa += delta) {
    pte_t *cur = (pte_t*)ptb, *pte;

    // 遍历页表级别：L2 -> L1 -> L0
    for (i8 level = 2; level >= 0; --level) {
      u64 vpn = va_level(va, level);

      pte = &cur[vpn];
      if (level == gra) { // 在目标粒度级别创建叶子 PTE
        *pte = ((pa >> 12) << 10) | PTE_V | attr;
        break;
      }
      if (!(*pte & PTE_V)) { // 需要创建非叶子 PTE（指向下一级页表）
        struct page* p = alloc_page();
        u64 new_pt_pa = pha(p);
        if (ut) // 非内核页表映射
          list_pushback(&ut->pages, &p->page_node);
        *pte = ((new_pt_pa >> 12) << 10) | PTE_V;
      }
      cur = (pte_t*)((*pte >> 10) << 12); // 进入下一级页表
    }
  }
}

void
task_vmmap(struct task* t, u64 va, u64 pa, u64 size, u16 attr, i8 gra)
{
  vmmap(t->pagetable, va, pa, size, attr, gra, t);
  t->vmas.vmas[t->vmas.nvma].va = va;
  t->vmas.vmas[t->vmas.nvma].pa = pa;
  t->vmas.vmas[t->vmas.nvma].len = size;
  t->vmas.vmas[t->vmas.nvma].attr = attr;
  t->vmas.vmas[t->vmas.nvma++].gra = gra;
}

void
copy_pagetable(struct task* c, struct task* p)
{
  struct vma* pvm = &p->vmas.vmas[0];
  while (pvm->pa > 0) {
    if (pvm->attr & PTE_X)
      task_vmmap(c, pvm->va, pvm->pa, pvm->len, pvm->attr, pvm->gra);
    else {
      struct page* page = alloc_page();
      list_pushback(&c->pages, &page->page_node);
      memcpy1((void*)page->paddr, (void*)pvm->pa, PGSIZE);
      task_vmmap(c, pvm->va, pha(page), pvm->len, pvm->attr, pvm->gra);
    }
    ++pvm;
  }
}



static void // 遍历页表
walk(pagetable_t ptb, u64 va_base, i8 level)
{
  for (u64 i = 0; i < 512; ++i) {
    pte_t pte = ptb[i];
    if (!(pte & PTE_V))
      continue;

    u64 va = va_base | (i << (12 + 9 * level));
    u64 pa = (pte >> 10) << 12;

    if (level == 0 || (pte & (PTE_R | PTE_W | PTE_X))) {
      print("VA: 0x%x -> PA: 0x%x | ", va, pa);
      if (pte & PTE_R)
        print("R");
      if (pte & PTE_W)
        print("W");
      if (pte & PTE_X)
        print("X");
      if (pte & PTE_U)
        print("U");
      if (pte & PTE_G)
        print("G");
      if (pte & PTE_A)
        print("A");
      if (pte & PTE_D)
        print("D");
      print("\n");
    } else {
      pagetable_t next = (pagetable_t)pa;
      walk(next, va, level - 1);
    }
  }
}
void
scan_pagetable(pagetable_t ptb)
{
  walk(ptb, 0, 2);
}


void
init_page(void)
{
  if (cpuid() == 0) {
    kernel_pgt = (pagetable_t)pha(alloc_page());
    vmmap(kernel_pgt, CLINT, CLINT, CLINT_SIZE, PTE_R | PTE_W, S_PAGE, NULL);
    vmmap(kernel_pgt, PLIC, PLIC, PLIC_SIZE, PTE_R | PTE_W, S_PAGE, NULL);
    vmmap(kernel_pgt, UART0, UART0, UART0_SIZE, PTE_R | PTE_W, S_PAGE, NULL);
    vmmap(kernel_pgt, VIRIO, VIRIO, VIRIO_SIZE, PTE_R | PTE_W, S_PAGE, NULL);
    vmmap(kernel_pgt, KBASE, KBASE, KCODE_SIZE, PTE_R | PTE_X, S_PAGE, NULL);

    vmmap(kernel_pgt, (u64)trampoline, (u64)trampoline, PGSIZE, PTE_R | PTE_X, S_PAGE, NULL);
    vmmap(kernel_pgt, TRAMPOLINE, (u64)trampoline, PGSIZE, PTE_R | PTE_X, S_PAGE, NULL);


    vmmap(kernel_pgt, KRODATA, KRODATA, KRODATA_SIZE, PTE_R, S_PAGE, NULL);
    vmmap(kernel_pgt, KDATA, KDATA, KDATA_SIZE, PTE_R | PTE_W, S_PAGE, NULL);

    u64 va = align_up(KDATA + KDATA_SIZE, PGSIZE);
    u64 bound = VA_TOP > PHY_TOP ? PHY_TOP : VA_TOP;
    for (; va % MPGSIZE && va < bound; va += PGSIZE)
      vmmap(kernel_pgt, va, va, PGSIZE, PTE_R | PTE_W, S_PAGE, NULL);
    for (; va % GPGSIZE && va < bound; va += MPGSIZE)
      vmmap(kernel_pgt, va, va, MPGSIZE, PTE_R | PTE_W, M_PAGE, NULL);
    for (; va + GPGSIZE < bound; va += GPGSIZE)
      vmmap(kernel_pgt, va, va, GPGSIZE, PTE_R | PTE_W, G_PAGE, NULL);
    for (; va < bound; va += PGSIZE)
      vmmap(kernel_pgt, va, va, PGSIZE, PTE_R | PTE_W, S_PAGE, NULL);
  }
  __sync_synchronize();
  asm volatile("sfence.vma zero, zero");
  kernel_satp = SATP_MODE | ((u64)kernel_pgt >> 12);
  w_satp(kernel_satp);
  asm volatile("sfence.vma zero, zero");
}
