#pragma once
#ifndef USER
#define SYS_FORK   0
#define SYS_EXIT   1
#define SYS_EXEC   2
#define SYS_WAIT   3
#define SYS_READ   4
#define SYS_WRITE  5
#define SYS_LSEEK  6
#define SYS_OPEN   7
#define SYS_DUP    8
#define SYS_CLOSE  9
#define SYS_LINK   10
#define SYS_UNLINK 11
#define SYS_MKDIR  12
#define SYS_RMDIR  13
#define SYS_MKNOD  14
#define SYS_CHDIR  15
#define SYS_ALLOC  16
#define SYS_FREE   17
#define SYS_PIPE   18
#define SYS_LS     19
#define SYS_THREAD 20
#define SYS_SLEEP  21

#ifndef AS
struct pt_regs;
typedef long (*syscall_t)(struct pt_regs*);
bool argstr(unsigned long uaddr, char* path);

long sys_fork(struct pt_regs* pt);
long sys_exit(struct pt_regs* pt);
long sys_exec(struct pt_regs* pt);
long sys_wait(struct pt_regs* pt);
long sys_read(struct pt_regs* pt);
long sys_write(struct pt_regs* pt);
long sys_lseek(struct pt_regs* pt);
long sys_open(struct pt_regs* pt);
long sys_dup(struct pt_regs* pt);
long sys_close(struct pt_regs* pt);
long sys_link(struct pt_regs* pt);
long sys_unlink(struct pt_regs* pt);
long sys_mkdir(struct pt_regs* pt);
long sys_rmdir(struct pt_regs* pt);
long sys_mknod(struct pt_regs* pt);
long sys_chdir(struct pt_regs* pt);
long sys_alloc(struct pt_regs* pt);
long sys_free(struct pt_regs* pt);
long sys_pipe(struct pt_regs* pt);
long sys_ls(struct pt_regs* pt);
long sys_thread(struct pt_regs* pt);
long sys_sleep(struct pt_regs* pt);

#endif
#endif