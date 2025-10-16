#pragma once
#include "type.h"

struct task;

struct sleeplock {
  const char*  lname;
  bool         locked;
  struct task* task;
};

void acquire_sleep(struct sleeplock* lock);
void release_sleep(struct sleeplock* lock);

#define INIT_SLEEPLOCK(name) struct sleeplock name = { .lname = #name, .locked = false, .task = NULL }