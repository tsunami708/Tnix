#include "fs/bio.h"
#include "util/spinlock.h"
#include "util/list.h"
#include "util/printf.h"
#include "dev/driver.h"



// desc: init_iobuf函数将struct buf通过链表串联,内核通过链表获取与释放缓冲块
static struct {
  struct buf bufs[NIOBUF];
  struct spinlock lock; // note: 保护链表自身和每一个缓冲块中除数据区data外的字段
  struct list_node head;
} bcache;



// desc: 检索并尝试获取bcache中存放dev设备上blockno块的缓冲块,若缓存未命中则交由调用方bread主动读取
static struct buf*
bget(dev_t dev, u32 blockno)
{
  struct buf* buf; // note: 在返回buf之前必须获取buf的阻塞锁,保证数据块的原子操作
  acquire_spin(&bcache.lock);
  struct list_node* node = bcache.head.next;

  // cache hit
  while (node != &bcache.head) {
    buf = container_of(node, struct buf, bcache_node);
    if (buf->dev == dev && buf->blockno == blockno) {
      ++buf->refc;
      release_spin(&bcache.lock);
      acquire_sleep(&buf->lock);
      return buf;
    }
    node = node->next;
  }

  // cache miss
  node = bcache.head.prev;
  while (node != &bcache.head) {
    buf = container_of(node, struct buf, bcache_node);
    if (buf->refc == 0) {
      buf->refc = 1;
      buf->dev = dev;
      buf->blockno = blockno;
      buf->valid = false; // note: valid设置为false,bread会根据此字段判断是否进行IO操作
      release_spin(&bcache.lock);
      acquire_sleep(&buf->lock);
      return buf;
    }
    node = node->prev;
  }

  panic("bcache exhausted"); // todo: 阻塞等待闲置缓冲块而不是panic
}

// desc: 读取块设备dev上块号为blockno的数据
// note: 并不总是发生设备IO,如果bget缓存命中则可以省去慢速IO
struct buf*
bread(dev_t dev, u32 blockno)
{
  struct buf* buf = bget(dev, blockno);
  // note: 当前线程必定持有buf的阻塞锁
  if (!buf->valid) {
    disk_read(buf);
    buf->valid = true;
  }
  return buf;
}

// desc: 将buf上数据进行持久化落盘
// note: 必须确保执行此方法的线程已经持有buf的阻塞锁
void
bwrite(struct buf* buf)
{
  if (!holding_sleep(&buf->lock))
    panic("write an iobuf,but not holding lock");
  disk_write(buf);
}


//  desc: 当前线程暂不使用缓存块,调用此方法释放阻塞锁并将其引用计数减一
//  note: 必须确保执行此方法的线程已经持有buf的阻塞锁
void
brelse(struct buf* buf)
{
  if (!holding_sleep(&buf->lock))
    panic("brelse");
  release_sleep(&buf->lock);
  acquire_spin(&bcache.lock);
  --buf->refc;
  if (buf->refc == 0) { // note: 如果引用计数变为0,则将缓冲块变更到链表头部,提高bget缓存命中率
    list_remove(&buf->bcache_node);
    list_pushfront(&bcache.head, &buf->bcache_node);
  }
  release_spin(&bcache.lock);
}

void
init_bcache(void)
{
  list_init(&bcache.head);
  bcache.lock.lname = "bcache";
  for (int i = 0; i < NIOBUF; ++i) {
    bcache.bufs[i].lock.lname = "iobuf";
    list_pushback(&bcache.head, &bcache.bufs[i].bcache_node);
  }
}