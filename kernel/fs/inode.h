#pragma once
#include "fs/fs.h"
#include "util/sleeplock.h"
#include "util/spinlock.h"

struct inode {
  struct sleeplock slep; // 仅保护di
  struct dinode di;
  struct spinlock spin;
  struct superblock* sb;
  u32 inum;
  u32 refc;
  dev_t dev; // 仅对设备文件有效
};

u32 ialloc(struct superblock* sb);
void ifree(struct inode* inode);

struct inode* iget(struct superblock* sb, u32 inum);
void iput(struct inode* inode);

void iread(struct inode* inode);
void iupdate(struct inode* inode);
bool iexist(struct superblock* sb, u32 inum);