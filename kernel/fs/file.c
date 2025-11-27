#include "mem/vm.h"
#include "mem/slot.h"
#include "fs/file.h"
#include "fs/inode.h"
#include "fs/dir.h"
#include "fs/bio.h"
#include "fs/pipe.h"
#include "util/spinlock.h"
#include "util/printf.h"
#include "util/string.h"

struct file*
falloc(void)
{
  struct file* f = alloc_file_slot();
  f->off = 0;
  f->refc = 1;
  f->lock.lname = "file";
  return f;
}

void
fdup(struct file* f)
{
  spin_get(&f->lock);
  if (f->refc < 1)
    panic("fdup: ref<1");
  ++f->refc;
  spin_put(&f->lock);
}

void
fclose(struct file* f)
{
  spin_get(&f->lock);
  if (f->refc == 0)
    panic("fclose: ref=0");
  --f->refc;
  if (f->refc == 0) {
    if ((f->type == INODE || f->type == DEVICE) && f->inode)
      iput(f->inode);
    else if (f->type == PIPE && f->pipe)
      pipeclose(f->pipe);
    spin_put(&f->lock);
    free_file_slot(f);
    return;
  }
  spin_put(&f->lock);
}

// f->off不是线程安全的
u32
fseek(struct file* f, int off, int whence)
{
  u32 r = f->off;
  if (f->type == INODE) {
    if (whence == SEEK_CUR) {
      if (off > 0)
        f->off = min(f->off + off, f->inode->di.fsize);
      else
        f->off -= min(f->off, -off);
    } else if (whence == SEEK_SET)
      f->off = min(f->inode->di.fsize, max(0, off));
  }
  return r;
}

static u32
blockno_of_data(struct file* f, u32 off)
{
  u32* iblock = f->inode->di.iblock;
  if (off < BSIZE * NDIRECT)
    return iblock[off / BSIZE];
  u32 i = (off - BSIZE * NDIRECT) / IDX_CNT_PER_INDIRECT_BLCOK;
  struct buf* b = bread(f->inode->sb->dev, iblock[i + NDIRECT]);
  u32 r = ((u32*)b->data)[off / BSIZE - NDIRECT - i * IDX_CNT_PER_INDIRECT_BLCOK];
  brelse(b);
  return r;
}

u32
fread(struct file* f, void* buf, u32 bytes, bool kernel)
{
  u32 pcur = f->off;
  u32 pend_bytes = min(bytes, f->inode->di.fsize - pcur), done_bytes = 0;
  dev_t dev = f->inode->sb->dev;
  struct buf* b;

  while (pend_bytes) {
    u32 len = min(BSIZE - pcur % BSIZE, pend_bytes);
    b = bread(dev, blockno_of_data(f, pcur));
    if (kernel)
      memcpy(buf + done_bytes, b->data + pcur % BSIZE, len);
    else
      copy_to_user(buf + done_bytes, b->data + pcur % BSIZE, len);
    brelse(b);
    done_bytes += len;
    pend_bytes -= len;
    pcur += len;
  }

  f->off += done_bytes;
  return done_bytes;
}

u32
fwrite(struct file* f, const void* buf, u32 bytes, bool kernel)
{
  u32 pcur = f->off;
  u32 free = f->inode->di.fsize - pcur;
  dev_t dev = f->inode->sb->dev;
  u32 pend_bytes = bytes, done_bytes = 0;

  struct buf* b;
  while (free < bytes) { // 扩容(申请新的数据块)
    b = data_block_alloc(f->inode);
    brelse(b);
    free += BSIZE;
  }

  while (pend_bytes) {
    u32 len = min(BSIZE - pcur % BSIZE, pend_bytes);
    b = bread(dev, blockno_of_data(f, pcur));
    if (kernel)
      memcpy(b->data + pcur % BSIZE, buf + done_bytes, len);
    else
      copy_from_user(b->data + pcur % BSIZE, buf + done_bytes, len);
    bwrite(b);
    brelse(b);
    done_bytes += len;
    pend_bytes -= len;
    pcur += len;
  }

  f->off += done_bytes;
  f->inode->di.fsize += done_bytes;
  iupdate(f->inode);
  return done_bytes;
}