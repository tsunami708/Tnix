#include "bio.h"
#include "spinlock.h"
#include "list.h"
#include "driver.h"
#include "printf.h"

#define NIOBUF 12

/*
  静态申请一块连续的struct iobuf
  init_iobuf函数会将struct iobuf通过链表串联,内核通过链表获取与释放缓冲块
  iobuf_bcache中的自旋锁用于保护链表自身和每一个缓冲块中除数据区data外的字段(如引用计数)
*/
static struct {
  struct iobuf bufs[NIOBUF];
  struct spinlock lock;
  struct list_node head;
} iobuf_bcache;


/*
  检索并尝试获取iobuf_bcache中存放dev设备上blockno块的缓冲块
  如果可以从链表中找到匹配的缓冲块,则缓存命中,后续不需要从设备中读取
*/
static struct iobuf*
get_iobuf(dev_t dev, u32 blockno)
{
  struct iobuf* buf;
  acquire_spin(&iobuf_bcache.lock);
  struct list_node* node = iobuf_bcache.head.next;

  // * fast path
  while (node != &iobuf_bcache.head) {
    buf = container_of(node, struct iobuf, bcache_node);
    if (buf->dev == dev && buf->blockno == blockno) {
      ++buf->refc;
      release_spin(&iobuf_bcache.lock);
      acquire_sleep(&buf->lock); //! 确保时刻至多一个线程可以操作缓冲块
      return buf;
    }
    node = node->next;
  }

  // * slow path
  node = iobuf_bcache.head.prev;
  while (node != &iobuf_bcache.head) {
    buf = container_of(node, struct iobuf, bcache_node);
    if (buf->refc == 0) {
      buf->refc = 1;
      buf->dev = dev;
      buf->blockno = blockno;
      buf->valid = false; //! valid设置为false,read_iobuf会根据此字段判断是否进行IO操作
      release_spin(&iobuf_bcache.lock);
      acquire_sleep(&buf->lock);
      return buf;
    }
    node = node->prev;
  }

  panic("iobuf_bcache exhausted"); // TODO 阻塞等待闲置缓冲块而不是panic
}

struct iobuf*
read_iobuf(dev_t dev, u32 blockno)
{
  struct iobuf* buf = get_iobuf(dev, blockno);
  //! 当前线程必定持有buf的阻塞锁
  if (!buf->valid) {
    disk_read(buf);
    buf->valid = true;
  }
  return buf;
}

void
write_iobuf(struct iobuf* buf)
{
  if (!holding_sleep(&buf->lock))
    panic("write_iobuf");
  disk_write(buf);
}

/*
  当前线程暂不使用已持有的缓存块,调用release_iobuf释放阻塞锁并将其引用计数减一
  如果引用计数变为0,则将缓冲块变更到链表头部,提高get_iobuf缓存命中率
*/
void
release_iobuf(struct iobuf* buf)
{
  if (!holding_sleep(&buf->lock))
    panic("release_iobuf");
  release_sleep(&buf->lock);
  acquire_spin(&iobuf_bcache.lock);
  --buf->refc; //! 不需要改变其他字段,get_iobuf需要使用
  if (buf->refc == 0) {
    list_remove(&buf->bcache_node);
    list_pushfront(&iobuf_bcache.head, &buf->bcache_node);
  }
  release_spin(&iobuf_bcache.lock);
}

void
init_iobuf()
{
  list_init(&iobuf_bcache.head);
  iobuf_bcache.lock.lname = "iobuf_bcache";
  for (int i = 0; i < NIOBUF; ++i) {
    iobuf_bcache.bufs[i].lock.lname = "iobuf";
    list_pushback(&iobuf_bcache.head, &iobuf_bcache.bufs[i].bcache_node);
  }
}