#include "fs/fs.h"
#include "fs/bio.h"
#include "fs/inode.h"
#include "task/cpu.h"
#include "task/task.h"
#include "util/string.h"
#include "util/printf.h"


// root filesystem
struct superblock rfs;

void
read_superblock(dev_t dev, struct superblock* sb)
{
  struct buf* buf = bread(dev, 1);
  memcpy(sb, buf->data, sizeof(struct superblock));
  brelse(buf);
}

struct buf*
read_imap(struct superblock* sb, u32 i)
{
  if (sb->inodes - sb->imap <= i)
    panic("read_imap out of range");
  return bread(sb->dev, i + sb->imap);
}
struct buf*
read_bmap(struct superblock* sb, u32 i)
{
  if (sb->blocks - sb->bmap <= i)
    panic("read_bmap out of range");
  return bread(sb->dev, i + sb->bmap);
}

u32
alloc_block(struct superblock* sb)
{
  struct buf* buf;
  int total = sb->blocks - sb->bmap;
  for (int i = 0; i < total; ++i) {
    buf = read_bmap(sb, i);
    u64* section = (void*)buf->data;

    int j;
    for (j = 0; j < BSIZE / 8; ++j) {
      if ((*(section + j) & 0xFFFFFFFFFFFFFFFF) != 0xFFFFFFFFFFFFFFFF) {
        section += j;
        break;
      }
    }
    if (j == BSIZE / 8) {
      brelse(buf);
      continue;
    }

    for (int k = 0; k < 64; ++k) {
      if ((*section & (1UL << k)) == 0) {
        *section |= 1UL << k;
        bwrite(buf);
        brelse(buf);
        return i * BSIZE + j * 8 + k + sb->blocks;
      }
    }
  }
  panic("block exhausted");
}
void
free_block(struct superblock* sb, u32 blockno)
{
  int i = (blockno - sb->blocks) / BSIZE;
  struct buf* buf = read_bmap(sb, i);
  int k = (blockno - sb->blocks) % BSIZE;
  u64* section = (void*)buf->data;
  section += k / 8;
  *section &= ~(1UL << (k % 8));
  bwrite(buf);
  brelse(buf);
}

extern struct inode* dlookup(const char* path);
void
fsinit(void)
{
  read_superblock(ROOTDEV, &rfs);
  mytask()->cwd = iget(&rfs, ROOTINUM);
  struct inode* i = dlookup("/bin/init");
  (void)i;
}