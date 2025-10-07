#pragma once
#include "type.h"

// M模式寄存器读写

static inline u64
r_mhartid()
{
  u64 x;
  asm volatile("csrr %0, mhartid" : "=r"(x));
  return x;
}

#define MSTATUS_MPP_MASK (3L << 11)
#define MSTATUS_MPP_S    (1L << 11)
#define MSTATUS_SIE      (1L << 1)
#define MSTATUS_MIE      (1L << 3)
#define MSTATUS_SPIE     (1L << 5)
#define MSTATUS_MPIE     (1L << 7)
static inline u64
r_mstatus()
{
  u64 x;
  asm volatile("csrr %0, mstatus" : "=r"(x));
  return x;
}

static inline u64
r_menvcfg()
{
  u64 x;
  asm volatile("csrr %0, 0x30a" : "=r"(x));
  return x;
}

static inline u64
r_mcounteren()
{
  u64 x;
  asm volatile("csrr %0, mcounteren" : "=r"(x));
  return x;
}

static inline void
w_mstatus(u64 x)
{
  asm volatile("csrw mstatus, %0" ::"r"(x));
}

static inline void
w_mepc(u64 x)
{
  asm volatile("csrw mepc, %0" ::"r"(x));
}

static inline void
w_medeleg(u64 x)
{
  asm volatile("csrw medeleg, %0" : : "r"(x));
}

static inline void
w_mideleg(u64 x)
{
  asm volatile("csrw mideleg, %0" : : "r"(x));
}

static inline void
w_pmpcfg0(u64 x)
{
  asm volatile("csrw pmpcfg0, %0" : : "r"(x));
}

static inline void
w_pmpaddr0(u64 x)
{
  asm volatile("csrw pmpaddr0, %0" : : "r"(x));
}

static inline void
w_menvcfg(u64 x)
{
  asm volatile("csrw 0x30a, %0" : : "r"(x));
}

static inline void
w_mcounteren(u64 x)
{
  asm volatile("csrw mcounteren, %0" : : "r"(x));
}

// S模式寄存器读写

static inline u64
r_time()
{
  u64 x;
  asm volatile("csrr %0, time" : "=r"(x));
  return x;
}

// PTE Sv39 --- 9-9-9-12
// 63-----37 | 36---28 | 27---19 | 18---10 | 9---8  | 7---0
//  reserved |  PPN[2] |  PPN[1] | PPN[0]  | RSW(OS)| attribute

// SATP
// 63---60 | 59---44 | 43---0
// MODE   |  ASID   | PPN
// MODE=8表示Sv39,ASID暂不使用,PPN为(根页表物理地址>>12)
#define PTE_V     (1 << 0)
#define PTE_R     (1 << 1)
#define PTE_W     (1 << 2)
#define PTE_X     (1 << 3)
#define PTE_U     (1 << 4)
#define PTE_G     (1 << 5)
#define PTE_A     (1 << 6)
#define PTE_D     (1 << 7)
#define SATP_MODE 0b1000UL << 60
static inline u64
r_satp()
{
  u64 x;
  asm volatile("csrr %0,satp" : "=r"(x));
  return x;
}
static inline void
w_satp(u64 x)
{
  asm volatile("csrw satp, %0" ::"r"(x));
}

#define SIE_SSIE (1L << 1) // 核间中断
#define SIE_SEIE (1L << 9) // 外设中断
#define SIE_STIE (1L << 5) // 时钟中断
static inline u64
r_sie()
{
  u64 x;
  asm volatile("csrr %0, sie" : "=r"(x));
  return x;
}
static inline void
w_sie(u64 x)
{
  asm volatile("csrw sie, %0" : : "r"(x));
}

static inline u64
r_sstatus()
{
  u64 x;
  asm volatile("csrr %0, sstatus" : "=r"(x));
  return x;
}

static inline void
w_sstatus(u64 x)
{
  asm volatile("csrw sstatus, %0" : : "r"(x));
}

static inline void
w_stimecmp(u64 x)
{
  asm volatile("csrw 0x14d, %0" : : "r"(x));
}

static inline void
w_stvec(u64 x)
{
  asm volatile("csrw stvec, %0" : : "r"(x));
}

// 通用寄存器读写

static inline u64
r_tp()
{
  u64 x;
  asm volatile("mv %0, tp" : "=r"(x));
  return x;
}
static inline void
w_tp(u64 x)
{
  asm volatile("mv tp, %0" ::"r"(x));
}