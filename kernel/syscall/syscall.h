#pragma once

#define SYS_FORK 0
#define SYS_EXIT 1
#define SYS_EXEC 2

#ifndef AS
struct pt_regs;
typedef unsigned long u64;
typedef u64 (*syscall_t)(struct pt_regs*);

u64 sys_fork(struct pt_regs* pt);
u64 sys_exit(struct pt_regs* pt);
u64 sys_exec(struct pt_regs* pt);
#endif