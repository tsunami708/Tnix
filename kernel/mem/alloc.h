#pragma once
#include "type.h"

#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))
#define ALIGN_UP(x, align)   (((x) + (align) - 1) & ~((align) - 1))

// 物理页
struct page {
  u64  paddr;
  bool inuse;
};

// 申请1页
struct page* kalloc();

// 释放1页
void kfree(struct page* p);

#define PA(page) (page->paddr)