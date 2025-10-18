#pragma once

#define ROOTNO  0 // 根目录inode
#define BSIZE   1024
#define NDIRECT 12

#ifndef MKFS
#include "type.h"
#include "sleeplock.h"

struct buf {
  bool valid; // 是否缓存硬盘块
  bool disk;  // 是否正在被设备使用
  u32 refc;
  u32 blockno;
  struct sleeplock lock;
  char data[BSIZE];
};
#else
typedef unsigned int u32;
typedef unsigned long long u64;
#endif


enum ftype {
  UNUSE,
  REGULAR,
  DIRECTORY,
};

struct superblock {
  u32 imap;
  u32 inodes;
  u32 bmap;
  u32 blocks;
  u32 max_inode;
  u32 max_nblock;
  char name[8];
};


struct dinode {
  enum ftype type;

  u32 nlink;
  u32 inum;
  u32 fsize;
  u32 iblock[NDIRECT];
};
