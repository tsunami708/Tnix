#include "config.h"
#include "fs/file.h"
#include "fs/inode.h"
#include "fs/dir.h"
#include "fs/fs.h"
#include "task/task.h"
#include "trap/pt_reg.h"
#include "syscall/syscall.h"

struct dev_op devsw[NDEV];

/*
  创建文件:
    申请inode节点标识一个新文件产生(设置imap和dinode)
    更新文件所属的目录文件(新增一条目录项)
    增加父目录的硬链接计数(如果创建的是目录)
*/
static bool
create(const char* path, enum ftype type, dev_t dev)
{
  char parentpath[MAX_PATH_LENGTH] = { 0 }, filename[MAX_PATH_LENGTH] = { 0 };
  path_split(path, parentpath, filename);
  struct inode* parent = dlookup(parentpath);
  if (parent == NULL)
    return false;

  struct superblock* sb = parent->sb;
  struct inode* new = iget(sb, ialloc(sb));
  ireset(new);
  new->di.nlink = 1;

  switch (type) {
  case REGULAR:
    new->di.type = REGULAR;
    break;
  case DIRECTORY:
    new->di.type = DIRECTORY;
    dentry_add(new, new->inum, ".");
    dentry_add(new, parent->inum, "..");
    ++parent->di.nlink;
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
  dentry_add(parent, new->inum, filename);
  iupdate(parent);
  return true;
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
    return -1;
  switch (f->type) {
  case NONE:
    return -1;
  case DEVICE:
    return devsw[t->files.f[fd]->inode->dev].read(udst, len);
  case INODE:
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
  switch (t->files.f[fd]->type) {
  case NONE:
    return -1;
  case DEVICE:
    return devsw[t->files.f[fd]->inode->dev].write(usrc, len);
  case INODE:
  case PIPE:
    return 0; // todo
  }
  return 0;
}

u64
sys_mknod(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -1;
  char path[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, path) == false)
    return -1;
  if (create(path, CHAR, pt->a1) == false)
    return -1;
  return 0;
}

u64
sys_open(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -1;
  char path[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, path) == false)
    return -1;
  int mode = pt->a1;
  struct inode* in = dlookup(path);
  if (in == NULL) {
    if (mode & O_CREAT) {
      if (create(path, REGULAR, -1) == false)
        return -1;
    } else
      return -1;
  }
  if (in->di.type == DIRECTORY) {
    iput(in);
    return -1;
  }

  struct file* f = falloc();
  struct task* t = mytask();
  f->inode = in;
  f->type = in->di.type == CHAR ? DEVICE : INODE;
  f->size = in->di.fsize;
  f->mode = mode;
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
    return -1;
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
    return -1;
  fclose(t->files.f[fd]);
  t->files.f[fd] = NULL;
  t->files.i = fd;
  return 0;
}

u64
sys_link(struct pt_regs* pt)
{
  return 0;
}
u64
sys_unlink(struct pt_regs* pt)
{
  return 0;
}
u64
sys_mkdir(struct pt_regs* pt)
{
  return 0;
}
u64
sys_rmdir(struct pt_regs* pt)
{
  return 0;
}
u64
sys_chdir(struct pt_regs* pt)
{
  return 0;
}
u64
sys_rename(struct pt_regs* pt)
{
  return 0;
}