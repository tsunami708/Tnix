#include "fs/pipe.h"
#include "util/printf.h"
#include "mem/vm.h"
#include "task/sche.h"

#define NPIPE 8
static struct {
  struct spinlock lock;
  struct pipe pipes[NPIPE];
} pipecache = { .lock.lname = "pipecache" };

static inline __attribute__((always_inline)) bool
pipe_isempty(struct pipe* p)
{
  return p->nread == p->nwrite;
}
static inline __attribute__((always_inline)) u32
pipe_readable_size(struct pipe* p)
{
  return p->nwrite - p->nread;
}
static inline __attribute__((always_inline)) u32
pipe_writable_size(struct pipe* p)
{
  return PIPE_SIZE - (p->nwrite - p->nread);
}


struct pipe*
pipealloc(void)
{
  spin_get(&pipecache.lock);
  for (int i = 0; i < NPIPE; ++i) {
    spin_get(&pipecache.pipes[i].lock);
    if (pipecache.pipes[i].refc == 0) {
      pipecache.pipes[i].refc = 2;
      pipecache.pipes[i].nread = pipecache.pipes[i].nwrite = 0;
      pipecache.pipes[i].lock.lname = "pipe";
      spin_put(&pipecache.pipes[i].lock);
      spin_put(&pipecache.lock);
      return pipecache.pipes + i;
    }
    spin_put(&pipecache.pipes[i].lock);
  }
  panic("pipealloc: exhausted");
}
void
pipeclose(struct pipe* p)
{
  spin_get(&p->lock);
  if (p->refc == 0)
    panic("pipeclose: refc=0");
  --p->refc;
  spin_put(&p->lock);
}
u32
piperead(struct pipe* p, void* udst, u32 len)
{
  u32 r;
  spin_get(&p->lock);
  while (pipe_isempty(p))
    sleep(&p->nread, &p->lock);
  u32 bytes = min(len, pipe_readable_size(p));
  u32 start = p->nread % PIPE_SIZE;
  u32 total_read = 0;
  r = bytes;
  while (bytes > 0) {
    u32 count = min(bytes, PIPE_SIZE - start);
    copy_to_user((char*)udst + total_read, p->buf + start, count);
    start = (start + count) % PIPE_SIZE;
    bytes -= count;
    total_read += count;
    p->nread += count;
  }
  wakeup(&p->nwrite);
  spin_put(&p->lock);
  return r;
}
u32
pipewrite(struct pipe* p, const void* usrc, u32 len)
{
  u32 r = len;
  spin_get(&p->lock);
  u32 total_written = 0;
  while (total_written < len) {
    u32 writable = pipe_writable_size(p);
    while (writable == 0) {
      wakeup(&p->nread);
      sleep(&p->nwrite, &p->lock);
      writable = pipe_writable_size(p);
    }
    u32 start = p->nwrite % PIPE_SIZE;
    u32 count = min(min(len - total_written, writable), PIPE_SIZE - start);
    copy_from_user(p->buf + start, (const char*)usrc + total_written, count);
    total_written += count;
    p->nwrite += count;
  }
  wakeup(&p->nread);
  spin_put(&p->lock);
  return r;
}