#pragma once
#include "types.h"
#include "util/riscv.h"

struct context {
  u64 ra, sp;
  u64 s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
};

struct cpu {
  // 顺序必须固定的字段
  u64 id;
  u64 cur_kstack;
  u64 cur_satp;
  struct task* cur_task;
  u64 trap_addr;
  /////

  bool raw_intr;
  u8 spinlevel;

  struct context ctx; // 调度器自身上下文
};


static inline __attribute__((always_inline)) struct cpu*
mycpu(void)
{
  return (struct cpu*)r_tp();
}

static inline __attribute__((always_inline)) struct task*
mytask(void)
{
  return mycpu()->cur_task;
}

static inline __attribute__((always_inline)) u64
cpuid(void)
{
  return mycpu()->id;
}

static inline __attribute__((always_inline)) void
push_intr(void)
{
  struct cpu* c = mycpu();
  if (c->spinlevel == 0)
    c->raw_intr = intr();
  cli();
  ++c->spinlevel;
}

static inline __attribute__((always_inline)) void
pop_intr(void)
{
  struct cpu* c = mycpu();
  --c->spinlevel;
  if (c->spinlevel == 0 && c->raw_intr == true)
    sti();
}