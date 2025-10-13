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
#include "cpu.h"
#include "config.h"
#include "plic.h"
#include "irqf.h"

// 不要改变struct pt_regs的字段顺序
// 异常上下文
struct pt_regs {
  u64 ra, sp, gp, tp; // tp其实可以不用保存
  u64 sepc, sstatus, stval, scause;
  u64 t0, t1, t2, t3, t4, t5, t6;
  u64 s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
  u64 a0, a1, a2, a3, a4, a5, a6, a7;
};

#define IS_INTR(scause)   ((scause & (1UL << 63)) != 0)
#define SCAUSE_EC(scause) (scause & ~(1UL << 63))

extern void ktrap_entry();
extern void utrap_entry();

extern void yield();
extern char trampoline[];

void
init_trap()
{
  w_stvec((u64)ktrap_entry);
  // ktrap_entry四字节对齐,地址低2位被解读为 Direct模式
}
static void
unknow_trap(struct pt_regs* pt)
{
  u64  ec      = SCAUSE_EC(pt->scause);
  bool intr    = IS_INTR(pt->scause);
  u64  stval   = pt->stval;
  u64  sepc    = pt->sepc;
  u64  sstatus = pt->sstatus;
  bool mode    = sstatus & SSTATUS_SPP;


  panic("CPU%d Occur an unknown %s trap \n  \
      scause %s %u\n  \
      stval %x\n  \
      sepc %x\n  \
      sstatus %x\n\n",
        cpuid(), mode == 0 ? "U" : "S", intr ? "interrupt" : "exception", ec, stval, sepc, sstatus);
}

// interrupt handler
static void asy_ipi(struct pt_regs*);    // 1
static void asy_timer(struct pt_regs*);  // 5
static void asy_extern(struct pt_regs*); // 9

// exception handler
static void syn_text_misaligned(struct pt_regs*);  // 0
static void syn_text_fault(struct pt_regs*);       // 1
static void syn_text_illegal(struct pt_regs*);     // 2
static void syn_breakpoint(struct pt_regs*);       // 3
static void syn_load_misaligned(struct pt_regs*);  // 4
static void syn_load_fault(struct pt_regs*);       // 5
static void syn_store_misaligned(struct pt_regs*); // 6
static void syn_store_fault(struct pt_regs*);      // 7
static void syn_syscall_u(struct pt_regs*);        // 8
static void syn_syscall_s(struct pt_regs*);        // 9
static void syn_text_page_fault(struct pt_regs*);  // 12
static void syn_load_page_fault(struct pt_regs*);  // 13
static void syn_store_page_fault(struct pt_regs*); // 15

typedef void (*trap_fn)(struct pt_regs* pt);
static trap_fn interrupt_funs[] = {
  unknow_trap, asy_ipi,     unknow_trap, unknow_trap, unknow_trap, asy_timer,
  unknow_trap, unknow_trap, unknow_trap, asy_extern,  unknow_trap,
};
static trap_fn exception_funs[] = {
  syn_text_misaligned,  syn_text_fault,       syn_text_illegal,    syn_breakpoint,      syn_load_misaligned,
  syn_load_fault,       syn_store_misaligned, syn_store_fault,     syn_syscall_u,       syn_syscall_s,
  unknow_trap,          unknow_trap,          syn_text_page_fault, syn_load_page_fault, unknow_trap,
  syn_store_page_fault, unknow_trap,
};

void
do_trap(struct pt_regs* pt)
{
  u64  scause    = pt->scause;
  u64  ec        = SCAUSE_EC(scause);
  bool from_user = ((pt->sstatus & SSTATUS_SPP) == 0);
  if (from_user) {
    print("cpu%d U-TRAP\n", cpuid());
    w_stvec((u64)ktrap_entry); // 进入do_trap时一定是关中断的
    sti();                     // 保存好trap上下文开启中断以允许嵌套(让S模式也能响应中断)
  } else
    print("cpu%d S-TRAP\n", cpuid()); // S模式不设计为嵌套中断

  if (IS_INTR(scause)) {
    ec = min(ec, sizeof(interrupt_funs) / sizeof(trap_fn) - 1);
    interrupt_funs[ec](pt);
  } else {
    ec = min(ec, sizeof(exception_funs) / sizeof(trap_fn) - 1);
    exception_funs[ec](pt);
  }

  if (from_user) {
    cli();
    w_stvec((u64)utrap_entry - (u64)trampoline + TRAMPOLINE);
  }

  // ! sstatus和spec可能会被嵌套trap所修改,sret会清SPP位
  w_sstatus(pt->sstatus);
  w_sepc(pt->sepc);
}

// interrupt handler
static void
asy_ipi(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  unknow_trap(pt);
}
static void
asy_timer(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  w_stimecmp(r_time() + TIME_CYCLE);
  if ((r_sstatus() & SSTATUS_SPP) == 0)
    yield();
}
static void
asy_extern(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  u32 irq = plic_claim();
  switch (irq) {
  case IRQ_UART0:
    do_uart_irq();
    break;
  case IRQ_DISK:
    do_disk_irq();
    break;
  default:
    unknow_trap(pt);
  }
}

// exception handler
static void
syn_text_misaligned(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  unknow_trap(pt);
}
static void
syn_text_fault(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  unknow_trap(pt);
}
static void
syn_text_illegal(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  unknow_trap(pt);
}
static void
syn_breakpoint(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  unknow_trap(pt);
}
static void
syn_load_misaligned(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  unknow_trap(pt);
}
static void
syn_load_fault(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  unknow_trap(pt);
}
static void
syn_store_misaligned(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  unknow_trap(pt);
}
static void
syn_store_fault(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  unknow_trap(pt);
}
static void
syn_syscall_u(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  pt->sepc += 4;
}
static void
syn_syscall_s(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  pt->sepc += 4;
}
static void
syn_text_page_fault(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  unknow_trap(pt);
}
static void
syn_load_page_fault(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  unknow_trap(pt);
}
static void
syn_store_page_fault(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  unknow_trap(pt);
}