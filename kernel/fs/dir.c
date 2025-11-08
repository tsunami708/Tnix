#include "fs/dir.h"
#include "fs/inode.h"
#include "fs/bio.h"
#include "task/cpu.h"
#include "task/task.h"
#include "util/string.h"

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

    int i = 0;
    bool found = false;
    struct superblock* sb = cur->sb;
    dev_t dev = sb->dev;

    while (cur->di.iblock[i] != 0 && ! found) {
      buf = bread(dev, cur->di.iblock[i]);
      struct dentry* dentry = (void*)buf->data;
      for (int j = 0; j < BSIZE / sizeof(struct dentry); ++j)
        if (strncmp(dname, (dentry + j)->name, DLENGTH) == 0) {
          iput(cur);
          cur = iget(sb, (dentry + j)->inum);
          found = true;
          break;
        }
      brelse(buf);
      ++i;
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