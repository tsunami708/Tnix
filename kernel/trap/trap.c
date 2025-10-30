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
#include "config.h"
#include "util/riscv.h"
#include "util/printf.h"
#include "task/cpu.h"
#include "dev/irqf.h"
#include "trap/pt_reg.h"
#include "trap/plic.h"


#define IS_INTR(scause)   ((scause & (1UL << 63)) != 0)
#define SCAUSE_EC(scause) (scause & ~(1UL << 63))

extern void ktrap_entry(void);
extern void utrap_entry(void);

extern void yield(void);
extern char trampoline[];

void
init_trap(void)
{
  w_stvec((u64)ktrap_entry);
  // ktrap_entry四字节对齐,地址低2位被解读为 Direct模式
}
static void
unknow_trap(struct pt_regs* pt)
{
  bool intr = IS_INTR(pt->scause);
  u64 ec = SCAUSE_EC(pt->scause);
  panic("%s-%u-sepc:%x-stval:%x", intr ? "int" : "exp", ec, pt->sepc, pt->stval);
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
  u64 sstatus = pt->sstatus;
  u64 sepc = pt->sepc;
  u64 scause = pt->scause;
  u64 ec = SCAUSE_EC(scause);
  bool from_user = ((sstatus & SSTATUS_SPP) == 0);

  if (from_user)
    w_stvec((u64)ktrap_entry); // 进入do_trap时一定是关中断的

  if (IS_INTR(scause)) {
    ec = min(ec, sizeof(interrupt_funs) / sizeof(trap_fn) - 1);
    interrupt_funs[ec](pt);
  } else {
    ec = min(ec, sizeof(exception_funs) / sizeof(trap_fn) - 1);
    exception_funs[ec](pt);
    if (ec == 8) // U-syscall
      sepc += 4;
  }

  if (from_user) {
    cli();
    w_stvec((u64)utrap_entry - (u64)trampoline + TRAMPOLINE);
  }

  // ! sstatus和spec可能会被嵌套trap所修改,sret会清SPP位
  w_sstatus(sstatus);
  w_sepc(sepc);
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
  // print("cpu%u trigger %s\n", cpuid(), __func__);
  w_stimecmp(r_time() + TIME_CYCLE);
  if ((r_sstatus() & SSTATUS_SPP) == 0)
    yield();
}
static void
asy_extern(struct pt_regs* pt)
{
  u32 irq = plic_claim();
  switch (irq) {
  case IRQ_NONE:
    break; // 中断已经被其他核处理
  case IRQ_UART0:
    do_uart_irq();
    break;
  case IRQ_DISK:
    do_disk_irq();
    break;
  default:
    unknow_trap(pt);
  }
  plic_complete(irq);
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

extern void do_syscall(struct pt_regs* pt);
static void
syn_syscall_u(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  do_syscall(pt);
}
static void
syn_syscall_s(struct pt_regs* pt)
{
  print("cpu%u trigger %s\n", cpuid(), __func__);
  unknow_trap(pt);
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