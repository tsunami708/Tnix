#include "bio.h"
#include "spinlock.h"
#include "list.h"
#include "driver.h"
#include "printf.h"

#define NIOBUF 16


// desc: init_iobuf函数将struct iobuf通过链表串联,内核通过链表获取与释放缓冲块
static struct {
  struct iobuf bufs[NIOBUF];
  struct spinlock lock; // note: 保护链表自身和每一个缓冲块中除数据区data外的字段(如引用计数)
  struct list_node head;
} iobuf_bcache;



// desc: 检索并尝试获取iobuf_bcache中存放dev设备上blockno块的缓冲块,若缓存未命中则交由调用方read_iobuf主动读取
static struct iobuf*
get_iobuf(dev_t dev, u32 blockno)
{
  struct iobuf* buf; // note: 在返回buf之前必须获取buf的阻塞锁,保证数据块的原子操作
  acquire_spin(&iobuf_bcache.lock);
  struct list_node* node = iobuf_bcache.head.next;

  // cache hit
  while (node != &iobuf_bcache.head) {
    buf = container_of(node, struct iobuf, bcache_node);
    if (buf->dev == dev && buf->blockno == blockno) {
      ++buf->refc;
      release_spin(&iobuf_bcache.lock);
      acquire_sleep(&buf->lock);
      return buf;
    }
    node = node->next;
  }

  // cache miss
  node = iobuf_bcache.head.prev;
  while (node != &iobuf_bcache.head) {
    buf = container_of(node, struct iobuf, bcache_node);
    if (buf->refc == 0) {
      buf->refc = 1;
      buf->dev = dev;
      buf->blockno = blockno;
      buf->valid = false; // note: valid设置为false,read_iobuf会根据此字段判断是否进行IO操作
      release_spin(&iobuf_bcache.lock);
      acquire_sleep(&buf->lock);
      return buf;
    }
    node = node->prev;
  }

  panic("iobuf_bcache exhausted"); // todo: 阻塞等待闲置缓冲块而不是panic
}


// desc: 读取块设备dev上块号为blockno的数据
// note: 并不总是发生设备IO,如果get_iobuf缓存命中则可以省去慢速IO
struct iobuf*
read_iobuf(dev_t dev, u32 blockno)
{
  struct iobuf* buf = get_iobuf(dev, blockno);
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
write_iobuf(struct iobuf* buf)
{
  if (!holding_sleep(&buf->lock))
    panic("write an iobuf,but not holding lock");
  disk_write(buf);
}


//  desc: 当前线程暂不使用缓存块,调用此方法释放阻塞锁并将其引用计数减一
//  note: 必须确保执行此方法的线程已经持有buf的阻塞锁
void
release_iobuf(struct iobuf* buf)
{
  if (!holding_sleep(&buf->lock))
    panic("release_iobuf");
  release_sleep(&buf->lock);
  acquire_spin(&iobuf_bcache.lock);
  --buf->refc;
  if (buf->refc == 0) { // note: 如果引用计数变为0,则将缓冲块变更到链表头部,提高get_iobuf缓存命中率
    list_remove(&buf->bcache_node);
    list_pushfront(&iobuf_bcache.head, &buf->bcache_node);
  }
  release_spin(&iobuf_bcache.lock);
}

void
init_bcache()
{
  list_init(&iobuf_bcache.head);
  iobuf_bcache.lock.lname = "iobuf_bcache";
  for (int i = 0; i < NIOBUF; ++i) {
    iobuf_bcache.bufs[i].lock.lname = "iobuf";
    list_pushback(&iobuf_bcache.head, &iobuf_bcache.bufs[i].bcache_node);
  }
}