#pragma once
#include "config.h"
#include "types.h"
#include "util/sleeplock.h"
#include "util/list.h"

struct buf {
  dev_t dev;
  u32 refc;
  u32 blockno;
  bool valid; // 是否已缓存dev设备上块号blockno块数据
  bool disk;  // 是否正在被设备使用
  struct list_node bcache_node;
  struct sleeplock lock; // 保证data的原子访问
  char data[BSIZE];      // 至多一个线程访问数据块
};

struct buf* bread(dev_t dev, u32 blockno);
void bwrite(struct buf* b);
void bzero(struct buf* b);
void brelse(struct buf* b);