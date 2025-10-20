#include "alloc.h"
#include "config.h"
#include "printf.h"
#include "spinlock.h"
#include "string.h"
#define page_num(addr) (addr - PHY_MEMORY) / PGSIZE
extern char end[]; // kernel.ld提供的内核静态数据区结束地址
static struct page phy_mem[PHY_SIZE / PGSIZE];

INIT_SPINLOCK(mem_spin);
INIT_LIST(pages_head);

void
init_memory()
{
  for (u64 phy_addr = PHY_MEMORY; phy_addr < PHY_TOP; phy_addr += PGSIZE) {
    struct page* p = phy_mem + page_num(phy_addr);

    p->paddr = phy_addr;
    if (phy_addr < align_up((u64)end, PGSIZE)) {
      p->inuse = true; // 这一部分被内核永久保留使用
    } else {
      p->inuse = false;
      list_pushback(&pages_head, &p->page_node);
    }
  }
}

struct page*
kalloc()
{
  acquire_spin(&mem_spin);
  if (pages_head.next == &pages_head)
    panic("memory exhausted");
  struct page* p = container_of(pages_head.next, struct page, page_node);

  p->inuse = true;
  list_remove(pages_head.next);
  release_spin(&mem_spin);
  memset((void*)p->paddr, 0, PGSIZE);
  return p;
}

void
kfree(struct page* p)
{
  acquire_spin(&mem_spin);
  if (!p->inuse)
    panic("double free page");
  p->inuse = false;
  list_pushback(&pages_head, &p->page_node);
  release_spin(&mem_spin);
}
