#pragma once
#include "config.h"
#include "types.h"
#include "util/list.h"
#include "util/spinlock.h"
#include "task/cpu.h"
#include "fs/file.h"
#include "mem/vm.h"

struct inode;

enum task_state {
  FREE,
  INIT,
  READY,
  RUN,
  SLEEP,
  EXIT,
};

struct trapframe {
  u64 ksatp;
};

struct fs_struct {
  struct file* files[NFILE];
  u32 fdx;
  struct inode* cwd;
};

struct task {
  u64 entry; // 进程代码入口地址
  u64 ustack;
  u64 kstack;
  pagetable_t pagetable;

  struct spinlock lock;

  u16 pid, tid;
  struct task* parent;
  struct list_node childs;
  struct list_node self;

  int exit_code;
  void* chan; // 等待事件
  enum task_state state;
  struct context ctx;

  struct fs_struct* fs_struct;
  struct mm_struct* mm_struct;

  char tname[16];
};

struct task* alloc_task(struct task* p);
void clean_source(struct task* t);
void reset_vma(struct task* t);