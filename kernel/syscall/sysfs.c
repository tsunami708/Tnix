#include "config.h"
#include "fs/file.h"
#include "fs/inode.h"
#include "fs/dir.h"
#include "fs/fs.h"
#include "fs/bio.h"
#include "fs/pipe.h"
#include "task/task.h"
#include "trap/pt_reg.h"
#include "util/string.h"
#include "util/printf.h"
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
  struct inode* in = dentry_find(path);
  if (in) {
    iput(in);
    return -1;
  }

  // 2.检查父目录是否存在
  char parentpath[MAX_PATH_LENGTH] = { 0 }, filename[DLENGTH] = { 0 };
  path_split(path, parentpath, filename);
  struct inode* parent = dentry_find(parentpath);
  if (parent == NULL)
    return -1;

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
    new->di.dev = dev;
    new->di.type = CHAR;
    break;
  default:
    return false;
  }
  iupdate(new);
  iput(new);

  // 4.更新父目录的目录项
  dentry_add(parent, new->inum, filename);
  iput(parent);
  return 0;
}
static void
move_fdi(struct task* t)
{
  for (u32 i = t->files.i; i < NFILE; ++i) {
    if (t->files.f[i] == NULL) {
      t->files.i = i;
      break;
    }
  }
}

long
sys_read(struct pt_regs* pt)
{
  int fd = pt->a0;
  void* udst = (void*)pt->a1;
  u32 len = pt->a2;
  struct task* t = mytask();
  struct file* f = t->files.f[fd];
  if (f == NULL)
    return -1;
  if ((f->mode & O_RDONLY) == 0 && (f->mode & O_RDWR) == 0)
    return -1;
  switch (f->type) {
  case NONE:
    return -1;
  case DEVICE:
    return devsw[t->files.f[fd]->inode->di.dev].read(udst, len);
  case INODE:
    return fread(f, udst, len, false);
  case PIPE:
    return piperead(f->pipe, udst, len);
  }
  return 0;
}

long
sys_write(struct pt_regs* pt)
{
  int fd = pt->a0;
  const void* usrc = (void*)pt->a1;
  u32 len = pt->a2;
  struct task* t = mytask();
  struct file* f = t->files.f[fd];
  if (f == NULL)
    return -1;
  if ((f->mode & O_WRONLY) == 0 && (f->mode & O_RDWR) == 0)
    return -1;
  switch (f->type) {
  case NONE:
    return -1;
  case DEVICE:
    return devsw[t->files.f[fd]->inode->di.dev].write(usrc, len);
  case INODE:
    return fwrite(f, usrc, len, false);
  case PIPE:
    return pipewrite(f->pipe, usrc, len);
  }
  return 0;
}

long
sys_lseek(struct pt_regs* pt)
{
  int fd = pt->a0, off = pt->a1;
  struct file* f = mytask()->files.f[fd];
  if (f == NULL || f->type != INODE)
    return -1;
  return fseek(f, off, SEEK_CUR);
}

long
sys_mknod(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -1;
  char path[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, path) == false)
    return -1;
  return create(path, CHAR, pt->a1);
}

long
sys_open(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -1;
  char path[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, path) == false)
    return -1;
  int mode = pt->a1;
  if ((mode & O_RDONLY) && (mode & O_WRONLY) && ((mode & O_RDWR) == 0))
    return -1;
  struct inode* in = dentry_find(path);
  if (in == NULL) {
    if (mode & O_CREAT) {
      int r = create(path, REGULAR, -1);
      if (r < 0)
        return r;
      else
        in = dentry_find(path);
    } else
      return -1;
  }
  if (in->di.type == DIRECTORY) {
    iput(in);
    return -1;
  }
  if ((mode & O_APPEND) == 0 && (mode & O_WRONLY || mode & O_RDWR)) {
    itrunc(in);
    iupdate(in);
  }

  struct file* f = falloc();
  struct task* t = mytask();
  f->inode = in;
  f->type = in->di.type == CHAR ? DEVICE : INODE;
  f->mode = mode;
  f->off = mode & O_APPEND ? in->di.fsize : 0;
  long r = t->files.i;
  t->files.f[r] = f;
  move_fdi(t);
  return r;
}

long
sys_dup(struct pt_regs* pt)
{
  struct task* t = mytask();
  int fd = pt->a0;
  if (t->files.f[fd] == NULL)
    return -1;
  long r = t->files.i;
  fdup(t->files.f[fd]);
  t->files.f[r] = t->files.f[fd];
  move_fdi(t);
  return r;
}

