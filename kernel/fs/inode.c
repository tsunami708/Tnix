#include "config.h"
#include "fs/inode.h"
#include "fs/bio.h"
#include "util/spinlock.h"
#include "util/printf.h"
#include "util/string.h"


#define IPB (BSIZE / sizeof(struct inode)) // 每块中的dinode个数
#define BPB (BSIZE * 8)                    // 每块比特位数

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

// desc: 检查inode是否存在于磁盘(文件是否存在)
bool
check_dinode(struct inode* inode)
{
  struct iobuf* buf = read_iobuf(inode->sb->dev, imap_blockno_of_inode(inode));
  u32 k = imap_offset_of_inode(inode);
  u64* section = (u64*)buf->data + k / 8;
  release_iobuf(buf);
  return (*section) & (1UL << (k % 8));
}

// desc: 从磁盘读取文件的属性信息
// note: 请确保线程持有inode的阻塞锁和inode存在于磁盘
void
read_dinode(struct inode* inode)
{
  if (!holding_sleep(&inode->lock))
    panic("read dinode,but not hold lock");

  u32 blockno = blockno_of_inode(inode);

  struct iobuf* buf = read_iobuf(inode->sb->dev, blockno);
  struct dinode* di = (void*)buf->data;
  di += offset_of_inode(inode);
  memcpy1(&inode->di, di, sizeof(struct dinode));
  release_iobuf(buf);
}

// desc: 更新文件属性到磁盘
//  note: 请确保线程持有inode的阻塞锁和inode存在于磁盘
void
write_dinode(struct inode* inode)
{
  if (!holding_sleep(&inode->lock))
    panic("write dinode,but not hold lock");

  u32 blockno = blockno_of_inode(inode);
  struct iobuf* buf = read_iobuf(inode->sb->dev, blockno);
  struct dinode* di = (void*)buf->data;
  di += offset_of_inode(inode);
  memcpy1(di, &inode->di, sizeof(struct dinode));
  write_iobuf(buf);
  release_iobuf(buf);
}

// desc: 申请一个inumber,获取imap首个0比特位(创建一个文件)
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

// desc: 永久性删除inode(删除一个文件)
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

/*
  desc: 从inode_icache数组中申请一个空闲的inode结构,
    * 如果返回inode的valid字段为真,则表示其他线程已引用此文件或之前的数据未被覆盖,属性信息已存在于内存
    * 否则表示当前线程是引用此文件的第一个线程,需要自行决定是否读取属性信息
*/
struct inode*
get_inode(struct superblock* sb, u32 inum)
{
  acquire_spin(&inode_icache.lock);

  // fast path
  for (int i = 0; i < NINODE; ++i) {
    acquire_spin(&inode_icache.inodes[i].spin);
    if (inode_icache.inodes[i].sb == sb && inode_icache.inodes[i].inum == inum) {
      ++inode_icache.inodes[i].refc;
      release_spin(&inode_icache.inodes[i].spin);
      release_spin(&inode_icache.lock);
      return inode_icache.inodes + i;
    }
    release_spin(&inode_icache.inodes[i].spin);
  }

  // slow path
  for (int i = 0; i < NINODE; ++i) {
    acquire_spin(&inode_icache.inodes[i].spin);
    if (inode_icache.inodes[i].refc == 0) {
      inode_icache.inodes[i].refc = 1;
      inode_icache.inodes[i].sb = sb;
      inode_icache.inodes[i].inum = inum;
      inode_icache.inodes[i].valid = false;
      release_spin(&inode_icache.inodes[i].spin);
      release_spin(&inode_icache.lock);
      return inode_icache.inodes + i;
    }
    release_spin(&inode_icache.inodes[i].spin);
  }

  panic("icache exhausted");
}

// desc: 线程不再引用某个文件时,执行此方法减少引用计数
void
put_inode(struct inode* inode)
{
  acquire_spin(&inode->spin);
  --inode->refc;
  release_spin(&inode->spin);
}

// desc: 如果get_inode返回的是无磁盘数据的inode,则立刻读盘
struct inode*
do_get_inode(struct superblock* sb, u32 inum)
{
  struct inode* inode = get_inode(sb, inum);
  acquire_spin(&inode->spin);
  if (!inode->valid) {
    release_spin(&inode->spin);
    if (check_dinode(inode)) {
      acquire_sleep(&inode->lock);
      inode->valid = true;
      read_dinode(inode);
      release_sleep(&inode->lock);
      return inode;
    } else
      return NULL;
  }
  release_spin(&inode->spin);
  return inode;
}


void
init_icache(void)
{
  inode_icache.lock.lname = "inode_icache";
  for (int i = 0; i < NINODE; ++i) {
    inode_icache.inodes[i].lock.lname = "inode-sleep";
    inode_icache.inodes[i].spin.lname = "inode-spin";
  }
}