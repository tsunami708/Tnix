#pragma once
#include "util/types.h"
#include "util/spinlock.h"

struct inode;
struct pipe {};

enum mode {
  O_RDONLY = 0b1,
  O_WRONLY = 0b10,
  O_RDWR = 0b100,
  O_TRUNC = 0b1000,
  O_APPEND = 0b10000,
  O_CREAT = 0b100000,
};

struct file {
  enum mode mode;
  enum type { NONE, PIPE, INODE, DEVICE } type;
  u32 size;
  u32 refc;
  u32 off;
  union {
    struct inode* inode;
    struct pipe* pipe;
  };
  struct spinlock lock;
};

struct dev_op {
  u32 (*read)(void*, u32);
  u32 (*write)(const void*, u32);
  bool valid;
};

struct file* falloc(void);
bool fopen(struct file* f, const char* path);
void fdup(struct file* f);
void fclose(struct file* f);
void fupdate(struct file* f);
u32 fseek(struct file* f, u32 off);
u32 fread(struct file* f, void* buf, u32 bytes);
u32 fwrite(struct file* f, const void* buf, u32 bytes);