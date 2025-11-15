#pragma once
#include "types.h"
#include "util/spinlock.h"

#define PIPE_SIZE 64
struct pipe {
  struct spinlock lock;
  u32 nread;  // 已读字节数
  u32 nwrite; // 已写字节数
  i8 refc;    // 0 1 2 3 4
  char buf[PIPE_SIZE];
};

struct pipe* pipealloc(void);
void pipeclose(struct pipe* p);
u32 piperead(struct pipe* p, void* udst, u32 len);
u32 pipewrite(struct pipe* p, const void* usrc, u32 len);

static inline __attribute__((always_inline)) void
pipeget(struct pipe* p)
{
  spin_get(&p->lock);
  ++p->refc;
  spin_put(&p->lock);
}