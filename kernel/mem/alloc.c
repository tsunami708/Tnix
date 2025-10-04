#include "alloc.h"
#include "config.h"
#include "printf.h"
#include "spinlock.h"

#define PAGE_NUM(addr) (addr - PHY_MEMORY) / PGSIZE
extern char        end[]; // kernel.ld提供的内核静态数据区结束地址
static struct page phy_mem[PHY_SIZE / PGSIZE];
static u32         normal_mem_i = 0; // 非内核永久保留内存起始页
INIT_SPINLOCK(mem);
void
init_mem()
{
  for (u64 phy_addr = PHY_MEMORY; phy_addr < PHY_TOP; phy_addr += PGSIZE) {
    phy_mem[PAGE_NUM(phy_addr)].paddr = phy_addr;
    if (phy_addr < ALIGN_UP((u64)end, PGSIZE)) {
      phy_mem[PAGE_NUM(phy_addr)].inuse = true; // 这一部分被内核永久保留使用
      ++normal_mem_i;
    } else
      phy_mem[PAGE_NUM(phy_addr)].inuse = false;
  }
}


struct page*
kalloc()
{
  acquire_spin(&mem);
  for (u32 i = normal_mem_i; i < NPAGE; ++i)
    if (!phy_mem[i].inuse) {
      phy_mem[i].inuse = true;
      release_spin(&mem);
      return phy_mem + i;
    }
  panic("memory exhausted");
}

void
kfree(struct page* p)
{
  acquire_spin(&mem);
  if (!phy_mem[PAGE_NUM(p->paddr)].inuse)
    panic("double free page");
  phy_mem[PAGE_NUM(p->paddr)].inuse = false;
  release_spin(&mem);
}
