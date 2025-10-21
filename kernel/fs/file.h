#pragma once
#include "type.h"

struct inode;

struct file {
  u32 refc;
  u32 offset;
  struct inode* inode;
};