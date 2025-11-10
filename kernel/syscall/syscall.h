#pragma once

#define SYS_FORK   0
#define SYS_EXIT   1
#define SYS_EXEC   2
#define SYS_WAIT   3
#define SYS_READ   4
#define SYS_WRITE  5
#define SYS_OPEN   6
#define SYS_DUP    7
#define SYS_CLOSE  8
#define SYS_LINK   9
#define SYS_UNLINK 10
#define SYS_MKDIR  11
#define SYS_RMDIR  12
#define SYS_MKNOD  13
#define SYS_CHDIR  14
#define SYS_MMAP   15
#define SYS_MUNMAP 16

#ifndef AS
struct pt_regs;
typedef unsigned long u64;
typedef u64 (*syscall_t)(struct pt_regs*);
bool argstr(u64 uaddr, char* path);

u64 sys_fork(struct pt_regs* pt);
u64 sys_exit(struct pt_regs* pt);
u64 sys_exec(struct pt_regs* pt);
u64 sys_wait(struct pt_regs* pt);
u64 sys_read(struct pt_regs* pt);
u64 sys_write(struct pt_regs* pt);
u64 sys_open(struct pt_regs* pt);
u64 sys_dup(struct pt_regs* pt);
u64 sys_close(struct pt_regs* pt);
u64 sys_link(struct pt_regs* pt);
u64 sys_unlink(struct pt_regs* pt);
u64 sys_mkdir(struct pt_regs* pt);
u64 sys_rmdir(struct pt_regs* pt);
u64 sys_mknod(struct pt_regs* pt);
u64 sys_chdir(struct pt_regs* pt);
u64 sys_mmap(struct pt_regs* pt);
u64 sys_munmap(struct pt_regs* pt);

enum errno {
  EINVAL = 1,
  EEXIST,
  ENOENT,
  ENOMEM,
  ENOTDIR,
  EISDIR,
  EFBIG,
  ENEMPTY,
  ECHILD,
  EACCES,
};

#endif