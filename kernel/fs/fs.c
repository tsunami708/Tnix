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
  struct buf* b = bread(dev, 1);
  memcpy(sb, b->data, sizeof(struct superblock));
  brelse(b);
}
struct buf*
read_nth_imap(struct superblock* sb, u32 n)
{
  if (sb->inodes - sb->imap <= n)
    panic("read_nth_imap: out of range");
  return bread(sb->dev, n + sb->imap);
}
struct buf*
read_nth_bmap(struct superblock* sb, u32 n)
{
  if (sb->blocks - sb->bmap <= n)
    panic("read_nth_bmap: out of range");
  return bread(sb->dev, n + sb->bmap);
}

/*从文件系统sb中申请一个数据块并持久化将bmap对应比特位设置为1*/
u32
balloc(struct superblock* sb)
{
  struct buf* b;
  int bmap_blockcnt = sb->blocks - sb->bmap;
  for (int i = 0; i < bmap_blockcnt; ++i) {
    b = read_nth_bmap(sb, i);
    u64* section = (void*)b->data;

    int j = 0;
    for (; j < BSIZE / 8; ++j) {
      if ((*(section) & 0xFFFFFFFFFFFFFFFFUL) != 0xFFFFFFFFFFFFFFFFUL)
        break;
      ++section;
    }
    if (j == BSIZE / 8) {
      brelse(b);
      continue;
    }

    for (int k = 0; k < 64; ++k) {
      if ((*section & (1UL << k)) == 0) {
        *section |= 1UL << k;
        bwrite(b);
        brelse(b);
        return i * BIT_CNT_PER_BLOCK + j * 64 + k + sb->blocks;
      }
    }
  }
  panic("balloc: space exhausted");
}
/*从文件系统中释放块号为blockno的数据块并持久化将bmap对应比特位设置为0*/
void
bfree(struct superblock* sb, u32 blockno)
{
  int i = (blockno - sb->blocks) / BIT_CNT_PER_BLOCK;
  int k = (blockno - sb->blocks) % BIT_CNT_PER_BLOCK;

  struct buf* b = read_nth_bmap(sb, i);
  u64* section = (void*)b->data;
  section += k / 64;
  if (((*section) & (1UL << (k % 64))) == 0)
    panic("bfree: double free");
  *section &= ~(1UL << (k % 64));
  bwrite(b);
  brelse(b);
}

void
fsinit(void)
{
  read_superblock(ROOTDEV, &rfs);
  mytask()->cwd = iget(&rfs, ROOTINUM);
}