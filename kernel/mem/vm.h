#pragma once
#include "util/types.h"

#define S_PAGE 0 // 4KB
#define M_PAGE 1 // 2MB
#define G_PAGE 2 // 1GB

enum vma_type {
  STACK,
  HEAP,
  TEXT,
  DATA,
};

struct task;
void svmmap(pagetable_t ptb, u64 va, u64 pa, u64 size, u16 attr, struct task* ut);
void mvmmap(pagetable_t ptb, u64 va, u64 pa, u64 size, u16 attr); //! 暂时只能由内核调用
void task_vmmap(struct task* t, u64 va, u64 pa, u64 size, u16 attr, enum vma_type type);
void vmunmap(pagetable_t ptb, u64 va, u64 size);

void copy_pagetable(struct task* c, struct task* p);
void scan_pagetable(pagetable_t ptb);

u64 va_to_pa(pagetable_t ptb, u64 va, pte_t** p);
void copy_to_user(void* udst, const void* ksrc, u32 bytes);
void copy_from_user(void* kdst, const void* usrc, u32 bytes);