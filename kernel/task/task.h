#pragma once
#include "type.h"
#include "list.h"
#include "spinlock.h"
#include "cpu.h"
#include "config.h"

#define UTRAMPOLINE align_down(MAXVA, PGSIZE)
#define USTACK      (UTRAMPOLINE - PGSIZE) // 4KB栈

enum task_state {
  INIT,
  READY,
  RUN,
  SLEEP,
  EXIT,
};

struct task {
  // 顺序必须固定的字段
  u64 entry; // 入口地址

  ////
  u64         kstack;
  pagetable_t pagetable;

  u64 ustack;
  u16 pid, tid;

  enum task_state state;
  struct spinlock lock;
  struct context  ctx;

  struct list_node pages;

  const char* tname;
};
