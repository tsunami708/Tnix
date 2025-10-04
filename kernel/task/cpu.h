#pragma once
#include "type.h"
#include "riscv.h"

struct cpu {
  char c;
};

static inline u64
cpuid()
{
  return r_tp();
}

struct cpu* mycpu();