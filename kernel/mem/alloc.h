#pragma once
#include "type.h"
#include "list.h"

// 物理页
struct page {
  u64              paddr;
  bool             inuse;
  struct list_node page_node;
};

// 申请1页
struct page* kalloc();

// 释放1页
void kfree(struct page* p);

#define pa(page) (page->paddr)