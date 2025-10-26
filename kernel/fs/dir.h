#pragma once

#ifndef MKFS
#include "config.h"
#include "util/types.h"
#endif

struct dentry {
  u32 ium;
  char name[DLENGTH] __attribute__((nonstring));
};

struct inode;
struct inode* lookup_dentry(const char* path);