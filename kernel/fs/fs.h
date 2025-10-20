#pragma once

#define BSIZE   1024
#define NDIRECT 12

#ifndef MKFS
#include "type.h"
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
