#pragma once
#include "util/types.h"
// 不要改变struct pt_regs的字段顺序
// 异常上下文
struct pt_regs {
  u64 ra, sp, gp, tp; // tp其实可以不用保存
  u64 sepc, sstatus, stval, scause;
  u64 t0, t1, t2, t3, t4, t5, t6;
  u64 s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
  u64 a0, a1, a2, a3, a4, a5, a6, a7;
};