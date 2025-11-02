#pragma once

#ifndef MKFS
#include "config.h"
#include "util/types.h"
#endif

struct dentry {
  u32 inum;
  char name[DLENGTH] __attribute__((nonstring));
};

#ifdef __cplusplus
static_assert(BSIZE % sizeof(struct dentry) == 0, "BSZIE must be divisible by dentry size");
#else
_Static_assert(BSIZE % sizeof(struct dentry) == 0, "BSZIE must be divisible by dentry size");
#endif
struct inode;
struct inode* dlookup(const char* path);