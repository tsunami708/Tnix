#pragma once
#include "types.h"
#include "util/riscv.h"
/*
  注意区分task上下文和trap上下文的区别
  trap入口是突然跳转的,和普通函数调用栈帧不同,不能依赖caller保存
*/
// 其他寄存器由调用者caller保存
// 不要修改struct context的字段顺序!
struct context {
  u64 ra, sp;
  u64 s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
};

struct cpu {
  // 顺序必须固定的字段
  u64 id;
  u64 cur_kstack; // cur_task的kstack副本
  u64 cur_satp;   // 当前task的satp
  struct task* cur_task;
  u64 trap_addr;
  /////

  bool raw_intr; // 不持有自旋锁时的中断状态
  u8 spinlevel;  // 自旋锁层数

  struct context ctx; // 调度器自身上下文
};


static inline struct cpu*
mycpu(void)
{
  return (struct cpu*)r_tp();
}

static inline struct task*
mytask(void)
{
  return mycpu()->cur_task;
}

static inline u64
cpuid(void)
{
  return mycpu()->id;
}

static inline void
push_intr(void)
{
  struct cpu* c = mycpu();
  if (c->spinlevel == 0)
    c->raw_intr = intr();
  cli();
  ++c->spinlevel;
}

static inline void
pop_intr(void)
{
  struct cpu* c = mycpu();
  --c->spinlevel;
  if (c->spinlevel == 0 && c->raw_intr == true)
    sti();
}