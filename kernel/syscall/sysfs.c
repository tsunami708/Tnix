#include "config.h"
#include "fs/file.h"
#include "fs/inode.h"
#include "fs/dir.h"
#include "fs/fs.h"
#include "task/task.h"
#include "trap/pt_reg.h"
#include "syscall/syscall.h"

struct dev_op devsw[NDEV];

static bool
create(const char* path, enum ftype type)
{
  char parentpath[MAX_PATH_LENGTH] = { 0 }, filename[MAX_PATH_LENGTH] = { 0 };
  path_split(path, parentpath, filename);
  if (dlookup(parentpath) == NULL)
    return false;
  return 1;
}

u64
sys_read(struct pt_regs* pt)
{
  int fd = pt->a0;
  void* udst = (void*)pt->a1;
  u32 len = pt->a3;
  struct task* t = mytask();
  switch (t->files.f[fd]->type) {
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
  if (create(path, CHAR) == false)
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
      if (create(path, REGULAR) == false)
        return -1;
    } else
      return -1;
  }
  if (in->di.type == DIRECTORY)
    return -1;

  struct file* f = falloc();
  struct task* t = mytask();
  f->inode = in;
  f->type = INODE;
  f->off = 0;
  f->refc = 1;
  f->size = in->di.fsize;
  f->mode = mode;
  u64 r = t->files.i;
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
  t->files.i = fd;
  return 0;
}