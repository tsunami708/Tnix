#include "fs/elf.h"
#include "fs/bio.h"
#include "fs/inode.h"
#include "mem/alloc.h"
#include "mem/vm.h"
#include "task/task.h"
#include "util/string.h"
#include "util/printf.h"

// 读取ELF文件头,并返回是否可执行
bool
read_elfhdr(struct inode* f, struct elfhdr* eh)
{
  // struct buf* b = bread(f->sb->dev, f->di.iblock[0]);
  // memcpy(eh, b->data, sizeof(struct elfhdr));
  // brelse(b);

  // 检查是否为ELF文件
  if (*(int*)eh->ident != ELF_MAGIC)
    goto not_exec;

  // 检查是否为可执行文件
  if (eh->entry == 0)
    goto not_exec;

  // 检查是否为RISC-V架构
  if (eh->machine != ELF_RISCV)
    goto not_exec;

  return true;
not_exec:
  return false;
}

// 解析ELF程序段表,加载代码段和数据段,并进行页表映射
void
load_segment(struct inode* f, struct elfhdr* eh, struct task* t)
{
}