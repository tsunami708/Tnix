#include "config.h"
#include "fs/inode.h"
#include "fs/bio.h"
#include "util/spinlock.h"
#include "util/printf.h"
#include "util/string.h"


#define IPB (BSIZE / sizeof(struct dinode)) // 每块中的dinode个数
#define BPB (BSIZE * 8)                     // 每块比特位数

static struct {
  struct inode inodes[NINODE];
  struct spinlock lock;
} icache;


static inline u32
blockno_of_inode(struct inode* inode)
{
  return inode->sb->inodes + inode->inum / IPB;
}
static inline u32
offset_of_inode(struct inode* inode) // dinode在其磁盘块上的偏移
{
  return inode->inum % IPB;
}
static inline u32
imap_blockno_of_inode(struct inode* inode)
{
  return inode->sb->imap + inode->inum / BPB;
}
static inline u32
imap_offset_of_inode(struct inode* inode) // dinode对于imap位在其磁盘块上的偏移
{
  return inode->inum % BPB;
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

// * 从磁盘读取文件的属性信息
// ! 请确保线程持有inode的阻塞锁和inode存在于磁盘
void
iread(struct inode* inode)
{
  if (!holding_sleep(&inode->slep))
    panic("read dinode,but not hold lock");

  u32 blockno = blockno_of_inode(inode);

  struct buf* buf = bread(inode->sb->dev, blockno);
  struct dinode* di = (void*)buf->data;
  di += offset_of_inode(inode);
  memcpy(&inode->di, di, sizeof(struct dinode));
  brelse(buf);
}

// * 更新文件属性到磁盘
// ! 请确保线程持有inode的阻塞锁和inode存在于磁盘
void
iupdate(struct inode* inode)
{
  if (!holding_sleep(&inode->slep))
    panic("update dinode,but not hold lock");

  u32 blockno = blockno_of_inode(inode);
  struct buf* buf = bread(inode->sb->dev, blockno);
  struct dinode* di = (void*)buf->data;
  di += offset_of_inode(inode);
  memcpy(di, &inode->di, sizeof(struct dinode));
  bwrite(buf);
  brelse(buf);
}

//* 创建文件
u32
ialloc(struct superblock* sb)
{
  struct buf* buf;
  int total = sb->inodes - sb->imap; // 文件系统imap总块数

  /*
    遍历imap块
    每一个imap块以64bit为单位(section)粗粒度查找
    若某个section不全为1,则说明存在空闲inode,进行1bit为单位细粒度查找
    找到即停止后续查找并刷盘返回
  */
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
      brelse(buf);
      continue;
    }

    for (int k = 0; k < 64; ++k) {
      if ((*section & (1UL << k)) == 0) {
        *section |= 1UL << k;
        bwrite(buf);
        brelse(buf);
        return i * BPB + j * 8 + k; // inum
      }
    }
  }
  panic("dinode exhausted");
}

//* 删除文件
void
ifree(struct inode* inode)
{
  struct superblock* sb = inode->sb;
  u32 i = imap_blockno_of_inode(inode);
  struct buf* buf = read_imap(sb, i);
  u32 k = imap_offset_of_inode(inode);
  u8* section = (void*)buf->data;
  section += k / 8;
  *section &= ~(1UL << (k % 8));
  bwrite(buf);
  brelse(buf);
}


struct inode*
iget(struct superblock* sb, u32 inum)
{
  acquire_spin(&icache.lock);
  for (int i = 0; i < NINODE; ++i) {
    acquire_spin(&icache.inodes[i].spin);
    if (icache.inodes[i].sb == sb && icache.inodes[i].inum == inum) {
      ++icache.inodes[i].refc;
      release_spin(&icache.inodes[i].spin);
      release_spin(&icache.lock);
      return icache.inodes + i;
    }
    release_spin(&icache.inodes[i].spin);
  }

  /*
    !涉及到阻塞的IO操作时必须要临时释放当前已经持有的所有自旋锁,
    否则在多核场景下当某个线程因为IO操作被切走而后被另一个核心调度,
    在释放自旋锁的时会出现核心号不匹配的问题
  */
  release_spin(&icache.lock);
  struct inode* in = NULL;
  if (!iexist(sb, inum))
    return in;
  acquire_spin(&icache.lock);

  for (int i = 0; i < NINODE; ++i) {
    acquire_spin(&icache.inodes[i].spin);
    if (icache.inodes[i].refc == 0) {
      icache.inodes[i].refc = 1;
      icache.inodes[i].sb = sb;
      icache.inodes[i].inum = inum;
      release_spin(&icache.inodes[i].spin);
      in = icache.inodes + i;
      break;
    }
    release_spin(&icache.inodes[i].spin);
  }
  release_spin(&icache.lock);

  if (in == NULL)
    panic("icache exhausted");

  acquire_sleep(&in->slep);
  iread(in);
  release_sleep(&in->slep);
  return in;
}

void
iput(struct inode* inode)
{
  acquire_spin(&inode->spin);
  --inode->refc;
  release_spin(&inode->spin);
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