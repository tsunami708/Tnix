#include "config.h"
#include "fs/inode.h"
#include "fs/bio.h"
#include "util/spinlock.h"
#include "util/printf.h"
#include "util/string.h"


#define DINODE_CNT_PER_BLOCK (BSIZE / sizeof(struct dinode))


static struct {
  struct inode inodes[NINODE];
  struct spinlock lock;
} icache;


static inline __attribute__((always_inline)) u32
blockno_of_inode(struct inode* inode)
{
  return inode->sb->inodes + inode->inum / DINODE_CNT_PER_BLOCK;
}
static inline __attribute__((always_inline)) u32
offset_of_inode(struct inode* inode) // dinode在其磁盘块上的偏移
{
  return inode->inum % DINODE_CNT_PER_BLOCK;
}
static inline __attribute__((always_inline)) u32
imap_blockno_of_inode(struct inode* inode)
{
  return inode->sb->imap + inode->inum / BIT_CNT_PER_BLOCK;
}
static inline __attribute__((always_inline)) u32
imap_offset_of_inode(struct inode* inode) // dinode对于imap位在其磁盘块上的偏移
{
  return inode->inum % BIT_CNT_PER_BLOCK;
}

bool
iexist(struct superblock* sb, u32 inum)
{
  struct inode t = { .sb = sb, .inum = inum };
  struct buf* buf = bread(sb->dev, imap_blockno_of_inode(&t));
  u32 k = imap_offset_of_inode(&t);
  u8* section = (u8*)buf->data + k / 8;
  brelse(buf);
  return (*section) & (1UL << (k % 8));
}


static void
iread(struct inode* inode)
{
  sleep_get(&inode->slep);
  u32 blockno = blockno_of_inode(inode);
  struct buf* buf = bread(inode->sb->dev, blockno);
  struct dinode* di = (void*)buf->data;
  di += offset_of_inode(inode);
  memcpy(&inode->di, di, sizeof(struct dinode));
  brelse(buf);
  sleep_put(&inode->slep);
}


void
iupdate(struct inode* inode)
{
  sleep_get(&inode->slep);

  u32 blockno = blockno_of_inode(inode);
  struct buf* buf = bread(inode->sb->dev, blockno);
  struct dinode* di = (void*)buf->data;
  di += offset_of_inode(inode);
  memcpy(di, &inode->di, sizeof(struct dinode));
  bwrite(buf);
  brelse(buf);

  sleep_put(&inode->slep);
}


u32
ialloc(struct superblock* sb)
{
  struct buf* b;
  u64* section;
  int imap_blockcnt = sb->inodes - sb->imap;

  /*
    遍历imap块
    每一个imap块以64bit为单位(section)粗粒度查找
    若某个section不全为1,则说明存在空闲inode,进行1bit为单位细粒度查找
    找到即停止后续查找并刷盘返回
  */
  for (int i = 0; i < imap_blockcnt; ++i) {
    b = read_nth_imap(sb, i);
    section = (void*)b->data;

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
        return i * BIT_CNT_PER_BLOCK + j * sizeof(u64) + k; // inum
      }
    }
  }
  panic("ialloc: dinode exhausted");
}


void
itrunc(struct inode* in)
{
  in->di.fsize = 0;

  struct buf* b;
  struct superblock* sb = in->sb;

  for (int i = 0; i < NDIRECT + NINDIRECT; ++i) {
    if (in->di.iblock[i] == 0)
      break;

    b = bread(sb->dev, in->di.iblock[i]);
    if (i >= NDIRECT) {
      u32* idx = (void*)b->data;
      for (int j = 0; j < IDX_CNT_PER_INDIRECT_BLCOK; ++j) {
        if (*idx == 0)
          break;
        struct buf* bb = bread(sb->dev, *idx);
        bzero(bb);
        brelse(bb);
        bfree(sb, *idx);
        ++idx;
      }
    }
    bzero(b);
    brelse(b);
    bfree(sb, in->di.iblock[i]);
    in->di.iblock[i] = 0;
  }
}


void
ifree(struct inode* in)
{
  itrunc(in);
  in->di.type = UNUSE;
  iupdate(in);

  struct buf* b;
  struct superblock* sb = in->sb;

  u32 i = imap_blockno_of_inode(in);
  b = read_nth_imap(sb, i - sb->imap);
  u32 k = imap_offset_of_inode(in);
  u8* section = (void*)b->data;
  section += k / 8;
  *section &= ~(1UL << (k % 8));
  bwrite(b);
  brelse(b);
}


struct inode*
iget(struct superblock* sb, u32 inum)
{
  spin_get(&icache.lock);
  for (int i = 0; i < NINODE; ++i) {
    spin_get(&icache.inodes[i].spin);
    if (icache.inodes[i].sb == sb && icache.inodes[i].inum == inum) {
      ++icache.inodes[i].refc;
      spin_put(&icache.inodes[i].spin);
      spin_put(&icache.lock);
      return icache.inodes + i;
    }
    spin_put(&icache.inodes[i].spin);
  }

  /*
    涉及到阻塞的IO操作时必须要临时释放当前已经持有的所有自旋锁,
    否则在多核场景下当某个线程因为IO操作被切走而后被另一个核心调度,
    在释放自旋锁的时会出现核心号不匹配的问题
  */
  spin_put(&icache.lock);
  struct inode* in = NULL;
  if (! iexist(sb, inum))
    return in;
  spin_get(&icache.lock);

  for (int i = 0; i < NINODE; ++i) {
    spin_get(&icache.inodes[i].spin);
    if (icache.inodes[i].refc == 0) {
      icache.inodes[i].refc = 1;
      icache.inodes[i].sb = sb;
      icache.inodes[i].inum = inum;
      spin_put(&icache.inodes[i].spin);
      in = icache.inodes + i;
      break;
    }
    spin_put(&icache.inodes[i].spin);
  }
  spin_put(&icache.lock);

  if (in == NULL)
    panic("iget: icache exhausted");

  iread(in);
  return in;
}

