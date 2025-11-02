#define MKFS
#include "kernel/fs/fs.h"
#include "kernel/fs/dir.h"

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

#include <queue>
#include <string>

using std::pair;
using std::queue;
using std::string;

//* USER CONFIG
constexpr u32 MAX_FILE_NUMBER = 8192 * 2; // 支持16384个文件
static_assert(MAX_FILE_NUMBER % BSIZE == 0);

constexpr u32 DATA_BLOCK_NUMBER = 1024 * 1024; // 支持2^20*BSIZE的文件内容数据区 (BSIZE=1024 ~ 1GB文件内容区)
static_assert(DATA_BLOCK_NUMBER % BSIZE == 0);
//*

constexpr u32 SB_START_BLOCKNO = 1;
constexpr u32 IMAP_START_BLOCKNO = 2;
constexpr u32 IMAP_SIZE = MAX_FILE_NUMBER / 8;
constexpr u32 IMAP_BLOCKNUM = IMAP_SIZE / BSIZE;

constexpr u32 DINODES_START_BLOCKNO = IMAP_START_BLOCKNO + IMAP_BLOCKNUM;
struct __attribute__((aligned(BSIZE))) {
  struct dinode dinodes[MAX_FILE_NUMBER];
} DINODES{};
constexpr u32 DINODES_SIZE = sizeof(DINODES);
constexpr u32 DINODES_BLOCKNUM = DINODES_SIZE / BSIZE;

constexpr u32 BMAP_START_BLOCKNO = DINODES_START_BLOCKNO + DINODES_BLOCKNUM;
constexpr u32 BMAP_SIZE = DATA_BLOCK_NUMBER / 8;
constexpr u32 BMAP_BLOCKNUM = BMAP_SIZE / BSIZE;

constexpr u32 MIN_DATA_BLOCKNO = BMAP_START_BLOCKNO + BMAP_BLOCKNUM;
constexpr u32 MAX_DATA_BLOCKNO = MIN_DATA_BLOCKNO + DATA_BLOCK_NUMBER;

constexpr u64 IMAGE_SIZE = BSIZE + BSIZE + IMAP_SIZE + BMAP_SIZE + DINODES_SIZE + DATA_BLOCK_NUMBER * BSIZE;

u32 free_inum = 0;
u32 free_blockno = MIN_DATA_BLOCKNO;
int disk_fd = -1;

u8 imap[IMAP_SIZE]{};
u8 bmap[BMAP_SIZE]{};


struct dinode* dinode_alloc(void);
u32 block_alloc(void);

u32 reg_meta_copy(struct dirent* d);
void reg_data_copy(struct dirent* d, u32 inum);
void dentry_name_copy(struct dentry* d, const char* name);
u32 directory_copy(const char* path, u32 pinum, bool root = false);

void superblock_write(void);
void dinode_bitmap_write(void);
void dinodes_write(void);
void block_bitmap_write(void);

int
main(int argc, char* argv[])
{
  if (argc != 2) {
    printf("Usage: mkfs {root dir path}\n");
    exit(1);
  }

  disk_fd = open("fs.img", O_CREAT | O_RDWR | O_TRUNC, 0666);
  if (disk_fd < 0) {
    perror("Failed to open fs.img: ");
    exit(1);
  }
  ftruncate64(disk_fd, IMAGE_SIZE);

  superblock_write();
  directory_copy(argv[1], -1, true);
  dinode_bitmap_write();
  dinodes_write();
  block_bitmap_write();

  close(disk_fd);
  return 0;
}

void
superblock_write(void)
{
  struct superblock sb = {
    .dev = 0,
    .imap = IMAP_START_BLOCKNO,
    .inodes = DINODES_START_BLOCKNO,
    .bmap = BMAP_START_BLOCKNO,
    .blocks = MIN_DATA_BLOCKNO,
    .max_inode = MAX_FILE_NUMBER - 1,
    .max_nblock = MAX_DATA_BLOCKNO,
    .name = "tsunami",
  };

  lseek(disk_fd, SB_START_BLOCKNO * BSIZE, SEEK_SET);
  write(disk_fd, &sb, sizeof(sb));
}
void
dinode_bitmap_write()
{
  lseek(disk_fd, IMAP_START_BLOCKNO * BSIZE, SEEK_SET);
  write(disk_fd, imap, IMAP_SIZE);
}
void
dinodes_write()
{
  lseek(disk_fd, DINODES_START_BLOCKNO * BSIZE, SEEK_SET);
  write(disk_fd, &DINODES, sizeof(DINODES));
}
void
block_bitmap_write()
{
  lseek(disk_fd, BMAP_START_BLOCKNO * BSIZE, SEEK_SET);
  write(disk_fd, bmap, BMAP_SIZE);
}

