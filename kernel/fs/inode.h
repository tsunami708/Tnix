#pragma once

#include "fs.h"
#include "sleeplock.h"

struct inode {
  u32 inum;

  struct sleeplock lock; // note: 仅保护di,其余字段由inode_icache的自旋锁保护
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

void read_dinode(struct inode* inode);
void write_dinode(struct inode* inode);
bool check_dinode(struct inode* inode);