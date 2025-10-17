#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#include "kernel/fs/fs.h"

#include <unordered_map>
#include <string>
#include <vector>
using fimap = std::unordered_map<std::string, u32>; //<filename,inode_num>
using child_dir = std::vector<std::string>;

u32 copy(const char* dirpath, u32 pinode, bool root = false);

/*
[boot] [superblock] [inode-bitmap] [inodes] [block-bitmap] [blocks]
! zone size(block) , 1024B - per block
boot: 1
superblock: 1
inode-bitmap: 1 (max file number: 8096)
block-bitmap: 4 (max file blcok: 32932)
*/


#define BLOCK_SIZE 1024
#define DINODE_NUM 8192
#define BLOCK_NUM  32768

char imap[DINODE_NUM / 8]{};
char bmap[BLOCK_NUM / 8]{};
char block[BLOCK_SIZE]{};

struct __attribute__((aligned(BLOCK_SIZE))) {
  struct dinode dinodes[sizeof(struct dinode) * DINODE_NUM];
} dinodes{};

const u64 BLOCK_START = (3UL + sizeof(dinodes) / BLOCK_SIZE + sizeof(bmap) / BLOCK_SIZE);


u32 free_i = 0; // imap中首个0的比特位索引
u32 free_b = 0; // bmap中首个0的比特位索引

static inline struct dinode*
bit_to_inode(u32 idx)
{
  return (struct dinode*)&dinodes + idx;
}

/*
  返回一个空闲的dinode节点,并将其在imap中对应的比特位设置为1且对inum字段进行赋值
*/
static inline struct dinode*
alloc_dinode()
{
  if (free_i > DINODE_NUM - 1) {
    printf("dinode exhausted\n");
    exit(1);
  }

  u32 index = free_i / 32;
  u32 bit = free_i % 32;

  u32* addr = (u32*)imap + index * 4;
  *addr |= 1 << bit;

  struct dinode* r = bit_to_inode(free_i);
  r->inum = free_i++;
  return r;
}

/*
  返回一个空闲block的块号,并将其在bmap中对应的比特位设置为1
*/
static inline u64
alloc_block_num()
{
  if (free_b >= BLOCK_NUM - 1) {
    printf("block exhausted\n");
    exit(1);
  }
  u32 index = free_b / 32;
  u32 bit = free_b % 32;

  u32* addr = (u32*)&bmap + index * 4;
  *addr |= 1 << bit;

  return BLOCK_START + free_b++;
}



int fs_fd; // fs.img句柄
#define err strerror(errno)

int
main()
{
  fs_fd = open("fs.img", O_CREAT | O_RDWR | O_TRUNC);
  if (fs_fd < 0) {
    perror("open fs.img fail:");
    return 1;
  }

  int r;
  r = ftruncate(fs_fd, 1024 * 1024 * 1024LL); // 1GB的硬盘,可用空间小于1GB
  if (r < 0) {
    perror("ftruncate fail:");
    return 1;
  }

  // 写入超级块
  lseek(fs_fd, BLOCK_SIZE, SEEK_SET);
  struct superblock sb = { .imap = 2, .inodes = 3, .name = "tsunami" };
  sb.bmap = sb.inodes + sizeof(dinodes) / BLOCK_SIZE;
  sb.blocks = sb.bmap + sizeof(bmap) / BLOCK_SIZE;
  sb.max_inode = DINODE_NUM - 1;
  sb.max_nblock = BLOCK_NUM + BLOCK_START - 1;
  r = write(fs_fd, &sb, sizeof(sb));
  if (r != sizeof(sb)) {
    perror("write superblock fail:");
    return 1;
  }

  copy("../user", -1, true);

  lseek(fs_fd, 2 * BLOCK_SIZE, SEEK_SET);
  if (write(fs_fd, imap, sizeof(imap)) != sizeof(imap)) {
    printf("write imap fail %s\n", err);
    return 1;
  }
  if (write(fs_fd, &dinodes, sizeof(dinodes)) != sizeof(dinodes)) {
    printf("write dinodes fail %s\n", err);
    return 1;
  }
  if (write(fs_fd, bmap, sizeof(bmap)) != sizeof(bmap)) {
    printf("write bmap fail %s\n", err);
    return 1;
  }

  close(fs_fd);
  return 0;
}


u32
copy(const char* dirpath, u32 pinode, bool root)
{
  DIR* dir = opendir(dirpath);

  if (dir == NULL) {
    printf("open directory %s fail: %s\n", dirpath, err);
    exit(1);
  }

  if (chdir(dirpath) < 0) {
    printf("chdir to %s fail: %s\n", dirpath, err);
    exit(1);
  }

  fimap maps;
  child_dir cds;

  struct dirent* entry;
  while ((entry = readdir(dir))) {
    // 1.处理非目录文件
    if (entry->d_type == DT_DIR) {
      if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        cds.push_back(entry->d_name);
    } else {
      int fd = open(entry->d_name, O_RDONLY);
      if (fd < 0) {
        printf("open file %s fail: %s\n", entry->d_name, err);
        exit(1);
      }

      // fill dinode
      struct dinode* di = alloc_dinode();
      di->type = REGULAR;

      maps[entry->d_name] = di->inum;

      // fill block
      ssize_t n;
      int i = 0;
      while ((n = read(fd, block, BLOCK_SIZE)) > 0) {
        di->fsize += n;
        di->iblock[i] = alloc_block_num();

        lseek(fs_fd, di->iblock[i] * BLOCK_SIZE, SEEK_SET);
        if (write(fs_fd, block, BLOCK_SIZE) != BLOCK_SIZE) {
          perror("write fs.img fail:");
          exit(1);
        }
        ++i;
      }
      if (n < 0) {
        printf("read file %s fail: %s\n", entry->d_name, err);
        exit(1);
      }
      close(fd);
    }
  }


  // 2.处理.目录
  char block[BLOCK_SIZE]{};
  struct dinode* di = alloc_dinode();
  di->type = DIRECTORY;
  di->fsize = BLOCK_SIZE;
  di->iblock[0] = alloc_block_num();

  // i. 写入 . 项
  int i = 0;
  memcpy(block + i, &di->inum, 4);
  memcpy(block + i + 4, ".", 2);
  i += 20;

  // ii. 写入 .. 项
  if (root)
    memcpy(block + i, &di->inum, 4);
  else
    memcpy(block + i, &pinode, 4);
  memcpy(block + i + 4, "..", 3);
  i += 20;

  // iii. 写入子目录目录项
  for (auto& cd : cds) {
    u32 cinode = copy(cd.c_str(), di->inum);
    memcpy(block + i, &cinode, 4);
    memcpy(block + i + 4, cd.c_str(), cd.length());
    i += 20;
  }

  // vi. 写入普通目录项
  for (auto& [fname, inode] : maps) {
    memcpy(block + i, &inode, 4);
    memcpy(block + i + 4, fname.c_str(), fname.length());
    i += 20;
  }
  lseek(fs_fd, di->iblock[0] * BLOCK_SIZE, SEEK_SET);
  if (write(fs_fd, block, BLOCK_SIZE) != BLOCK_SIZE) {
    printf("write . dir fail: %s\n", err);
    exit(1);
  }
  closedir(dir);
  return di->inum;
}

/*
目录文件格式:
  4B  |    16B   |
inode | filename  ~20B
inode | filename  ~20B
inode | filename  ~20B
inode | filename  ~20B
inode | filename  ~20B

为简单处理,目录只允许分配一个内容块,即一个目录下最多允许51个文件
*/