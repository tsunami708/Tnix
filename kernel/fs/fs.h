#pragma once
typedef unsigned int u32;
typedef unsigned long long u64;


#define NDIRECT 3


enum ftype {
  UNUSE,
  REGULAR,
  DIRECTORY,
};

struct superblock {
  u32 imap;
  u32 inodes;
  u32 bmap;
  u32 blocks;
  u32 max_inode;
  u32 max_nblock;
  char name[8];
};

struct buf {
  char data[1024];
};

struct dinode {
  enum ftype type;

  u32 inum;
  u32 fsize;
  u32 iblock[NDIRECT];
};

struct inode {
  struct dinode disk_copy;
};