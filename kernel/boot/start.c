#include "config.h"
#include "type.h"
#include "riscv.h"

extern void main();

__attribute__((aligned(16))) char cpu_stack[PGSIZE * NCPU];


static void
init_timer()
{
  w_menvcfg(r_menvcfg() | (1L << 63)); // 为 S 模式启用 stimecmp
  w_mcounteren(r_mcounteren() | 2);    // 使能S\U模式下的 time 系统寄存器
  w_stimecmp(r_time() + 1000000);
}

void
start()
{
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
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
  w_tp(r_mhartid());
  init_timer();
  asm volatile("mret");

  // mret做的事
  // 1:恢复设置MIE,把mstatus 寄存器中的MPIE 字段设置到 mstatus 寄存器的MIE
  // 2:将处理器模式设置成之前保存到MPP字段的处理器模式。
  // 3:mepc寄存器保存的值设置到PC寄存器中
}