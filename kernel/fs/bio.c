/*
  ! buffer layer
  每一个buffer都是一个临界资源,作为对硬盘数据块的内存拷贝,某一个时刻至多有一个线程访问
  当多个线程尝试访问一个buffer时,未获得锁的线程会阻塞睡眠
*/


#include "fs.h"
#include "spinlock.h"
#include "driver.h"
#include "string.h"
#include "printf.h"

#define NBUF 16

static struct {
  struct buf bufs[NBUF];
  struct spinlock lock;
} bcache; // buffer pool

/*
  从bcache列表中尝试寻找blockno匹配的缓存块 (缓存命中)
  如果没有找到,则返回一个valid字段为false的缓存块,之后由bread填充 (未缓存)
*/
static struct buf*
bget(u32 blockno)
{
  struct buf* buf;
  acquire_spin(&bcache.lock);

  for (int i = 0; i < NBUF; ++i) {
    buf = bcache.bufs + i;
    if (buf->blockno == blockno) {
      ++buf->refc;
      release_spin(&bcache.lock);
      acquire_sleep(&buf->lock);
      return buf; // cached
    }
  }

  // not cached
  for (int i = 0; i < NBUF; ++i) {
    buf = bcache.bufs + i;
    if (!buf->valid) {
      ++buf->refc;
      buf->valid = true;
      buf->blockno = blockno;
      release_spin(&bcache.lock);
      acquire_sleep(&buf->lock);
      return buf;
    }
  }

  panic("buffer exhausted");
}

struct buf*
bread(u32 blockno)
{
  struct buf* buf = bget(blockno);
  if (!buf->valid) // 数据未缓存
    disk_read(buf);
  return buf;
}

void
bwrite(struct buf* buf)
{
  if (!holding_sleep(&buf->lock))
    panic("bwrite illegal buf");
  disk_write(buf);
}

void
brelease(struct buf* buf)
{
  if (!holding_sleep(&buf->lock))
    panic("brelease illegal buf");
  --buf->refc;
  release_sleep(&buf->lock);
}

void
init_buffer()
{
  strncpy(bcache.lock.lname, "bcache", strlen("bcache"));
  for (int i = 0; i < NBUF; ++i)
    strncpy(bcache.bufs[i].lock.lname, "buffer", strlen("buffer"));
}