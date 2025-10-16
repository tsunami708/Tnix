#include "plic.h"

void
init_plic()
{
  if (cpuid() == 0) {
    set_plic_priority(IRQ_UART0, 1);
    set_plic_priority(IRQ_DISK, 1);
  }

  set_plic_my_threshold(0);
  set_plic_my_enable(IRQ_UART0, 1);
  set_plic_my_enable(IRQ_DISK, 1);

  w_sie(r_sie() | SIE_SEIE);
}