#pragma once
#include "type.h"

struct cpu;

struct spinlock {
  char lname[16];
  bool locked;
  struct cpu* cpu;
};

void acquire_spin(struct spinlock* lock);
void release_spin(struct spinlock* lock);

#define INIT_SPINLOCK(name) struct spinlock name = { .lname = #name, .locked = false, .cpu = NULL }
