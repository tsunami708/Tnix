#pragma once
#include "config.h"
#include "util/types.h"
#include "util/sleeplock.h"
#include "util/list.h"

struct iobuf {
  char data[BSIZE];      // note: 至多一个线程访问数据块
  struct sleeplock lock; // note:仅保护data

  struct list_node bcache_node;

  bool valid; // 是否已缓存dev设备上块号blockno块数据
  bool disk;  // 是否正在被设备使用

  dev_t dev;
  u32 refc;
  u32 blockno;
};

struct iobuf* read_iobuf(dev_t dev, u32 blockno);
void write_iobuf(struct iobuf* buf);
void release_iobuf(struct iobuf* buf);