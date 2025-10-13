/*
• Interrupt Priorities registers:
  The interrupt priority for each interrupt source.
• Interrupt Pending Bits registers:
  The interrupt pending status of each interrupt source.
• Interrupt Enables registers:
  The enablement of interrupt source of each context.
• Priority Thresholds registers:
  The interrupt priority threshold of each context.
• Interrupt Claim registers:
  The register to acquire interrupt source ID of each context.
• Interrupt Completion registers:
  The register to send interrupt completion message to the associated gateway.
*/
#pragma once
#include "type.h"
#include "config.h"
#include "cpu.h"

#define PLIC_BASE PLIC
/*设置每个中断的优先级*/
#define PLIC_PRIORITY(irq) (volatile u32*)(PLIC_BASE + (irq << 2))

/*每个中断的 pending位，一位表示一个中断*/
#define PLIC_PENDING(irq) (volatile u32*)(PLIC_BASE + 0x1000 + ((irq >> 5) << 2))

/*中断使能位：每个处理器内核都有对应的中断使能位*/
#define PLIC_ENABLE(hartid) (volatile u32*)(PLIC_BASE + 0x2000 + hartid * 0x80)

/*设置每个处理器中断的优先级阈值，当中断优先级大于阈值才会触发中断*/
#define PLIC_THRESHOLD(hartid) (volatile u32*)(PLIC_BASE + 0x200000 + hartid * 0x1000)

/*请求/完成寄存器*/
#define PLIC_CLAIM(hartid)     (volatile u32*)(PLIC_BASE + 0x200004 + hartid * 0x1000)
#define PLIC_COMPETION(hartid) (volatile u32*)(PLIC_BASE + 0x200004 + hartid * 0x1000)

#define IRQ_DISK  1
#define IRQ_UART0 10

static inline void
set_plic_priority(u32 irq, u32 pri)
{
  *PLIC_PRIORITY(irq) = pri;
}
static inline void
set_plic_my_enable(u32 irq, bool enable)
{
  u32 index = irq / 32;
  u32 bit   = irq % 32;

  volatile u32* reg = (PLIC_ENABLE(cpuid()) + index * 4);
  if (enable)
    *reg |= (1 << bit);
  else
    *reg &= ~(1 << bit);
}
static inline void
set_plic_my_threshold(u32 thr)
{
  *(PLIC_THRESHOLD(cpuid())) = thr;
}
static inline u32
plic_claim(void)
{
  return *PLIC_CLAIM(cpuid());
}
static inline void
plic_complete(u32 irq)
{
  *PLIC_CLAIM(cpuid()) = irq;
}