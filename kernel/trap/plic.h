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
#include "config.h"

#include "util/types.h"
#include "task/cpu.h"

#define PLIC_BASE PLIC

/*
  设置每个中断的优先级
  ! 0xC000000~0xC000FFC
  ! ISQ : 0~1023
*/
// #define PLIC_PRIORITY(irq) (volatile u32*)(PLIC_BASE + irq * 4)
#define PLIC_PRIORITY(irq) (volatile u32*)(PLIC_BASE + (irq << 2))

/*
  中断使能位：每个处理器内核都有对应的中断使能位
  只使用监管模式的上下文
  ! 0xC0002000~0xC01FFFFC
  0号核S-context 0xC0002080 (每一个bit对应一个IRQ使能)
  1号核S-context 0xC0002180
*/
// #define PLIC_ENABLE(hartid) (volatile u32*)(PLIC_BASE + 0x2000 + hartid * 0x100 + 0x80)
#define PLIC_ENABLE(hartid) (volatile u32*)(PLIC_BASE + 0x2000 + (hartid << 8) + 0x80)

/*
  设置每个处理器中断的优先级阈值，当中断优先级大于阈值才会触发中断
  只使用监管模式的阈值
*/
// #define PLIC_THRESHOLD(hartid) (volatile u32*)(PLIC_BASE + 0x200000 + hartid * 0x2000 + 0x1000)
#define PLIC_THRESHOLD(hartid) (volatile u32*)(PLIC_BASE + 0x200000 + (hartid << 13) + 0x1000)

/*
  请求/完成寄存器
  只使用监管模式
*/
#define PLIC_CLAIM(hartid)     (volatile u32*)(PLIC_BASE + 0x200000 + (hartid << 13) + 0x1004)
#define PLIC_COMPETION(hartid) (volatile u32*)(PLIC_BASE + 0x200000 + (hartid << 13) + 0x1004)

#define IRQ_NONE  0
#define IRQ_DISK  1
#define IRQ_UART0 10

static inline void
set_plic_priority(u32 irq, u32 pri)
{
  *PLIC_PRIORITY(irq) = pri;
}

static inline void
set_plic_my_threshold(u32 thr)
{
  *(PLIC_THRESHOLD(cpuid())) = thr;
}

// cpu核向PLIC询问IRQ
static inline u32
plic_claim(void)
{
  return *PLIC_CLAIM(cpuid());
}

// cpu核告知PLIC IRQ已处理
static inline void
plic_complete(u32 irq)
{
  *PLIC_CLAIM(cpuid()) = irq;
}


static inline void
set_plic_my_enable(u32 irq, bool enable)
{
  u32 index = irq / 32;
  u32 bit = irq % 32;

  volatile u32* reg = (PLIC_ENABLE(cpuid()) + index * 4);
  if (enable)
    *reg |= (1 << bit);
  else
    *reg &= ~(1 << bit);
}