long
sys_close(struct pt_regs* pt)
{
  struct task* t = mytask();
  int fd = pt->a0;
  if (t->files.f[fd] == NULL)
    return -1;
  fclose(t->files.f[fd]);
  t->files.f[fd] = NULL;
  t->files.i = min(fd, t->files.i);
  return 0;
}

long
sys_link(struct pt_regs* pt)
{
  if (pt->a0 == 0 || pt->a1 == 0)
    return -1;
  char oldpath[MAX_PATH_LENGTH] = { 0 }, newpath[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, oldpath) == false || argstr(pt->a1, newpath) == false)
    return -1;

  struct inode* in = dentry_find(newpath);
  if (in) { // 目录项重复
    iput(in);
    return -1;
  }

  in = dentry_find(oldpath);
  if (in == NULL)
    return -1;
  if (in->di.type != REGULAR) {
    iput(in);
    return -1;
  }

  u32 inum = in->inum;
  ++in->di.nlink;
  iupdate(in);
  iput(in);

  char ppath[MAX_PATH_LENGTH] = { 0 }, name[DLENGTH] = { 0 };
  path_split(newpath, ppath, name);
  in = dentry_find(ppath);
  dentry_add(in, inum, name);
  iput(in);
  return 0;
}

long
sys_unlink(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -1;
  char path[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, path) == false)
    return -1;
  struct inode* in = dentry_find(path);
  if (in == NULL)
    return -1;
  if (in->di.type != REGULAR) {
    iput(in);
    return -1;
  }
  --in->di.nlink;
  if (in->di.nlink == 0)
    ifree(in);
  else
    iupdate(in);
  iput(in);

  char ppath[MAX_PATH_LENGTH] = { 0 }, name[DLENGTH] = { 0 };
  path_split(path, ppath, name);
  in = dentry_find(ppath);
  dentry_del(in, name);
  iupdate(in);
  iput(in);
  return 0;
}

long
sys_mkdir(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -1;
  char path[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, path) == false)
    return -1;
  return create(path, DIRECTORY, -1);
}

long
sys_rmdir(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -1;
  char path[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, path) == false)
    return -1;
  struct inode* c = dentry_find(path);
  if (c == NULL)
    return -1;
  if (c->di.type != DIRECTORY)
    return -1;

  // 检查是否为空目录
  struct buf* b = bread(c->sb->dev, c->di.iblock[0]);
  if (*(u64*)b->data != 2) {
    brelse(b);
    return -1;
  }
  brelse(b);
  iput(c);

  char pdir[MAX_PATH_LENGTH] = { 0 }, name[MAX_PATH_LENGTH] = { 0 };
  path_split(path, pdir, name);
  struct inode* p = dentry_find(pdir);
  dentry_del(p, name);
  --p->di.nlink;
  iupdate(p);
  iput(p);
  return 0;
}

long
sys_chdir(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -1;
  char path[MAX_PATH_LENGTH] = { 0 };
  if (argstr(pt->a0, path) == false)
    return -1;
  struct inode* in = dentry_find(path);
  if (in == NULL)
    return -1;
  iput(mytask()->cwd);
  mytask()->cwd = in;
  return 0;
}

long
sys_pipe(struct pt_regs* pt)
{
  if (pt->a0 == 0)
    return -1;
  int* fds = (int*)pt->a0;
  struct task* t = mytask();
  struct pipe* p = pipealloc();
  struct file *rf = falloc(), *wf = falloc();
  rf->pipe = wf->pipe = p;
  rf->type = wf->type = PIPE;
  rf->mode = O_RDONLY;
  wf->mode = O_WRONLY;

  t->files.f[t->files.i] = rf;
  copy_to_user(fds, &t->files.i, sizeof(int));
  move_fdi(t);
  t->files.f[t->files.i] = wf;
  copy_to_user(fds + 1, &t->files.i, sizeof(int));
  move_fdi(t);
  return 0;
}

long
sys_ls(struct pt_regs*)
{
  char name[DLENGTH + 1] = { 0 };
  struct inode* cur = mytask()->cwd;
  int blockcnt = iblock_cnt(cur);
  for (int i = 0; i < blockcnt; ++i) {
    struct buf* b = data_block_get(cur, i);
    struct dentry* dt = (struct dentry*)b->data + 1;
    u64 cnt = *(u64*)b->data, k = 0;
    while (k != cnt) {
      if (*dt->name) {
        memcpy(name, dt->name, DLENGTH);
        print("%s ", name);
        ++k;
      }
      ++dt;
    }
    brelse(b);
  }
  print("\n", name);
  return 0;
}