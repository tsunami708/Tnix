#include "cpu.h"
#include "alloc.h"
#include "printf.h"

volatile bool cpu_ok = false;
extern void   init_mem();
extern void   init_page();

void
main()
{
  if (cpuid() == 0) {
    init_mem(); // 物理地址初始化
    // init_page(); // 内核页表初始化
    __sync_synchronize();
    cpu_ok = true;
  } else {
    while (!cpu_ok)
      ;
    __sync_synchronize();
  }
  while (1) {
    struct page* p = kalloc();
    print("cpu%d get page %x\n", cpuid(), pa(p));
    kfree(p);
  }
}