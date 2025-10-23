#pragma once

#ifndef MKFS
#include "type.h"
#endif

#define DLENGTH 16
struct dentry {
  u32 ium;
  char name[DLENGTH] __attribute__((nonstring));
};

struct inode;
struct inode* lookup_dentry(const char* path);