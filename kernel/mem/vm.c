#include "config.h"
#include "alloc.h"
#include "printf.h"
#include "riscv.h"
#include "vm.h"
#include "cpu.h"

// 2->1->0
#define va_level(va, level) (((va >> 12) >> (9 * level)) & 0x1FFUL)
pagetable_t kernel_pgt; // 内核根页表


extern char etext[];
extern char brodata[];
extern char erodata[];
extern char end[];
#define KCODE_SIZE   (align_up((u64)etext, PGSIZE) - KERNELBASE)
#define KRODATA      ((u64)brodata)
#define KRODATA_SIZE (align_up((u64)erodata, PGSIZE) - KRODATA)
#define KDATA        (KRODATA + KRODATA_SIZE)
#define KDATA_SIZE   ((align_up((u64)end, PGSIZE)) - KDATA)

/*
  将虚拟地址va映射到物理地址pa,范围size
  va,pa必须对齐到页大小
  attr为页设置属性
  granularity页粒度
*/
void
vmmap(u64 va, u64 pa, u64 size, u16 attr, i8 granularity)
{
  if (va % PGSIZE || pa % PGSIZE)
    panic("vmmap not aligned");
  if (va + size > MAXVA || pa + size > PHY_TOP)
    panic("out of range");
  u64 delta;
  switch (granularity) {
  case S_PAGE:
    delta = PGSIZE;
    break;
  case M_PAGE:
    delta = PGSIZE << 10;
    break; // 2MB
  case G_PAGE:
    delta = PGSIZE << 18;
    break; // 1GB
  default:
    panic("vmmap unknown granularity");
  }

  u64 bound = align_up(va + size, delta);
  for (; va < bound; va += delta, pa += delta) {
    pte_t *cur = (pte_t*)kernel_pgt, *pte;

    // 遍历页表级别：L2 -> L1 -> L0
    for (i8 level = 2; level >= 0; --level) {
      u64 vpn = va_level(va, level);

      pte = &cur[vpn];
      if (level == granularity) { // 在目标粒度级别创建叶子 PTE
        *pte = ((pa >> 12) << 10) | PTE_V | attr;
        break;
      }
      if (!(*pte & PTE_V)) { // 需要创建非叶子 PTE（指向下一级页表）
        u64 new_pt_pa = pha(kalloc());

        *pte = ((new_pt_pa >> 12) << 10) | PTE_V;
      }
      cur = (pte_t*)((*pte >> 10) << 12); // 进入下一级页表
    }
  }
}

void
init_page()
{
  if (cpuid() == 0) {
    kernel_pgt = (pagetable_t)pha(kalloc());
    vmmap(CLINT, CLINT, CLINT_SIZE, PTE_R | PTE_W, S_PAGE);
    vmmap(PLIC, PLIC, PLIC_SIZE, PTE_R | PTE_W, S_PAGE);
    vmmap(UART0, UART0, UART0_SIZE, PTE_R | PTE_W, S_PAGE);
    vmmap(VIRIO, VIRIO, VIRIO_SIZE, PTE_R | PTE_W, S_PAGE);
    vmmap(KERNELBASE, KERNELBASE, KCODE_SIZE, PTE_R | PTE_X, S_PAGE);
    vmmap(KRODATA, KRODATA, KRODATA_SIZE, PTE_R, S_PAGE);
    vmmap(KDATA, KDATA, KDATA_SIZE, PTE_R | PTE_W, S_PAGE);
  }
  __sync_synchronize();
  asm volatile("sfence.vma zero, zero");
  w_satp(SATP_MODE | ((u64)kernel_pgt >> 12));
  asm volatile("sfence.vma zero, zero");
}
