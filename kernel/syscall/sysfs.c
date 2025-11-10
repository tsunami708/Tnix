#include "config.h"
#include "fs/file.h"
#include "fs/inode.h"
#include "fs/dir.h"
#include "fs/fs.h"
#include "fs/bio.h"
#include "task/task.h"
#include "trap/pt_reg.h"
#include "util/string.h"
#include "syscall/syscall.h"

struct dev_op devsw[NDEV];

/*
  创建文件:
    申请inode节点标识一个新文件产生(设置imap和dinode)
    更新文件所属的目录文件(新增一条目录项)
    增加父目录的硬链接计数(如果创建的是目录)
*/
static int
create(const char* path, enum ftype type, dev_t dev)
{
  // 1.如果目录项已经存在,则不能再创建
  struct inode* in = dlookup(path);
  if (in) {
    iput(in);
    return -EEXIST;
  }

  // 2.检查父目录是否存在
  char parentpath[MAX_PATH_LENGTH] = { 0 }, filename[MAX_PATH_LENGTH] = { 0 };
  path_split(path, parentpath, filename);
  struct inode* parent = dlookup(parentpath);
  if (parent == NULL)
    return -ENOENT;

  // 3.申请inode节点~创建文件元数据
  struct superblock* sb = parent->sb;
  struct inode* new = iget(sb, ialloc(sb));
  new->di.nlink = 1;

  switch (type) {
  case REGULAR:
    new->di.type = REGULAR;
    break;
  case DIRECTORY:
    new->di.type = DIRECTORY;
    dentry_add(new, new->inum, ".");
    dentry_add(new, parent->inum, "..");
    ++parent->di.nlink; //..
    break;
  case CHAR:
    if (! devsw[dev].valid)
      return false;
    new->dev = dev;
    new->di.type = CHAR;
    break;
  default:
    return false;
  }
  iupdate(new);

  // 4.更新父目录的目录项
  dentry_add(parent, new->inum, filename);
  iupdate(parent);
  return 0;
}

u64
sys_read(struct pt_regs* pt)
{
  int fd = pt->a0;
  void* udst = (void*)pt->a1;
  u32 len = pt->a3;
  struct task* t = mytask();
  struct file* f = t->files.f[fd];
  if (f == NULL)
    return -EINVAL;
  if ((f->mode & O_RDONLY) == 0 || (f->mode & O_RDWR) == 0)
    return -EACCES;
  switch (f->type) {
  case NONE:
    return -EINVAL;
  case DEVICE:
    return devsw[t->files.f[fd]->inode->dev].read(udst, len);
  case INODE:
    return fread(f, udst, len);
  case PIPE:
    return 0; // todo
  }
  return 0;
}

u64
sys_write(struct pt_regs* pt)
{
  int fd = pt->a0;
  const void* usrc = (void*)pt->a1;
  u32 len = pt->a3;
  struct task* t = mytask();
  struct file* f = t->files.f[fd];
  if (f == NULL)
    return -EINVAL;
  if ((f->mode & O_WRONLY) == 0 || (f->mode & O_RDWR) == 0)
    return -EACCES;
  switch (f->type) {
  case NONE:
    return -EINVAL;
  case DEVICE:
    return devsw[t->files.f[fd]->inode->dev].write(usrc, len);
  case INODE:
    return fwrite(f, usrc, len);
  case PIPE:
    return 0; // todo
  }
  return 0;
}

u64
sys_mknod(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -EINVAL;
  char path[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, path) == false)
    return -EINVAL;
  return create(path, CHAR, pt->a1);
}

u64
sys_open(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -EINVAL;
  char path[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, path) == false)
    return -EINVAL;
  int mode = pt->a1;
  if ((mode & O_RDONLY) && (mode & O_WRONLY) && ((mode & O_RDONLY) == 0))
    return -EINVAL;
  struct inode* in = dlookup(path);
  if (in == NULL) {
    if (mode & O_CREAT)
      return create(path, REGULAR, -1);
    else
      return -ENOENT;
  }
  if (in->di.type == DIRECTORY) {
    iput(in);
    return -EISDIR;
  }

  struct file* f = falloc();
  struct task* t = mytask();
  f->inode = in;
  f->type = in->di.type == CHAR ? DEVICE : INODE;
  f->size = in->di.fsize;
  f->mode = mode;
  f->off = 0;
  u64 r = t->files.i++;
  t->files.f[r] = f;
  return r;
}

