#pragma once
#include "type.h"
#include "sleeplock.h"
#include "list.h"

#define BSIZE 1024

struct iobuf {
  char data[BSIZE];      //! 至多一个线程访问数据块
  struct sleeplock lock; // !仅保护data

  struct list_node bcache_node;

  bool valid; // 是否已缓存硬盘块
  bool disk;  // 是否正在被设备使用

  dev_t dev;
  u32 refc;
  u32 blockno;
};

struct iobuf* read_iobuf(dev_t dev, u32 blockno);
void write_iobuf(struct iobuf* buf);
void release_iobuf(struct iobuf* buf);