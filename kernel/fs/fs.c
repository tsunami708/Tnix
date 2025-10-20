#include "fs.h"
#include "bio.h"
#include "string.h"
#include "printf.h"

#define ROOTDEV 1
struct superblock root_sb;

void
fsinit()
{
  struct iobuf* sb_buf = read_iobuf(ROOTDEV, 1);
  struct superblock* sb = (void*)sb_buf->data;
  memcpy(&root_sb, sb, sizeof(struct superblock));
  release_iobuf(sb_buf);
  print("fsinit done , superblock info:\n");
  print("    fs_name:%s\n", root_sb.name);
  print("    imap:%u\n    inodes:%u\n    bmap:%u\n", root_sb.imap, root_sb.inodes, root_sb.bmap);
  print("    blocks:%u\n    max_i:%u\n    max_b:%u\n", root_sb.blocks, root_sb.max_inode, root_sb.max_nblock);
}