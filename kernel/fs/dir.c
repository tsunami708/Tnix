#include "fs/dir.h"
#include "fs/inode.h"
#include "fs/bio.h"
#include "task/cpu.h"
#include "task/task.h"
#include "util/string.h"
#include "syscall/syscall.h"

extern struct superblock rfs;
#define DENTRY_MAXCNT_PER_BLOCK (BSIZE / sizeof(struct dentry) - 1)
/*
  提取路径中的首个目录项写入到dname中
  * eg : "dir0/dir1/dir3/file"
  * dname="dir0"
  * p="dir1/dri3/file"
*/
static const char*
parse_path(const char* path, char dname[DLENGTH])
{
  if (*path == '\0')
    return NULL;
  const char* p = path;
  while (*p != '/' && *p != '\0')
    ++p;
  memset(dname, 0, DLENGTH);
  if (*p == '/') {
    strncpy(dname, path, p - path);
    return p + 1;
  }
  strncpy(dname, path, p - path + 1);
  return p;
}


static void
dentry_name_set(char dst[DLENGTH], const char* src)
{
  for (int i = 0; i < DLENGTH; ++i) {
    dst[i] = src[i];
    if (src[i] == '\0')
      break;
  }
}


static bool
dentry_name_equal(const char s1[DLENGTH], const char* s2)
{
  int i = 0;
  for (; i < DLENGTH; ++i) {
    if (s1[i] != s2[i])
      return false;
    if (s1[i] == '\0' && s2[i] == '\0')
      return true;
  }
  if (i == DLENGTH && s2[i] == '\0')
    return true;
  return false;
}

// 不会拷贝\0,调用者主动使用{0}初始化接收缓冲区
void
path_split(const char* path, char* parentpath, char* filename)
{
  int m = -1, n = 0;
  while (path[n]) {
    if (path[n] == '/')
      m = n;
    ++n;
  }
  if (m == -1) {
    *parentpath = '.';
    memcpy(filename, path, n);
  } else {
    memcpy(parentpath, path, m);
    memcpy(filename, path + m + 1, n - m - 1);
  }
}

struct inode*
dentry_find(const char* path)
{
  struct inode* cur;
  if (*path == '/') {
    cur = iget(&rfs, ROOTINUM);
    ++path;
  } else
    cur = iget(mytask()->fs_struct->cwd->sb, mytask()->fs_struct->cwd->inum);

  struct buf* b;
  struct superblock* sb;
  const char* p;
  bool found;
  int blockcnt;
  char dname[DLENGTH];
  while ((p = parse_path(path, dname))) {
    path = p;
    found = false;
    sb = cur->sb;
    blockcnt = iblock_cnt(cur);

    for (int i = 0; i < blockcnt && ! found; ++i) {
      b = data_block_get(cur, i);
      struct dentry* dt = (struct dentry*)b->data + 1;
      u64 cnt = *(u64*)b->data;
      for (int j = 1, k = 0; j <= DENTRY_MAXCNT_PER_BLOCK && k < cnt; ++j) {
        if (dentry_name_equal(dt->name, dname)) {
          iput(cur);
          cur = iget(sb, dt->inum);
          found = true;
          break;
        }
        if (*dt->name != '\0')
          ++k;
        ++dt;
      }
      brelse(b);
    }

    if (! found) {
      iput(cur);
      return NULL;
    }
  }
  return cur;
}


int
dentry_add(struct inode* dir, u32 inum, const char* name)
{
  struct buf* b;
  struct dentry* dt;
  u64* cnt;
  int blockcnt = iblock_cnt(dir);
  bool done = false;

  for (int i = 0; i < blockcnt && ! done; ++i) {
    b = data_block_get(dir, i);
    dt = (struct dentry*)b->data + 1;
    cnt = (u64*)b->data;

    if (*cnt == DENTRY_MAXCNT_PER_BLOCK) {
      brelse(b);
      continue;
    }

    for (int j = 1; j <= DENTRY_MAXCNT_PER_BLOCK; ++j) {
      if (*(dt->name) == '\0') {
        done = true;
        dt->inum = inum;
        dentry_name_set(dt->name, name);
        ++(*cnt);
        bwrite(b);
        break;
      }
      ++dt;
    }

    brelse(b);
  }

  if (! done) {
    b = data_block_alloc(dir);
    dt = (struct dentry*)b->data + 1;
    *(u64*)b->data = 1;
    dt->inum = inum;
    dentry_name_set(dt->name, name);
    bwrite(b);
    brelse(b);
    dir->di.fsize += BSIZE;
    iupdate(dir);
  }
  return 0;
}


void
dentry_del(struct inode* dir, const char* name)
{
  struct buf* b;
  struct dentry* dt;
  u64* cnt;
  int blockcnt = iblock_cnt(dir);
  bool done = false;

  for (int i = 0; i < blockcnt && ! done; ++i) {
    b = data_block_get(dir, i);
    dt = (struct dentry*)b->data + 1;
    cnt = (u64*)b->data;

    for (int j = 1, k = 0; j <= DENTRY_MAXCNT_PER_BLOCK && k < *cnt; ++j) {
      if (dentry_name_equal(dt->name, name)) {
        done = true;
        *(u64*)(dt->name) = 0;
        --(*cnt);
        if (*cnt == 0) {
          idx_remove(dir, i);
          dir->di.fsize -= BSIZE;
          iupdate(dir);
          bfree(dir->sb, b->blockno);
        } else
          bwrite(b);
        break;
      }
      if (*dt->name != '\0')
        ++k;
      ++dt;
    }
    brelse(b);
  }
}
