#include "inode.h"
#include "bio.h"
#include "spinlock.h"
#include "printf.h"
#include "string.h"

#define NINODE 50
static struct {
  struct inode inodes[NINODE];
  struct spinlock lock;
} inode_icache;


// static void
// read_dinode(struct inode* inode)
// {
//   if (!holding_sleep(&inode->lock))
//     panic("read_dinode");

//   u32 blockno = (inode->di.inum / BSIZE) + inode->sb->inodes;
//   struct iobuf* buf = read_iobuf(inode->sb->dev, blockno);
//   struct dinode* di = (void*)buf->data;
//   di += inode->di.inum - di->inum;
//   memcpy(&inode->di, di, sizeof(struct dinode));
//   release_iobuf(buf);
// }
// static void
// write_dinote(struct inode* inode)
// {
//   if (!holding_sleep(&inode->lock))
//     panic("write_dinode");

//   u32 blockno = (inode->di.inum / BSIZE) + inode->sb->inodes;
//   struct iobuf* buf = read_iobuf(inode->sb->dev, blockno);
//   struct dinode* di = (void*)buf->data;
//   di += inode->di.inum - di->inum;
//   memcpy(di, &inode->di, sizeof(struct dinode));
//   write_iobuf(buf);
//   release_iobuf(buf);
// }

u32
alloc_inode(struct superblock* sb)
{
  struct iobuf* buf;
  int total = sb->inodes - sb->imap;
  for (int i = 0; i < total; ++i) {
    buf = read_imap(sb, i);
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
    }

    for (int k = 0; k < 64; ++k) {
      if ((*section & (1UL << k)) == 0) {
        *section |= 1UL << k;
        write_iobuf(buf);
        release_iobuf(buf);
        return i * BSIZE + j * 8 + k; // inum
      }
    }
  }
  panic("dinode exhausted");
}
void
free_inode(struct inode* inode)
{
  struct superblock* sb = inode->sb;
  u32 inum = inode->di.inum;
  int i = inum / BSIZE;
  struct iobuf* buf = read_bmap(sb, i);
  int k = inum % BSIZE;
  u64* section = (void*)buf->data;
  section += k / 8;
  *section &= ~(1UL << (k % 8));
  write_iobuf(buf);
  release_iobuf(buf);
}

struct inode*
get_inode(struct superblock* sb, u32 inum)
{
  acquire_spin(&inode_icache.lock);

  // fast path
  for (int i = 0; i < NINODE; ++i) {
    if (inode_icache.inodes[i].sb == sb && inode_icache.inodes[i].di.inum == inum) {
      ++inode_icache.inodes[i].refc;
      release_spin(&inode_icache.lock); //* 不需要获取阻塞锁
      return inode_icache.inodes + i;
    }
  }

  // slow path
  for (int i = 0; i < NINODE; ++i) {
    if (inode_icache.inodes[i].refc == 0) {
      inode_icache.inodes[i].refc = 1;
      inode_icache.inodes[i].sb = sb;
      inode_icache.inodes[i].valid = false;
      release_spin(&inode_icache.lock);
      return inode_icache.inodes + i;
    }
  }

  panic("icache exhausted");
}
void
put_inode(struct inode* inode)
{
  acquire_spin(&inode_icache.lock);
  --inode->refc;
  if (inode->refc == 0)
    inode->valid = false;
  release_spin(&inode_icache.lock);
}



void
init_icache()
{
  inode_icache.lock.lname = "inode_icache";
  for (int i = 0; i < NINODE; ++i)
    inode_icache.inodes[i].lock.lname = "inode";
}