#pragma once
#include "type.h"
#include "list.h"
#include "spinlock.h"
#include "cpu.h"

enum task_state {
  INIT,
  READY,
  RUN,
  SLEEP,
  EXIT,
};

struct task {
  // 顺序必须固定的字段
  u64 entry;  // 入口地址
  u64 ustack; // 用户栈页地址
  u64 kstack; // 内核栈页地址
  ////

  pagetable_t pagetable;


  u16 pid, tid;

  enum task_state state;
  struct spinlock lock;
  struct context  ctx;

  struct list_node pages;

  const char* tname;
};
