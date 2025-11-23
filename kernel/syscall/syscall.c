#include "syscall/syscall.h"
#include "trap/pt_reg.h"
#include "util/printf.h"
#include "util/string.h"


static syscall_t syscalls[] = {
  [SYS_FORK] sys_fork,   [SYS_EXIT] sys_exit,     [SYS_EXEC] sys_exec,   [SYS_WAIT] sys_wait,   [SYS_READ] sys_read,
  [SYS_WRITE] sys_write, [SYS_LSEEK] sys_lseek,   [SYS_OPEN] sys_open,   [SYS_DUP] sys_dup,     [SYS_CLOSE] sys_close,
  [SYS_LINK] sys_link,   [SYS_UNLINK] sys_unlink, [SYS_MKDIR] sys_mkdir, [SYS_RMDIR] sys_rmdir, [SYS_MKNOD] sys_mknod,
  [SYS_CHDIR] sys_chdir, [SYS_ALLOC] sys_alloc,   [SYS_FREE] sys_free,   [SYS_PIPE] sys_pipe,   [SYS_LS] sys_ls,
  [SYS_SLEEP] sys_sleep,
};


void
do_syscall(struct pt_regs* pt)
{
  int sysno = pt->a7;
  if (sysno < 0 || sysno > sizeof(syscalls) / sizeof(syscall_t) - 1)
    panic("do_syscall: illegal sysno");
  u64 ret = syscalls[sysno](pt);
  mypt()->a0 = ret; //* 对于fork的妥协,必须使用mypt()获取pt
}

bool
argstr(u64 uaddr, char* path)
{
  extern void kill(void);
  pte_t* pte;
  u64 paddr = va_to_pa(mytask()->pagetable, uaddr, &pte);
  if (paddr == 0 || ((*pte) & (PTE_U | PTE_R)) != (PTE_U | PTE_R))
    kill();
  int len = strlen((char*)paddr);
  if (len + 1 > MAX_PATH_LENGTH)
    return false;
  memcpy(path, (void*)paddr, len);
  return true;
}