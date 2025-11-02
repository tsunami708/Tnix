#pragma once
#include "util/types.h"
#include "util/spinlock.h"

struct inode;
struct pipe {};

struct file {
  enum type { PIPE, INODE } type;
  u32 size;
  u32 refc;
  u32 off;
  union {
    struct inode* inode;
    struct pipe* pipe;
  };
  struct spinlock lock;
};

struct file* falloc(void);
bool fopen(struct file* f, const char* path);
void fdup(struct file* f);
void fclose(struct file* f);
void fupdate(struct file* f);
u32 fseek(struct file* f, u32 off);
u32 fread(struct file* f, void* buf, u32 bytes);
u32 fwrite(struct file* f, const void* buf, u32 bytes);