u64
sys_dup(struct pt_regs* pt)
{
  struct task* t = mytask();
  int fd = pt->a0;
  if (t->files.f[fd] == NULL)
    return -EINVAL;
  u64 r = t->files.i;
  fdup(t->files.f[fd]);
  t->files.f[r] = t->files.f[fd];
  for (int j = r; j < NFILE; ++j)
    if (t->files.f[j] == NULL)
      t->files.i = j;
  return r;
}

u64
sys_close(struct pt_regs* pt)
{
  struct task* t = mytask();
  int fd = pt->a0;
  if (t->files.f[fd] == NULL)
    return -EINVAL;
  fclose(t->files.f[fd]);
  t->files.f[fd] = NULL;
  t->files.i = fd;
  return 0;
}

u64
sys_link(struct pt_regs* pt)
{
  if (pt->a0 == 0 || pt->a1 == 0)
    return -EINVAL;
  char oldpath[MAX_PATH_LENGTH] = { 0 }, newpath[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, oldpath) == false || argstr(pt->a1, newpath) == false)
    return -EINVAL;

  struct inode* in = dlookup(newpath);
  if (in) { // 目录项重复
    iput(in);
    return -EINVAL;
  }

  in = dlookup(oldpath);
  if (in == NULL)
    return -ENOENT;
  if (in->di.type != REGULAR) {
    iput(in);
    return -EINVAL;
  }

  u32 inum = in->inum;
  ++in->di.nlink;
  iupdate(in);
  iput(in);

  char ppath1[MAX_PATH_LENGTH] = { 0 }, ppath2[MAX_PATH_LENGTH] = { 0 }, name[MAX_PATH_LENGTH] = { 0 };
  path_split(oldpath, ppath1, name);
  in = dlookup(ppath1);
  dentry_add(in, inum, name);
  iupdate(in);
  iput(in);
  path_split(newpath, ppath2, name);
  if (strncmp(ppath1, ppath2, MAX_PATH_LENGTH) != 0) {
    in = dlookup(ppath2);
    dentry_add(in, inum, name);
    iupdate(in);
    iput(in);
  }
  return 0;
}

u64
sys_unlink(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -EINVAL;
  char path[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, path) == false)
    return -EINVAL;
  struct inode* in = dlookup(path);
  if (in == NULL)
    return -ENOENT;
  if (in->di.type != REGULAR) {
    iput(in);
    return -EINVAL;
  }
  --in->di.nlink;
  if (in->di.nlink == 0)
    ifree(in);
  else
    iupdate(in);
  iput(in);

  char ppath[MAX_PATH_LENGTH] = { 0 }, name[MAX_PATH_LENGTH] = { 0 };
  path_split(path, ppath, name);
  in = dlookup(ppath);
  dentry_del(in, name);
  iupdate(in);
  iput(in);
  return 0;
}

u64
sys_mkdir(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -EINVAL;
  char path[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, path) == false)
    return -EINVAL;
  return create(path, DIRECTORY, -1);
}

u64
sys_rmdir(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -EINVAL;
  char path[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, path) == false)
    return -EINVAL;
  struct inode* c = dlookup(path);
  if (c == NULL)
    return -ENOENT;
  if (c->di.type != DIRECTORY)
    return -EINVAL;

  // 检查是否为空目录
  struct buf* b = bread(c->sb->dev, c->di.iblock[0]);
  if (*(u64*)b->data != 2) {
    brelse(b);
    return -ENEMPTY;
  }
  brelse(b);
  iput(c);

  char pdir[MAX_PATH_LENGTH] = { 0 }, name[MAX_PATH_LENGTH] = { 0 };
  path_split(path, pdir, name);
  struct inode* p = dlookup(pdir);
  dentry_del(p, name);
  --p->di.nlink;
  iput(p);
  return 0;
}

u64
sys_chdir(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -EINVAL;
  char path[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, path) == false)
    return -EINVAL;
  struct inode* in = dlookup(path);
  iput(mytask()->cwd);
  mytask()->cwd = in;
  return 0;
}
