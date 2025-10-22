#include "inode.h"
#include "bio.h"
#include "spinlock.h"
#include "printf.h"
#include "string.h"

#define NINODE 50
#define IPB    (BSIZE / sizeof(struct inode)) // 每块中的dinode个数
#define BPB    (BSIZE * 8)                    // 每块比特位数

static struct {
  struct inode inodes[NINODE];
  struct spinlock lock;
} inode_icache;

// desc: inode磁盘数据所在块号
static inline u32
blockno_of_inode(struct inode* inode)
{
  return inode->sb->inodes + inode->inum / IPB;
}
// desc: inode在所属磁盘块中的偏移量
static inline u32
offset_of_inode(struct inode* inode)
{
  return inode->inum % IPB;
}
// desc: inode对应的imap所在块号
static inline u32
imap_blockno_of_inode(struct inode* inode)
{
  return inode->sb->imap + inode->inum / BPB;
}
// desc: inode对于的imap位在其imap块中的偏移 [0,BSIZE*8-1]
static inline u32
imap_offset_of_inode(struct inode* inode)
{
  return inode->inum % BPB;
}

// desc: 检查inode是否存在于磁盘
bool
check_dinode(struct inode* inode)
{
  struct iobuf* buf = read_iobuf(inode->sb->dev, imap_blockno_of_inode(inode));
  u32 k = imap_offset_of_inode(inode);
  u64* section = (u64*)buf->data + k / 8;
  return (*section) & (1UL << (k % 8));
}

// note: 确保线程持有inode的阻塞锁和inode存在于磁盘
void
read_dinode(struct inode* inode)
{
  if (!holding_sleep(&inode->lock))
    panic("read dinode,but not hold lock");

  u32 blockno = blockno_of_inode(inode);

  struct iobuf* buf = read_iobuf(inode->sb->dev, blockno);
  struct dinode* di = (void*)buf->data;
  di += offset_of_inode(inode);
  memcpy(&inode->di, di, sizeof(struct dinode));
  release_iobuf(buf);
}

// note: 确保线程持有inode的阻塞锁
void
write_dinode(struct inode* inode)
{
  if (!holding_sleep(&inode->lock))
    panic("write dinode,but not hold lock");

  u32 blockno = blockno_of_inode(inode);
  struct iobuf* buf = read_iobuf(inode->sb->dev, blockno);
  struct dinode* di = (void*)buf->data;
  di += offset_of_inode(inode);
  memcpy(di, &inode->di, sizeof(struct dinode));
  write_iobuf(buf);
  release_iobuf(buf);
}

// desc: 申请一个inumber,获取imap首个0比特位;等价于创建一个文件
u32
alloc_inode(struct superblock* sb)
{
  struct iobuf* buf;
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
      release_iobuf(buf);
      continue;
    }

    for (int k = 0; k < 64; ++k) {
      if ((*section & (1UL << k)) == 0) {
        *section |= 1UL << k;
        write_iobuf(buf);
        release_iobuf(buf);
        return i * BPB + j * 8 + k; // inum
      }
    }
  }
  panic("dinode exhausted");
}

// desc: 用久性删除inode;等价于删除一个文件
void
free_inode(struct inode* inode)
{
  struct superblock* sb = inode->sb;
  u32 i = imap_blockno_of_inode(inode);
  struct iobuf* buf = read_imap(sb, i);
  u32 k = imap_offset_of_inode(inode);
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
    if (inode_icache.inodes[i].sb == sb && inode_icache.inodes[i].inum == inum) {
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