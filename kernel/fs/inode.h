#pragma once

#include "fs.h"
#include "sleeplock.h"

struct inode {
  struct sleeplock lock; // 保护di
  struct dinode di;
  /*
   *  enum ftype type;
   *  u32 nlink;
   *  u32 inum;
   *  u32 fsize;
   *  u32 iblock[NDIRECT];
   */

  struct superblock* sb;
  bool valid;
  u32 refc;
};

u32 alloc_inode(struct superblock* sb);
void free_inode(struct inode* inode);

struct inode* get_inode(struct superblock* sb, u32 inum);
void put_inode(struct inode* inode);
