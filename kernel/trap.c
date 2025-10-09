/*
RISC-V trap硬件处理流程:(trap处理已委派给S模式)
  1.保存PC值到sepc,(如果trap类型为exception,stval为trap发生的虚拟地址,spec为trap发送的指令地址)
  2.更新trap类型到scause
  3.保存trap前的interrupt状态(sie字段赋值给sstatus的spie字段)
  4.保存trap前的处理器模式至sstatus的spp字段
  5.关闭本地中断,sstatus的sie置零
  6.S模式运行并跳转到stvec指向的trap处理逻辑

RISC-V trap软件处理流程:
  1.保存必要的上下文
  2.查询scause确定trap处理程序
  3.恢复上下文并指向sret返回现场(对于系统调用ecall,手动将sepc+4)

  sret指令:
    1. sstatus: sie<--spie , CPU-mode<--spp
    2. pc<--sepc

(pt_regs)trap context: 通过内核栈保存与恢复
  1. x1~x31通用寄存器
  2. sepc,sstatus,scause,stval
*/
#include "type.h"
#include "riscv.h"
#include "printf.h"
#include "config.h"

// 不要改变struct pt_regs的字段顺序
struct pt_regs {
  u64 ra, sp, gp, tp;
  u64 sepc, sstatus, stval, scause;
  u64 t0, t1, t2, t3, t4, t5, t6;
  u64 s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
  u64 a0, a1, a2, a3, a4, a5, a6, a7;
};


extern void ktrap_entry();
extern void yield();

void
init_trap()
{
  w_stvec((u64)ktrap_entry);
  // ktrap_entry四字节对齐,地址低2位被解读为Direct模式
}

static int
unknow_trap(struct pt_regs* pt)
{
  panic("%s %s %u", __func__, pt->scause & (1UL << 63) ? "int" : "ex", pt->scause);
  return 0;
}

// interrupt handler
static int asy_ipi(struct pt_regs*);
static int asy_timer(struct pt_regs*);
static int asy_extern(struct pt_regs*);

// exception handler
static int syn_text_misaligned(struct pt_regs*);
static int syn_text_fault(struct pt_regs*);
static int syn_text_illegal(struct pt_regs*);
static int syn_breakpoint(struct pt_regs*);
static int syn_load_fault(struct pt_regs*);
static int syn_store_misaligned(struct pt_regs*);
static int syn_store_fault(struct pt_regs*);
static int syn_syscall(struct pt_regs*);
static int syn_text_page_fault(struct pt_regs*);
static int syn_load_page_fault(struct pt_regs*);
static int syn_store_page_fault(struct pt_regs*);

typedef int (*trap_fn)(struct pt_regs* pt);
static trap_fn interrupt_funs[] = {
  unknow_trap, asy_ipi,     unknow_trap, unknow_trap, unknow_trap, asy_timer,
  unknow_trap, unknow_trap, unknow_trap, asy_extern,  unknow_trap,
};
static trap_fn exception_funs[] = {
  syn_text_misaligned,  syn_text_fault,      syn_text_illegal, syn_breakpoint,       unknow_trap, syn_load_fault,
  syn_store_misaligned, syn_store_fault,     syn_syscall,      syn_syscall,          unknow_trap, unknow_trap,
  syn_text_page_fault,  syn_load_page_fault, unknow_trap,      syn_store_page_fault, unknow_trap,
};

int
do_trap(struct pt_regs* pt, u64 scause)
{
  u64 ec = scause & ~(1UL << 63);
  if (scause & (1UL << 63)) {
    ec = min(ec, sizeof(interrupt_funs) / sizeof(trap_fn) - 1);
    return interrupt_funs[ec](pt);
  }
  ec = min(ec, sizeof(exception_funs) / sizeof(trap_fn) - 1);
  return exception_funs[ec](pt);
}

// interrupt handler
static int
asy_ipi(struct pt_regs* pt)
{
  unknow_trap(pt);
  return 0;
}
static int
asy_timer(struct pt_regs* pt)
{
  print("%s\n", __func__);
  w_stimecmp(r_time() + TIME_CYCLE);
  if ((r_sstatus() & SSTATUS_SPP) == 0) {
    print("timer interrupt from U\n");
    yield();
  } else {
    print("timer interrupt from S\n");
  }
  return 0;
}
static int
asy_extern(struct pt_regs* pt)
{
  unknow_trap(pt);
  return 0;
}

// exception handler
static int
syn_text_misaligned(struct pt_regs* pt)
{
  unknow_trap(pt);
  return 0;
}
static int
syn_text_fault(struct pt_regs* pt)
{
  unknow_trap(pt);
  return 0;
}
static int
syn_text_illegal(struct pt_regs* pt)
{
  unknow_trap(pt);
  return 0;
}
static int
syn_breakpoint(struct pt_regs* pt)
{
  unknow_trap(pt);
  return 0;
}
static int
syn_load_fault(struct pt_regs* pt)
{
  unknow_trap(pt);
  return 0;
}
static int
syn_store_misaligned(struct pt_regs* pt)
{
  unknow_trap(pt);
  return 0;
}
static int
syn_store_fault(struct pt_regs* pt)
{
  unknow_trap(pt);
  return 0;
}
static int
syn_syscall(struct pt_regs* pt)
{
  print("%s\n", __func__);
  pt->sepc += 4;
  return 0;
}
static int
syn_text_page_fault(struct pt_regs* pt)
{
  unknow_trap(pt);
  return 0;
}
static int
syn_load_page_fault(struct pt_regs* pt)
{
  unknow_trap(pt);
  return 0;
}
static int
syn_store_page_fault(struct pt_regs* pt)
{
  unknow_trap(pt);
  return 0;
}