#include "plic.h"

void
init_plic()
{
  bool is_cpu0 = cpuid();
  if (is_cpu0)
    set_plic_my_threshold(0);
  for (int irq = 0; irq < 1024; ++irq) {
    set_plic_my_enable(irq, false);
    if (is_cpu0)
      set_plic_priority(irq, 1);
  }
  set_plic_my_enable(IRQ_UART0, true);
  set_plic_my_enable(IRQ_DISK, true);
  w_sie(r_sie() | SIE_SEIE);
}