#pragma once
#include "fs/fs.h"
#include "util/sleeplock.h"
#include "util/spinlock.h"

struct inode {
  struct sleeplock slep; // 仅保护di
  struct spinlock spin;
  struct dinode di;
  struct superblock* sb;
  u32 inum;
  u32 refc;
};

u32 ialloc(struct superblock* sb);
void itrunc(struct inode* in);
void ifree(struct inode* in);

struct inode* iget(struct superblock* sb, u32 inum);
void iput(struct inode* inode);

void iupdate(struct inode* inode);
bool iexist(struct superblock* sb, u32 inum);

struct buf* data_block_get(struct inode* in, u32 i);
struct buf* data_block_alloc(struct inode* in);
void idx_remove(struct inode* in, u32 i);

static inline int
iblock_cnt(struct inode* in)
{
  return in->di.fsize / BSIZE - (in->di.fsize % BSIZE == 0 ? 0 : 1);
}