#pragma once
#include "util/types.h"
#include "util/list.h"

// 物理页
struct page {
  u64 paddr;
  bool inuse;
  struct list_node page_node;
};

// 申请1页
struct page* alloc_page(void);

// 释放1页
void free_page(struct page* p);

#define pha(page) (page->paddr)