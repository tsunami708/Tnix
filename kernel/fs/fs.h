#pragma once

#ifndef MKFS
#include "config.h"
#include "types.h"
struct inode;
struct buf;
struct superblock;

#define IDX_CNT_PER_INDIRECT_BLCOK (BSIZE / sizeof(u32))

void read_superblock(dev_t dev, struct superblock* sb);
struct buf* read_nth_imap(struct superblock* sb, u32 n);
struct buf* read_nth_bmap(struct superblock* sb, u32 n);

u32 balloc(struct superblock* sb);
void bfree(struct superblock* sb, u32 blockno);
#else
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long u64;
#define BSIZE     1024
#define NDIRECT   12
#define NINDIRECT 3
#define DLENGTH   28
#endif


enum ftype {
  UNUSE,
  REGULAR,
  DIRECTORY,
  CHAR,
};

struct superblock {
#ifndef MKFS
  dev_t dev;
#else
  u32 dev; // 避免与sys/types.h冲突
#endif
  u32 imap;
  u32 inodes;
  u32 bmap;
  u32 blocks;
  u32 max_inum;
  u32 max_blockno;
  char name[8];
};


struct dinode {
  enum ftype type;
#ifndef MKFS
  dev_t dev; // 设备文件的设备号
#else
  u32 dev;
#endif
  u32 nlink;
  u32 fsize;
  u32 iblock[NDIRECT + NINDIRECT];
};
