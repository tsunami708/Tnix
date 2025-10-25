#pragma once
#include "type.h"
#include "list.h"
#include "spinlock.h"
#include "cpu.h"
#include "file.h"

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