struct dinode*
dinode_alloc(void)
{
  if (free_inum > MAX_FILE_NUMBER - 1) {
    printf("file too much\n");
    exit(1);
  }

  struct dinode* r = DINODES.dinodes + free_inum;

  u32 byte_off = free_inum / 8;
  u8 bit_off = free_inum % 8;
  imap[byte_off] |= (1U << bit_off);

  ++free_inum;
  return r;
}
u32
block_alloc(void)
{
  if (free_blockno > MAX_DATA_BLOCKNO) {
    printf("file too big\n");
    exit(1);
  }

  u32 r = free_blockno;

  u32 byte_off = (free_blockno - MIN_DATA_BLOCKNO) / 8;
  u8 bit_off = (free_blockno - MIN_DATA_BLOCKNO) % 8;
  bmap[byte_off] |= (1U << bit_off);

  ++free_blockno;
  return r;
}

u32
reg_meta_copy(struct dirent* d)
{
  if (d->d_type != DT_REG) {
    printf("%s only support regular file\n", __func__);
    exit(1);
  }
  int fd = open(d->d_name, O_RDONLY);
  struct dinode* di = dinode_alloc();
  struct stat fs;
  fstat(fd, &fs);
  di->fsize = fs.st_size;
  di->nlink = 1;
  di->type = REGULAR;

  int i = 0;
  while (i < NDIRECT && fs.st_size > 0) {
    di->iblock[i] = block_alloc();
    ++i;
    fs.st_size -= BSIZE;
  }

  i = 0;
  while (i < NIDIRECT && fs.st_size > 0) {
    di->iblock[i + NDIRECT] = block_alloc();
    u32 indirect_index_block[BSIZE / sizeof(u32)] = { 0 };
    for (int j = 0; j < sizeof(indirect_index_block) / sizeof(u32) && fs.st_size > 0; ++j) {
      indirect_index_block[j] = block_alloc();
      fs.st_size -= BSIZE;
    }
    lseek(disk_fd, di->iblock[i + NDIRECT] * BSIZE, SEEK_SET);
    write(disk_fd, indirect_index_block, sizeof(indirect_index_block));
    ++i;
  }

  if (fs.st_size > 0) {
    printf("File %s too big\n", d->d_name);
    exit(0);
  }

  close(fd);
  return di - DINODES.dinodes;
}
void
reg_data_copy(struct dirent* d, u32 inum)
{
  if (d->d_type != DT_REG) {
    printf("%s only support regular file\n", __func__);
    exit(1);
  }
  struct dinode* di = DINODES.dinodes + inum;
  char buf[BSIZE] = { 0 };
  int fd = open(d->d_name, O_RDONLY);

  for (int i = 0; i < NDIRECT && di->iblock[i] > 0; ++i) {
    lseek(disk_fd, di->iblock[i] * BSIZE, SEEK_SET);
    read(fd, buf, BSIZE);
    write(disk_fd, buf, BSIZE);
  }

  for (int i = 0; i < NIDIRECT && di->iblock[i + NDIRECT] > 0; ++i) {
    u32 idx[BSIZE / sizeof(u32)] = { 0 };
    lseek(disk_fd, di->iblock[i + NDIRECT] * BSIZE, SEEK_SET);
    read(disk_fd, idx, sizeof(idx));
    for (int j = 0; j < sizeof(idx) / sizeof(u32) && idx[j] > 0; ++j) {
      lseek(disk_fd, idx[j] * BSIZE, SEEK_SET);
      read(fd, buf, BSIZE);
      write(disk_fd, buf, BSIZE);
    }
  }

  close(fd);
}
u32
directory_copy(const char* path, u32 pinum, bool root)
{
  DIR* dir = opendir(path);
  chdir(path);

  queue<string> child_dirs;
  queue<pair<string, u32> > namei;

  struct dinode* di = dinode_alloc();
  u32 inum = di - DINODES.dinodes;
  di->type = DIRECTORY;
  di->nlink = 2; // 目录文件不允许创建硬链接,.和..是例外

  u32 entry_cnt = 0;
  struct dirent* entry;
  while ((entry = readdir(dir))) {
    if (strlen(entry->d_name) > DLENGTH) {
      printf("%s name too long\n", entry->d_name);
      exit(1);
    }
    ++entry_cnt;
    if (entry->d_type == DT_REG) {
      u32 inum = reg_meta_copy(entry);
      reg_data_copy(entry, inum);
      namei.push({ entry->d_name, inum });
    }
    if (entry->d_type == DT_DIR) {
      if (string(entry->d_name) != "." && string(entry->d_name) != "..") {
        child_dirs.push(entry->d_name);
        ++di->nlink;
      }
    }
  }

  di->fsize = entry_cnt * sizeof(struct dentry);
  long dsize = di->fsize;
  int i = 0;
  while (i < NDIRECT && dsize > 0) {
    di->iblock[i] = block_alloc();
    ++i;
    dsize -= BSIZE;
  }
  i = 0;
  while (i < NIDIRECT && dsize > 0) {
    di->iblock[i + NDIRECT] = block_alloc();
    u32 indirect_index_block[BSIZE / sizeof(u32)] = { 0 };
    for (int j = 0; j < sizeof(indirect_index_block) / sizeof(u32) && dsize > 0; ++j) {
      indirect_index_block[j] = block_alloc();
      dsize -= BSIZE;
    }
    lseek(disk_fd, di->iblock[i + NDIRECT] * BSIZE, SEEK_SET);
    write(disk_fd, indirect_index_block, sizeof(indirect_index_block));
    ++i;
  }

  if (dsize > 0) {
    printf("Directory %s too big\n", path);
    exit(1);
  }

  for (int i = 0; i < NDIRECT && di->iblock[i] > 0; ++i) {
    struct dentry dts[BSIZE / sizeof(dentry)] = { 0 };
    int j = 0;
    if (i == 0) [[unlikely]] {
      struct dentry s = { .inum = inum, .name = "." };
      struct dentry p = { .inum = root ? inum : pinum, .name = ".." };
      memcpy(dts + j++, &s, sizeof(s));
      memcpy(dts + j++, &p, sizeof(p));
    }
    while (j < sizeof(dts) / sizeof(dentry) && !child_dirs.empty()) {
      struct dentry c;
      c.inum = directory_copy(child_dirs.front().c_str(), inum);
      dentry_name_copy(&c, child_dirs.front().c_str());
      child_dirs.pop();
      memcpy(dts + j++, &c, sizeof(c));
    }

    while (j < sizeof(dts) / sizeof(dentry) && !namei.empty()) {
      struct dentry c;
      c.inum = namei.front().second;
      dentry_name_copy(&c, namei.front().first.c_str());
      namei.pop();
      memcpy(dts + j++, &c, sizeof(c));
    }

    lseek(disk_fd, di->iblock[i] * BSIZE, SEEK_SET);
    write(disk_fd, dts, sizeof(dts));
  }

  for (int i = 0; i < NIDIRECT && di->iblock[i + NDIRECT] > 0; ++i) {
    u32 idx[BSIZE / sizeof(u32)] = { 0 };
    lseek(disk_fd, di->iblock[i + NDIRECT] * BSIZE, SEEK_SET);
    read(disk_fd, idx, sizeof(idx));
    for (int j = 0; j < sizeof(idx) / sizeof(u32) && idx[j] > 0; ++j) {
      struct dentry dts[BSIZE / sizeof(dentry)] = { 0 };
      int k = 0;
      while (k < sizeof(dts) / sizeof(dentry) && !child_dirs.empty()) {
        struct dentry c;
        c.inum = directory_copy(child_dirs.front().c_str(), inum);
        dentry_name_copy(&c, child_dirs.front().c_str());
        child_dirs.pop();
        memcpy(dts + k++, &c, sizeof(c));
      }

      while (k < sizeof(dts) / sizeof(dentry) && !namei.empty()) {
        struct dentry c;
        c.inum = namei.front().second;
        dentry_name_copy(&c, namei.front().first.c_str());
        namei.pop();
        memcpy(dts + k++, &c, sizeof(c));
      }

      lseek(disk_fd, idx[j] * BSIZE, SEEK_SET);
      write(disk_fd, dts, sizeof(dts));
    }
  }

  chdir("..");
  closedir(dir);
  return inum;
}

void
dentry_name_copy(struct dentry* d, const char* name)
{
  memset(d->name, 0, DLENGTH);
  memcpy(d->name, name, strlen(name));
}