#include "config.h"
#include "type.h"
#include "riscv.h"
#include "cpu.h"
struct pt_regs;
extern void main();
extern int  do_trap(struct pt_regs*, u64);

__attribute__((aligned(16))) char cpu_stack[PGSIZE * NCPU];

struct cpu cpus[NCPU];

static void
init_timer()
{
  w_menvcfg(r_menvcfg() | (1UL << 63)); // 为 S 模式启用 stimecmp
  w_mcounteren(r_mcounteren() | 2);     // 使能S\U模式下的 time 系统寄存器
  w_stimecmp(r_time() + TIME_CYCLE);
}

void
start()
{
  u64 cpuid             = r_mhartid();
  cpus[cpuid].id        = cpuid;
  cpus[cpuid].trap_addr = (u64)do_trap;
  w_tp((u64)(cpus + cpuid));
  /*每一个核的tp寄存器保存自身的struct cpu地址,在整个运行期间保持不变*/

  // mstatus的bit11-bit12标识trap到M模式时之前的模式
  // 在启动阶段通过伪造一个trap方便后续以S模式进入到初始化函数
  u64 mstatus = r_mstatus();
  mstatus &= ~MSTATUS_MPP_MASK;
  mstatus &= ~MSTATUS_MIE; // 确保中断处于关闭状态
  mstatus &= ~MSTATUS_SIE;
  mstatus &= ~MSTATUS_MPIE;
  mstatus &= ~MSTATUS_SPIE;
  mstatus |= MSTATUS_MPP_S;
  w_mstatus(mstatus);
  w_mepc((u64)main); // mret的跳转地址


  w_satp(0);                    // 关闭分页
  w_medeleg(0xFFFF);            // 将异常委托给S模式
  w_mideleg(0xFFFF);            // 将中断委托给S模式
  w_pmpaddr0(0x3FFFFFFFFFFFFF); // 授权S模式物理地址访问空间
  w_pmpcfg0(0xF);               // 授权S模式物理地址访问权限

  init_timer();
  w_sie(r_sie() | SIE_STIE | SIE_SSIE);
  asm volatile("mret");

  // mret做的事
  // 1:恢复设置MIE,把mstatus 寄存器中的MPIE 字段设置到 mstatus 寄存器的MIE
  // 2:将处理器模式设置成之前保存到MPP字段的处理器模式。
  // 3:mepc寄存器保存的值设置到PC寄存器中
}