#pragma once
#include "util/types.h"

#define S_PAGE 0 // 4KB
#define M_PAGE 1 // 2MB
#define G_PAGE 2 // 1GB

struct task;
void vmmap(pagetable_t ptb, u64 va, u64 pa, u64 size, u16 attr, i8 gra, struct task* ut);
void task_vmmap(struct task* t, u64 va, u64 pa, u64 size, u16 attr, i8 gra);
void copy_pagetable(struct task* c, struct task* p);
void scan_pagetable(pagetable_t ptb);

bool copy_to_user(void* udst, const void* ksrc, u32 bytes);
bool copy_from_user(void* kdst, const void* usrc, u32 bytes);