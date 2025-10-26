#pragma once
#include "util/types.h"

struct task;

struct sleeplock {
  const char* lname;
  bool locked;
  struct task* task;
};

bool holding_sleep(struct sleeplock* lock);
void acquire_sleep(struct sleeplock* lock);
void release_sleep(struct sleeplock* lock);

#define INIT_SLEEPLOCK(name) struct sleeplock name = { .lname = #name, .locked = false, .task = NULL }