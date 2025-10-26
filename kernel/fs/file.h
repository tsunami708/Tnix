#pragma once
#include "util/types.h"

struct inode;

struct pipe {}; // todo:

struct file {
  u32 refc;
  u32 off;
  union {
    struct inode* inode;
    struct pipe* pipe;
  };
};

struct file* alloc_file(void);
void dup_file(struct file* f);
void close_file(struct file* f);