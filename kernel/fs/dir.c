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
  memset1(dname, 0, DLENGTH);
  if (*p == '/') {
    strncpy1(dname, path, p - path);
    return p + 1;
  }
  strncpy1(dname, path, p - path + 1);
  return p;
}

struct inode*
lookup_dentry(const char* path)
{
  struct inode* cur;
  struct iobuf* buf;

  if (*path == '/') {
    cur = do_get_inode(&rfs, ROOTINUM);
    ++path;
  } else
    cur = do_get_inode(mytask()->cwd->sb, mytask()->cwd->inum);


  char dname[DLENGTH];
  const char* p;
  while ((p = parse_path(path, dname))) {
    path = p;

    int i = 0;
    bool found = false;
    struct superblock* sb = cur->sb;
    dev_t dev = sb->dev;

    while (cur->di.iblock[i] != 0 && !found) {
      buf = read_iobuf(dev, cur->di.iblock[i]);
      struct dentry* dentry = (void*)buf->data;
      for (int j = 0; j < BSIZE / sizeof(struct dentry); ++j)
        if (strncmp1(dname, (dentry + j)->name, DLENGTH) == 0) {
          put_inode(cur);
          cur = do_get_inode(sb, (dentry + j)->ium);
          found = true;
          break;
        }
      release_iobuf(buf);
      ++i;
    }
    if (!found) {
      put_inode(cur);
      return NULL; // 无效路径
    }
  }
  return cur;
}
