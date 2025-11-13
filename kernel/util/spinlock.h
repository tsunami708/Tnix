#pragma once
#include "types.h"

struct cpu;

struct spinlock {
  const char* lname;
  bool locked;
  struct cpu* cpu;
};

void spin_get(struct spinlock* lock);
void spin_put(struct spinlock* lock);

#define INIT_SPINLOCK(name) struct spinlock name = { .lname = #name, .locked = false, .cpu = NULL }
