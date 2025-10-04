#include "cpu.h"
#include "alloc.h"
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
  } else {
    while (!cpu_ok)
      ;
    __sync_synchronize();
  }
  while (1) {
    struct page* p = kalloc();
    print("cpu%d get a page %x\n", cpuid(), PA(p));
    kfree(p);
  }
}