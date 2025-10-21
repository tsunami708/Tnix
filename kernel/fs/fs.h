#pragma once

#define BSIZE   1024
#define NDIRECT 12
#define ROOTDEV 0

#ifndef MKFS
#include "type.h"
struct inode;
struct iobuf;
struct superblock;

void read_superblock(dev_t dev, struct superblock* sb);

struct iobuf* read_imap(struct superblock* sb, u32 i);
struct iobuf* read_bmap(struct superblock* sb, u32 i);

u32 alloc_block(struct superblock* sb);
void free_block(struct superblock* sb, u32 blockno);
#else
typedef unsigned int u32;
typedef unsigned long u64;
#endif


enum ftype {
  UNUSE,
  REGULAR,
  DIRECTORY,
};

struct superblock {
  dev_t dev;
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
