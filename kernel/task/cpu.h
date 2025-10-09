#pragma once
#include "type.h"
#include "riscv.h"
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
  u64          id;
  u64          cur_kstack; // cur_task的kstack副本
  pagetable_t  cur_ptb;    // cur_task的pagetable副本
  struct task* cur_task;
  /////

  u8 intr; // 中断状态,0表示中断开启,>0为关闭

  struct context ctx;
};


static inline struct cpu*
mycpu()
{
  return (struct cpu*)r_tp();
}

static inline struct task*
mytask()
{
  return mycpu()->cur_task;
}

static inline u64
cpuid()
{
  return mycpu()->id;
}

static inline void
push_intr()
{
  cli();
  ++mycpu()->intr;
}

static inline void
pop_intr()
{
  --mycpu()->intr;
  if (mycpu()->intr == 0)
    sti();
}