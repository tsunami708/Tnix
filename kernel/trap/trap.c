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
#include "dev/irqf.h"
#include "trap/pt_reg.h"
#include "trap/plic.h"


#define IS_INTR(scause)   ((scause & (1UL << 63)) != 0)
#define SCAUSE_EC(scause) (scause & ~(1UL << 63))

extern void ktrap_entry(void);
extern void utrap_entry(void);

extern void yield(void);
extern char trampoline[];

// interrupt handler
#define ASY_IPI    1
#define ASY_TIMER  5
#define ASY_EXTERN 9
static void asy_timer(struct pt_regs*);
static void asy_extern(struct pt_regs*);

// exception handler
#define SYN_TEXT_MISALIGNED  0
#define SYN_TEXT_FAULT       1
#define SYN_TEXT_ILLEGAL     2
#define SYN_BREAKPOINT       3
#define SYN_LOAD_MISALIGNED  4
#define SYN_LOAD_FAULT       5
#define SYN_STORE_MISALIGNED 6
#define SYN_STORE_FAULT      7
#define SYN_SYSCALL_U        8
#define SYN_SYSCALL_S        9
#define SYN_TEXT_PAGE_FAULT  12
#define SYN_LOAD_PAGE_FAULT  13
#define SYN_STORE_PAGE_FAULT 15
static void syn_syscall_u(struct pt_regs*);


static const char* interrupt_name[10] = { [0 ... 9] = "UNKNOW" };
static const char* exception_name[16] = { [0 ... 15] = "UNKNOW" };
static void
unknow_trap(struct pt_regs* pt)
{
  bool intr = IS_INTR(pt->scause);
  u64 ec = SCAUSE_EC(pt->scause);
  panic("%s-sepc:%x-stval:%x", intr ? interrupt_name[ec] : exception_name[ec], pt->sepc, pt->stval);
}
typedef void (*trap_fn)(struct pt_regs* pt);
static trap_fn interrupt_funs[10] = { [0 ... 9] = unknow_trap };
static trap_fn exception_funs[16] = { [0 ... 15] = unknow_trap };


void
init_trap(void)
{
  interrupt_name[ASY_IPI] = "IPI";
  interrupt_name[ASY_TIMER] = "TIMER";
  interrupt_name[ASY_EXTERN] = "EXTERN";
  exception_name[SYN_TEXT_MISALIGNED] = "TEXT_MISALIGNED";
  exception_name[SYN_TEXT_FAULT] = "TEXT_FAULT";
  exception_name[SYN_TEXT_ILLEGAL] = "TEXT_ILLEGAL";
  exception_name[SYN_BREAKPOINT] = "BREAKPOINT";
  exception_name[SYN_LOAD_MISALIGNED] = "LOAD_MISALIGNED";
  exception_name[SYN_LOAD_FAULT] = "LOAD_FAULT";
  exception_name[SYN_STORE_MISALIGNED] = "STORE_MISALIGNED";
  exception_name[SYN_STORE_FAULT] = "STORE_FAULT";
  exception_name[SYN_SYSCALL_U] = "SYSCALL_U";
  exception_name[SYN_SYSCALL_S] = "SYSCALL_S";
  exception_name[SYN_TEXT_PAGE_FAULT] = "TEXT_PAGE_FAULT";
  exception_name[SYN_LOAD_PAGE_FAULT] = "LOAD_PAGE_FAULT";
  exception_name[SYN_STORE_PAGE_FAULT] = "STORE_PAGE_FAULT";
  interrupt_funs[ASY_TIMER] = asy_timer;
  interrupt_funs[ASY_EXTERN] = asy_extern;
  exception_funs[SYN_SYSCALL_U] = syn_syscall_u;
  w_stvec((u64)ktrap_entry); // ktrap_entry四字节对齐,地址低2位被解读为 Direct模式
}

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
    if (ec == 8) // syscall
      sepc += 4;
  }

  if (from_user) {
    cli();
    w_stvec((u64)utrap_entry - (u64)trampoline + TRAMPOLINE);
  }

  // sstatus和spec可能会被嵌套异常所修改,sret会清SPP位
  w_sstatus(sstatus);
  w_sepc(sepc);
}

static void
asy_timer(struct pt_regs* pt)
{
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
    break;
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

static inline __attribute__((always_inline)) void
syn_syscall_u(struct pt_regs* pt)
{
  extern void do_syscall(struct pt_regs * pt);
  do_syscall(pt);
}
