#include "fs/dir.h"
#include "fs/inode.h"
#include "fs/bio.h"
#include "task/cpu.h"
#include "task/task.h"
#include "util/string.h"
#include "syscall/syscall.h"

extern struct superblock rfs;

/*
  desc: 提取路径中的首个目录项写入到dname中
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
dentry_name_copy(char* dst, const char* src)
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

struct inode*
dlookup(const char* path)
{
  struct inode* cur;
  struct buf* buf;

  if (*path == '/') {
    cur = iget(&rfs, ROOTINUM);
    ++path;
  } else
    cur = iget(mytask()->cwd->sb, mytask()->cwd->inum);


  char dname[DLENGTH];
  const char* p;
  while ((p = parse_path(path, dname))) {
    path = p;

    bool found = false;
    struct superblock* sb = cur->sb;
    int maxi = (cur->di.fsize / BSIZE - (cur->di.fsize % BSIZE == 0 ? 1 : 0));

    for (int i = 0; i <= maxi && ! found; ++i) {
      buf = data_block_get(cur, i);
      struct dentry* dentry = (void*)buf->data;
      for (int j = 1; j < BSIZE / sizeof(struct dentry); ++j)
        if (dentry_name_equal((dentry + j)->name, dname)) {
          iput(cur);
          cur = iget(sb, (dentry + j)->inum);
          found = true;
          break;
        }
      brelse(buf);
    }

    if (! found) {
      iput(cur);
      return NULL; // 无效路径
    }
  }
  return cur;
}

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

#define DENTRY_MAXCNT_PER_BLOCK (BSIZE / sizeof(struct dentry))
int
dentry_add(struct inode* dir, u32 inum, const char* name)
{
  struct buf* b;
  int maxi = (dir->di.fsize / BSIZE - (dir->di.fsize % BSIZE == 0 ? 1 : 0));
  bool done = false;
  for (int i = 0; i <= maxi && ! done; ++i) {
    b = data_block_get(dir, i);
    struct dentry* dt = (void*)b->data;
    u64* cnt = (u64*)dt;

    if (*cnt == DENTRY_MAXCNT_PER_BLOCK) {
      brelse(b);
      continue;
    }

    for (int j = 1; j < BSIZE / sizeof(struct dentry); ++j)
      if (*((dt + j)->name) == '\0') {
        done = true;
        (dt + j)->inum = inum;
        dentry_name_copy((dt + j)->name, name);
        ++(*cnt);
        bwrite(b);
        break;
      }

    brelse(b);
  }

  if (! done) {
    b = data_block_alloc(dir);
    if (b == NULL)
      return -EFBIG;
    struct dentry* dt = (void*)b->data;
    *(u64*)dt = 1;
    (dt + 1)->inum = inum;
    dentry_name_copy((dt + 1)->name, name);
    brelse(b);
    dir->di.fsize += BSIZE;
  }
  return 0;
}
void
dentry_del(struct inode* dir, const char* name)
{
  struct buf* b;
  int maxi = (dir->di.fsize / BSIZE - (dir->di.fsize % BSIZE == 0 ? 1 : 0));
  bool done = false;
  for (int i = 0; i <= maxi && ! done; ++i) {
    b = data_block_get(dir, i);
    struct dentry* dt = (void*)b->data;
    u64* cnt = (u64*)dt;

    for (int j = 1; j < BSIZE / sizeof(struct dentry); ++j)
      if (dentry_name_equal((dt + j)->name, name)) {
        done = true;
        *(u64*)((dt + j)->name) = 0;
        --(*cnt);
        if (*cnt == 0) {
          idx_remove(dir, i);
          dir->di.fsize -= BSIZE;
          free_block(dir->sb, b->blockno);
        } else
          bwrite(b);
        break;
      }
    brelse(b);
  }
}
