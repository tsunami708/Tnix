#include "fs.h"
#include "bio.h"
#include "cpu.h"
#include "task.h"
#include "inode.h"
#include "string.h"
#include "printf.h"


// root filesystem
struct superblock rfs;

void
read_superblock(dev_t dev, struct superblock* sb)
{
  struct iobuf* buf = read_iobuf(dev, 1);
  memcpy(sb, buf->data, sizeof(struct superblock));
  release_iobuf(buf);
}

struct iobuf*
read_imap(struct superblock* sb, u32 i)
{
  if (sb->inodes - sb->imap <= i)
    panic("read_imap out of range");
  return read_iobuf(sb->dev, i + sb->imap);
}
struct iobuf*
read_bmap(struct superblock* sb, u32 i)
{
  if (sb->blocks - sb->bmap <= i)
    panic("read_bmap out of range");
  return read_iobuf(sb->dev, i + sb->bmap);
}

u32
alloc_block(struct superblock* sb)
{
  struct iobuf* buf;
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
      release_iobuf(buf);
      continue;
      ;
    }

    for (int k = 0; k < 64; ++k) {
      if ((*section & (1UL << k)) == 0) {
        *section |= 1UL << k;
        write_iobuf(buf);
        release_iobuf(buf);
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
  struct iobuf* buf = read_bmap(sb, i);
  int k = (blockno - sb->blocks) % BSIZE;
  u64* section = (void*)buf->data;
  section += k / 8;
  *section &= ~(1UL << (k % 8));
  write_iobuf(buf);
  release_iobuf(buf);
}

void
fsinit()
{
  read_superblock(ROOTDEV, &rfs);
  print("fsinit done , superblock info:\n");
  print("    fs_name:%s\n", rfs.name);
  print("    imap:%u\n    inodes:%u\n    bmap:%u\n", rfs.imap, rfs.inodes, rfs.bmap);
  print("    blocks:%u\n    max_i:%u\n    max_b:%u\n", rfs.blocks, rfs.max_inode, rfs.max_nblock);
  mytask()->cwd = do_get_inode(&rfs, ROOTINUM);
}