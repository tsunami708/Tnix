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

struct vma {
  u64 va;
  u64 pa;
  u64 len;
  u16 attr;
  enum vma_type type;
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

  struct {
    struct file* f[NFILE];
    u32 i;
  } files;
  struct inode* cwd;

  struct list_node pages; // 进程私有物理页面链表
  struct {
    struct vma v[NVMA];
    int n;
  } vmas;
  u64 next_heap;

  char tname[16];
};

struct task* alloc_task(struct task* p);
void clean_source(struct task* t);
void reset_vma(struct task* t);