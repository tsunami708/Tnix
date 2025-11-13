#pragma once

#ifndef MKFS
#include "config.h"
#include "types.h"
#endif
struct inode;
struct dentry {
  u32 inum;
  char name[DLENGTH] __attribute__((nonstring));
};
static_assert(BSIZE % sizeof(struct dentry) == 0, "BSIZE must be divisible by dentry size");

void path_split(const char* path, char* parentpath, char* filename);
struct inode* dentry_find(const char* path);
int dentry_add(struct inode* dir, u32 inum, const char* name);
void dentry_del(struct inode* dir, const char* name);