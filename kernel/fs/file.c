#include "fs/file.h"
#include "fs/inode.h"
#include "fs/dir.h"
#include "fs/bio.h"
#include "util/spinlock.h"
#include "util/printf.h"
#include "util/string.h"

#define MFILES 50
static struct {
  struct spinlock lock;
  struct file files[MFILES];
} fcache = { .lock.lname = "fcache" };

struct file*
falloc(void)
{
  acquire_spin(&fcache.lock);
  for (int i = 0; i < MFILES; ++i) {
    acquire_spin(&fcache.files[i].lock);
    if (fcache.files[i].refc == 0) {
      fcache.files[i].refc = 1;
      fcache.files[i].off = 0;
      release_spin(&fcache.files[i].lock);
      release_spin(&fcache.lock);
      return fcache.files + i;
    }
    release_spin(&fcache.files[i].lock);
  }
  panic("file exhausted");
}

bool
fopen(struct file* f, const char* path)
{
  f->inode = dlookup(path);
  if (!f->inode)
    return false;
  f->size = f->inode->di.fsize;
  f->off = 0;
  f->type = INODE;
  return true;
}

void
fdup(struct file* f)
{
  acquire_spin(&f->lock);
  if (f->refc < 1)
    panic("illegal fdup");
  ++f->refc;
  release_spin(&f->lock);
}

void
fclose(struct file* f)
{
  acquire_spin(&f->lock);
  if (f->refc == 0)
    panic("illegal fclose");
  --f->refc;
  release_spin(&f->lock);
}

void
fupdate(struct file* f)
{
  f->inode->di.fsize = f->size;
  iupdate(f->inode);
}

u32
fseek(struct file* f, u32 off)
{
  u32 r = f->off;
  if (f->type == INODE) {
    if (off >= f->size)
      panic("fseek too long");
    f->off = off;
  }
  return r;
}

static u32
blockno_of_data(struct inode* in, u32 off)
{
  u32* iblock = in->di.iblock;
  if (off < BSIZE * NDIRECT)
    return iblock[off / BSIZE];
  u32 i = (off - BSIZE * NDIRECT) / IDX_COUNT_PER_INDIRECT_BLCOK;
  struct buf* b = bread(in->sb->dev, iblock[i + NDIRECT]);
  u32 r = ((u32*)b->data)[off / BSIZE - NDIRECT - i * IDX_COUNT_PER_INDIRECT_BLCOK];
  brelse(b);
  return r;
}

u32
fread(struct file* f, void* buf, u32 bytes)
{
  u32 pcur = f->off;
  u32 pend_bytes = min(bytes, f->size - pcur), done_bytes = 0;

  struct buf* b;
  while (pend_bytes > 0) {
    u32 len = min(BSIZE - pcur % BSIZE, pend_bytes);
    b = bread(f->inode->sb->dev, blockno_of_data(f->inode, pcur));
    memcpy(buf + done_bytes, b->data + pcur % BSIZE, len);
    brelse(b);
    done_bytes += len;
    pend_bytes -= len;
    pcur += len;
  }

  f->off += done_bytes;
  return done_bytes;
}

u32
fwrite(struct file* f, const void* buf, u32 bytes)
{
  return 0; // ? todo
}