#include "fs/bio.h"
#include "util/spinlock.h"
#include "util/list.h"
#include "util/printf.h"
#include "dev/driver.h"



// init_iobuf函数将struct buf通过链表串联,内核通过链表获取与释放缓冲块
static struct {
  struct buf bufs[NIOBUF];
  struct spinlock lock; // 保护链表自身和每一个缓冲块中除数据区data外的字段
  struct list_node head;
} bcache;


// 检索并尝试获取bcache中存放dev设备上blockno块的缓冲块,若缓存未命中则交由调用方bread主动读取
static struct buf*
bget(dev_t dev, u32 blockno)
{
  struct buf* b; // 在返回buf之前必须获取buf的阻塞锁,保证数据块的原子操作
  spin_get(&bcache.lock);
  struct list_node* node = bcache.head.next;

  // cache hit
  while (node != &bcache.head) {
    b = container_of(node, struct buf, bcache_node);
    if (b->dev == dev && b->blockno == blockno) {
      ++b->refc;
      spin_put(&bcache.lock);
      sleep_get(&b->lock);
      return b;
    }
    node = node->next;
  }

  // cache miss
  node = bcache.head.prev;
  while (node != &bcache.head) {
    b = container_of(node, struct buf, bcache_node);
    if (b->refc == 0) {
      b->refc = 1;
      b->dev = dev;
      b->blockno = blockno;
      b->valid = false; // valid设置为false,bread会根据此字段判断是否进行IO操作
      spin_put(&bcache.lock);
      sleep_get(&b->lock);
      return b;
    }
    node = node->prev;
  }

  panic("bget: bcache exhausted"); // ? 阻塞等待闲置缓冲块而不是panic
}

// 读取块设备dev上块号为blockno的数据
// 并不总是发生设备IO,如果bget缓存命中则可以省去慢速IO
struct buf*
bread(dev_t dev, u32 blockno)
{
  struct buf* b = bget(dev, blockno);
  // 当前线程必定持有buf的阻塞锁
  if (! b->valid) {
    disk_read(b);
    b->valid = true;
  }
  return b;
}

// 将buf上数据进行持久化落盘
void
bwrite(struct buf* b)
{
  if (! sleep_holding(&b->lock))
    panic("bwrite: not holding lock");
  disk_write(b);
}

void
bzero(struct buf* b)
{
  u64* sec = (void*)b->data;
  for (int i = 0; i < BSIZE / 8; ++i) {
    *sec = 0;
    ++sec;
  }
  bwrite(b);
}

// 当前线程暂不使用缓存块,调用此方法释放阻塞锁并将其引用计数减一
void
brelse(struct buf* b)
{
  if (! sleep_holding(&b->lock))
    panic("brelse: not holding lock");
  sleep_put(&b->lock);
  spin_get(&bcache.lock);
  --b->refc;
  if (b->refc == 0) { //   如果引用计数变为0,则将缓冲块变更到链表头部,提高bget缓存命中率
    list_remove(&b->bcache_node);
    list_pushfront(&bcache.head, &b->bcache_node);
  }
  spin_put(&bcache.lock);
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