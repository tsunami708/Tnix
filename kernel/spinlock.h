#pragma once
#include "type.h"

struct cpu;

struct spinlock {
  const char* lname;
  bool locked;
  struct cpu* cpu;
};

void acquire_spin(struct spinlock* lock);
void release_spin(struct spinlock* lock);

#define INIT_SPINLOCK(name) struct spinlock name = { .lname = #name, .locked = false, .cpu = NULL }
