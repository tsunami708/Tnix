#pragma once
#include "util/types.h"

#define ELF_MAGIC 0x464C457FU

#define ELF_PROG_LOAD 1

#define ELF_PROG_FLAG_EXEC  1
#define ELF_PROG_FLAG_WRITE 2
#define ELF_PROG_FLAG_READ  4

#define ELF_RISCV 243

// ELF File header
struct elfhdr {
  u8 ident[16]; //
  u16 type;
  u16 machine; //
  u32 version;
  u64 entry; //
  u64 phoff; //
  u64 shoff;
  u32 flags;
  u16 ehsize;
  u16 phentsize; //
  u16 phnum;     //
  u16 shentsize;
  u16 shnum;
  u16 shstrndx;
};

// Program segment header
struct proghdr {
  u32 type;
  u32 flags;
  u64 off;
  u64 vaddr;
  u64 paddr;
  u64 filesz;
  u64 memsz;
  u64 align;
};

struct task;
struct inode;
void load_segment(struct inode* f, struct elfhdr* eh, struct task* t);
bool read_elfhdr(struct inode* f, struct elfhdr* eh);
