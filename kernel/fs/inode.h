#pragma once

#include "fs/fs.h"
#include "util/sleeplock.h"
#include "util/spinlock.h"

struct inode {
  struct sleeplock lock; // note: 仅保护di
  struct dinode di;
  /*
   *  enum ftype type;
   *  u32 nlink;
   *  u32 inum;
   *  u32 fsize;
   *  u32 iblock[NDIRECT];
   */

  struct spinlock spin;
  struct superblock* sb;
  u32 inum;
  bool valid;
  u32 refc;
};

u32 alloc_inode(struct superblock* sb);
void free_inode(struct inode* inode);

struct inode* get_inode(struct superblock* sb, u32 inum);
struct inode* do_get_inode(struct superblock* sb, u32 inum);
void put_inode(struct inode* inode);

void read_dinode(struct inode* inode);
void write_dinode(struct inode* inode);
bool check_dinode(struct inode* inode);