void
iput(struct inode* inode)
{
  spin_get(&inode->spin);
  --inode->refc;
  spin_put(&inode->spin);
}


struct buf*
data_block_get(struct inode* in, u32 i)
{
  struct buf* r;
  if (i < NDIRECT)
    r = bread(in->sb->dev, in->di.iblock[i]);
  else {
    struct buf* b = bread(in->sb->dev, in->di.iblock[NDIRECT + (i - NDIRECT) / IDX_CNT_PER_INDIRECT_BLCOK]);
    u32 blockno = *(((u32*)b->data) + (i - NDIRECT) % IDX_CNT_PER_INDIRECT_BLCOK);
    brelse(b);
    r = bread(in->sb->dev, blockno);
  }
  return r;
}

#define FILE_MAX_DATA_BLOCKNUM (NDIRECT + NINDIRECT * sizeof(u32))
struct buf*
data_block_alloc(struct inode* in)
{ // in->di.fsize%BSIZE==0为真;如果为假则说明有数据块为满,内核不应该为其分配新的数据块
  u32 curn = iblock_cnt(in);
  if (curn == FILE_MAX_DATA_BLOCKNUM)
    panic("data_block_alloc: file too big");

  u32 blockno = balloc(in->sb);
  if (curn < NDIRECT)
    in->di.iblock[curn] = blockno;
  else {
    struct buf* b;
    curn -= NDIRECT;
    if (curn % IDX_CNT_PER_INDIRECT_BLCOK == 0) { // 需要分配新的间接索引块
      u32 iblockno = balloc(in->sb);
      in->di.iblock[NDIRECT + curn / IDX_CNT_PER_INDIRECT_BLCOK] = iblockno;
      b = bread(in->sb->dev, iblockno);
      *((u32*)(b->data)) = blockno;
    } else {
      b = bread(in->sb->dev, in->di.iblock[NDIRECT + curn / IDX_CNT_PER_INDIRECT_BLCOK]);
      *((u32*)(b->data) + curn % IDX_CNT_PER_INDIRECT_BLCOK) = blockno;
    }
    bwrite(b);
    brelse(b);
  }
  return bread(in->sb->dev, blockno);
}

/*
  删除一个数据块
  仅目录文件使用(普通文件不会出现删除中间某个数据块的情况)
*/
void
idx_remove(struct inode* in, u32 i)
{
  int blockcnt = iblock_cnt(in);
  struct buf* b;
  dev_t dev = in->sb->dev;

  if (blockcnt < NDIRECT) {
    b = bread(dev, in->di.iblock[i]);
    bzero(b);
    brelse(b);
    for (int j = i; j < blockcnt; ++j)
      in->di.iblock[j] = in->di.iblock[j + 1];
  } else {
    struct buf* nb;
    if (i < NDIRECT) {
      b = bread(dev, in->di.iblock[i]);
      bzero(b);
      brelse(b);
      for (int j = i; j < NDIRECT - 1; ++j)
        in->di.iblock[j] = in->di.iblock[j + 1];

      b = bread(dev, in->di.iblock[NDIRECT]);
      in->di.iblock[NDIRECT - 1] = *(u32*)b->data;
      brelse(b);

      for (int j = NDIRECT; j < NDIRECT + NINDIRECT && in->di.iblock[j]; ++j) {
        b = bread(dev, in->di.iblock[j]);
        u32* idx = (void*)b->data;
        for (int k = 0; *idx && k < IDX_CNT_PER_INDIRECT_BLCOK - 1; ++k) {
          *idx = *(idx + 1);
          ++idx;
        }
        if (*idx) {
          if (in->di.iblock[j + 1]) {
            nb = bread(dev, in->di.iblock[j + 1]);
            *idx = *(u32*)b->data;
            brelse(nb);
          } else
            *idx = 0;
        }
        bwrite(b);
        bzero(b);
        brelse(b);
      }

    } else {
      for (int j = NDIRECT + (i - NDIRECT) / IDX_CNT_PER_INDIRECT_BLCOK; j < NDIRECT + NINDIRECT && in->di.iblock[j];
           ++j) {
        b = bread(dev, in->di.iblock[j]);

        u32* idx = (void*)b->data;
        int k = 0;

        if (j == NDIRECT + (i - NDIRECT) / IDX_CNT_PER_INDIRECT_BLCOK) {
          k = (i - NDIRECT) % IDX_CNT_PER_INDIRECT_BLCOK;
          idx += k;
        }

        for (; *idx && k < IDX_CNT_PER_INDIRECT_BLCOK - 1; ++k) {
          *idx = *(idx + 1);
          ++idx;
        }
        if (*idx) {
          if (in->di.iblock[j + 1]) {
            nb = bread(dev, in->di.iblock[j + 1]);
            *idx = *(u32*)b->data;
            brelse(nb);
          } else
            *idx = 0;
        }
        bwrite(b);
        bzero(b);
        brelse(b);
      }
    }
  }
}

void
init_icache(void)
{
  icache.lock.lname = "icache";
  for (int i = 0; i < NINODE; ++i) {
    icache.inodes[i].slep.lname = "inode-sleep";
    icache.inodes[i].spin.lname = "inode-spin";
  }
}