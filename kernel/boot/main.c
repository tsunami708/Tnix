#include "cpu.h"
#include "printf.h"
volatile bool cpu_ok = false;
extern void   init_mem();

void
main()
{
  if (cpuid() == 0) {
    init_mem();
    __sync_synchronize();
    cpu_ok = true;
    for (int i = 0; i < 1000; ++i)
      printf("cpu0 output\n");
    panic("ENDING");
  } else {
    while (!cpu_ok)
      ;
    __sync_synchronize();
    while (1)
      printf("cpu%d output\n", cpuid());
  }
}