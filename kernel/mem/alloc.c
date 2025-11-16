#include "config.h"
#include "mem/alloc.h"
#include "task/task.h"
#include "util/printf.h"
#include "util/spinlock.h"
#include "util/string.h"

#define page_num(addr) (addr - PHY_MEMORY) / PGSIZE
extern char end[]; // kernel.ld提供的内核静态数据区结束地址
static struct page phy_mem[PHY_SIZE / PGSIZE];

struct page*
page(u64 paddr)
{
  return phy_mem + (paddr - PHY_MEMORY) / PGSIZE;
}

INIT_SPINLOCK(mem_spin);
INIT_LIST(pages_head);

void
init_memory(void)
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
alloc_page(void)
{
  spin_get(&mem_spin);
  if (pages_head.next == &pages_head)
    panic("alloc_page: memory exhausted");
  struct page* p = container_of(pages_head.next, struct page, page_node);
  p->inuse = true;
  list_remove(pages_head.next);
  spin_put(&mem_spin);
  memset((void*)p->paddr, 0, PGSIZE);
  return p;
}
struct page*
alloc_page_for_task(struct task* t)
{
  struct page* p = alloc_page();
  list_pushback(&t->pages, &p->page_node);
  return p;
}


void
free_page(struct page* p)
{
  spin_get(&mem_spin);
  if (! p->inuse)
    panic("free_page: double free page");
  p->inuse = false;
  list_pushback(&pages_head, &p->page_node);
  spin_put(&mem_spin);
}
