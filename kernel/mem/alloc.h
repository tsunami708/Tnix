#pragma once
#include "types.h"
#include "util/list.h"
struct task;
// 物理页
struct page {
  struct list_node page_node;
  u64 paddr;
  bool inuse;
};

struct page* page(u64 paddr);

// 申请1页
struct page* alloc_page(void);
struct page* alloc_page_for_task(struct task* t);
void free_page(struct page* p);
static inline __attribute__((always_inline)) void
free_page_for_task(struct page* p)
{
  list_remove(&p->page_node);
  free_page(p);
}