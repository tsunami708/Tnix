#include "fs/elf.h"
#include "fs/dir.h"
#include "fs/file.h"
#include "mem/vm.h"
#include "mem/alloc.h"
#include "task/task.h"

struct file*
read_elfhdr(const char* path, struct elfhdr* eh)
{
  struct file* f = falloc();
  f->inode = dentry_find(path);
  if (f->inode == NULL)
    goto not_exec;
  f->off = 0;
  f->type = INODE;

  fread(f, eh, sizeof(struct elfhdr), true);

  // 检查是否为ELF文件
  if (*(int*)eh->ident != ELF_MAGIC)
    goto not_exec;

  // 检查是否为可执行文件
  if (eh->entry == 0)
    goto not_exec;

  // 检查是否为RISC-V架构
  if (eh->machine != ELF_RISCV)
    goto not_exec;

  return f;

not_exec:
  fclose(f);
  return NULL;
}

// 解析ELF程序段表,加载代码段和数据段,并进行页表映射
void
load_segment(struct task* t, struct file* f, struct elfhdr* eh)
{
  int seg_cnt = eh->phnum;

  struct proghdr pg;
  fseek(f, eh->phoff, SEEK_SET);
  while (seg_cnt--) {
    fread(f, &pg, sizeof(pg), true);
    if (pg.type == ELF_PROG_LOAD && (pg.filesz > 0 || pg.memsz > 0)) {
      struct page* p = alloc_page();
      list_pushback(&t->pages, &p->page_node);
      int roff = fseek(f, pg.off, SEEK_SET);
      fread(f, (void*)p->paddr, pg.filesz, true);
      fseek(f, roff, SEEK_SET);

      u16 attr = PTE_U;
      if (pg.flags & ELF_PROG_FLAG_READ)
        attr |= PTE_R;
      if (pg.flags & ELF_PROG_FLAG_WRITE)
        attr |= PTE_W;
      if (pg.flags & ELF_PROG_FLAG_EXEC)
        attr |= PTE_X;
      task_vmmap(t, pg.vaddr, p->paddr, PGSIZE, attr, (attr & PTE_X) ? TEXT : DATA);
    }
  }
}