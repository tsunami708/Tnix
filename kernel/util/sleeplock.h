#pragma once
#include "types.h"

struct task;

struct sleeplock {
  const char* lname;
  bool locked;
  struct task* task;
};

bool sleep_holding(struct sleeplock* lock);
void sleep_get(struct sleeplock* lock);
void sleep_put(struct sleeplock* lock);

#define INIT_SLEEPLOCK(name) struct sleeplock name = { .lname = #name, .locked = false, .task = NULL }