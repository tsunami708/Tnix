#pragma once
#define NPROC 32 // 最大进程数

// 页大小
#define PGSIZE  4096UL
#define MPGSIZE (PGSIZE << 9)  // 2MB
#define GPGSIZE (PGSIZE << 18) // 1GB

// 最大虚拟地址
/*
Sv39的合法地址空间为0~0x3FFFFFFFFF 和 0xFFFFFFC000000000~0xFFFFFFFFFFFFFFFF
各256GB,为了简单,只使用低地址空间,也已足够
*/
#define MAXVA      0x3FFFFFFFFFUL        // 最大合法虚拟地址
#define VA_TOP     0x4000000000UL        // MAXVA+1
#define TRAMPOLINE (VA_TOP - PGSIZE)     // 用于模式切换的跳板页高虚拟地址起始处
#define TRAPFRAME  (TRAMPOLINE - PGSIZE) // 用于模式切换的TRAP页高虚拟地址起始处
#define USTACK     (TRAPFRAME - PGSIZE)  // 用户栈的起始地址

// 硬件属性
#define NCPU       8
#define TIME_CYCLE 10000000UL // 时间片

#define PHY_MEMORY 0x80000000UL
#define PHY_SIZE   0x20000000UL // 512MB
#define PHY_TOP    (PHY_MEMORY + PHY_SIZE)
#define NPAGE      (PHY_SIZE / PGSIZE)
#define KBASE      PHY_MEMORY // 内核代码起始处

#define CLINT      0x2000000UL
#define CLINT_SIZE 0x10000UL

#define PLIC      0xC000000UL
#define PLIC_SIZE 0x600000UL

#define UART0      0x10000000UL
#define UART0_SIZE PGSIZE

#define VIRIO      0x10001000UL
#define VIRIO_SIZE PGSIZE