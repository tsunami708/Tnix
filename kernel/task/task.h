#pragma once
#include "config.h"
#include "util/types.h"
#include "util/list.h"
#include "util/spinlock.h"
#include "task/cpu.h"
#include "fs/file.h"

struct inode;

enum task_state {
  INIT,
  READY,
  RUN,
  SLEEP,
  EXIT,
};

struct trapframe {
  u64 ksatp;
};

struct task {
  // 顺序必须固定的字段
  u64 entry;  // 入口地址
  u64 ustack; // 用户栈页地址
  u64 kstack; // 内核栈页地址
  ////

  pagetable_t pagetable;


  u16 pid, tid;
  struct task* parent;

  void* chan;

  struct {
    struct file f[NFILE];
    u8 i;
  } files;

  enum task_state state;
  struct spinlock lock;
  struct context ctx;
  struct inode* cwd;

  struct list_node pages;

  const char* tname;
};
