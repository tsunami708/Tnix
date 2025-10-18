#include "fs.h"
#include "string.h"
#include "printf.h"


struct superblock root_sb;


void
fsinit()
{
  struct buf* sb_buf = bread(1);
  struct superblock* sb = (struct superblock*)sb_buf->data;
  memcpy(&root_sb, sb, sizeof(struct superblock));
  print("fsinit done , superblock info:\n");
  print("    fs_name:%s\n", root_sb.name);
  print("    imap:%u\n    inodes:%u\n    bmap:%u\n", root_sb.imap, root_sb.inodes, root_sb.bmap);
  print("    blocks:%u\n    max_i:%u\n    max_b:%u\n", root_sb.blocks, root_sb.max_inode, root_sb.max_nblock);
}