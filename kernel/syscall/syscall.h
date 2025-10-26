#pragma once

#define SYS_FORK 0
#define SYS_EXIT 1
#define SYS_EXEC 2

#ifndef AS
typedef unsigned long u64;
typedef u64 (*syscall_t)(void);

u64 sys_fork(void);
u64 sys_exit(void);
u64 sys_exec(void);
#endif