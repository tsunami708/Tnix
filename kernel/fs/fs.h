#pragma once
typedef unsigned int u32;
typedef unsigned long long u64;

#define ROOTNO  0 // 根目录inode
#define BSIZE   1024
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
  bool disk; // 是否正在被设备使用
  u32 blockno;
  char data[BSIZE];
};

struct dinode {
  enum ftype type;

  u32 nlink;
  u32 inum;
  u32 fsize;
  u32 iblock[NDIRECT];
};
